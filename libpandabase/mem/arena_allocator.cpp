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

#include "arena_allocator.h"

#include "arena.h"
#include "utils/logger.h"
#include "pool_manager.h"
#include "trace/trace.h"
#include "mem/base_mem_stats.h"
#include <cstdlib>
#include <cstring>

namespace panda {

template <bool use_oom_handler>
ArenaAllocatorT<use_oom_handler>::ArenaAllocatorT(SpaceType space_type, BaseMemStats *mem_stats,
                                                  bool limit_alloc_size_by_pool)
    : memStats_(mem_stats), space_type_(space_type), limit_alloc_size_by_pool_(limit_alloc_size_by_pool)
{
    ASSERT(!use_oom_handler);
    if (!ON_STACK_ALLOCATION_ENABLED) {
        arenas_ = PoolManager::AllocArena(DEFAULT_ARENA_SIZE, space_type_, AllocatorType::ARENA_ALLOCATOR, this);
        ASSERT(arenas_ != nullptr);
        AllocArenaMemStats(DEFAULT_ARENA_SIZE);
    }
}

template <bool use_oom_handler>
ArenaAllocatorT<use_oom_handler>::ArenaAllocatorT(OOMHandler oom_handler, SpaceType space_type, BaseMemStats *mem_stats,
                                                  bool limit_alloc_size_by_pool)
    : memStats_(mem_stats),
      space_type_(space_type),
      oom_handler_(oom_handler),
      limit_alloc_size_by_pool_(limit_alloc_size_by_pool)
{
    ASSERT(use_oom_handler);
    if (!ON_STACK_ALLOCATION_ENABLED) {
        arenas_ = PoolManager::AllocArena(DEFAULT_ARENA_SIZE, space_type_, AllocatorType::ARENA_ALLOCATOR, this);
        ASSERT(arenas_ != nullptr);
        AllocArenaMemStats(DEFAULT_ARENA_SIZE);
    }
}

template <bool use_oom_handler>
ArenaAllocatorT<use_oom_handler>::~ArenaAllocatorT()
{
    Arena *cur = arenas_;
    while (cur != nullptr) {
        Arena *tmp;
        tmp = cur->GetNextArena();
        PoolManager::FreeArena(cur);
        cur = tmp;
    }
}

template <bool use_oom_handler>
inline void *ArenaAllocatorT<use_oom_handler>::AllocateAndAddNewPool(size_t size, Alignment alignment)
{
    void *mem = arenas_->Alloc(size, alignment);
    if (mem == nullptr) {
        bool add_new_pool = false;
        if (limit_alloc_size_by_pool_) {
            add_new_pool = AddArenaFromPool(std::max(AlignUp(size, alignment) + sizeof(Arena), DEFAULT_ARENA_SIZE));
        } else {
            add_new_pool = AddArenaFromPool(DEFAULT_ARENA_SIZE);
        }
        if (UNLIKELY(!add_new_pool)) {
            LOG(DEBUG, ALLOC) << "Can not add new pool for " << SpaceTypeToString(space_type_);
            return nullptr;
        }
        mem = arenas_->Alloc(size, alignment);
        ASSERT(!limit_alloc_size_by_pool_ || mem != nullptr);
    }
    return mem;
}

template <bool use_oom_handler>
void *ArenaAllocatorT<use_oom_handler>::Alloc(size_t size, Alignment align)
{
    trace::ScopedTrace scoped_trace("ArenaAllocator allocate");
    LOG(DEBUG, ALLOC) << "ArenaAllocator: try to alloc " << size << " with align " << align;
    void *ret = nullptr;
    if (ON_STACK_ALLOCATION_ENABLED && UNLIKELY(!arenas_)) {
        LOG(DEBUG, ALLOC) << "\tTry to allocate from stack";
        ret = buff_.Alloc(size, align);
        LOG_IF(ret, INFO, ALLOC) << "\tallocate from stack buffer";
        if (ret == nullptr) {
            ret = AllocateAndAddNewPool(size, align);
        }
    } else {
        ret = AllocateAndAddNewPool(size, align);
    }
    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if constexpr (use_oom_handler) {
        if (ret == nullptr) {
            oom_handler_();
        }
    }
    LOG(INFO, ALLOC) << "ArenaAllocator: allocated " << size << " bytes aligned by " << align;
    AllocArenaMemStats(size);
    return ret;
}

template <bool use_oom_handler>
void ArenaAllocatorT<use_oom_handler>::Resize(size_t new_size)
{
    LOG(DEBUG, ALLOC) << "ArenaAllocator: resize to new size " << new_size;
    size_t cur_size = GetAllocatedSize();
    if (cur_size <= new_size) {
        LOG_IF(cur_size < new_size, FATAL, ALLOC) << "ArenaAllocator: resize to bigger size than we have. Do nothing";
        return;
    }

    size_t bytes_to_delete = cur_size - new_size;
    // Try to delete unused arenas
    while ((arenas_ != nullptr) && (bytes_to_delete != 0)) {
        Arena *next = arenas_->GetNextArena();
        size_t cur_arena_size = arenas_->GetOccupiedSize();
        if (cur_arena_size < bytes_to_delete) {
            // We need to free the whole arena
            PoolManager::FreeArena(arenas_);
            arenas_ = next;
            bytes_to_delete -= cur_arena_size;
        } else {
            arenas_->Resize(cur_arena_size - bytes_to_delete);
            bytes_to_delete = 0;
        }
    }
    if ((ON_STACK_ALLOCATION_ENABLED) && (bytes_to_delete > 0)) {
        size_t stack_size = buff_.GetOccupiedSize();
        ASSERT(stack_size >= bytes_to_delete);
        buff_.Resize(stack_size - bytes_to_delete);
        bytes_to_delete = 0;
    }
    ASSERT(bytes_to_delete == 0);
}

template <bool use_oom_handler>
bool ArenaAllocatorT<use_oom_handler>::AddArenaFromPool(size_t pool_size)
{
    ASSERT(pool_size != 0);
    pool_size = AlignUp(pool_size, PANDA_POOL_ALIGNMENT_IN_BYTES);
    Arena *new_arena = PoolManager::AllocArena(pool_size, space_type_, GetAllocatorType(), this);
    if (UNLIKELY(new_arena == nullptr)) {
        return false;
    }
    new_arena->LinkTo(arenas_);
    arenas_ = new_arena;
    return true;
}

template <bool use_oom_handler>
size_t ArenaAllocatorT<use_oom_handler>::GetAllocatedSize() const
{
    size_t size = 0;
    if (ON_STACK_ALLOCATION_ENABLED) {
        size += buff_.GetOccupiedSize();
    }
    for (Arena *cur = arenas_; cur != nullptr; cur = cur->GetNextArena()) {
        size += cur->GetOccupiedSize();
    }
    return size;
}

template class ArenaAllocatorT<true>;
template class ArenaAllocatorT<false>;

}  // namespace panda
