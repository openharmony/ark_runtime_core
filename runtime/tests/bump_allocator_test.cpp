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

#include <ctime>

#include "gtest/gtest.h"
#include "runtime/mem/alloc_config.h"
#include "runtime/mem/bump-allocator-inl.h"

namespace panda::mem {

template <bool UseTlabs>
using NonObjectBumpAllocator =
    BumpPointerAllocator<EmptyMemoryConfig, BumpPointerAllocatorLockConfig::CommonLock, UseTlabs>;

class BumpAllocatorTest : public testing::Test {
public:
    BumpAllocatorTest()
    {
#ifdef PANDA_NIGHTLY_TEST_ON
        seed_ = std::time(NULL);
#else
        seed_ = 0x0BADDEAD;
#endif
        srand(seed_);
        panda::mem::MemConfig::Initialize(0, 8_MB, 0, 0);
        PoolManager::Initialize();
    }

    ~BumpAllocatorTest()
    {
        for (auto i : allocated_mem_mmap_) {
            panda::os::mem::UnmapRaw(std::get<0>(i), std::get<1>(i));
        }
        for (auto i : allocated_arenas) {
            delete i;
        }
        PoolManager::Finalize();
        panda::mem::MemConfig::Finalize();
    }

protected:
    Arena *AllocateArena(size_t size)
    {
        void *mem = panda::os::mem::MapRWAnonymousRaw(size);
        ASAN_UNPOISON_MEMORY_REGION(mem, size);
        std::pair<void *, size_t> new_pair {mem, size};
        allocated_mem_mmap_.push_back(new_pair);
        auto arena = new Arena(size, mem);
        allocated_arenas.push_back(arena);
        return arena;
    }

