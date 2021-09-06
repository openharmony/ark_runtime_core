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
#include "runtime/include/runtime.h"
#include "runtime/mem/alloc_config.h"
#include "runtime/mem/freelist_allocator-inl.h"
#include "runtime/tests/allocator_test_base.h"

namespace panda::mem {

using NonObjectFreeListAllocator = FreeListAllocator<EmptyAllocConfigWithCrossingMap>;

class FreeListAllocatorTest : public AllocatorTest<NonObjectFreeListAllocator> {
public:
    FreeListAllocatorTest()
    {
        // We need to create a runtime instance to be able to use CrossingMap.
        options_.SetShouldLoadBootPandaFiles(false);
        options_.SetShouldInitializeIntrinsics(false);
        Runtime::Create(options_);
        thread_ = panda::MTManagedThread::GetCurrent();
        thread_->ManagedCodeBegin();
        if (!CrossingMapSingleton::IsCreated()) {
            CrossingMapSingleton::Create();
            crossingmap_manual_handling_ = true;
        }
    }

    ~FreeListAllocatorTest()
    {
        thread_->ManagedCodeEnd();
        ClearPoolManager();
        if (crossingmap_manual_handling_) {
            CrossingMapSingleton::Destroy();
        }
        Runtime::Destroy();
    }

protected:
    panda::MTManagedThread *thread_ {nullptr};
    static constexpr size_t DEFAULT_POOL_SIZE_FOR_ALLOC = NonObjectFreeListAllocator::GetMinPoolSize();
    static constexpr size_t DEFAULT_POOL_ALIGNMENT_FOR_ALLOC = FREELIST_DEFAULT_ALIGNMENT;
    static constexpr size_t POOL_HEADER_SIZE = sizeof(NonObjectFreeListAllocator::MemoryPoolHeader);
    static constexpr size_t MAX_ALLOC_SIZE = NonObjectFreeListAllocator::GetMaxSize();

    void AddMemoryPoolToAllocator(NonObjectFreeListAllocator &alloc) override
    {
        os::memory::LockHolder lock(pool_lock_);
        Pool pool = PoolManager::GetMmapMemPool()->AllocPool(DEFAULT_POOL_SIZE_FOR_ALLOC, SpaceType::SPACE_TYPE_OBJECT,
                                                             AllocatorType::FREELIST_ALLOCATOR, &alloc);
        ASSERT(pool.GetSize() == DEFAULT_POOL_SIZE_FOR_ALLOC);
        if (pool.GetMem() == nullptr) {
            ASSERT_TRUE(0 && "Can't get a new pool from PoolManager");
        }
        allocated_pools_by_pool_manager_.push_back(pool);
        if (!alloc.AddMemoryPool(pool.GetMem(), pool.GetSize())) {
            ASSERT_TRUE(0 && "Can't add mem pool to allocator");
        }
    }

    void AddMemoryPoolToAllocatorProtected(NonObjectFreeListAllocator &alloc) override
    {
        // We use common PoolManager from Runtime. Therefore, we have the same pool allocation for both cases.
        AddMemoryPoolToAllocator(alloc);
    }

    bool AllocatedByThisAllocator(NonObjectFreeListAllocator &allocator, void *mem) override
    {
        return allocator.AllocatedByFreeListAllocator(mem);
    }

    void ClearPoolManager(bool clear_crossing_map = false)
    {
        for (auto i : allocated_pools_by_pool_manager_) {
            PoolManager::GetMmapMemPool()->FreePool(i.GetMem(), i.GetSize());
            if (clear_crossing_map) {
                // We need to remove corresponding Pools from the CrossingMap
                CrossingMapSingleton::RemoveCrossingMapForMemory(i.GetMem(), i.GetSize());
            }
        }
        allocated_pools_by_pool_manager_.clear();
    }

