/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/mman.h>

#include "libpandabase/mem/mem.h"
#include "libpandabase/os/mem.h"
#include "libpandabase/utils/asan_interface.h"
#include "libpandabase/utils/logger.h"
#include "libpandabase/utils/math_helpers.h"
#include "runtime/include/class_linker.h"
#include "runtime/include/runtime.h"
#include "runtime/mem/alloc_config.h"
#include "runtime/mem/region_allocator-inl.h"
#include "runtime/mem/tlab.h"
#include "runtime/tests/allocator_test_base.h"

#include "runtime/mem/rem_set-inl.h"
#include "runtime/mem/region_space.h"
#include "runtime/mem/gc/gc.h"

namespace panda::mem::test {

using NonObjectRegionAllocator = RegionAllocator<EmptyAllocConfigWithCrossingMap>;
using RemSetWithCommonLock = RemSet<RemSetLockConfig::CommonLock>;

class RemSetTest : public testing::Test {
public:
    RemSetTest()
    {
        options_.SetShouldLoadBootPandaFiles(false);
        options_.SetShouldInitializeIntrinsics(false);
        Runtime::Create(options_);
        thread_ = panda::MTManagedThread::GetCurrent();
        thread_->ManagedCodeBegin();
        Init();
    }

    ~RemSetTest()
    {
        thread_->ManagedCodeEnd();
        card_table_ = nullptr;
        Runtime::Destroy();
    }

    void Init()
    {
        auto lang = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
        ext_ = Runtime::GetCurrent()->GetClassLinker()->GetExtension(lang);
        card_table_ = MakePandaUnique<CardTable>(Runtime::GetCurrent()->GetInternalAllocator(),
                                                 PoolManager::GetMmapMemPool()->GetMinObjectAddress(),
                                                 PoolManager::GetMmapMemPool()->GetTotalObjectSize());
        card_table_->Initialize();
    }

protected:
    panda::MTManagedThread *thread_ {nullptr};
    RuntimeOptions options_;
    ClassLinkerExtension *ext_ {nullptr};
    PandaUniquePtr<CardTable> card_table_ {nullptr};
};

TEST_F(RemSetTest, AddRefTest)
{
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    NonObjectRegionAllocator allocator(mem_stats);
    auto cls = ext_->CreateClass(nullptr, 0, 0, sizeof(panda::Class));
    cls->SetObjectSize(allocator.GetMaxRegularObjectSize());

    auto obj1 = allocator.Alloc(allocator.GetMaxRegularObjectSize());
    static_cast<ObjectHeader *>(obj1)->SetClass(cls);
    auto region1 = Region::AddrToRegion(obj1);

    auto obj2 = allocator.Alloc(allocator.GetMaxRegularObjectSize());
    static_cast<ObjectHeader *>(obj2)->SetClass(cls);
    auto region2 = Region::AddrToRegion(obj2);

    // simulate gc process: mark obj2 and update live bitmap with mark bitmap
    region2->GetMarkBitmap()->Set(obj2);
    region2->SwapMarkBitmap();

    auto remset1 = region1->GetRemSet();
    remset1->SetCardTable(card_table_.get());
    remset1->AddRef(obj2);

    PandaVector<void *> test_list;
    auto visitor = [&test_list](void *obj) { test_list.push_back(obj); };
    remset1->VisitMarkedCards(visitor);
    ASSERT_EQ(test_list.size(), 1);
    auto first = test_list.front();
    ASSERT_EQ(first, obj2);

    ext_->FreeClass(cls);
    delete mem_stats;
}

TEST_F(RemSetTest, AddRefWithAddrTest)
{
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    NonObjectRegionAllocator allocator(mem_stats);
    auto cls = ext_->CreateClass(nullptr, 0, 0, sizeof(panda::Class));
    cls->SetObjectSize(allocator.GetMaxRegularObjectSize());

    auto obj1 = allocator.Alloc(allocator.GetMaxRegularObjectSize());
    static_cast<ObjectHeader *>(obj1)->SetClass(cls);
    auto region1 = Region::AddrToRegion(obj1);

    auto obj2 = allocator.Alloc(allocator.GetMaxRegularObjectSize());
    static_cast<ObjectHeader *>(obj2)->SetClass(cls);
    auto region2 = Region::AddrToRegion(obj2);

    // simulate gc process: mark obj2 and update live bitmap with mark bitmap
    region1->GetMarkBitmap()->Set(obj1);
    region1->SwapMarkBitmap();

    RemSetWithCommonLock::AddRefWithAddr(obj1, obj2);
    auto remset2 = region2->GetRemSet();
    remset2->SetCardTable(card_table_.get());

    PandaVector<void *> test_list;
    auto visitor = [&test_list](void *obj) { test_list.push_back(obj); };
    remset2->VisitMarkedCards(visitor);
    ASSERT_EQ(test_list.size(), 0);

    region1->AddFlag(panda::mem::RegionFlag::IS_OLD);
    region1->RmvFlag(panda::mem::RegionFlag::IS_EDEN);
    RemSetWithCommonLock::AddRefWithAddr(obj1, obj2);
    remset2->VisitMarkedCards(visitor);
    ASSERT_EQ(test_list.size(), 1);

    auto first = test_list.front();
    ASSERT_EQ(first, obj1);

    ext_->FreeClass(cls);
    delete mem_stats;
}

TEST_F(RemSetTest, TravelObjectToAddRefTest)
{
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    NonObjectRegionAllocator allocator(mem_stats);
    auto cls = ext_->CreateClass(nullptr, 0, 0, sizeof(panda::Class));
    cls->SetObjectSize(allocator.GetMaxRegularObjectSize());
    cls->SetRefFieldsNum(1, false);
    auto offset = ObjectHeader::ObjectHeaderSize();
    cls->SetRefFieldsOffset(offset, false);

    auto obj1 = allocator.Alloc(allocator.GetMaxRegularObjectSize());
    static_cast<ObjectHeader *>(obj1)->SetClass(cls);
    auto region1 = Region::AddrToRegion(obj1);

    auto obj2 = allocator.Alloc(allocator.GetMaxRegularObjectSize());
    static_cast<ObjectHeader *>(obj2)->SetClass(cls);
    auto region2 = Region::AddrToRegion(obj2);

    // simulate gc process: mark obj2 and update live bitmap with mark bitmap
    region1->GetMarkBitmap()->Set(obj1);
    region1->SwapMarkBitmap();

    static_cast<ObjectHeader *>(obj1)->SetFieldObject<false, false>(offset, static_cast<ObjectHeader *>(obj2));

    RemSetWithCommonLock::TraverseObjectToAddRef(obj1);
    auto remset2 = region2->GetRemSet();
    remset2->SetCardTable(card_table_.get());

    PandaVector<void *> test_list;
    auto visitor = [&test_list](void *obj) { test_list.push_back(obj); };
    remset2->VisitMarkedCards(visitor);
    ASSERT_EQ(test_list.size(), 0);

    region1->AddFlag(panda::mem::RegionFlag::IS_OLD);
    region1->RmvFlag(panda::mem::RegionFlag::IS_EDEN);
    RemSetWithCommonLock::TraverseObjectToAddRef(obj1);
    remset2->VisitMarkedCards(visitor);
    ASSERT_EQ(test_list.size(), 1);

    auto first = test_list.front();
    ASSERT_EQ(first, obj1);

    ext_->FreeClass(cls);
    delete mem_stats;
}

}  // namespace panda::mem::test
