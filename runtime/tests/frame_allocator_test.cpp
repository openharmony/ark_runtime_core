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
#include <cstdint>
#include <limits>
#include <string>

#include "libpandabase/mem/mem.h"
#include "libpandabase/mem/mem_config.h"
#include "libpandabase/mem/pool_manager.h"
#include "libpandabase/utils/logger.h"
#include "runtime/mem/frame_allocator-inl.h"
#include "runtime/tests/allocator_test_base.h"

namespace panda::mem {

class FrameAllocatorTest : public AllocatorTest<FrameAllocator<>> {
public:
    FrameAllocatorTest()
    {
        panda::mem::MemConfig::Initialize(0, 256_MB, 0, 0);
    }

    ~FrameAllocatorTest()
    {
        panda::mem::MemConfig::Finalize();
    }

protected:
    void AddMemoryPoolToAllocator([[maybe_unused]] FrameAllocator<> &allocator) override {}
    void AddMemoryPoolToAllocatorProtected([[maybe_unused]] FrameAllocator<> &allocator) override {}
    bool AllocatedByThisAllocator([[maybe_unused]] FrameAllocator<> &allocator, [[maybe_unused]] void *mem) override
    {
        return false;
    }

    void PrintMemory(void *dst, size_t size)
    {
        std::cout << "Print at memory: ";
        auto *mem = static_cast<uint8_t *>(dst);
        for (size_t i = 0; i < size; i++) {
            std::cout << *mem++;
        }
        std::cout << std::endl;
    }

    void PrintAtIndex(size_t idx, size_t size)
    {
        std::cout << "Print at index:  ";
        ASSERT(idx + size < BYTE_ARRAY_SIZE);
        uint8_t *mem = static_cast<uint8_t *>(&byte_array_[idx]);
        for (size_t i = 0; i < size; i++) {
            std::cout << *mem++;
        }
        std::cout << std::endl;
    }

    void SetUp() override
    {
        PoolManager::Initialize();
    }

    void TearDown() override
    {
        PoolManager::Finalize();
    }

    size_t AllocNewArena(FrameAllocator<> *alloc)
    {
        bool is_allocated = alloc->TryAllocateNewArena();
        ASSERT(is_allocated);
        return is_allocated ? alloc->biggest_arena_size_ : 0;
    }

