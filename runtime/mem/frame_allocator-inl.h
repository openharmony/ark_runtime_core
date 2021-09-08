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

#ifndef PANDA_RUNTIME_MEM_FRAME_ALLOCATOR_INL_H_
#define PANDA_RUNTIME_MEM_FRAME_ALLOCATOR_INL_H_

#include "runtime/mem/frame_allocator.h"

#include <cstring>

#include "libpandabase/mem/pool_manager.h"
#include "libpandabase/utils/logger.h"

namespace panda::mem {

using StackFrameAllocator = FrameAllocator<>;

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_FRAME_ALLOCATOR(level) LOG(level, ALLOC) << "FrameAllocator: "

template <Alignment AlignmenT, bool UseMemsetT>
inline FrameAllocator<AlignmenT, UseMemsetT>::FrameAllocator()
{
    LOG_FRAME_ALLOCATOR(DEBUG) << "Initializing of FrameAllocator";
    mem_pool_alloc_ = PoolManager::GetMmapMemPool();
    cur_arena_ = mem_pool_alloc_->AllocArena<FramesArena>(FIRST_ARENA_SIZE, SpaceType::SPACE_TYPE_INTERNAL,
                                                          AllocatorType::FRAME_ALLOCATOR, this);
    last_alloc_arena_ = cur_arena_;
    biggest_arena_size_ = FIRST_ARENA_SIZE;
    arena_size_need_to_grow_ = true;
    LOG_FRAME_ALLOCATOR(INFO) << "Initializing of FrameAllocator finished";
}

template <Alignment AlignmenT, bool UseMemsetT>
inline FrameAllocator<AlignmenT, UseMemsetT>::~FrameAllocator()
{
    LOG_FRAME_ALLOCATOR(DEBUG) << "Destroying of FrameAllocator";
    while (last_alloc_arena_ != nullptr) {
        LOG_FRAME_ALLOCATOR(DEBUG) << "Free arena at addr " << std::hex << last_alloc_arena_;
        FramesArena *prev_arena = last_alloc_arena_->GetPrevArena();
        mem_pool_alloc_->FreeArena<FramesArena>(last_alloc_arena_);
        last_alloc_arena_ = prev_arena;
    }
    cur_arena_ = nullptr;
    LOG_FRAME_ALLOCATOR(INFO) << "Destroying of FrameAllocator finished";
}

template <Alignment AlignmenT, bool UseMemsetT>
inline bool FrameAllocator<AlignmenT, UseMemsetT>::TryAllocateNewArena(size_t size)
{
    size_t arena_size = GetNextArenaSize(size);
    LOG_FRAME_ALLOCATOR(DEBUG) << "Try to allocate a new arena with size " << arena_size;
    auto new_arena = mem_pool_alloc_->AllocArena<FramesArena>(arena_size, SpaceType::SPACE_TYPE_INTERNAL,
                                                              AllocatorType::FRAME_ALLOCATOR, this);
    if (new_arena == nullptr) {
        LOG_FRAME_ALLOCATOR(DEBUG) << "Couldn't get memory for a new arena";
        arena_size_need_to_grow_ = false;
        return false;
    }
    last_alloc_arena_->LinkNext(new_arena);
    new_arena->LinkPrev(last_alloc_arena_);
    last_alloc_arena_ = new_arena;
    empty_arenas_count_++;
    LOG_FRAME_ALLOCATOR(DEBUG) << "Successfully allocate new arena with addr " << std::hex << new_arena;
    return true;
}

template <Alignment AlignmenT, bool UseMemsetT>
inline void *FrameAllocator<AlignmenT, UseMemsetT>::Alloc(size_t size)
{
    ASSERT(AlignUp(size, GetAlignmentInBytes(AlignmenT)) == size);
    // Try to get free memory from current arenas
    void *mem = TryToAllocate(size);

    if (UNLIKELY(mem == nullptr)) {
        LOG_FRAME_ALLOCATOR(DEBUG) << "Can't allocate " << size << " bytes for a new frame in current arenas";
        if (!TryAllocateNewArena(size)) {
            LOG_FRAME_ALLOCATOR(DEBUG) << "Can't allocate a new arena, return nullptr";
            return nullptr;
        }
        mem = TryToAllocate(size);
        if (mem == nullptr) {
            LOG_FRAME_ALLOCATOR(DEBUG) << "Can't allocate memory in a totally free arena, change default arenas sizes";
            return nullptr;
        }
    }

    ASSERT(AlignUp(ToUintPtr(mem), GetAlignmentInBytes(AlignmenT)) == ToUintPtr(mem));
    LOG_FRAME_ALLOCATOR(INFO) << "Allocated memory at addr " << std::hex << mem;
    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if constexpr (UseMemsetT) {
        (void)memset_s(mem, size, 0x00, size);
    }
    return mem;
}

template <Alignment AlignmenT, bool UseMemsetT>
inline void FrameAllocator<AlignmenT, UseMemsetT>::Free(void *mem)
{
    ASSERT(cur_arena_ != nullptr);  // must has been initialized!
    ASSERT(ToUintPtr(mem) == AlignUp(ToUintPtr(mem), GetAlignmentInBytes(AlignmenT)));
    if (cur_arena_->InArena(mem)) {
        cur_arena_->Free(mem);
    } else {
        ASSERT(cur_arena_->GetOccupiedSize() == 0);
        ASSERT(cur_arena_->GetPrevArena() != nullptr);

        cur_arena_ = cur_arena_->GetPrevArena();
        ASSERT(cur_arena_->InArena(mem));
        cur_arena_->Free(mem);
        if (UNLIKELY((empty_arenas_count_ + 1) > FRAME_ALLOC_MAX_FREE_ARENAS_THRESHOLD)) {
            FreeLastArena();
        } else {
            empty_arenas_count_++;
        }
    }
    LOG_FRAME_ALLOCATOR(INFO) << "Free memory at addr " << std::hex << mem;
}

template <Alignment AlignmenT, bool UseMemsetT>
inline void *FrameAllocator<AlignmenT, UseMemsetT>::TryToAllocate(size_t size)
{
    // Try to allocate memory in the current arena:
    ASSERT(cur_arena_ != nullptr);
    void *mem = cur_arena_->Alloc(size);
    if (LIKELY(mem != nullptr)) {
        return mem;
    }
    // We don't have enough memory in current arena, try to allocate in the next one:
    FramesArena *next_arena = cur_arena_->GetNextArena();
    if (next_arena == nullptr) {
        LOG_FRAME_ALLOCATOR(DEBUG) << "TryToPush failed - we don't have a free arena";
        return nullptr;
    }
    mem = next_arena->Alloc(size);
    if (LIKELY(mem != nullptr)) {
        ASSERT(empty_arenas_count_ > 0);
        empty_arenas_count_--;
        cur_arena_ = next_arena;
        return mem;
    }
    LOG_FRAME_ALLOCATOR(DEBUG) << "Couldn't allocate " << size << " bytes of memory in the totally free arena."
                               << " Change initial sizes of arenas";
    return nullptr;
}

template <Alignment AlignmenT, bool UseMemsetT>
inline size_t FrameAllocator<AlignmenT, UseMemsetT>::GetNextArenaSize(size_t size)
{
    if (arena_size_need_to_grow_) {
        biggest_arena_size_ += ARENA_SIZE_GREW_LEVEL;
        if (biggest_arena_size_ < size) {
            biggest_arena_size_ = RoundUp(size, ARENA_SIZE_GREW_LEVEL);
        }
    } else {
        arena_size_need_to_grow_ = true;
    }
    return biggest_arena_size_;
}

template <Alignment AlignmenT, bool UseMemsetT>
inline void FrameAllocator<AlignmenT, UseMemsetT>::FreeLastArena()
{
    ASSERT(last_alloc_arena_ != nullptr);
    FramesArena *arena_to_free = last_alloc_arena_;
    last_alloc_arena_ = arena_to_free->GetPrevArena();
    if (arena_to_free == cur_arena_) {
        cur_arena_ = last_alloc_arena_;
    }
    if (last_alloc_arena_ == nullptr) {
        LOG_FRAME_ALLOCATOR(DEBUG) << "Clear the last arena in the list";
    } else {
        last_alloc_arena_->ClearNextLink();
    }
    LOG_FRAME_ALLOCATOR(DEBUG) << "Free the arena at addr " << std::hex << arena_to_free;
    mem_pool_alloc_->FreeArena<FramesArena>(arena_to_free);
    arena_size_need_to_grow_ = false;
}

template <Alignment AlignmenT, bool UseMemsetT>
inline bool FrameAllocator<AlignmenT, UseMemsetT>::Contains(void *mem)
{
    auto cur_arena = cur_arena_;
    while (cur_arena != nullptr) {
        LOG_FRAME_ALLOCATOR(DEBUG) << "check InAllocator arena at addr " << std::hex << cur_arena;
        if (cur_arena->InArena(mem)) {
            return true;
        }
        cur_arena = cur_arena->GetPrevArena();
    }
    return false;
}

#undef LOG_FRAME_ALLOCATOR

}  // namespace panda::mem
#endif  // PANDA_RUNTIME_MEM_FRAME_ALLOCATOR_INL_H_
