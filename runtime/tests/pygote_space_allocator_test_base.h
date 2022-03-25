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

#ifndef PANDA_RUNTIME_TESTS_PYGOTE_SPACE_ALLOCATOR_TEST_BASE_H_
#define PANDA_RUNTIME_TESTS_PYGOTE_SPACE_ALLOCATOR_TEST_BASE_H_

#include <sys/mman.h>
#include <gtest/gtest.h>

#include "libpandabase/os/mem.h"
#include "libpandabase/utils/logger.h"
#include "runtime/mem/runslots_allocator-inl.h"
#include "runtime/mem/pygote_space_allocator-inl.h"
#include "runtime/include/object_header.h"
#include "runtime/mem/refstorage/global_object_storage.h"

namespace panda::mem {

class PygoteSpaceAllocatorTest : public testing::Test {
public:
    using PygoteAllocator = PygoteSpaceAllocator<ObjectAllocConfig>;

    PygoteSpaceAllocatorTest() {}

    ~PygoteSpaceAllocatorTest() {}

protected:
    PygoteAllocator *GetPygoteSpaceAllocator()
    {
        return thread_->GetVM()->GetHeapManager()->GetObjectAllocator().AsObjectAllocator()->GetPygoteSpaceAllocator();
    }

    Class *GetObjectClass()
    {
        auto runtime = panda::Runtime::GetCurrent();
        LanguageContext ctx = runtime->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
        return runtime->GetClassLinker()->GetExtension(ctx)->GetClassRoot(ClassRoot::OBJECT);
    }

    void PygoteFork()
    {
        thread_->ManagedCodeEnd();
        auto runtime = panda::Runtime::GetCurrent();
        runtime->PreZygoteFork();
        runtime->PostZygoteFork();
        thread_->ManagedCodeBegin();
    }

    void TriggerGc()
    {
        auto gc = thread_->GetVM()->GetGC();
        auto task = GCTask(GCTaskCause::EXPLICIT_CAUSE);
        // trigger tenured gc
        gc->WaitForGCInManaged(task);
        gc->WaitForGCInManaged(task);
        gc->WaitForGCInManaged(task);
    }

    panda::MTManagedThread *thread_ {nullptr};
    RuntimeOptions options_;

    void InitAllocTest();

    void ForkedAllocTest();

    void NonMovableLiveObjectAllocTest();

    void NonMovableUnliveObjectAllocTest();

    void MovableLiveObjectAllocTest();

    void MovableUnliveObjectAllocTest();

