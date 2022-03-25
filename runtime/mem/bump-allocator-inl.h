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

#ifndef PANDA_RUNTIME_MEM_BUMP_ALLOCATOR_INL_H_
#define PANDA_RUNTIME_MEM_BUMP_ALLOCATOR_INL_H_

#include "libpandabase/utils/logger.h"
#include "runtime/include/mem/allocator.h"
#include "runtime/mem/bump-allocator.h"
#include "runtime/mem/object_helpers.h"
#include "runtime/mem/alloc_config.h"

namespace panda::mem {

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_BUMP_ALLOCATOR(level) LOG(level, ALLOC) << "BumpPointerAllocator: "

template <typename AllocConfigT, typename LockConfigT, bool UseTlabs>
BumpPointerAllocator<AllocConfigT, LockConfigT, UseTlabs>::BumpPointerAllocator(Pool pool, SpaceType type_allocation,
                                                                                MemStatsType *mem_stats,
                                                                                size_t tlabs_max_count)
    : arena_(pool.GetSize(), pool.GetMem()),
      tlab_manager_(tlabs_max_count),
      type_allocation_(type_allocation),
      mem_stats_(mem_stats)
{
    LOG_BUMP_ALLOCATOR(DEBUG) << "Initializing of BumpPointerAllocator";
    AllocConfigT::InitializeCrossingMapForMemory(pool.GetMem(), arena_.GetSize());
    LOG_BUMP_ALLOCATOR(INFO) << "Initializing of BumpPointerAllocator finished";
    ASSERT(UseTlabs ? tlabs_max_count > 0 : tlabs_max_count == 0);
}

template <typename AllocConfigT, typename LockConfigT, bool UseTlabs>
BumpPointerAllocator<AllocConfigT, LockConfigT, UseTlabs>::~BumpPointerAllocator()
{
    LOG_BUMP_ALLOCATOR(DEBUG) << "Destroying of BumpPointerAllocator";
    LOG_BUMP_ALLOCATOR(INFO) << "Destroying of BumpPointerAllocator finished";
}

template <typename AllocConfigT, typename LockConfigT, bool UseTlabs>
void BumpPointerAllocator<AllocConfigT, LockConfigT, UseTlabs>::Reset()
{
    // Remove CrossingMap and create to avoid check in Alloc method
    if (LIKELY(arena_.GetOccupiedSize() > 0)) {
        AllocConfigT::RemoveCrossingMapForMemory(arena_.GetMem(), arena_.GetSize());
        AllocConfigT::InitializeCrossingMapForMemory(arena_.GetMem(), arena_.GetSize());
    }
    arena_.Reset();
    if constexpr (UseTlabs) {
        tlab_manager_.Reset();
    }
}

template <typename AllocConfigT, typename LockConfigT, bool UseTlabs>
void BumpPointerAllocator<AllocConfigT, LockConfigT, UseTlabs>::ExpandMemory(void *mem, size_t size)
{
    LOG_BUMP_ALLOCATOR(DEBUG) << "Expand memory: Add " << std::dec << size << " bytes of memory at addr " << std::hex
                              << mem;
    ASSERT(ToUintPtr(arena_.GetArenaEnd()) == ToUintPtr(mem));
    if constexpr (UseTlabs) {
        UNREACHABLE();
    }
    arena_.ExpandArena(mem, size);
    AllocConfigT::InitializeCrossingMapForMemory(mem, size);
}

template <typename AllocConfigT, typename LockConfigT, bool UseTlabs>
void *BumpPointerAllocator<AllocConfigT, LockConfigT, UseTlabs>::Alloc(size_t size, Alignment alignment)
{
    os::memory::LockHolder lock(allocator_lock_);
    LOG_BUMP_ALLOCATOR(DEBUG) << "Try to allocate " << std::dec << size << " bytes of memory";
    ASSERT(alignment == DEFAULT_ALIGNMENT);
    // We need to align up it here to write correct used memory size inside MemStats.
    // (each element allocated via BumpPointer allocator has DEFAULT_ALIGNMENT alignment).
    size = AlignUp(size, DEFAULT_ALIGNMENT_IN_BYTES);
    void *mem = nullptr;
    // NOLINTNEXTLINE(readability-braces-around-statements)
    if constexpr (!UseTlabs) {
        // Use common scenario
        mem = arena_.Alloc(size, alignment);
        // NOLINTNEXTLINE(readability-misleading-indentation)
    } else {
        // We must take TLABs occupied memory into account.
        ASSERT(arena_.GetFreeSize() >= tlab_manager_.GetTLABsOccupiedSize());
        if (arena_.GetFreeSize() - tlab_manager_.GetTLABsOccupiedSize() >= size) {
            mem = arena_.Alloc(size, alignment);
        }
    }
    if (mem == nullptr) {
        LOG_BUMP_ALLOCATOR(DEBUG) << "Couldn't allocate memory";
        return nullptr;
    }
    AllocConfigT::OnAlloc(size, type_allocation_, mem_stats_);
    AllocConfigT::AddToCrossingMap(mem, size);
    AllocConfigT::MemoryInit(mem, size);
    return mem;
}

template <typename AllocConfigT, typename LockConfigT, bool UseTlabs>
TLAB *BumpPointerAllocator<AllocConfigT, LockConfigT, UseTlabs>::CreateNewTLAB(size_t size)
{
    os::memory::LockHolder lock(allocator_lock_);
    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if constexpr (!UseTlabs) {
        UNREACHABLE();
    }
    LOG_BUMP_ALLOCATOR(DEBUG) << "Try to create a TLAB with size " << std::dec << size;
    ASSERT(size == AlignUp(size, DEFAULT_ALIGNMENT_IN_BYTES));
    TLAB *tlab = nullptr;
    ASSERT(arena_.GetFreeSize() >= tlab_manager_.GetTLABsOccupiedSize());
    if (arena_.GetFreeSize() - tlab_manager_.GetTLABsOccupiedSize() >= size) {
        tlab = tlab_manager_.GetUnusedTLABInstance();
        if (tlab != nullptr) {
            tlab_manager_.IncreaseTLABsOccupiedSize(size);
            uintptr_t end_of_arena = ToUintPtr(arena_.GetArenaEnd());
            ASSERT(end_of_arena >= tlab_manager_.GetTLABsOccupiedSize());
            void *tlab_buffer_start = ToVoidPtr(end_of_arena - tlab_manager_.GetTLABsOccupiedSize());
            ASAN_UNPOISON_MEMORY_REGION(tlab_buffer_start, size);
            AllocConfigT::MemoryInit(tlab_buffer_start, size);
            tlab->Fill(tlab_buffer_start, size);
            LOG_BUMP_ALLOCATOR(INFO) << "Created new TLAB with size " << std::dec << size << " at addr " << std::hex
                                     << tlab_buffer_start;
        } else {
            LOG_BUMP_ALLOCATOR(DEBUG) << "Reached the limit of TLABs inside the allocator";
        }
    } else {
        LOG_BUMP_ALLOCATOR(DEBUG) << "Don't have enough memory for new TLAB with size " << std::dec << size;
    }
    return tlab;
}

template <typename AllocConfigT, typename LockConfigT, bool UseTlabs>
void BumpPointerAllocator<AllocConfigT, LockConfigT, UseTlabs>::VisitAndRemoveAllPools(const MemVisitor &mem_visitor)
{
    os::memory::LockHolder lock(allocator_lock_);
    AllocConfigT::RemoveCrossingMapForMemory(arena_.GetMem(), arena_.GetSize());
    mem_visitor(arena_.GetMem(), arena_.GetSize());
}

template <typename AllocConfigT, typename LockConfigT, bool UseTlabs>
void BumpPointerAllocator<AllocConfigT, LockConfigT, UseTlabs>::VisitAndRemoveFreePools([
    [maybe_unused]] const MemVisitor &mem_visitor)
{
    os::memory::LockHolder lock(allocator_lock_);
    // We should do nothing here
}

template <typename AllocConfigT, typename LockConfigT, bool UseTlabs>
void BumpPointerAllocator<AllocConfigT, LockConfigT, UseTlabs>::IterateOverObjects(
    const std::function<void(ObjectHeader *object_header)> &object_visitor)
{
    os::memory::LockHolder lock(allocator_lock_);
    LOG_BUMP_ALLOCATOR(DEBUG) << "Iteration over objects started";
    void *cur_ptr = arena_.GetAllocatedStart();
    void *end_ptr = arena_.GetAllocatedEnd();
    while (cur_ptr < end_ptr) {
        auto object_header = static_cast<ObjectHeader *>(cur_ptr);
        size_t object_size = GetObjectSize(cur_ptr);
        object_visitor(object_header);
        cur_ptr = ToVoidPtr(AlignUp(ToUintPtr(cur_ptr) + object_size, DEFAULT_ALIGNMENT_IN_BYTES));
    }
    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if constexpr (UseTlabs) {
        LOG_BUMP_ALLOCATOR(DEBUG) << "Iterate over TLABs";
        // Iterate over objects in TLABs:
        tlab_manager_.IterateOverTLABs([&](TLAB *tlab) {
            tlab->IterateOverObjects(object_visitor);
            return true;
        });
        LOG_BUMP_ALLOCATOR(DEBUG) << "Iterate over TLABs finished";
    }
    LOG_BUMP_ALLOCATOR(DEBUG) << "Iteration over objects finished";
}

template <typename AllocConfigT, typename LockConfigT, bool UseTlabs>
template <typename MemVisitor>
void BumpPointerAllocator<AllocConfigT, LockConfigT, UseTlabs>::IterateOverObjectsInRange(const MemVisitor &mem_visitor,
                                                                                          void *left_border,
                                                                                          void *right_border)
{
    ASSERT(ToUintPtr(right_border) >= ToUintPtr(left_border));
    // if the range crosses different allocators memory pools
    ASSERT(ToUintPtr(right_border) - ToUintPtr(left_border) ==
           (CrossingMapSingleton::GetCrossingMapGranularity() - 1U));
    ASSERT((ToUintPtr(right_border) & (~(CrossingMapSingleton::GetCrossingMapGranularity() - 1U))) ==
           (ToUintPtr(left_border) & (~(CrossingMapSingleton::GetCrossingMapGranularity() - 1U))));

    os::memory::LockHolder lock(allocator_lock_);
    LOG_BUMP_ALLOCATOR(DEBUG) << "IterateOverObjectsInRange for range [" << std::hex << left_border << ", "
                              << right_border << "]";
    MemRange input_mem_range(ToUintPtr(left_border), ToUintPtr(right_border));
    if (arena_.GetOccupiedSize() > 0) {
        MemRange arena_occupied_mem_range(ToUintPtr(arena_.GetAllocatedStart()),
                                          ToUintPtr(arena_.GetAllocatedEnd()) - 1);
        // In this case, we iterate over objects in intersection of memory range of occupied memory via arena_.Alloc()
        // and memory range of input range
        if (arena_occupied_mem_range.IsIntersect(input_mem_range)) {
            void *start_ptr =
                ToVoidPtr(std::max(input_mem_range.GetStartAddress(), arena_occupied_mem_range.GetStartAddress()));
            void *end_ptr =
                ToVoidPtr(std::min(input_mem_range.GetEndAddress(), arena_occupied_mem_range.GetEndAddress()));

            void *obj_addr = AllocConfigT::FindFirstObjInCrossingMap(start_ptr, end_ptr);
            if (obj_addr != nullptr) {
                ASSERT(arena_occupied_mem_range.GetStartAddress() <= ToUintPtr(obj_addr) &&
                       ToUintPtr(obj_addr) <= arena_occupied_mem_range.GetEndAddress());
                void *current_ptr = obj_addr;
                while (current_ptr < end_ptr) {
                    auto *object_header = static_cast<ObjectHeader *>(current_ptr);
                    size_t object_size = GetObjectSize(current_ptr);
                    mem_visitor(object_header);
                    current_ptr = ToVoidPtr(AlignUp(ToUintPtr(current_ptr) + object_size, DEFAULT_ALIGNMENT_IN_BYTES));
                }
            }
        }
    }
    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if constexpr (UseTlabs) {
        // If we didn't allocate any TLAB then we don't need iterate by TLABs
        if (tlab_manager_.GetTLABsOccupiedSize() == 0) {
            return;
        }
        uintptr_t end_of_arena = ToUintPtr(arena_.GetArenaEnd());
        uintptr_t start_tlab = end_of_arena - tlab_manager_.GetTLABsOccupiedSize();
        MemRange tlabs_mem_range(start_tlab, end_of_arena - 1);
        // In this case, we iterate over objects in intersection of memory range of TLABs
        // and memory range of input range
        if (tlabs_mem_range.IsIntersect(input_mem_range)) {
            void *start_ptr = ToVoidPtr(std::max(input_mem_range.GetStartAddress(), tlabs_mem_range.GetStartAddress()));
            void *end_ptr = ToVoidPtr(std::min(input_mem_range.GetEndAddress(), tlabs_mem_range.GetEndAddress()));
            tlab_manager_.IterateOverTLABs(
                [&mem_visitor, mem_range = MemRange(ToUintPtr(start_ptr), ToUintPtr(end_ptr))](TLAB *tlab) -> bool {
                    tlab->IterateOverObjectsInRange(mem_visitor, mem_range);
                    return true;
                });
        }
    }
}

template <typename AllocConfigT, typename LockConfigT, bool UseTlabs>
MemRange BumpPointerAllocator<AllocConfigT, LockConfigT, UseTlabs>::GetMemRange()
{
    return MemRange(ToUintPtr(arena_.GetAllocatedStart()), ToUintPtr(arena_.GetArenaEnd()) - 1);
}

template <typename AllocConfigT, typename LockConfigT, bool UseTlabs>
template <typename ObjectMoveVisitorT>
void BumpPointerAllocator<AllocConfigT, LockConfigT, UseTlabs>::CollectAndMove(
    const GCObjectVisitor &death_checker, const ObjectMoveVisitorT &object_move_visitor)
{
    IterateOverObjects([&](ObjectHeader *object_header) {
        // We are interested only in moving alive objects, after that we cleanup arena
        if (death_checker(object_header) == ObjectStatus::ALIVE_OBJECT) {
            object_move_visitor(object_header);
        }
    });
}

template <typename AllocConfigT, typename LockConfigT, bool UseTlabs>
bool BumpPointerAllocator<AllocConfigT, LockConfigT, UseTlabs>::ContainObject(const ObjectHeader *obj)
{
    bool result = false;
    result = arena_.InArena(const_cast<ObjectHeader *>(obj));
    if ((UseTlabs) && (!result)) {
        // Check TLABs
        tlab_manager_.IterateOverTLABs([&](TLAB *tlab) {
            result = tlab->ContainObject(obj);
            return !result;
        });
    }
    return result;
}

template <typename AllocConfigT, typename LockConfigT, bool UseTlabs>
bool BumpPointerAllocator<AllocConfigT, LockConfigT, UseTlabs>::IsLive(const ObjectHeader *obj)
{
    ASSERT(ContainObject(obj));
    void *obj_mem = static_cast<void *>(const_cast<ObjectHeader *>(obj));
    if (arena_.InArena(obj_mem)) {
        void *current_obj = AllocConfigT::FindFirstObjInCrossingMap(obj_mem, obj_mem);
        if (UNLIKELY(current_obj == nullptr)) {
            return false;
        }
        while (current_obj < obj_mem) {
            size_t object_size = GetObjectSize(current_obj);
            current_obj = ToVoidPtr(AlignUp(ToUintPtr(current_obj) + object_size, DEFAULT_ALIGNMENT_IN_BYTES));
        }
        return current_obj == obj_mem;
    }
    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if constexpr (UseTlabs) {
        bool result = false;
        tlab_manager_.IterateOverTLABs([&](TLAB *tlab) {
            if (tlab->ContainObject(obj)) {
                tlab->IterateOverObjects([&](ObjectHeader *object_header) {
                    if (object_header == obj) {
                        result = true;
                    }
                });
                return false;
            }
            return true;
        });
        return result;
    }
    return false;
}

#undef LOG_BUMP_ALLOCATOR

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_BUMP_ALLOCATOR_INL_H_
