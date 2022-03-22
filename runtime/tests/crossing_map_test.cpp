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

#include <ctime>

#include "gtest/gtest.h"
#include "runtime/mem/gc/crossing_map.h"
#include "runtime/mem/internal_allocator-inl.h"

namespace panda::mem {

class CrossingMapTest : public testing::Test {
public:
    CrossingMapTest()
    {
#ifdef PANDA_NIGHTLY_TEST_ON
        seed_ = std::time(NULL);
#else
        seed_ = 0xDEADBEEF;
#endif
        srand(seed_);
        panda::mem::MemConfig::Initialize(MEMORY_POOL_SIZE, MEMORY_POOL_SIZE, 0, 0);
        PoolManager::Initialize();
        start_addr_ = GetPoolMinAddress();
        mem_stats_ = new mem::MemStatsType();
        internal_allocator_ = new InternalAllocatorT<InternalAllocatorConfig::PANDA_ALLOCATORS>(mem_stats_);
        crossing_map_ = new CrossingMap(internal_allocator_, start_addr_, GetPoolSize());
        crossing_map_->Initialize();
        crossing_map_->InitializeCrossingMapForMemory(ToVoidPtr(start_addr_), GetPoolSize());
    }

    ~CrossingMapTest()
    {
        crossing_map_->RemoveCrossingMapForMemory(ToVoidPtr(start_addr_), GetPoolSize());
        crossing_map_->Destroy();
        delete crossing_map_;
        delete static_cast<Allocator *>(internal_allocator_);
        PoolManager::Finalize();
        panda::mem::MemConfig::Finalize();
        delete mem_stats_;
    }

protected:
    CrossingMap *GetCrossingMap()
    {
        return crossing_map_;
    }

    void *GetRandomObjAddr(size_t size)
    {
        ASSERT(size < GetPoolSize());
        uintptr_t rand_offset = rand() % (GetPoolSize() - size);
        rand_offset = (rand_offset >> CrossingMap::CROSSING_MAP_OBJ_ALIGNMENT)
                      << CrossingMap::CROSSING_MAP_OBJ_ALIGNMENT;
        return ToVoidPtr(start_addr_ + rand_offset);
    }

    void *AddPage(void *addr)
    {
        return ToVoidPtr(ToUintPtr(addr) + PAGE_SIZE);
    }

    void *IncreaseAddr(void *addr, size_t value)
    {
        return ToVoidPtr(ToUintPtr(addr) + value);
    }

    void *DecreaseAddr(void *addr, size_t value)
    {
        return ToVoidPtr(ToUintPtr(addr) - value);
    }

    size_t GetMapNumFromAddr(void *addr)
    {
        return crossing_map_->GetMapNumFromAddr(addr);
    }

    static constexpr size_t MIN_GAP_BETWEEN_OBJECTS = 1 << CrossingMap::CROSSING_MAP_OBJ_ALIGNMENT;

    static constexpr size_t MEMORY_POOL_SIZE = 64_MB;

    static constexpr size_t POOLS_SIZE = CrossingMap::CROSSING_MAP_STATIC_ARRAY_GRANULARITY;

    uintptr_t GetPoolMinAddress()
    {
        return PoolManager::GetMmapMemPool()->GetMinObjectAddress();
    }

    size_t GetPoolSize()
    {
        return PoolManager::GetMmapMemPool()->GetMaxObjectAddress() -
               PoolManager::GetMmapMemPool()->GetMinObjectAddress();
    }

    void *GetLastObjectByte(void *obj_addr, size_t obj_size)
    {
        ASSERT(obj_size != 0);
        return ToVoidPtr(ToUintPtr(obj_addr) + obj_size - 1U);
    }