    void DeallocateLastArena(FrameAllocator<> *alloc)
    {
        alloc->FreeLastArena();
    }
};

TEST_F(FrameAllocatorTest, SmallAllocateTest)
{
    constexpr size_t ITERATIONS = 32;
    constexpr size_t FRAME_SIZE = 256;
    FrameAllocator<> alloc;
    std::array<void *, ITERATIONS + 1> array {nullptr};
    for (size_t i = 1; i <= ITERATIONS; i++) {
        array[i] = alloc.Alloc(FRAME_SIZE);
        ASSERT_NE(array[i], nullptr);
        *(static_cast<uint64_t *>(array[i])) = i;
    }
    for (size_t i = ITERATIONS; i != 0; i--) {
        ASSERT_EQ(*(static_cast<uint64_t *>(array[i])), i);
        alloc.Free(array[i]);
    }
}

template <Alignment alignment>
void AlignmentTest(FrameAllocator<alignment> &alloc)
{
    constexpr size_t MAX_SIZE = 256;
    std::array<void *, MAX_SIZE + 1> array {nullptr};
    for (size_t i = 1; i <= MAX_SIZE; i++) {
        array[i] = alloc.Alloc(i * GetAlignmentInBytes(alignment));
        if (array[i] == nullptr) {
            break;
        }
        ASSERT_NE(array[i], nullptr);
        ASSERT_EQ(ToUintPtr(array[i]), AlignUp(ToUintPtr(array[i]), GetAlignmentInBytes(alignment)));
        *(static_cast<uint64_t *>(array[i])) = i;
    }
    for (size_t i = MAX_SIZE; i != 0; i--) {
        if (array[i] == nullptr) {
            break;
        }
        ASSERT_EQ(*(static_cast<uint64_t *>(array[i])), i);
        alloc.Free(array[i]);
    }
}

TEST_F(FrameAllocatorTest, DefaultAlignmentTest)
{
    FrameAllocator<> alloc;
    AlignmentTest(alloc);
}

TEST_F(FrameAllocatorTest, NonDefaultAlignmentTest)
{
    FrameAllocator<Alignment::LOG_ALIGN_4> alloc4;
    AlignmentTest(alloc4);
    FrameAllocator<Alignment::LOG_ALIGN_5> alloc5;
    AlignmentTest(alloc5);
}

TEST_F(FrameAllocatorTest, CycledAllocateFreeForHugeFramesTest)
{
    constexpr size_t ITERATIONS = 1024;
    constexpr size_t FRAME_SIZE = 512;
    constexpr int CYCLE_COUNT = 16;

    FrameAllocator<> alloc;
    std::vector<std::pair<void *, size_t>> vec;

    for (int j = 0; j < CYCLE_COUNT; j++) {
        for (size_t i = 1; i <= ITERATIONS; i++) {
            void *mem = alloc.Alloc(FRAME_SIZE);
            ASSERT_TRUE(mem != nullptr) << "Didn't allocate " << FRAME_SIZE << " bytes in " << j
                                        << " cycle, seed: " << seed_;
            vec.emplace_back(mem, SetBytesFromByteArray(mem, FRAME_SIZE));
        }
        for (size_t i = 1; i <= ITERATIONS / 2U; i++) {
            std::pair<void *, size_t> last_pair = vec.back();
            ASSERT_TRUE(CompareBytesWithByteArray(last_pair.first, FRAME_SIZE, last_pair.second))
                << "iteration: " << i << ", size: " << FRAME_SIZE << ", address: " << std::hex << last_pair.first
                << ", index in byte array: " << last_pair.second << ", seed: " << seed_;
            alloc.Free(last_pair.first);
            vec.pop_back();
        }
    }
    while (!vec.empty()) {
        std::pair<void *, size_t> last_pair = vec.back();
        ASSERT_TRUE(CompareBytesWithByteArray(last_pair.first, FRAME_SIZE, last_pair.second))
            << "vector size: " << vec.size() << ", size: " << FRAME_SIZE << ", address: " << std::hex << last_pair.first
            << ", index in byte array: " << last_pair.second << ", seed: " << seed_;
        alloc.Free(last_pair.first);
        vec.pop_back();
    }
}

TEST_F(FrameAllocatorTest, ValidateArenaGrownPolicy)
{
    constexpr size_t ITERATIONS = 16;
    FrameAllocator<> alloc;
    size_t last_alloc_arena_size = 0;
    for (size_t i = 0; i < ITERATIONS; i++) {
        size_t new_arena_size = AllocNewArena(&alloc);
        ASSERT_EQ(new_arena_size > last_alloc_arena_size, true);
        last_alloc_arena_size = new_arena_size;
    }

    for (size_t i = 0; i < ITERATIONS; i++) {
        DeallocateLastArena(&alloc);
    }

    size_t new_arena_size = AllocNewArena(&alloc);
    ASSERT_EQ(new_arena_size == last_alloc_arena_size, true);
}

TEST_F(FrameAllocatorTest, CheckAddrInsideAllocator)
{
    constexpr size_t ITERATIONS = 16;
    static constexpr size_t FRAME_SIZE = 256;
    void *invalid_addr = std::malloc(10);

    FrameAllocator<> alloc;
    ASSERT_FALSE(alloc.Contains(invalid_addr));
    for (size_t i = 0; i < ITERATIONS; i++) {
        AllocNewArena(&alloc);
    }
    void *addr1_inside = alloc.Alloc(FRAME_SIZE);
    ASSERT_TRUE(alloc.Contains(addr1_inside));
    ASSERT_FALSE(alloc.Contains(invalid_addr));

    alloc.Free(addr1_inside);
    ASSERT_FALSE(alloc.Contains(addr1_inside));
    ASSERT_FALSE(alloc.Contains(invalid_addr));

    addr1_inside = alloc.Alloc(FRAME_SIZE);
    for (size_t i = 0; i < ITERATIONS; i++) {
        AllocNewArena(&alloc);
    }
    auto *addr2_inside = alloc.Alloc(FRAME_SIZE * 2);
    ASSERT_TRUE(alloc.Contains(addr1_inside));
    ASSERT_TRUE(alloc.Contains(addr2_inside));
    ASSERT_FALSE(alloc.Contains(invalid_addr));
    free(invalid_addr);
}

}  // namespace panda::mem
