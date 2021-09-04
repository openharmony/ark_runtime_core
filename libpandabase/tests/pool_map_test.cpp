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

#include "gtest/gtest.h"
#include "mem/pool_map.h"
#include "mem/mem_pool.h"
#include "mem/mem.h"

#include <ctime>
#include <algorithm>

namespace panda {

class PoolMapTest : public testing::Test {
public:
    PoolMapTest()
    {
#ifdef PANDA_NIGHTLY_TEST_ON
        seed_ = std::time(NULL);
#else
        seed_ = 0xDEADBEEF;
#endif
        srand(seed_);
    }

    ~PoolMapTest()
    {
        ResetPoolMap();
    }

protected:
    static constexpr size_t MINIMAL_POOL_SIZE = PANDA_POOL_ALIGNMENT_IN_BYTES;

    void AddToPoolMap(Pool pool, SpaceType space_type, AllocatorType allocator_type,
                      const void *allocator_addr = nullptr)
    {
        if (allocator_addr == nullptr) {
            allocator_addr = pool.GetMem();
        }
        pools_.push_back(pool);
        pool_map_.AddPoolToMap(pool.GetMem(), pool.GetSize(), space_type, allocator_type, allocator_addr);
    }

    void RemovePoolFromMap(Pool pool)
    {
        auto items = std::remove(pools_.begin(), pools_.end(), pool);
        ASSERT(items != pools_.end());
        pools_.erase(items, pools_.end());
        pool_map_.RemovePoolFromMap(pool.GetMem(), pool.GetSize());
    }

    void ResetPoolMap()
    {
        for (auto i : pools_) {
            pool_map_.RemovePoolFromMap(i.GetMem(), i.GetSize());
        }
        pools_.clear();
    }

    bool IsEmptyPoolMap() const
    {
        return pool_map_.IsEmpty();
    }

    SpaceType GetRandSpaceType() const
    {
        int rand_index = rand() % ALL_SPACE_TYPES.size();
        return ALL_SPACE_TYPES[rand_index];
    }

    AllocatorType GetRandAllocatorType() const
    {
        int rand_index = rand() % ALL_ALLOCATOR_TYPES.size();
        return ALL_ALLOCATOR_TYPES[rand_index];
    }

    size_t RandHeapAddr() const
    {
        return AlignUp(rand() % PANDA_MAX_HEAP_SIZE, DEFAULT_ALIGNMENT_IN_BYTES);
    }

    void CheckRandomPoolAddress(Pool pool, SpaceType space_type, AllocatorType allocator_type,
                                uintptr_t allocator_addr) const
    {
        void *pool_addr = RandAddrFromPool(pool);
        ASSERT_EQ(GetSpaceTypeForAddr(pool_addr), space_type);
        ASSERT_EQ(GetAllocatorInfoForAddr(pool_addr).GetType(), allocator_type);
        ASSERT_EQ(ToUintPtr(GetAllocatorInfoForAddr(pool_addr).GetAllocatorHeaderAddr()), allocator_addr);
    }

private:
    void *RandAddrFromPool(Pool pool) const
    {
        return ToVoidPtr(ToUintPtr(pool.GetMem()) + rand() % pool.GetSize());
    }

    AllocatorInfo GetAllocatorInfoForAddr(void *addr) const
    {
        return pool_map_.GetAllocatorInfo(addr);
    }

    SpaceType GetSpaceTypeForAddr(void *addr) const
    {
        return pool_map_.GetSpaceType(addr);
    }

    static constexpr std::array<SpaceType, 6> ALL_SPACE_TYPES = {SpaceType::SPACE_TYPE_OBJECT,
                                                                 SpaceType::SPACE_TYPE_HUMONGOUS_OBJECT,
                                                                 SpaceType::SPACE_TYPE_NON_MOVABLE_OBJECT,
                                                                 SpaceType::SPACE_TYPE_INTERNAL,
                                                                 SpaceType::SPACE_TYPE_CODE,
                                                                 SpaceType::SPACE_TYPE_COMPILER};