    void MuchObjectAllocTest();
};

inline void PygoteSpaceAllocatorTest::InitAllocTest()
{
    [[maybe_unused]] auto pygote_space_allocator = GetPygoteSpaceAllocator();
    auto cls = GetObjectClass();

    auto non_movable_header = panda::ObjectHeader::CreateNonMovable(cls);
    ASSERT_NE(non_movable_header, nullptr);
    ASSERT_TRUE(pygote_space_allocator->ContainObject(static_cast<ObjectHeader *>(non_movable_header)));
    ASSERT_TRUE(pygote_space_allocator->IsLive(static_cast<ObjectHeader *>(non_movable_header)));

    auto movable_header = panda::ObjectHeader::Create(cls);
    ASSERT_NE(non_movable_header, nullptr);
    ASSERT_FALSE(pygote_space_allocator->ContainObject(static_cast<ObjectHeader *>(movable_header)));

    pygote_space_allocator->Free(non_movable_header);
    ASSERT_TRUE(pygote_space_allocator->ContainObject(static_cast<ObjectHeader *>(non_movable_header)));
    ASSERT_FALSE(pygote_space_allocator->IsLive(static_cast<ObjectHeader *>(non_movable_header)));
}

inline void PygoteSpaceAllocatorTest::ForkedAllocTest()
{
    [[maybe_unused]] auto pygote_space_allocator = GetPygoteSpaceAllocator();
    auto cls = GetObjectClass();

    PygoteFork();

    auto non_movable_header = panda::ObjectHeader::CreateNonMovable(cls);
    ASSERT_NE(non_movable_header, nullptr);
    ASSERT_FALSE(pygote_space_allocator->ContainObject(static_cast<ObjectHeader *>(non_movable_header)));

    auto movable_header = panda::ObjectHeader::Create(cls);
    ASSERT_NE(movable_header, nullptr);
    ASSERT_FALSE(pygote_space_allocator->ContainObject(static_cast<ObjectHeader *>(movable_header)));
}

inline void PygoteSpaceAllocatorTest::NonMovableLiveObjectAllocTest()
{
    [[maybe_unused]] auto pygote_space_allocator = GetPygoteSpaceAllocator();
    auto cls = GetObjectClass();
    auto global_object_storage = thread_->GetVM()->GetGlobalObjectStorage();

    auto non_movable_header = panda::ObjectHeader::CreateNonMovable(cls);
    ASSERT_NE(non_movable_header, nullptr);
    ASSERT_TRUE(pygote_space_allocator->ContainObject(static_cast<ObjectHeader *>(non_movable_header)));
    ASSERT_TRUE(pygote_space_allocator->IsLive(static_cast<ObjectHeader *>(non_movable_header)));
    [[maybe_unused]] auto *ref =
        global_object_storage->Add(non_movable_header, panda::mem::Reference::ObjectType::GLOBAL);

    PygoteFork();

    ASSERT_TRUE(pygote_space_allocator->ContainObject(static_cast<ObjectHeader *>(non_movable_header)));
    ASSERT_TRUE(pygote_space_allocator->IsLive(static_cast<ObjectHeader *>(non_movable_header)));

    TriggerGc();

    ASSERT_TRUE(pygote_space_allocator->ContainObject(static_cast<ObjectHeader *>(non_movable_header)));
    ASSERT_TRUE(pygote_space_allocator->IsLive(static_cast<ObjectHeader *>(non_movable_header)));

    pygote_space_allocator->Free(non_movable_header);
    ASSERT_TRUE(pygote_space_allocator->ContainObject(static_cast<ObjectHeader *>(non_movable_header)));
    ASSERT_FALSE(pygote_space_allocator->IsLive(static_cast<ObjectHeader *>(non_movable_header)));
}

inline void PygoteSpaceAllocatorTest::NonMovableUnliveObjectAllocTest()
{
    [[maybe_unused]] auto pygote_space_allocator = GetPygoteSpaceAllocator();
    auto cls = GetObjectClass();
    auto global_object_storage = thread_->GetVM()->GetGlobalObjectStorage();

    auto non_movable_header = panda::ObjectHeader::CreateNonMovable(cls);
    ASSERT_NE(non_movable_header, nullptr);
    ASSERT_TRUE(pygote_space_allocator->ContainObject(static_cast<ObjectHeader *>(non_movable_header)));
    ASSERT_TRUE(pygote_space_allocator->IsLive(static_cast<ObjectHeader *>(non_movable_header)));
    [[maybe_unused]] auto *ref =
        global_object_storage->Add(non_movable_header, panda::mem::Reference::ObjectType::GLOBAL);

    PygoteFork();

    ASSERT_TRUE(pygote_space_allocator->ContainObject(static_cast<ObjectHeader *>(non_movable_header)));
    ASSERT_TRUE(pygote_space_allocator->IsLive(static_cast<ObjectHeader *>(non_movable_header)));
    global_object_storage->Remove(ref);

    TriggerGc();

    ASSERT_TRUE(pygote_space_allocator->ContainObject(static_cast<ObjectHeader *>(non_movable_header)));
    ASSERT_FALSE(pygote_space_allocator->IsLive(static_cast<ObjectHeader *>(non_movable_header)));
}

inline void PygoteSpaceAllocatorTest::MovableLiveObjectAllocTest()
{
    [[maybe_unused]] auto pygote_space_allocator = GetPygoteSpaceAllocator();
    auto cls = GetObjectClass();
    auto global_object_storage = thread_->GetVM()->GetGlobalObjectStorage();

    auto movable_header = panda::ObjectHeader::Create(cls);
    ASSERT_NE(movable_header, nullptr);
    ASSERT_FALSE(pygote_space_allocator->ContainObject(static_cast<ObjectHeader *>(movable_header)));
    [[maybe_unused]] auto *ref = global_object_storage->Add(movable_header, panda::mem::Reference::ObjectType::GLOBAL);

    PygoteFork();

    auto obj = global_object_storage->Get(ref);
    ASSERT_TRUE(pygote_space_allocator->ContainObject(static_cast<ObjectHeader *>(obj)));
    ASSERT_TRUE(pygote_space_allocator->IsLive(static_cast<ObjectHeader *>(obj)));

    TriggerGc();

    ASSERT_TRUE(pygote_space_allocator->ContainObject(static_cast<ObjectHeader *>(obj)));
    ASSERT_TRUE(pygote_space_allocator->IsLive(static_cast<ObjectHeader *>(obj)));
}

inline void PygoteSpaceAllocatorTest::MovableUnliveObjectAllocTest()
{
    [[maybe_unused]] auto pygote_space_allocator = GetPygoteSpaceAllocator();
    auto cls = GetObjectClass();
    auto global_object_storage = thread_->GetVM()->GetGlobalObjectStorage();

    auto movable_header = panda::ObjectHeader::Create(cls);
    ASSERT_NE(movable_header, nullptr);
    ASSERT_FALSE(pygote_space_allocator->ContainObject(static_cast<ObjectHeader *>(movable_header)));
    [[maybe_unused]] auto *ref = global_object_storage->Add(movable_header, panda::mem::Reference::ObjectType::GLOBAL);

    PygoteFork();

    auto obj = global_object_storage->Get(ref);
    ASSERT_TRUE(pygote_space_allocator->ContainObject(static_cast<ObjectHeader *>(obj)));
    ASSERT_TRUE(pygote_space_allocator->IsLive(static_cast<ObjectHeader *>(obj)));
    global_object_storage->Remove(ref);

    TriggerGc();

    ASSERT_TRUE(pygote_space_allocator->ContainObject(static_cast<ObjectHeader *>(obj)));
    ASSERT_FALSE(pygote_space_allocator->IsLive(static_cast<ObjectHeader *>(obj)));
}

inline void PygoteSpaceAllocatorTest::MuchObjectAllocTest()
{
    [[maybe_unused]] auto pygote_space_allocator = GetPygoteSpaceAllocator();
    auto cls = GetObjectClass();
    auto global_object_storage = thread_->GetVM()->GetGlobalObjectStorage();

    static constexpr size_t obj_num = 1024;

    PandaVector<Reference *> movable_refs;
    PandaVector<Reference *> non_movable_refs;
    for (size_t i = 0; i < obj_num; i++) {
        auto movable = panda::ObjectHeader::Create(cls);
        movable_refs.push_back(global_object_storage->Add(movable, panda::mem::Reference::ObjectType::GLOBAL));
        auto non_movable = panda::ObjectHeader::CreateNonMovable(cls);
        non_movable_refs.push_back(global_object_storage->Add(non_movable, panda::mem::Reference::ObjectType::GLOBAL));
    }

    PygoteFork();

    PandaVector<ObjectHeader *> movable_objs;
    PandaVector<ObjectHeader *> non_movable_objs;
    for (auto movalbe_ref : movable_refs) {
        auto obj = global_object_storage->Get(movalbe_ref);
        ASSERT_TRUE(pygote_space_allocator->ContainObject(static_cast<ObjectHeader *>(obj)));
        ASSERT_TRUE(pygote_space_allocator->IsLive(static_cast<ObjectHeader *>(obj)));
        global_object_storage->Remove(movalbe_ref);
        movable_objs.push_back(obj);
    }

    for (auto non_movalbe_ref : non_movable_refs) {
        auto obj = global_object_storage->Get(non_movalbe_ref);
        ASSERT_TRUE(pygote_space_allocator->ContainObject(static_cast<ObjectHeader *>(obj)));
        ASSERT_TRUE(pygote_space_allocator->IsLive(static_cast<ObjectHeader *>(obj)));
        global_object_storage->Remove(non_movalbe_ref);
        non_movable_objs.push_back(obj);
    }

    TriggerGc();

    for (auto movalbe_obj : movable_objs) {
        ASSERT_TRUE(pygote_space_allocator->ContainObject(static_cast<ObjectHeader *>(movalbe_obj)));
        ASSERT_FALSE(pygote_space_allocator->IsLive(static_cast<ObjectHeader *>(movalbe_obj)));
    }

    for (auto non_movalbe_obj : non_movable_objs) {
        ASSERT_TRUE(pygote_space_allocator->ContainObject(static_cast<ObjectHeader *>(non_movalbe_obj)));
        ASSERT_FALSE(pygote_space_allocator->IsLive(static_cast<ObjectHeader *>(non_movalbe_obj)));
    }
}

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_TESTS_PYGOTE_SPACE_ALLOCATOR_TEST_BASE_H_
