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

#include "libpandabase/mem/mem.h"
#include "libpandabase/os/mem.h"
#include "libpandabase/utils/logger.h"
#include "runtime/tests/allocator_test_base.h"
#include "runtime/mem/internal_allocator-inl.h"

#include <gtest/gtest.h>

namespace panda::mem::test {

class InternalAllocatorTest : public testing::Test {
public:
    InternalAllocatorTest()
    {
        panda::mem::MemConfig::Initialize(0, MEMORY_POOL_SIZE, 0, 0);
        PoolManager::Initialize();
        mem_stats_ = new mem::MemStatsType();
        allocator_ = new InternalAllocatorT<InternalAllocatorConfig::PANDA_ALLOCATORS>(mem_stats_);
    }

    ~InternalAllocatorTest()
    {
        delete static_cast<Allocator *>(allocator_);
        PoolManager::Finalize();
        panda::mem::MemConfig::Finalize();
        delete mem_stats_;
    }

protected:
    mem::MemStatsType *mem_stats_ {nullptr};
    InternalAllocatorPtr allocator_ {nullptr};

    static constexpr size_t MEMORY_POOL_SIZE = 16_MB;

    void InfinitiveAllocate(size_t alloc_size)
    {
        void *mem = nullptr;
        do {
            mem = allocator_->Alloc(alloc_size);
        } while (mem != nullptr);
    }

    // Check that we don't have OOM and there is free space for mem pools
    bool CheckFreeSpaceForPools() const
    {
        size_t current_space_size = PoolManager::GetMmapMemPool()->internal_space_current_size_;
        size_t max_space_size = PoolManager::GetMmapMemPool()->internal_space_max_size_;
        ASSERT(current_space_size <= max_space_size);
        return (max_space_size - current_space_size) >= InternalAllocator<>::RunSlotsAllocatorT::GetMinPoolSize();
    }
};

TEST_F(InternalAllocatorTest, AvoidInfiniteLoopTest)
{
    // Regular object sizes
    InfinitiveAllocate(RunSlots<>::MaxSlotSize());
    // Large object sizes
    InfinitiveAllocate(FreeListAllocator<EmptyMemoryConfig>::GetMaxSize());
    // Humongous object sizes
    InfinitiveAllocate(FreeListAllocator<EmptyMemoryConfig>::GetMaxSize() + 1);
}

struct A {
    static size_t count;
    A()
    {
        value = ++count;
    }
    ~A()
    {
        --count;
    }

    uint8_t value;
};

size_t A::count = 0;

TEST_F(InternalAllocatorTest, NewDeleteArray)
{
    constexpr size_t COUNT = 5;

    auto arr = allocator_->New<A[]>(COUNT);
    ASSERT_NE(arr, nullptr);
    ASSERT_EQ(ToUintPtr(arr) % DEFAULT_ALIGNMENT_IN_BYTES, 0);
    ASSERT_EQ(A::count, COUNT);
    for (uint8_t i = 1; i <= COUNT; ++i) {
        ASSERT_EQ(arr[i - 1].value, i);
    }
    allocator_->DeleteArray(arr);
    ASSERT_EQ(A::count, 0);
}

TEST_F(InternalAllocatorTest, ZeroSizeTest)
{
    ASSERT(allocator_->Alloc(0) == nullptr);
    // Check that zero-size allocation did not result in infinite pool allocations
    ASSERT(CheckFreeSpaceForPools());

    // Checks on correct allocations of different size
    // Regular object size
    void *mem = allocator_->Alloc(RunSlots<>::MaxSlotSize());
    ASSERT(mem != nullptr);
    allocator_->Free(mem);

    // Large object size
    mem = allocator_->Alloc(FreeListAllocator<EmptyMemoryConfig>::GetMaxSize());
    ASSERT(mem != nullptr);
    allocator_->Free(mem);

    // Humongous object size
    mem = allocator_->Alloc(FreeListAllocator<EmptyMemoryConfig>::GetMaxSize() + 1);
    ASSERT(mem != nullptr);
    allocator_->Free(mem);
}

}  // namespace panda::mem::test