    std::vector<Pool> allocated_pools_by_pool_manager_;
    RuntimeOptions options_;
    bool crossingmap_manual_handling_ {false};
    // Mutex, which allows only one thread to add pool to the pool vector
    os::memory::Mutex pool_lock_;
};

TEST_F(FreeListAllocatorTest, SimpleAllocateDifferentObjSizeTest)
{
    LOG(DEBUG, ALLOC) << "SimpleAllocateDifferentObjSizeTest";
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    NonObjectFreeListAllocator allocator(mem_stats);
    AddMemoryPoolToAllocator(allocator);
    for (size_t i = 23; i < 300; i++) {
        void *mem = allocator.Alloc(i);
        LOG(DEBUG, ALLOC) << "Allocate obj with size " << i << " at " << std::hex << mem;
    }
    delete mem_stats;
}

TEST_F(FreeListAllocatorTest, AllocateWriteFreeTest)
{
    AllocateAndFree(FREELIST_ALLOCATOR_MIN_SIZE, 512);
}

TEST_F(FreeListAllocatorTest, AllocateRandomFreeTest)
{
    static constexpr size_t ALLOC_SIZE = FREELIST_ALLOCATOR_MIN_SIZE;
    AllocateFreeDifferentSizesTest<ALLOC_SIZE, 2 * ALLOC_SIZE>(512);
}

TEST_F(FreeListAllocatorTest, AllocateTooBigObjTest)
{
    AllocateTooBigObjectTest<MAX_ALLOC_SIZE + 1>();
}

TEST_F(FreeListAllocatorTest, AlignmentAllocTest)
{
    static constexpr size_t POOLS_COUNT = 2;
    AlignedAllocFreeTest<FREELIST_ALLOCATOR_MIN_SIZE, MAX_ALLOC_SIZE / 4096>(POOLS_COUNT);
}

TEST_F(FreeListAllocatorTest, AllocateTooMuchTest)
{
    static constexpr size_t ALLOC_SIZE = FREELIST_ALLOCATOR_MIN_SIZE;
    AllocateTooMuchTest(ALLOC_SIZE, DEFAULT_POOL_SIZE_FOR_ALLOC / ALLOC_SIZE);
}

TEST_F(FreeListAllocatorTest, ObjectIteratorTest)
{
    ObjectIteratorTest<FREELIST_ALLOCATOR_MIN_SIZE, MAX_ALLOC_SIZE>();
}

TEST_F(FreeListAllocatorTest, ObjectCollectionTest)
{
    ObjectCollectionTest<FREELIST_ALLOCATOR_MIN_SIZE, MAX_ALLOC_SIZE>();
}

TEST_F(FreeListAllocatorTest, ObjectIteratorInRangeTest)
{
    ObjectIteratorInRangeTest<FREELIST_ALLOCATOR_MIN_SIZE, MAX_ALLOC_SIZE>(
        CrossingMapSingleton::GetCrossingMapGranularity());
}

TEST_F(FreeListAllocatorTest, AsanTest)
{
    AsanTest();
}

TEST_F(FreeListAllocatorTest, VisitAndRemoveFreePoolsTest)
{
    static constexpr size_t POOLS_COUNT = 5;
    VisitAndRemoveFreePools<POOLS_COUNT>(MAX_ALLOC_SIZE);
}

TEST_F(FreeListAllocatorTest, AllocatedByFreeListAllocatorTest)
{
    AllocatedByThisAllocatorTest();
}

TEST_F(FreeListAllocatorTest, FailedLinksTest)
{
    static constexpr size_t min_alloc_size = FREELIST_ALLOCATOR_MIN_SIZE;
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    NonObjectFreeListAllocator allocator(mem_stats);
    AddMemoryPoolToAllocator(allocator);
    std::pair<void *, size_t> pair;

    std::array<std::pair<void *, size_t>, 3> memory_elements;
    for (size_t i = 0; i < 3; i++) {
        void *mem = allocator.Alloc(min_alloc_size);
        ASSERT_TRUE(mem != nullptr);
        size_t index = SetBytesFromByteArray(mem, min_alloc_size);
        std::pair<void *, size_t> new_pair(mem, index);
        memory_elements.at(i) = new_pair;
    }

    pair = memory_elements[1];
    ASSERT_TRUE(CompareBytesWithByteArray(std::get<0>(pair), min_alloc_size, std::get<1>(pair)));
    allocator.Free(std::get<0>(pair));

    pair = memory_elements[0];
    ASSERT_TRUE(CompareBytesWithByteArray(std::get<0>(pair), min_alloc_size, std::get<1>(pair)));
    allocator.Free(std::get<0>(pair));

    {
        void *mem = allocator.Alloc(min_alloc_size * 2);
        ASSERT_TRUE(mem != nullptr);
        size_t index = SetBytesFromByteArray(mem, min_alloc_size * 2);
        std::pair<void *, size_t> new_pair(mem, index);
        memory_elements.at(0) = new_pair;
    }

    {
        void *mem = allocator.Alloc(min_alloc_size);
        ASSERT_TRUE(mem != nullptr);
        size_t index = SetBytesFromByteArray(mem, min_alloc_size);
        std::pair<void *, size_t> new_pair(mem, index);
        memory_elements.at(1) = new_pair;
    }

    {
        pair = memory_elements[0];
        ASSERT_TRUE(CompareBytesWithByteArray(std::get<0>(pair), min_alloc_size * 2, std::get<1>(pair)));
        allocator.Free(std::get<0>(pair));
    }

    {
        pair = memory_elements[1];
        ASSERT_TRUE(CompareBytesWithByteArray(std::get<0>(pair), min_alloc_size, std::get<1>(pair)));
        allocator.Free(std::get<0>(pair));
    }

    {
        pair = memory_elements[2];
        ASSERT_TRUE(CompareBytesWithByteArray(std::get<0>(pair), min_alloc_size, std::get<1>(pair)));
        allocator.Free(std::get<0>(pair));
    }
    delete mem_stats;
}

TEST_F(FreeListAllocatorTest, MaxAllocationSizeTest)
{
    static constexpr size_t alloc_size = MAX_ALLOC_SIZE;
    static constexpr size_t alloc_count = 2;
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    NonObjectFreeListAllocator allocator(mem_stats);
    AddMemoryPoolToAllocator(allocator);
    std::array<void *, alloc_count> memory_elements;
    for (size_t i = 0; i < alloc_count; i++) {
        void *mem = allocator.Alloc(alloc_size);
        ASSERT_TRUE(mem != nullptr);
        memory_elements.at(i) = mem;
    }
    for (size_t i = 0; i < alloc_count; i++) {
        allocator.Free(memory_elements.at(i));
    }
    delete mem_stats;
}

TEST_F(FreeListAllocatorTest, AllocateTheWholePoolFreeAndAllocateAgainTest)
{
    size_t min_size_power_of_two;
    if ((FREELIST_ALLOCATOR_MIN_SIZE & (FREELIST_ALLOCATOR_MIN_SIZE - 1)) == 0U) {
        min_size_power_of_two = panda::helpers::math::GetIntLog2(FREELIST_ALLOCATOR_MIN_SIZE);
    } else {
        min_size_power_of_two = ceil(std::log(FREELIST_ALLOCATOR_MIN_SIZE) / std::log(2U));
    }
    if (((1 << min_size_power_of_two) - sizeof(freelist::MemoryBlockHeader)) < FREELIST_ALLOCATOR_MIN_SIZE) {
        min_size_power_of_two++;
    }
    size_t alloc_size = (1 << min_size_power_of_two) - sizeof(freelist::MemoryBlockHeader);
    // To cover all memory we need to consider pool header size at first bytes of pool memory.
    size_t first_alloc_size = (1 << min_size_power_of_two) - sizeof(freelist::MemoryBlockHeader) - POOL_HEADER_SIZE;
    if (first_alloc_size < FREELIST_ALLOCATOR_MIN_SIZE) {
        first_alloc_size = (1 << (min_size_power_of_two + 1)) - sizeof(freelist::MemoryBlockHeader) - POOL_HEADER_SIZE;
    }
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    NonObjectFreeListAllocator allocator(mem_stats);
    AddMemoryPoolToAllocator(allocator);
    std::vector<void *> memory_elements;
    size_t alloc_count = 0;

    // Allocate first element
    void *first_alloc_mem = allocator.Alloc(first_alloc_size);
    ASSERT_TRUE(first_alloc_mem != nullptr);

    // Allocate and use the whole alloc pool
    while (true) {
        void *mem = allocator.Alloc(alloc_size);
        if (mem == nullptr) {
            break;
        }
        alloc_count++;
        memory_elements.push_back(mem);
    }

    // Free all elements
    allocator.Free(first_alloc_mem);
    for (size_t i = 0; i < alloc_count; i++) {
        allocator.Free(memory_elements.back());
        memory_elements.pop_back();
    }

    // Allocate first element again
    first_alloc_mem = allocator.Alloc(first_alloc_size);
    ASSERT_TRUE(first_alloc_mem != nullptr);

    // Allocate again
    for (size_t i = 0; i < alloc_count; i++) {
        void *mem = allocator.Alloc(alloc_size);
        ASSERT_TRUE(mem != nullptr);
        memory_elements.push_back(mem);
    }

    // Free all elements again
    allocator.Free(first_alloc_mem);
    for (size_t i = 0; i < alloc_count; i++) {
        allocator.Free(memory_elements.back());
        memory_elements.pop_back();
    }
    delete mem_stats;
}

TEST_F(FreeListAllocatorTest, MTAllocFreeTest)
{
    static constexpr size_t MIN_ELEMENTS_COUNT = 500;
    static constexpr size_t MAX_ELEMENTS_COUNT = 1000;
#if defined(PANDA_TARGET_ARM64) || defined(PANDA_TARGET_32)
    // We have an issue with QEMU during MT tests. Issue 2852
    static constexpr size_t THREADS_COUNT = 1;
#else
    static constexpr size_t THREADS_COUNT = 10;
#endif
    static constexpr size_t MAX_MT_ALLOC_SIZE = MAX_ALLOC_SIZE / 128;
    static constexpr size_t MT_TEST_RUN_COUNT = 5;
    // Threads can concurrently add Pools to the allocator, therefore, we must make it into account
    // And also we must take fragmentation into account
    ASSERT_TRUE(mem::MemConfig::GetObjectPoolSize() >
                2 * (AlignUp(MAX_ELEMENTS_COUNT * MAX_MT_ALLOC_SIZE, DEFAULT_POOL_SIZE_FOR_ALLOC)) +
                    THREADS_COUNT * DEFAULT_POOL_SIZE_FOR_ALLOC);
    for (size_t i = 0; i < MT_TEST_RUN_COUNT; i++) {
        MT_AllocFreeTest<FREELIST_ALLOCATOR_MIN_SIZE, MAX_MT_ALLOC_SIZE, THREADS_COUNT>(MIN_ELEMENTS_COUNT,
                                                                                        MAX_ELEMENTS_COUNT);
        ClearPoolManager(true);
    }
}

TEST_F(FreeListAllocatorTest, MTAllocIterateTest)
{
    static constexpr size_t MIN_ELEMENTS_COUNT = 500;
    static constexpr size_t MAX_ELEMENTS_COUNT = 1000;
#if defined(PANDA_TARGET_ARM64) || defined(PANDA_TARGET_32)
    // We have an issue with QEMU during MT tests. Issue 2852
    static constexpr size_t THREADS_COUNT = 1;
#else
    static constexpr size_t THREADS_COUNT = 10;
#endif
    static constexpr size_t MAX_MT_ALLOC_SIZE = MAX_ALLOC_SIZE / 128;
    static constexpr size_t MT_TEST_RUN_COUNT = 5;
    // Threads can concurrently add Pools to the allocator, therefore, we must make it into account
    // And also we must take fragmentation into account
    ASSERT_TRUE(mem::MemConfig::GetObjectPoolSize() >
                2 * (AlignUp(MAX_ELEMENTS_COUNT * MAX_MT_ALLOC_SIZE, DEFAULT_POOL_SIZE_FOR_ALLOC)) +
                    THREADS_COUNT * DEFAULT_POOL_SIZE_FOR_ALLOC);
    for (size_t i = 0; i < MT_TEST_RUN_COUNT; i++) {
        MT_AllocIterateTest<FREELIST_ALLOCATOR_MIN_SIZE, MAX_MT_ALLOC_SIZE, THREADS_COUNT>(
            MIN_ELEMENTS_COUNT, MAX_ELEMENTS_COUNT, CrossingMapSingleton::GetCrossingMapGranularity());
        ClearPoolManager(true);
    }
}

TEST_F(FreeListAllocatorTest, MTAllocCollectTest)
{
    static constexpr size_t MIN_ELEMENTS_COUNT = 500;
    static constexpr size_t MAX_ELEMENTS_COUNT = 1000;
#if defined(PANDA_TARGET_ARM64) || defined(PANDA_TARGET_32)
    // We have an issue with QEMU during MT tests. Issue 2852
    static constexpr size_t THREADS_COUNT = 1;
#else
    static constexpr size_t THREADS_COUNT = 10;
#endif
    static constexpr size_t MAX_MT_ALLOC_SIZE = MAX_ALLOC_SIZE / 128;
    static constexpr size_t MT_TEST_RUN_COUNT = 5;
    // Threads can concurrently add Pools to the allocator, therefore, we must make it into account
    // And also we must take fragmentation into account
    ASSERT_TRUE(mem::MemConfig::GetObjectPoolSize() >
                2 * (AlignUp(MAX_ELEMENTS_COUNT * MAX_MT_ALLOC_SIZE, DEFAULT_POOL_SIZE_FOR_ALLOC)) +
                    THREADS_COUNT * DEFAULT_POOL_SIZE_FOR_ALLOC);
    for (size_t i = 0; i < MT_TEST_RUN_COUNT; i++) {
        MT_AllocCollectTest<FREELIST_ALLOCATOR_MIN_SIZE, MAX_MT_ALLOC_SIZE, THREADS_COUNT>(MIN_ELEMENTS_COUNT,
                                                                                           MAX_ELEMENTS_COUNT);
        ClearPoolManager(true);
    }
}

}  // namespace panda::mem