    static constexpr std::array<AllocatorType, 8> ALL_ALLOCATOR_TYPES = {
        AllocatorType::RUNSLOTS_ALLOCATOR, AllocatorType::FREELIST_ALLOCATOR, AllocatorType::HUMONGOUS_ALLOCATOR,
        AllocatorType::ARENA_ALLOCATOR,    AllocatorType::BUMP_ALLOCATOR,     AllocatorType::TLAB_ALLOCATOR,
        AllocatorType::REGION_ALLOCATOR,   AllocatorType::FRAME_ALLOCATOR};

    unsigned int seed_;
    std::vector<Pool> pools_;
    PoolMap pool_map_;
};

TEST_F(PoolMapTest, TwoConsistentPoolsTest)
{
    static constexpr size_t FIRST_POOL_SIZE = 4 * MINIMAL_POOL_SIZE;
    static constexpr size_t SECOND_POOL_SIZE = 10 * MINIMAL_POOL_SIZE;
    static constexpr uintptr_t FIRST_POOL_ADDR = 0;
    static constexpr uintptr_t SECOND_POOL_ADDR = FIRST_POOL_ADDR + FIRST_POOL_SIZE;
    static constexpr SpaceType FIRST_SPACE_TYPE = SpaceType::SPACE_TYPE_INTERNAL;
    static constexpr SpaceType SECOND_SPACE_TYPE = SpaceType::SPACE_TYPE_OBJECT;
    static constexpr AllocatorType FIRST_ALLOCATOR_TYPE = AllocatorType::RUNSLOTS_ALLOCATOR;
    static constexpr AllocatorType SECOND_ALLOCATOR_TYPE = AllocatorType::FREELIST_ALLOCATOR;

    uintptr_t first_pool_allocator_header_addr = RandHeapAddr();

    Pool first_pool(FIRST_POOL_SIZE, ToVoidPtr(FIRST_POOL_ADDR));
    Pool second_pool(SECOND_POOL_SIZE, ToVoidPtr(SECOND_POOL_ADDR));

    AddToPoolMap(first_pool, FIRST_SPACE_TYPE, FIRST_ALLOCATOR_TYPE, ToVoidPtr(first_pool_allocator_header_addr));
    AddToPoolMap(second_pool, SECOND_SPACE_TYPE, SECOND_ALLOCATOR_TYPE);

    CheckRandomPoolAddress(first_pool, FIRST_SPACE_TYPE, FIRST_ALLOCATOR_TYPE, first_pool_allocator_header_addr);
    // We haven't initialized second allocator header address.
    // Therefore it must return a pointer to the first pool byte.
    CheckRandomPoolAddress(second_pool, SECOND_SPACE_TYPE, SECOND_ALLOCATOR_TYPE, SECOND_POOL_ADDR);

    // Check that we remove elements from pool map correctly
    RemovePoolFromMap(first_pool);
    RemovePoolFromMap(second_pool);

    ASSERT_TRUE(IsEmptyPoolMap());
    ResetPoolMap();
}

TEST_F(PoolMapTest, AddRemoveDifferentPoolsTest)
{
    static constexpr size_t MAX_POOL_SIZE = 256 * MINIMAL_POOL_SIZE;
    static constexpr size_t ITERATIONS = 200;
    static constexpr uintptr_t POOL_START_ADDR = PANDA_POOL_ALIGNMENT_IN_BYTES;
    for (size_t i = 0; i < ITERATIONS; i++) {
        size_t pool_size = AlignUp(rand() % MAX_POOL_SIZE, PANDA_POOL_ALIGNMENT_IN_BYTES);
        SpaceType space = GetRandSpaceType();
        AllocatorType allocator = GetRandAllocatorType();
        Pool pool(pool_size, ToVoidPtr(POOL_START_ADDR));

        AddToPoolMap(pool, space, allocator);
        CheckRandomPoolAddress(pool, space, allocator, POOL_START_ADDR);
        RemovePoolFromMap(pool);
    }

    ASSERT_TRUE(IsEmptyPoolMap());
    ResetPoolMap();
}

}  // namespace panda
