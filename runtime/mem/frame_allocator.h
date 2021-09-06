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

#ifndef PANDA_RUNTIME_MEM_FRAME_ALLOCATOR_H_
#define PANDA_RUNTIME_MEM_FRAME_ALLOCATOR_H_

#include <securec.h>
#include <array>

#include "libpandabase/mem/arena.h"
#include "libpandabase/mem/mem.h"
#include "libpandabase/mem/mmap_mem_pool-inl.h"

namespace panda::mem {

//                                          Allocation flow looks like that:
//
//    Allocate arenas for frames                  Frames free              Return arenas   Second allocated arena
//            (stage 1)                            (stage 2)                 (stage 3)     will be bigger than the
//                                                                                         second at stage 1
//                        |-----|                             |-----|                                   |-----|
//                        |     |                             |     |                                   |     |
//              |-----|   |     |                   |-----|   |     |                                   |     |
//              |xxxxx|   |     |                   |     |   |     |                                   |     |
//    |-----|   |xxxxx|   |xxxxx|         |-----|   |     |   |     |         |-----|         |-----|   |     |
//    |xxxxx|   |xxxxx|   |xxxxx|  ---->  |     |   |     |   |     |  ---->  |     |  ---->  |xxxxx|   |xxxxx|
//    |xxxxx|   |xxxxx|   |xxxxx|         |     |   |     |   |     |         |     |         |xxxxx|   |xxxxx|
//    |xxxxx|   |xxxxx|   |xxxxx|         |     |   |     |   |     |         |     |         |xxxxx|   |xxxxx|
//    |xxxxx|   |xxxxx|   |xxxxx|         |     |   |     |   |     |         |     |         |xxxxx|   |xxxxx|
//    |xxxxx|   |xxxxx|   |xxxxx|         |xxxxx|   |     |   |     |         |xxxxx|         |xxxxx|   |xxxxx|
//    |-----|   |-----|   |-----|         |-----|   |-----|   |-----|         |-----|         |-----|   |-----|

// Frame allocator uses arenas and works like a stack -
// it will give memory from the top and can delete only last allocated memory.
template <Alignment AlignmenT = DEFAULT_FRAME_ALIGNMENT, bool UseMemsetT = true>
class FrameAllocator {
public:
    FrameAllocator();
    ~FrameAllocator();
    FrameAllocator(const FrameAllocator &) noexcept = delete;
    FrameAllocator(FrameAllocator &&) noexcept = default;
    FrameAllocator &operator=(const FrameAllocator &) noexcept = delete;
    FrameAllocator &operator=(FrameAllocator &&) noexcept = default;

    [[nodiscard]] void *Alloc(size_t size);

    // We must free objects allocated by this allocator strictly in reverse order
    void Free(void *mem);

    /**
     * \brief Returns true if address inside current allocator.
     */
    bool Contains(void *mem);

    static constexpr AllocatorType GetAllocatorType()
    {
        return AllocatorType::FRAME_ALLOCATOR;
    }

private:
    using FramesArena = DoubleLinkedAlignedArena<AlignmenT>;
    static constexpr size_t FIRST_ARENA_SIZE = 256_KB;
    static_assert(FIRST_ARENA_SIZE % PANDA_POOL_ALIGNMENT_IN_BYTES == 0);
    static constexpr size_t ARENA_SIZE_GREW_LEVEL = FIRST_ARENA_SIZE;
    static constexpr size_t FRAME_ALLOC_MIN_FREE_MEMORY_THRESHOLD = FIRST_ARENA_SIZE / 2;
    static constexpr size_t FRAME_ALLOC_MAX_FREE_ARENAS_THRESHOLD = 1;

    /**
     * \brief Heuristic for arena size increase.
     * @return new size
     */
    size_t GetNextArenaSize(size_t size);

    /**
     * \brief Try to allocate an arena from the memory.
     * @return true on success, or false on fail
     */
    bool TryAllocateNewArena(size_t size = ARENA_SIZE_GREW_LEVEL);

    /**
     * \brief Try to allocate memory for a frame in the current arena or in the next one if it exists.
     * @param size - size of the allocated memory
     * @return pointer to the allocated memory on success, or nullptr on fail
     */
    void *TryToAllocate(size_t size);

    /**
     * \brief Free last_allocated_arena_, i.e., free last arena in the list.
     */
    void FreeLastArena();

    // A pointer to the current arena with the last allocated frame
    FramesArena *cur_arena_ {nullptr};

    // A pointer to the last allocated arena (so it is equal to the top arena in the list)
    FramesArena *last_alloc_arena_ {nullptr};

    // The biggest arena size during FrameAllocator workflow. Needed for computing a new arena size.
    size_t biggest_arena_size_ {0};

    // A marker which tells us if we need to increase the size of a new arena or not.
    bool arena_size_need_to_grow_ {true};

    size_t empty_arenas_count_ {0};

    MmapMemPool *mem_pool_alloc_ {nullptr};
    friend class FrameAllocatorTest;
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_FRAME_ALLOCATOR_H_