    unsigned int GetSeed() const
    {
        return seed_;
    }

private:
    unsigned int seed_ {0};
    InternalAllocatorPtr internal_allocator_ {nullptr};
    uintptr_t start_addr_ {0};
    CrossingMap *crossing_map_ {nullptr};
    mem::MemStatsType *mem_stats_ {nullptr};
};

TEST_F(CrossingMapTest, OneSmallObjTest)
{
    static constexpr size_t OBJ_SIZE = 1;
    void *obj_addr = GetRandomObjAddr(OBJ_SIZE);
    GetCrossingMap()->AddObject(obj_addr, OBJ_SIZE);
    ASSERT_TRUE(GetCrossingMap()->FindFirstObject(obj_addr, obj_addr) == obj_addr) << " seed = " << GetSeed();
    ASSERT_TRUE(GetCrossingMap()->FindFirstObject(AddPage(obj_addr), AddPage(obj_addr)) == nullptr)
        << " seed = " << GetSeed();
}

TEST_F(CrossingMapTest, BigSmallObjTest)
{
    static constexpr size_t OBJ_SIZE = PAGE_SIZE * 2;
    void *obj_addr = GetRandomObjAddr(OBJ_SIZE);
    GetCrossingMap()->AddObject(obj_addr, OBJ_SIZE);
    ASSERT_TRUE(GetCrossingMap()->FindFirstObject(obj_addr, ToVoidPtr(ToUintPtr(obj_addr) + OBJ_SIZE)) == obj_addr)
        << " seed = " << GetSeed();
    if (PANDA_CROSSING_MAP_MANAGE_CROSSED_BORDER) {
        ASSERT_TRUE(GetCrossingMap()->FindFirstObject(AddPage(obj_addr), ToVoidPtr(ToUintPtr(obj_addr) + OBJ_SIZE)) ==
                    obj_addr)
            << " seed = " << GetSeed();
    }
    GetCrossingMap()->RemoveObject(obj_addr, OBJ_SIZE);
    ASSERT_TRUE(GetCrossingMap()->FindFirstObject(obj_addr, ToVoidPtr(ToUintPtr(obj_addr) + OBJ_SIZE)) == nullptr)
        << " seed = " << GetSeed();
    if (PANDA_CROSSING_MAP_MANAGE_CROSSED_BORDER) {
        ASSERT_TRUE(GetCrossingMap()->FindFirstObject(AddPage(obj_addr), ToVoidPtr(ToUintPtr(obj_addr) + OBJ_SIZE)) ==
                    nullptr)
            << " seed = " << GetSeed();
    }
}

TEST_F(CrossingMapTest, HugeObjTest)
{
    static constexpr size_t OBJ_SIZE = MEMORY_POOL_SIZE >> 1;
    void *obj_addr = GetRandomObjAddr(OBJ_SIZE);
    GetCrossingMap()->AddObject(obj_addr, OBJ_SIZE);
    ASSERT_TRUE(GetCrossingMap()->FindFirstObject(obj_addr, obj_addr) == obj_addr) << " seed = " << GetSeed();
    if (PANDA_CROSSING_MAP_MANAGE_CROSSED_BORDER) {
        for (size_t i = 1_MB; i < OBJ_SIZE; i += 1_MB) {
            void *addr = ToVoidPtr(ToUintPtr(obj_addr) + i);
            ASSERT_TRUE(GetCrossingMap()->FindFirstObject(addr, addr) == obj_addr) << " seed = " << GetSeed();
        }
    }
    GetCrossingMap()->RemoveObject(obj_addr, OBJ_SIZE);
    ASSERT_TRUE(GetCrossingMap()->FindFirstObject(obj_addr, obj_addr) == nullptr) << " seed = " << GetSeed();
    if (PANDA_CROSSING_MAP_MANAGE_CROSSED_BORDER) {
        for (size_t i = 1_MB; i < OBJ_SIZE; i += 1_MB) {
            void *addr = ToVoidPtr(ToUintPtr(obj_addr) + i);
            ASSERT_TRUE(GetCrossingMap()->FindFirstObject(addr, addr) == nullptr) << " seed = " << GetSeed();
        }
    }
}

TEST_F(CrossingMapTest, TwoSequentialObjectsTest)
{
    static constexpr size_t FIRST_OBJ_SIZE = MIN_GAP_BETWEEN_OBJECTS;
    static constexpr size_t SECOND_OBJ_SIZE = 1_KB;
    // Add some extra memory for possible shifts
    void *first_obj_addr = GetRandomObjAddr(FIRST_OBJ_SIZE + SECOND_OBJ_SIZE + FIRST_OBJ_SIZE);
    void *second_obj_addr = IncreaseAddr(first_obj_addr, FIRST_OBJ_SIZE);
    // We must be sure that these objects will be saved in the same locations
    if (GetMapNumFromAddr(first_obj_addr) != GetMapNumFromAddr(second_obj_addr)) {
        first_obj_addr = IncreaseAddr(first_obj_addr, FIRST_OBJ_SIZE);
        second_obj_addr = IncreaseAddr(first_obj_addr, FIRST_OBJ_SIZE);
        ASSERT_TRUE(GetMapNumFromAddr(first_obj_addr) == GetMapNumFromAddr(second_obj_addr)) << " seed = " << GetSeed();
    }
    GetCrossingMap()->AddObject(first_obj_addr, FIRST_OBJ_SIZE);
    GetCrossingMap()->AddObject(second_obj_addr, SECOND_OBJ_SIZE);

    ASSERT_TRUE(GetCrossingMap()->FindFirstObject(first_obj_addr, first_obj_addr) == first_obj_addr)
        << " seed = " << GetSeed();

    GetCrossingMap()->RemoveObject(first_obj_addr, FIRST_OBJ_SIZE, second_obj_addr);
    ASSERT_TRUE(GetCrossingMap()->FindFirstObject(first_obj_addr, first_obj_addr) == second_obj_addr)
        << " seed = " << GetSeed();

    GetCrossingMap()->RemoveObject(second_obj_addr, SECOND_OBJ_SIZE);
    ASSERT_TRUE(GetCrossingMap()->FindFirstObject(first_obj_addr, first_obj_addr) == nullptr)
        << " seed = " << GetSeed();
}

TEST_F(CrossingMapTest, TwoNonSequentialObjectsTest)
{
    static constexpr size_t FIRST_OBJ_SIZE = MIN_GAP_BETWEEN_OBJECTS;
    static constexpr size_t GAP_BETWEEN_OBJECTS = 1_MB;
    static constexpr size_t SECOND_OBJ_SIZE = 1_KB;
    // Add some extra memory for possible shifts
    void *first_obj_addr = GetRandomObjAddr(FIRST_OBJ_SIZE + SECOND_OBJ_SIZE + GAP_BETWEEN_OBJECTS);
    void *second_obj_addr = IncreaseAddr(first_obj_addr, FIRST_OBJ_SIZE + GAP_BETWEEN_OBJECTS);

    GetCrossingMap()->AddObject(first_obj_addr, FIRST_OBJ_SIZE);
    GetCrossingMap()->AddObject(second_obj_addr, SECOND_OBJ_SIZE);

    ASSERT_TRUE(GetCrossingMap()->FindFirstObject(first_obj_addr, second_obj_addr) == first_obj_addr)
        << " seed = " << GetSeed();

    GetCrossingMap()->RemoveObject(first_obj_addr, FIRST_OBJ_SIZE, second_obj_addr);
    ASSERT_TRUE(GetCrossingMap()->FindFirstObject(first_obj_addr, first_obj_addr) == nullptr)
        << " seed = " << GetSeed();
    ASSERT_TRUE(GetCrossingMap()->FindFirstObject(first_obj_addr, second_obj_addr) == second_obj_addr)
        << " seed = " << GetSeed();

    GetCrossingMap()->RemoveObject(second_obj_addr, SECOND_OBJ_SIZE);
    ASSERT_TRUE(GetCrossingMap()->FindFirstObject(first_obj_addr, second_obj_addr) == nullptr)
        << " seed = " << GetSeed();
}

TEST_F(CrossingMapTest, ThreeSequentialObjectsTest)
{
    static constexpr size_t FIRST_OBJ_SIZE = 4_MB;
    static constexpr size_t SECOND_OBJ_SIZE = MIN_GAP_BETWEEN_OBJECTS;
    static constexpr size_t THIRD_OBJ_SIZE = 1_KB;
    // Add some extra memory for possible shifts
    void *first_obj_addr = GetRandomObjAddr(FIRST_OBJ_SIZE + SECOND_OBJ_SIZE + THIRD_OBJ_SIZE + 3 * SECOND_OBJ_SIZE);
    void *second_obj_addr = IncreaseAddr(first_obj_addr, FIRST_OBJ_SIZE);
    void *third_obj_addr = IncreaseAddr(second_obj_addr, SECOND_OBJ_SIZE);

    // We must be sure that the first object will cross the borders for the second one
    if (GetMapNumFromAddr(GetLastObjectByte(first_obj_addr, FIRST_OBJ_SIZE)) != GetMapNumFromAddr(second_obj_addr)) {
        first_obj_addr = IncreaseAddr(first_obj_addr, SECOND_OBJ_SIZE);
        second_obj_addr = IncreaseAddr(first_obj_addr, FIRST_OBJ_SIZE);
        third_obj_addr = IncreaseAddr(second_obj_addr, SECOND_OBJ_SIZE);
        ASSERT_TRUE(GetMapNumFromAddr(GetLastObjectByte(first_obj_addr, FIRST_OBJ_SIZE)) ==
                    GetMapNumFromAddr(second_obj_addr))
            << " seed = " << GetSeed();
    }

    // We must be sure that the second and the third object will be saved in the same locations
    if (GetMapNumFromAddr(second_obj_addr) != GetMapNumFromAddr(third_obj_addr)) {
        first_obj_addr = IncreaseAddr(first_obj_addr, 2 * SECOND_OBJ_SIZE);
        second_obj_addr = IncreaseAddr(first_obj_addr, FIRST_OBJ_SIZE);
        third_obj_addr = IncreaseAddr(second_obj_addr, SECOND_OBJ_SIZE);
        ASSERT_TRUE(GetMapNumFromAddr(GetLastObjectByte(first_obj_addr, FIRST_OBJ_SIZE)) ==
                    GetMapNumFromAddr(second_obj_addr))
            << " seed = " << GetSeed();
        ASSERT_TRUE(GetMapNumFromAddr(second_obj_addr) == GetMapNumFromAddr(third_obj_addr)) << " seed = " << GetSeed();
    }

    GetCrossingMap()->AddObject(first_obj_addr, FIRST_OBJ_SIZE);
    GetCrossingMap()->AddObject(second_obj_addr, SECOND_OBJ_SIZE);
    GetCrossingMap()->AddObject(third_obj_addr, THIRD_OBJ_SIZE);

    if (PANDA_CROSSING_MAP_MANAGE_CROSSED_BORDER) {
        ASSERT_TRUE(GetCrossingMap()->FindFirstObject(second_obj_addr, second_obj_addr) == first_obj_addr)
            << " seed = " << GetSeed();
    } else {
        ASSERT_TRUE(GetCrossingMap()->FindFirstObject(second_obj_addr, second_obj_addr) == second_obj_addr)
            << " seed = " << GetSeed();
    }

    GetCrossingMap()->RemoveObject(second_obj_addr, SECOND_OBJ_SIZE, third_obj_addr, first_obj_addr, FIRST_OBJ_SIZE);
    if (PANDA_CROSSING_MAP_MANAGE_CROSSED_BORDER) {
        ASSERT_TRUE(GetCrossingMap()->FindFirstObject(second_obj_addr, second_obj_addr) == first_obj_addr)
            << " seed = " << GetSeed();
    } else {
        ASSERT_TRUE(GetCrossingMap()->FindFirstObject(second_obj_addr, second_obj_addr) == third_obj_addr)
            << " seed = " << GetSeed();
    }
    GetCrossingMap()->RemoveObject(third_obj_addr, THIRD_OBJ_SIZE, nullptr, first_obj_addr, FIRST_OBJ_SIZE);
    if (PANDA_CROSSING_MAP_MANAGE_CROSSED_BORDER) {
        ASSERT_TRUE(GetCrossingMap()->FindFirstObject(second_obj_addr, second_obj_addr) == first_obj_addr)
            << " seed = " << GetSeed();
    } else {
        ASSERT_TRUE(GetCrossingMap()->FindFirstObject(second_obj_addr, second_obj_addr) == nullptr)
            << " seed = " << GetSeed();
    }

    GetCrossingMap()->RemoveObject(first_obj_addr, FIRST_OBJ_SIZE);
    ASSERT_TRUE(GetCrossingMap()->FindFirstObject(second_obj_addr, second_obj_addr) == nullptr)
        << " seed = " << GetSeed();
}

TEST_F(CrossingMapTest, InitializeCrosingMapForMemoryTest)
{
    static constexpr size_t POOL_COUNT = 6;
    static constexpr size_t GRANULARITY = 2;
    GetCrossingMap()->RemoveCrossingMapForMemory(ToVoidPtr(GetPoolMinAddress()), GetPoolSize());
    void *start_addr =
        GetRandomObjAddr((POOLS_SIZE * 2 + PANDA_POOL_ALIGNMENT_IN_BYTES) * POOL_COUNT + PANDA_POOL_ALIGNMENT_IN_BYTES);
    uintptr_t aligned_start_addr = AlignUp(ToUintPtr(start_addr), PANDA_POOL_ALIGNMENT_IN_BYTES);

    std::array<bool, POOL_COUNT> deleted_pools;
    for (size_t i = 0; i < POOL_COUNT; i++) {
        void *pool_addr = ToVoidPtr(aligned_start_addr + i * (POOLS_SIZE * 2 + PANDA_POOL_ALIGNMENT_IN_BYTES));
        GetCrossingMap()->InitializeCrossingMapForMemory(pool_addr, POOLS_SIZE * 2);
        deleted_pools[i] = false;
    }

    for (size_t i = 0; i < POOL_COUNT; i += GRANULARITY) {
        void *pool_addr = ToVoidPtr(aligned_start_addr + i * (POOLS_SIZE * 2 + PANDA_POOL_ALIGNMENT_IN_BYTES));
        GetCrossingMap()->RemoveCrossingMapForMemory(pool_addr, POOLS_SIZE * 2);
        deleted_pools[i] = true;
    }

    for (size_t i = 0; i < POOL_COUNT; i++) {
        if (deleted_pools[i] == true) {
            continue;
        }
        void *pool_addr = ToVoidPtr(aligned_start_addr + i * (POOLS_SIZE * 2 + PANDA_POOL_ALIGNMENT_IN_BYTES));
        GetCrossingMap()->RemoveCrossingMapForMemory(pool_addr, POOLS_SIZE * 2);
    }

    GetCrossingMap()->InitializeCrossingMapForMemory(ToVoidPtr(GetPoolMinAddress()), GetPoolSize());
}

}  // namespace panda::mem
