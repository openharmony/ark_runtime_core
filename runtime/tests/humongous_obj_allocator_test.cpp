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

#include <sys/mman.h>

#include "libpandabase/mem/mem.h"
#include "libpandabase/os/mem.h"
#include "libpandabase/utils/asan_interface.h"
#include "libpandabase/utils/logger.h"
#include "libpandabase/utils/math_helpers.h"
#include "runtime/mem/alloc_config.h"
#include "runtime/mem/humongous_obj_allocator-inl.h"
#include "runtime/tests/allocator_test_base.h"

namespace panda::mem {

using NonObjectHumongousObjAllocator = HumongousObjAllocator<EmptyAllocConfigWithCrossingMap>;

class HumongousObjAllocatorTest : public AllocatorTest<NonObjectHumongousObjAllocator> {
public:
    HumongousObjAllocatorTest()
    {
        panda::mem::MemConfig::Initialize(0, 1024_MB, 0, 0);
        PoolManager::Initialize();
    }

    ~HumongousObjAllocatorTest()
    {
        ClearPoolManager();
        PoolManager::Finalize();
        panda::mem::MemConfig::Finalize();
    }

protected:
    static constexpr size_t MIN_ALLOC_SIZE = 1_MB;
    static constexpr size_t MAX_ALLOC_SIZE = 9_MB;
    static constexpr Alignment HUMONGOUS_LOG_MAX_ALIGN = LOG_ALIGN_11;
    static constexpr size_t DEFAULT_POOL_SIZE_FOR_ALLOC =
        NonObjectHumongousObjAllocator::GetMinPoolSize(MAX_ALLOC_SIZE);
    static constexpr size_t POOL_HEADER_SIZE = sizeof(NonObjectHumongousObjAllocator::MemoryPoolHeader);

    void AddMemoryPoolToAllocator(NonObjectHumongousObjAllocator &alloc) override
    {
        AddMemoryPoolToAllocator(alloc, DEFAULT_POOL_SIZE_FOR_ALLOC);
    }

    void AddMemoryPoolToAllocator(NonObjectHumongousObjAllocator &alloc, size_t size)
    {
        os::memory::LockHolder lock(pool_lock_);
        size = AlignUp(size, PANDA_POOL_ALIGNMENT_IN_BYTES);
        Pool pool = PoolManager::GetMmapMemPool()->AllocPool(AlignUp(size, PANDA_POOL_ALIGNMENT_IN_BYTES),
                                                             SpaceType::SPACE_TYPE_INTERNAL,
                                                             AllocatorType::HUMONGOUS_ALLOCATOR, &alloc);
        ASSERT(pool.GetSize() >= size);
        if (pool.GetMem() == nullptr) {
            ASSERT_TRUE(0 && "Can't get a new pool from PoolManager");
        }
        allocated_pools_by_pool_manager_.push_back(pool);
        if (!alloc.AddMemoryPool(pool.GetMem(), size)) {
            ASSERT_TRUE(0 && "Can't add mem pool to allocator");
        }
    }

    void AddMemoryPoolToAllocatorProtected(NonObjectHumongousObjAllocator &alloc) override
    {
        // We use common PoolManager from Runtime. Therefore, we have the same pool allocation for both cases.
        AddMemoryPoolToAllocator(alloc);
    }

    bool AllocatedByThisAllocator(NonObjectHumongousObjAllocator &allocator, void *mem) override
    {
        return allocator.AllocatedByHumongousObjAllocator(mem);
    }

    void ClearPoolManager()
    {
        for (auto i : allocated_pools_by_pool_manager_) {
            PoolManager::GetMmapMemPool()->FreePool(i.GetMem(), i.GetSize());
        }
        allocated_pools_by_pool_manager_.clear();
    }

