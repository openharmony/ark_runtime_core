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

#include <array>
#include <thread>

#include "gtest/gtest.h"
#include "iostream"
#include "runtime/class_linker_context.h"
#include "runtime/include/class_linker.h"
#include "runtime/include/runtime.h"
#include "runtime/include/panda_vm.h"
#include "runtime/mem/vm_handle.h"
#include "runtime/handle_base-inl.h"
#include "runtime/handle_scope-inl.h"
#include "runtime/mem/mem_stats.h"
#include "runtime/mem/mem_stats_default.h"

namespace panda::mem::test {

class MemStatsGCTest : public testing::Test {
public:
    void SetupRuntime(std::string gc_type)
    {
        RuntimeOptions options;
        options.SetShouldLoadBootPandaFiles(false);
        options.SetShouldInitializeIntrinsics(false);
        options.SetUseTlabForAllocations(false);
        options.SetGcType(gc_type);
        options.SetRunGcInPlace(true);
        bool success = Runtime::Create(options);
        ASSERT_TRUE(success) << "Cannot create Runtime";
        thread_ = panda::MTManagedThread::GetCurrent();
        thread_->ManagedCodeBegin();
    }

    template <uint64_t object_count>
    void MemStatsTest(uint64_t tries, size_t object_size);

    void TearDown() override
    {
        thread_->ManagedCodeEnd();
        bool success = Runtime::Destroy();
        ASSERT_TRUE(success) << "Cannot destroy Runtime";
    }

    panda::MTManagedThread *thread_;
};

template <uint64_t object_count>
void MemStatsGCTest::MemStatsTest(uint64_t tries, size_t object_size)
{
    ASSERT(object_size >= sizeof(coretypes::String));
    mem::MemStatsType *stats = thread_->GetVM()->GetMemStats();
    ASSERT_NE(stats, nullptr);

    auto class_linker = Runtime::GetCurrent()->GetClassLinker();
    ASSERT_NE(class_linker, nullptr);
    auto allocator = class_linker->GetAllocator();

    std::string simple_string;
    for (size_t j = 0; j < object_size - sizeof(coretypes::String); j++) {
        simple_string.append("x");
    }
    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto object_allocator = thread_->GetVM()->GetHeapManager()->GetObjectAllocator().AsObjectAllocator();
    thread_->GetVM()->GetGC()->WaitForGCInManaged(GCTask(GCTaskCause::EXPLICIT_CAUSE));

    size_t alloc_size = simple_string.size() + sizeof(coretypes::String);
    size_t aligment_size;
    size_t aligment_diff;
    if (alloc_size < object_allocator->GetRegularObjectMaxSize()) {
        aligment_size = 1UL << RunSlots<>::ConvertToPowerOfTwoUnsafe(alloc_size);
        aligment_diff = aligment_size - alloc_size;
    } else {
        aligment_size = AlignUp(alloc_size, GetAlignmentInBytes(FREELIST_DEFAULT_ALIGNMENT));
        aligment_diff = 2U * (aligment_size - alloc_size);
    }

    uint64_t allocated_objects = stats->GetTotalObjectsAllocated();
    uint64_t allocated_bytes = stats->GetAllocated(SpaceType::SPACE_TYPE_OBJECT);
    uint64_t freed_objects = stats->GetTotalObjectsFreed();
    uint64_t freed_bytes = stats->GetFreed(SpaceType::SPACE_TYPE_OBJECT);
    uint64_t diff_total = 0;
    std::array<VMHandle<coretypes::String> *, object_count> handlers;
    for (size_t i = 0; i < tries; i++) {
        [[maybe_unused]] HandleScope<ObjectHeader *> scope(thread_);
        for (uint64_t j = 0; j < object_count; j++) {
            coretypes::String *string_obj =
                coretypes::String::CreateFromMUtf8(reinterpret_cast<const uint8_t *>(&simple_string[0]),
                                                   simple_string.length(), ctx, Runtime::GetCurrent()->GetPandaVM());
            ASSERT_NE(string_obj, nullptr);
            handlers[j] = allocator->New<VMHandle<coretypes::String>>(thread_, string_obj);
        }
        allocated_objects += object_count;
        allocated_bytes += object_count * alloc_size;
        diff_total += object_count * aligment_diff;
        ASSERT_EQ(allocated_objects, stats->GetTotalObjectsAllocated());
        ASSERT_LE(allocated_bytes, stats->GetAllocated(SpaceType::SPACE_TYPE_OBJECT));
        ASSERT_GE(allocated_bytes + diff_total, stats->GetAllocated(SpaceType::SPACE_TYPE_OBJECT));

        // run GC
        thread_->GetVM()->GetGC()->WaitForGCInManaged(GCTask(GCTaskCause::EXPLICIT_CAUSE));
        ASSERT_EQ(allocated_objects, stats->GetTotalObjectsAllocated());
        ASSERT_LE(allocated_bytes, stats->GetAllocated(SpaceType::SPACE_TYPE_OBJECT));
        ASSERT_GE(allocated_bytes + diff_total, stats->GetAllocated(SpaceType::SPACE_TYPE_OBJECT));
        ASSERT_EQ(freed_objects, stats->GetTotalObjectsFreed());
        ASSERT_LE(freed_bytes, stats->GetFreed(SpaceType::SPACE_TYPE_OBJECT));
        ASSERT_GE(freed_bytes + diff_total, stats->GetFreed(SpaceType::SPACE_TYPE_OBJECT));

        for (uint64_t j = 0; j < object_count; j++) {
            allocator->Delete(handlers[j]);
        }
        freed_objects += object_count;
        freed_bytes += object_count * alloc_size;
    }
}

constexpr size_t OBJECTS_SIZE[] = {
    32,   // RunSlots: aligned & object_size = RunSlot size
    72,   // RunSlots: aligned & object_size != RunSlot size
    129,  // RunSlots: not aligned
    512,  // FreeList: aligned
    1025  // FreeList: not aligned
};
constexpr size_t NUM_SIZES = sizeof(OBJECTS_SIZE) / sizeof(OBJECTS_SIZE[0]);

TEST_F(MemStatsGCTest, GenGcTest)
{
    constexpr uint64_t OBJECTS_COUNT = 80;
    constexpr uint64_t TRIES = 4;

    SetupRuntime("gen-gc");
    for (size_t i = 0; i < NUM_SIZES; i++) {
        MemStatsTest<OBJECTS_COUNT>(TRIES, OBJECTS_SIZE[i]);
    }
}

TEST_F(MemStatsGCTest, StwGcTest)
{
    constexpr uint64_t OBJECTS_COUNT = 500;
    constexpr uint64_t TRIES = 10;

    SetupRuntime("stw");
    for (size_t i = 0; i < NUM_SIZES; i++) {
        MemStatsTest<OBJECTS_COUNT>(TRIES, OBJECTS_SIZE[i]);
    }
}

}  // namespace panda::mem::test