    std::vector<std::pair<void *, size_t>> allocated_mem_mmap_;
    std::vector<Arena *> allocated_arenas;
    unsigned seed_ {0};
};

TEST_F(BumpAllocatorTest, AlignedAlloc)
{
    testing::FLAGS_gtest_death_test_style = "fast";

    constexpr size_t BUFF_SIZE = SIZE_1M;
    constexpr size_t ARRAY_SIZE = 1024;
    auto pool = PoolManager::GetMmapMemPool()->AllocPool(BUFF_SIZE, SpaceType::SPACE_TYPE_INTERNAL,
                                                         AllocatorType::BUMP_ALLOCATOR);
    mem::MemStatsType mem_stats;
    NonObjectBumpAllocator<false> bp_allocator(pool, SpaceType::SPACE_TYPE_INTERNAL, &mem_stats);
    Alignment align = DEFAULT_ALIGNMENT;
    std::array<int *, ARRAY_SIZE> arr;

    size_t mask = GetAlignmentInBytes(align) - 1;

    // Allocations
    srand(seed_);
    for (size_t i = 0; i < ARRAY_SIZE; ++i) {
        arr[i] = static_cast<int *>(bp_allocator.Alloc(sizeof(int), align));
        *arr[i] = rand() % std::numeric_limits<int>::max();
    }

    // Allocations checking
    srand(seed_);
    for (size_t i = 0; i < ARRAY_SIZE; ++i) {
        ASSERT_NE(arr[i], nullptr) << "value of i: " << i << ", align: " << align << ", seed:" << seed_;
        ASSERT_EQ(reinterpret_cast<size_t>(arr[i]) & mask, static_cast<size_t>(0))
            << "value of i: " << i << ", align: " << align << ", seed:" << seed_;
        ASSERT_EQ(*arr[i], rand() % std::numeric_limits<int>::max())
            << "value of i: " << i << ", align: " << align << ", seed:" << seed_;
    }
    static_assert(LOG_ALIGN_MAX != DEFAULT_ALIGNMENT, "We expect minimal alignment != DEFAULT_ALIGNMENT");
    void *ptr;
#ifndef NDEBUG
    EXPECT_DEATH_IF_SUPPORTED(ptr = bp_allocator.Alloc(sizeof(int), LOG_ALIGN_MAX), "alignment == DEFAULT_ALIGNMENT")
        << ", seed:" << seed_;
#endif
    ptr = bp_allocator.Alloc(SIZE_1M);
    ASSERT_EQ(ptr, nullptr) << "Here Alloc with allocation size = 1 MB should return nullptr"
                            << ", seed:" << seed_;
}

TEST_F(BumpAllocatorTest, CreateTLABAndAlloc)
{
    using ALLOC_TYPE = uint64_t;
    static_assert(sizeof(ALLOC_TYPE) % DEFAULT_ALIGNMENT_IN_BYTES == 0);
    constexpr size_t TLAB_SIZE = SIZE_1M;
    constexpr size_t COMMON_BUFFER_SIZE = SIZE_1M;
    constexpr size_t ALLOC_SIZE = sizeof(ALLOC_TYPE);
    constexpr size_t TLAB_ALLOC_COUNT_SIZE = TLAB_SIZE / ALLOC_SIZE;
    constexpr size_t COMMON_ALLOC_COUNT_SIZE = COMMON_BUFFER_SIZE / ALLOC_SIZE;

    size_t mask = DEFAULT_ALIGNMENT_IN_BYTES - 1;

    std::array<ALLOC_TYPE *, TLAB_ALLOC_COUNT_SIZE> tlab_elements;
    std::array<ALLOC_TYPE *, TLAB_ALLOC_COUNT_SIZE> common_elements;
    auto pool = PoolManager::GetMmapMemPool()->AllocPool(TLAB_SIZE + COMMON_BUFFER_SIZE, SpaceType::SPACE_TYPE_INTERNAL,
                                                         AllocatorType::BUMP_ALLOCATOR);
    mem::MemStatsType mem_stats;
    NonObjectBumpAllocator<true> allocator(pool, SpaceType::SPACE_TYPE_OBJECT, &mem_stats, 1);
    {
        // Allocations in common buffer
        srand(seed_);
        for (size_t i = 0; i < COMMON_ALLOC_COUNT_SIZE; ++i) {
            common_elements[i] = static_cast<ALLOC_TYPE *>(allocator.Alloc(sizeof(ALLOC_TYPE)));
            ASSERT_TRUE(common_elements[i] != nullptr) << ", seed:" << seed_;
            *common_elements[i] = rand() % std::numeric_limits<ALLOC_TYPE>::max();
        }

        TLAB *tlab = allocator.CreateNewTLAB(TLAB_SIZE);
        ASSERT_TRUE(tlab != nullptr) << ", seed:" << seed_;
        ASSERT_TRUE(allocator.CreateNewTLAB(TLAB_SIZE) == nullptr) << ", seed:" << seed_;
        // Allocations in TLAB
        srand(seed_);
        for (size_t i = 0; i < TLAB_ALLOC_COUNT_SIZE; ++i) {
            tlab_elements[i] = static_cast<ALLOC_TYPE *>(tlab->Alloc(sizeof(ALLOC_TYPE)));
            ASSERT_TRUE(tlab_elements[i] != nullptr) << ", seed:" << seed_;
            *tlab_elements[i] = rand() % std::numeric_limits<ALLOC_TYPE>::max();
        }

        // Check that we don't have memory in the buffer:
        ASSERT_TRUE(allocator.Alloc(sizeof(ALLOC_TYPE)) == nullptr);
        ASSERT_TRUE(tlab->Alloc(sizeof(ALLOC_TYPE)) == nullptr);

        // Allocations checking in common buffer
        srand(seed_);
        for (size_t i = 0; i < COMMON_ALLOC_COUNT_SIZE; ++i) {
            ASSERT_NE(common_elements[i], nullptr) << "value of i: " << i << ", seed:" << seed_;
            ASSERT_EQ(reinterpret_cast<size_t>(common_elements[i]) & mask, static_cast<size_t>(0))
                << "value of i: " << i << ", seed:" << seed_;
            ASSERT_EQ(*common_elements[i], rand() % std::numeric_limits<ALLOC_TYPE>::max())
                << "value of i: " << i << ", seed:" << seed_;
        }

        // Allocations checking in TLAB
        srand(seed_);
        for (size_t i = 0; i < TLAB_ALLOC_COUNT_SIZE; ++i) {
            ASSERT_NE(tlab_elements[i], nullptr) << "value of i: " << i << ", seed:" << seed_;
            ASSERT_EQ(reinterpret_cast<size_t>(tlab_elements[i]) & mask, static_cast<size_t>(0))
                << "value of i: " << i << ", seed:" << seed_;
            ASSERT_EQ(*tlab_elements[i], rand() % std::numeric_limits<ALLOC_TYPE>::max())
                << "value of i: " << i << ", seed:" << seed_;
        }
    }
    allocator.Reset();
    {
        TLAB *tlab = allocator.CreateNewTLAB(TLAB_SIZE);
        ASSERT_TRUE(tlab != nullptr) << ", seed:" << seed_;
        ASSERT_TRUE(allocator.CreateNewTLAB(TLAB_SIZE) == nullptr) << ", seed:" << seed_;
        // Allocations in TLAB
        srand(seed_);
        for (size_t i = 0; i < TLAB_ALLOC_COUNT_SIZE; ++i) {
            tlab_elements[i] = static_cast<ALLOC_TYPE *>(tlab->Alloc(sizeof(ALLOC_TYPE)));
            ASSERT_TRUE(tlab_elements[i] != nullptr) << ", seed:" << seed_;
            *tlab_elements[i] = rand() % std::numeric_limits<ALLOC_TYPE>::max();
        }

        // Allocations in common buffer
        srand(seed_);
        for (size_t i = 0; i < COMMON_ALLOC_COUNT_SIZE; ++i) {
            common_elements[i] = static_cast<ALLOC_TYPE *>(allocator.Alloc(sizeof(ALLOC_TYPE)));
            ASSERT_TRUE(common_elements[i] != nullptr) << ", seed:" << seed_;
            *common_elements[i] = rand() % std::numeric_limits<ALLOC_TYPE>::max();
        }

        // Check that we don't have memory in the buffer:
        ASSERT_TRUE(allocator.Alloc(sizeof(ALLOC_TYPE)) == nullptr);
        ASSERT_TRUE(tlab->Alloc(sizeof(ALLOC_TYPE)) == nullptr);

        // Allocations checking in TLAB
        srand(seed_);
        for (size_t i = 0; i < TLAB_ALLOC_COUNT_SIZE; ++i) {
            ASSERT_NE(tlab_elements[i], nullptr) << "value of i: " << i << ", seed:" << seed_;
            ASSERT_EQ(reinterpret_cast<size_t>(tlab_elements[i]) & mask, static_cast<size_t>(0))
                << "value of i: " << i << ", seed:" << seed_;
            ASSERT_EQ(*tlab_elements[i], rand() % std::numeric_limits<ALLOC_TYPE>::max())
                << "value of i: " << i << ", seed:" << seed_;
        }

        // Allocations checking in common buffer
        srand(seed_);
        for (size_t i = 0; i < COMMON_ALLOC_COUNT_SIZE; ++i) {
            ASSERT_NE(common_elements[i], nullptr) << "value of i: " << i << ", seed:" << seed_;
            ASSERT_EQ(reinterpret_cast<size_t>(common_elements[i]) & mask, static_cast<size_t>(0))
                << "value of i: " << i << ", seed:" << seed_;
            ASSERT_EQ(*common_elements[i], rand() % std::numeric_limits<ALLOC_TYPE>::max())
                << "value of i: " << i << ", seed:" << seed_;
        }
    }
}

TEST_F(BumpAllocatorTest, CreateTooManyTLABS)
{
    constexpr size_t TLAB_SIZE = SIZE_1M;
    constexpr size_t TLAB_COUNT = 3;
    auto pool = PoolManager::GetMmapMemPool()->AllocPool(TLAB_SIZE * TLAB_COUNT, SpaceType::SPACE_TYPE_INTERNAL,
                                                         AllocatorType::BUMP_ALLOCATOR);
    mem::MemStatsType mem_stats;
    NonObjectBumpAllocator<true> allocator(pool, SpaceType::SPACE_TYPE_OBJECT, &mem_stats, TLAB_COUNT - 1);
    {
        for (size_t i = 0; i < TLAB_COUNT - 1; i++) {
            TLAB *tlab = allocator.CreateNewTLAB(TLAB_SIZE);
            ASSERT_TRUE(tlab != nullptr) << ", seed:" << seed_;
        }
        TLAB *tlab = allocator.CreateNewTLAB(TLAB_SIZE);
        ASSERT_TRUE(tlab == nullptr) << ", seed:" << seed_;
    }
    allocator.Reset();
    {
        for (size_t i = 0; i < TLAB_COUNT - 1; i++) {
            TLAB *tlab = allocator.CreateNewTLAB(TLAB_SIZE);
            ASSERT_TRUE(tlab != nullptr) << ", seed:" << seed_;
        }
        TLAB *tlab = allocator.CreateNewTLAB(TLAB_SIZE);
        ASSERT_TRUE(tlab == nullptr) << ", seed:" << seed_;
    }
}

}  // namespace panda::mem
