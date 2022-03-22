/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#include <gtest/gtest.h>
#include <sys/mman.h>

#include "libpandabase/mem/mem.h"
#include "libpandabase/os/mem.h"
#include "libpandabase/utils/asan_interface.h"
#include "runtime/include/class_linker.h"
#include "runtime/include/runtime.h"
#include "runtime/mem/freelist_allocator-inl.h"
#include "runtime/mem/gc/hybrid-gc/hybrid_object_allocator.h"
#include "runtime/mem/humongous_obj_allocator-inl.h"
#include "runtime/tests/class_linker_test_extension.h"

namespace panda::mem {
class HybridObjectAllocatorTest : public testing::Test {
public:
    HybridObjectAllocatorTest()
    {
        options_.SetShouldLoadBootPandaFiles(false);
        options_.SetShouldInitializeIntrinsics(false);
        Runtime::Create(options_);
        thread_ = panda::MTManagedThread::GetCurrent();
        thread_->ManagedCodeBegin();
    }
    ~HybridObjectAllocatorTest()
    {
        thread_->ManagedCodeEnd();
        Runtime::Destroy();
    }

    bool AllocatedByLargeObjAllocator(FreeListAllocator<ObjectAllocConfig> *allocator, void *mem)
    {
        return allocator->AllocatedByFreeListAllocator(mem);
    }

    bool AllocatedByHumongousObjAllocator(HumongousObjAllocator<ObjectAllocConfig> *allocator, void *mem)
    {
        return allocator->AllocatedByHumongousObjAllocator(mem);
    }

protected:
    panda::MTManagedThread *thread_ {nullptr};
    RuntimeOptions options_;
};

TEST_F(HybridObjectAllocatorTest, AllocateInLargeAllocator)
{
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    HybridObjectAllocator allocator(mem_stats, false);
    ClassLinker *class_linker = Runtime::GetCurrent()->GetClassLinker();
    ASSERT_NE(class_linker, nullptr);
    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);

    auto allocate_helper = [&ctx](HybridObjectAllocator &alloc, ClassRoot class_root, size_t size) -> void * {
        Class *klass = nullptr;
        klass = Runtime::GetCurrent()->GetClassLinker()->GetExtension(ctx)->GetClassRoot(class_root);
        return alloc.AllocateInLargeAllocator(size, DEFAULT_ALIGNMENT, klass);
    };

    void *mem = nullptr;
    mem = allocate_helper(allocator, ClassRoot::CLASS, HybridObjectAllocator::GetLargeThreshold());
    ASSERT_EQ(mem, nullptr);

    mem = allocate_helper(allocator, ClassRoot::ARRAY_I8, HybridObjectAllocator::GetLargeThreshold());
    ASSERT_NE(mem, nullptr);
    ASSERT_TRUE(AllocatedByLargeObjAllocator(allocator.GetLargeObjectAllocator(), mem));

    size_t size = HybridObjectAllocator::LargeObjectAllocator::GetMaxSize() + 1;
    mem = allocate_helper(allocator, ClassRoot::ARRAY_I8, size);
    ASSERT_NE(mem, nullptr);
    ASSERT_TRUE(AllocatedByHumongousObjAllocator(allocator.GetHumongousObjectAllocator(), mem));

    mem = allocate_helper(allocator, ClassRoot::STRING, HybridObjectAllocator::GetLargeThreshold());
    ASSERT_NE(mem, nullptr);
    ASSERT_TRUE(AllocatedByLargeObjAllocator(allocator.GetLargeObjectAllocator(), mem));

    size = HybridObjectAllocator::LargeObjectAllocator::GetMaxSize() + 1;
    mem = allocate_helper(allocator, ClassRoot::STRING, size);
    ASSERT_NE(mem, nullptr);
    ASSERT_TRUE(AllocatedByHumongousObjAllocator(allocator.GetHumongousObjectAllocator(), mem));
    delete mem_stats;
}

TEST_F(HybridObjectAllocatorTest, AllocateInNonLargeAllocator)
{
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    HybridObjectAllocator allocator(mem_stats, false);
    void *mem = allocator.Allocate(HybridObjectAllocator::GetLargeThreshold(), DEFAULT_ALIGNMENT, nullptr);
    ASSERT_NE(mem, nullptr);
    delete mem_stats;
}

}  // namespace panda::mem
