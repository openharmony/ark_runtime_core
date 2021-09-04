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

#include "mem/arena.h"
#include "mem/pool_manager.h"
#include "mem/mem.h"
#include "mem/mem_config.h"
#include "mem/mmap_mem_pool-inl.h"

#include "gtest/gtest.h"
#include "utils/logger.h"

namespace panda {

class ArenaTest : public testing::Test {
public:
    ArenaTest()
    {
        panda::mem::MemConfig::Initialize(0, 16_MB, 0, 0);
        PoolManager::Initialize();
    }

    ~ArenaTest()
    {
        PoolManager::Finalize();
        panda::mem::MemConfig::Finalize();
    }

protected:
    static constexpr size_t ARENA_SIZE = 1_MB;

    template <class ArenaT>
    ArenaT *CreateArena(size_t size) const
    {
        return PoolManager::GetMmapMemPool()->AllocArena<ArenaT>(size, SpaceType::SPACE_TYPE_INTERNAL,
                                                                 AllocatorType::ARENA_ALLOCATOR);
    }

    template <class ArenaT>
    void GetOccupiedAndFreeSizeTestImplementation(size_t arena_size, size_t alloc_size) const
    {
        ASSERT_TRUE(arena_size != 0);
        ASSERT_TRUE(alloc_size != 0);
        ArenaT *arena = CreateArena<ArenaT>(arena_size);
        size_t old_free_size = arena->GetFreeSize();
        ASSERT_TRUE(arena->Alloc(alloc_size) != nullptr);
        ASSERT_TRUE(arena->GetOccupiedSize() == alloc_size);
        ASSERT_TRUE(old_free_size - alloc_size == arena->GetFreeSize());
    }

    template <class ArenaT>
    void ResizeAndResetTestImplementation(size_t arena_size, size_t alloc_size) const
    {
        ASSERT_TRUE(arena_size != 0);
        ASSERT_TRUE(alloc_size != 0);
        ArenaT *arena = CreateArena<ArenaT>(arena_size);
        ASSERT_TRUE(alloc_size * 2U <= arena->GetFreeSize());
        void *first_allocation = arena->Alloc(alloc_size);
        void *second_allocation = arena->Alloc(alloc_size);
        ASSERT_TRUE(first_allocation != nullptr);
        ASSERT_TRUE(first_allocation != nullptr);
        ASSERT_TRUE(arena->GetOccupiedSize() == 2U * alloc_size);
        arena->Resize(alloc_size);
        ASSERT_TRUE(arena->GetOccupiedSize() == alloc_size);
        void *third_allocation = arena->Alloc(alloc_size);
        // we expect that we get the same address
        ASSERT_TRUE(ToUintPtr(second_allocation) == ToUintPtr(third_allocation));
        ASSERT_TRUE(arena->GetOccupiedSize() == 2U * alloc_size);
        arena->Reset();
        ASSERT_TRUE(arena->GetOccupiedSize() == 0);
    }
};

TEST_F(ArenaTest, GetOccupiedAndFreeSizeTest)
{
    static constexpr size_t ALLOC_SIZE = AlignUp(ARENA_SIZE / 2, GetAlignmentInBytes(ARENA_DEFAULT_ALIGNMENT));
    static constexpr Alignment ARENA_ALIGNMENT = LOG_ALIGN_4;
    static constexpr size_t ALIGNED_ALLOC_SIZE = AlignUp(ALLOC_SIZE, GetAlignmentInBytes(ARENA_ALIGNMENT));
    GetOccupiedAndFreeSizeTestImplementation<Arena>(ARENA_SIZE, ALLOC_SIZE);
    GetOccupiedAndFreeSizeTestImplementation<AlignedArena<ARENA_ALIGNMENT>>(ARENA_SIZE, ALIGNED_ALLOC_SIZE);
    GetOccupiedAndFreeSizeTestImplementation<DoubleLinkedAlignedArena<ARENA_ALIGNMENT>>(ARENA_SIZE, ALIGNED_ALLOC_SIZE);
}

TEST_F(ArenaTest, ResizeAndResetTest)
{
    static constexpr size_t ALLOC_SIZE = AlignUp(ARENA_SIZE / 3, GetAlignmentInBytes(ARENA_DEFAULT_ALIGNMENT));
    static constexpr Alignment ARENA_ALIGNMENT = LOG_ALIGN_4;
    static constexpr size_t ALIGNED_ALLOC_SIZE = AlignUp(ALLOC_SIZE, GetAlignmentInBytes(ARENA_ALIGNMENT));
    ResizeAndResetTestImplementation<Arena>(ARENA_SIZE, ALLOC_SIZE);
    ResizeAndResetTestImplementation<AlignedArena<ARENA_ALIGNMENT>>(ARENA_SIZE, ALIGNED_ALLOC_SIZE);
    ResizeAndResetTestImplementation<DoubleLinkedAlignedArena<ARENA_ALIGNMENT>>(ARENA_SIZE, ALIGNED_ALLOC_SIZE);
}

}  // namespace panda