    std::vector<Pool> allocated_pools_by_pool_manager_;
    // Mutex, which allows only one thread to add pool to the pool vector
    os::memory::Mutex pool_lock_;
};

TEST_F(HumongousObjAllocatorTest, CheckIncorrectMemoryPoolReusageTest)
{
    static constexpr size_t POOL_SIZE = 4_MB;
    static constexpr Alignment OBJECT_ALIGNMENT = DEFAULT_ALIGNMENT;
    static constexpr size_t FIRST_OBJECT_SIZE = POOL_SIZE - PANDA_POOL_ALIGNMENT_IN_BYTES;
    static constexpr size_t SECOND_OBJECT_SIZE = POOL_SIZE - GetAlignmentInBytes(OBJECT_ALIGNMENT);
    ASSERT(PANDA_POOL_ALIGNMENT_IN_BYTES > GetAlignmentInBytes(OBJECT_ALIGNMENT));
    ASSERT_TRUE(NonObjectHumongousObjAllocator::GetMinPoolSize(FIRST_OBJECT_SIZE) == POOL_SIZE);
    mem::MemStatsType mem_stats;
    NonObjectHumongousObjAllocator allocator(&mem_stats);
    AddMemoryPoolToAllocator(allocator, POOL_SIZE);
    void *first_object = allocator.Alloc(FIRST_OBJECT_SIZE, OBJECT_ALIGNMENT);
    ASSERT_TRUE(first_object != nullptr);
    allocator.Free(first_object);
    void *second_object = allocator.Alloc(SECOND_OBJECT_SIZE, OBJECT_ALIGNMENT);
    ASSERT_TRUE(second_object == nullptr);
}

TEST_F(HumongousObjAllocatorTest, SimpleAllocateDifferentObjSizeTest)
{
    static constexpr size_t MAX_ELEMENTS_COUNT = 20;
    LOG(DEBUG, ALLOC) << "SimpleAllocateDifferentObjSizeTest";
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    NonObjectHumongousObjAllocator allocator(mem_stats);
    std::vector<void *> values;
    for (size_t i = 0; i < MAX_ELEMENTS_COUNT; i++) {
        size_t pool_size = DEFAULT_POOL_SIZE_FOR_ALLOC + PAGE_SIZE * i;
        size_t alloc_size = pool_size - sizeof(POOL_HEADER_SIZE) - GetAlignmentInBytes(LOG_ALIGN_MAX);
        AddMemoryPoolToAllocator(allocator, pool_size);
        void *mem = allocator.Alloc(alloc_size);
        ASSERT_TRUE(mem != nullptr);
        values.push_back(mem);
        LOG(DEBUG, ALLOC) << "Allocate obj with size " << alloc_size << " at " << std::hex << mem;
    }
    for (auto i : values) {
        allocator.Free(i);
    }
    for (size_t i = 0; i < MAX_ELEMENTS_COUNT; i++) {
        void *mem = allocator.Alloc(MAX_ALLOC_SIZE);
        ASSERT_TRUE(mem != nullptr);
    }
    delete mem_stats;
}

TEST_F(HumongousObjAllocatorTest, AllocateWriteFreeTest)
{
    static constexpr size_t ELEMENTS_COUNT = 100;
    static constexpr size_t POOLS_COUNT = ELEMENTS_COUNT;
    AllocateAndFree(MIN_ALLOC_SIZE, ELEMENTS_COUNT, POOLS_COUNT);
}

TEST_F(HumongousObjAllocatorTest, AllocateRandomFreeTest)
{
    static constexpr size_t ELEMENTS_COUNT = 100;
    static constexpr size_t POOLS_COUNT = ELEMENTS_COUNT;
    AllocateFreeDifferentSizesTest<MIN_ALLOC_SIZE, MAX_ALLOC_SIZE>(ELEMENTS_COUNT, POOLS_COUNT);
}

TEST_F(HumongousObjAllocatorTest, AlignmentAllocTest)
{
    static constexpr size_t MAX_ALLOC = MIN_ALLOC_SIZE + 10;
    static constexpr size_t POOLS_COUNT =
        (MAX_ALLOC - MIN_ALLOC_SIZE + 1) * (HUMONGOUS_LOG_MAX_ALIGN - LOG_ALIGN_MIN + 1);
    AlignedAllocFreeTest<MIN_ALLOC_SIZE, MAX_ALLOC, LOG_ALIGN_MIN, HUMONGOUS_LOG_MAX_ALIGN>(POOLS_COUNT);
}

TEST_F(HumongousObjAllocatorTest, AllocateTooMuchTest)
{
    static constexpr size_t ELEMENTS_COUNT = 2;
    AllocateTooMuchTest(MIN_ALLOC_SIZE, ELEMENTS_COUNT);
}

TEST_F(HumongousObjAllocatorTest, ObjectIteratorTest)
{
    static constexpr size_t FREE_GRANULARITY = 1;
    static constexpr size_t POOLS_COUNT = 50;
    ObjectIteratorTest<MIN_ALLOC_SIZE, MAX_ALLOC_SIZE, LOG_ALIGN_MIN, HUMONGOUS_LOG_MAX_ALIGN>(FREE_GRANULARITY,
                                                                                               POOLS_COUNT);
}

TEST_F(HumongousObjAllocatorTest, ObjectCollectionTest)
{
    static constexpr size_t FREE_GRANULARITY = 1;
    static constexpr size_t POOLS_COUNT = 50;
    ObjectCollectionTest<MIN_ALLOC_SIZE, MAX_ALLOC_SIZE, LOG_ALIGN_MIN, HUMONGOUS_LOG_MAX_ALIGN>(FREE_GRANULARITY,
                                                                                                 POOLS_COUNT);
}

TEST_F(HumongousObjAllocatorTest, ObjectIteratorInRangeTest)
{
    static constexpr size_t FREE_GRANULARITY = 4;
    static constexpr size_t POOLS_COUNT = 50;
    ObjectIteratorInRangeTest<MIN_ALLOC_SIZE, MAX_ALLOC_SIZE, LOG_ALIGN_MIN, HUMONGOUS_LOG_MAX_ALIGN>(
        CrossingMapSingleton::GetCrossingMapGranularity(), FREE_GRANULARITY, POOLS_COUNT);
}

TEST_F(HumongousObjAllocatorTest, AsanTest)
{
    static constexpr size_t ELEMENTS_COUNT = 100;
    static constexpr size_t POOLS_COUNT = ELEMENTS_COUNT;
    static constexpr size_t FREE_GRANULARITY = 3;
    AsanTest<ELEMENTS_COUNT>(FREE_GRANULARITY, POOLS_COUNT);
}

TEST_F(HumongousObjAllocatorTest, VisitAndRemoveFreePoolsTest)
{
    static_assert(PANDA_HUMONGOUS_OBJ_ALLOCATOR_RESERVED_MEM_MAX_POOL_SIZE < MAX_ALLOC_SIZE);
    static constexpr size_t POOLS_COUNT = 5;
    VisitAndRemoveFreePools<POOLS_COUNT>(MAX_ALLOC_SIZE);
}

TEST_F(HumongousObjAllocatorTest, AllocatedByHumongousObjAllocatorTest)
{
    AllocatedByThisAllocatorTest();
}

TEST_F(HumongousObjAllocatorTest, MTAllocFreeTest)
{
    static constexpr size_t MIN_ELEMENTS_COUNT = 10;
    static constexpr size_t MAX_ELEMENTS_COUNT = 20;
#if defined(PANDA_TARGET_ARM64) || defined(PANDA_TARGET_32)
    // We have an issue with QEMU during MT tests. Issue 2852
    static constexpr size_t THREADS_COUNT = 1;
#else
    static constexpr size_t THREADS_COUNT = 5;
#endif
    static constexpr size_t MT_TEST_RUN_COUNT = 5;
    for (size_t i = 0; i < MT_TEST_RUN_COUNT; i++) {
        MT_AllocFreeTest<MIN_ALLOC_SIZE, MAX_ALLOC_SIZE, THREADS_COUNT>(MIN_ELEMENTS_COUNT, MAX_ELEMENTS_COUNT);
        ClearPoolManager();
    }
}

TEST_F(HumongousObjAllocatorTest, MTAllocIterateTest)
{
    static constexpr size_t MIN_ELEMENTS_COUNT = 10;
    static constexpr size_t MAX_ELEMENTS_COUNT = 20;
#if defined(PANDA_TARGET_ARM64) || defined(PANDA_TARGET_32)
    // We have an issue with QEMU during MT tests. Issue 2852
    static constexpr size_t THREADS_COUNT = 1;
#else
    static constexpr size_t THREADS_COUNT = 5;
#endif
    static constexpr size_t MT_TEST_RUN_COUNT = 5;
    for (size_t i = 0; i < MT_TEST_RUN_COUNT; i++) {
        MT_AllocIterateTest<MIN_ALLOC_SIZE, MAX_ALLOC_SIZE, THREADS_COUNT>(
            MIN_ELEMENTS_COUNT, MAX_ELEMENTS_COUNT, CrossingMapSingleton::GetCrossingMapGranularity());
        ClearPoolManager();
    }
}

TEST_F(HumongousObjAllocatorTest, MTAllocCollectTest)
{
    static constexpr size_t MIN_ELEMENTS_COUNT = 10;
    static constexpr size_t MAX_ELEMENTS_COUNT = 20;
#if defined(PANDA_TARGET_ARM64) || defined(PANDA_TARGET_32)
    // We have an issue with QEMU during MT tests. Issue 2852
    static constexpr size_t THREADS_COUNT = 1;
#else
    static constexpr size_t THREADS_COUNT = 5;
#endif
    static constexpr size_t MT_TEST_RUN_COUNT = 5;
    for (size_t i = 0; i < MT_TEST_RUN_COUNT; i++) {
        MT_AllocCollectTest<MIN_ALLOC_SIZE, MAX_ALLOC_SIZE, THREADS_COUNT>(MIN_ELEMENTS_COUNT, MAX_ELEMENTS_COUNT);
        ClearPoolManager();
    }
}

}  // namespace panda::mem
