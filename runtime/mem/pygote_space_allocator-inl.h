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

#ifndef PANDA_RUNTIME_MEM_PYGOTE_SPACE_ALLOCATOR_INL_H_
#define PANDA_RUNTIME_MEM_PYGOTE_SPACE_ALLOCATOR_INL_H_

#include "libpandabase/utils/logger.h"
#include "runtime/include/mem/allocator.h"
#include "runtime/mem/object_helpers.h"
#include "runtime/mem/pygote_space_allocator.h"
#include "runtime/mem/runslots_allocator-inl.h"
#include "runtime/include/runtime.h"

namespace panda::mem {

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_PYGOTE_SPACE_ALLOCATOR(level) LOG(level, ALLOC) << "PygoteSpaceAllocator: "

template <typename AllocConfigT>
PygoteSpaceAllocator<AllocConfigT>::PygoteSpaceAllocator(MemStatsType *mem_stats)
    : runslots_alloc_(mem_stats), mem_stats_(mem_stats)
{
    LOG_PYGOTE_SPACE_ALLOCATOR(INFO) << "Initializing of PygoteSpaceAllocator";
}

template <typename AllocConfigT>
PygoteSpaceAllocator<AllocConfigT>::~PygoteSpaceAllocator()
{
    auto cur = arena_;
    while (cur != nullptr) {
        auto tmp = cur->GetNextArena();
        PoolManager::FreeArena(cur);
        cur = tmp;
    }
    auto allocator = Runtime::GetCurrent()->GetInternalAllocator();
    for (const auto &bitmap : live_bitmaps_) {
        allocator->Delete(bitmap->GetBitMap().data());
        allocator->Delete(bitmap);
    }
    LOG_PYGOTE_SPACE_ALLOCATOR(INFO) << "Destroying of PygoteSpaceAllocator";
}

template <typename AllocConfigT>
inline void PygoteSpaceAllocator<AllocConfigT>::SetState(PygoteSpaceState new_state)
{
    // must move to next state
    ASSERT(new_state > state_);
    state_ = new_state;

    if (state_ == STATE_PYGOTE_FORKED) {
        // build bitmaps for used pools
        runslots_alloc_.memory_pool_.VisitAllPoolsWithOccupiedSize(
            [this](void *mem, size_t used_size, size_t /* size */) { CreateLiveBitmap(mem, used_size); });
        runslots_alloc_.IterateOverObjects([this](ObjectHeader *object) {
            if (!live_bitmaps_.empty()) {
                for (auto bitmap : live_bitmaps_) {
                    if (bitmap->IsAddrInRange(object)) {
                        bitmap->Set(object);
                        return;
                    }
                }
            }
        });

        // trim unused pages in runslots allocator
        runslots_alloc_.TrimUnsafe();

        // only trim the last arena
        if (arena_ != nullptr && arena_->GetFreeSize() >= panda::os::mem::GetPageSize()) {
            uintptr_t start = AlignUp(ToUintPtr(arena_->GetAllocatedEnd()), panda::os::mem::GetPageSize());
            uintptr_t end = ToUintPtr(arena_->GetArenaEnd());
            os::mem::ReleasePages(start, end);
        }
    }
}

template <typename AllocConfigT>
inline void *PygoteSpaceAllocator<AllocConfigT>::Alloc(size_t size, Alignment align)
{
    ASSERT(state_ == STATE_PYGOTE_INIT || state_ == STATE_PYGOTE_FORKING);

    // alloc from runslots firstly, if failed, try to alloc from new arena
    // or mark card table with object header, also it will reduce the bitmap count which will reduce the gc mark time.
    void *obj = runslots_alloc_.template Alloc<false>(size, align);
    if (obj != nullptr) {
        return obj;
    }

    if (state_ == STATE_PYGOTE_INIT) {
        // try again in lock
        static os::memory::Mutex pool_lock;
        os::memory::LockHolder lock(pool_lock);
        obj = runslots_alloc_.Alloc(size, align);
        if (obj != nullptr) {
            return obj;
        }
        auto pool = PoolManager::GetMmapMemPool()->AllocPool(RunSlotsAllocator<AllocConfigT>::GetMinPoolSize(),
                                                             space_type_, AllocatorType::RUNSLOTS_ALLOCATOR, this);
        if (UNLIKELY(pool.GetMem() == nullptr)) {
            return nullptr;
        }
        if (!runslots_alloc_.AddMemoryPool(pool.GetMem(), pool.GetSize())) {
            LOG(FATAL, ALLOC) << "PygoteSpaceAllocator: couldn't add memory pool to object allocator";
        }
        // alloc object again
        obj = runslots_alloc_.Alloc(size, align);
    } else {
        if (arena_ != nullptr) {
            obj = arena_->Alloc(size, align);
        }
        if (obj == nullptr) {
            auto new_arena =
                PoolManager::AllocArena(DEFAULT_ARENA_SIZE, space_type_, AllocatorType::ARENA_ALLOCATOR, this);
            if (new_arena == nullptr) {
                return nullptr;
            }
            CreateLiveBitmap(new_arena, DEFAULT_ARENA_SIZE);
            new_arena->LinkTo(arena_);
            arena_ = new_arena;
            obj = arena_->Alloc(size, align);
        }
        live_bitmaps_.back()->Set(obj);  // mark live in bitmap
        AllocConfigT::OnAlloc(size, space_type_, mem_stats_);
        AllocConfigT::MemoryInit(obj, size);
    }

    return obj;
}

template <typename AllocConfigT>
inline void PygoteSpaceAllocator<AllocConfigT>::Free(void *mem)
{
    if (!live_bitmaps_.empty()) {
        for (auto bitmap : live_bitmaps_) {
            if (bitmap->IsAddrInRange(mem)) {
                bitmap->Clear(mem);
                return;
            }
        }
    }

    if (state_ == STATE_PYGOTE_FORKED) {
        return;
    }

    if (runslots_alloc_.ContainObject(reinterpret_cast<ObjectHeader *>(mem))) {
        runslots_alloc_.Free(mem);
    }
}

template <typename AllocConfigT>
inline bool PygoteSpaceAllocator<AllocConfigT>::ContainObject(const ObjectHeader *object)
{
    // see if in runslots firstly
    if (runslots_alloc_.ContainObject(object)) {
        return true;
    }

    // see if in arena list
    auto cur = arena_;
    while (cur != nullptr) {
        if (cur->InArena(const_cast<ObjectHeader *>(object))) {
            return true;
        }
        cur = cur->GetNextArena();
    }
    return false;
}

template <typename AllocConfigT>
inline bool PygoteSpaceAllocator<AllocConfigT>::IsLive(const ObjectHeader *object)
{
    if (!live_bitmaps_.empty()) {
        for (auto bitmap : live_bitmaps_) {
            if (bitmap->IsAddrInRange(object)) {
                return bitmap->Test(object);
            }
        }
    }

    if (state_ == STATE_PYGOTE_FORKED) {
        return false;
    }

    return runslots_alloc_.ContainObject(object) && runslots_alloc_.IsLive(object);
}

template <typename AllocConfigT>
inline void PygoteSpaceAllocator<AllocConfigT>::CreateLiveBitmap(void *heap_begin, size_t heap_size)
{
    auto allocator = Runtime::GetCurrent()->GetInternalAllocator();
    auto bitmap_data = allocator->Alloc(MarkBitmap::GetBitMapSizeInByte(heap_size));
    ASSERT(bitmap_data != nullptr);
    auto bitmap = allocator->Alloc(sizeof(MarkBitmap));
    ASSERT(bitmap != nullptr);
    auto bitmap_obj = new (bitmap) MarkBitmap(heap_begin, heap_size, bitmap_data);
    bitmap_obj->ClearAllBits();
    live_bitmaps_.emplace_back(bitmap_obj);
}

template <typename AllocConfigT>
inline void PygoteSpaceAllocator<AllocConfigT>::ClearLiveBitmaps()
{
    for (auto bitmap : live_bitmaps_) {
        bitmap->ClearAllBits();
    }
}

template <typename AllocConfigT>
template <typename Visitor>
inline void PygoteSpaceAllocator<AllocConfigT>::IterateOverObjectsInRange(const Visitor &visitor, void *start,
                                                                          void *end)
{
    if (!live_bitmaps_.empty()) {
        for (auto bitmap : live_bitmaps_) {
            auto [left, right] = bitmap->GetHeapRange();
            left = std::max(ToUintPtr(start), left);
            right = std::min(ToUintPtr(end), right);
            if (left < right) {
                bitmap->IterateOverMarkedChunkInRange(ToVoidPtr(left), ToVoidPtr(right), [&visitor](void *mem) {
                    visitor(reinterpret_cast<ObjectHeader *>(mem));
                });
            }
        }
    } else {
        ASSERT(arena_ == nullptr);
        runslots_alloc_.IterateOverObjectsInRange(visitor, start, end);
    }
}

template <typename AllocConfigT>
inline void PygoteSpaceAllocator<AllocConfigT>::IterateOverObjects(const ObjectVisitor &object_visitor)
{
    if (!live_bitmaps_.empty()) {
        for (auto bitmap : live_bitmaps_) {
            bitmap->IterateOverMarkedChunks([&object_visitor](void *mem) {
                object_visitor(static_cast<ObjectHeader *>(static_cast<void *>(mem)));
            });
        }
        if (state_ != STATE_PYGOTE_FORKED) {
            runslots_alloc_.IterateOverObjects(object_visitor);
        }
    } else {
        ASSERT(arena_ == nullptr);
        runslots_alloc_.IterateOverObjects(object_visitor);
    }
}

template <typename AllocConfigT>
inline void PygoteSpaceAllocator<AllocConfigT>::VisitAndRemoveAllPools(const MemVisitor &mem_visitor)
{
    // IterateOverPools only used when allocator should be destroyed
    auto cur = arena_;
    while (cur != nullptr) {
        auto tmp = cur->GetNextArena();
        PoolManager::FreeArena(cur);
        cur = tmp;
    }
    arena_ = nullptr;  // avoid to duplicated free
    runslots_alloc_.VisitAndRemoveAllPools(mem_visitor);
}

template <typename AllocConfigT>
inline void PygoteSpaceAllocator<AllocConfigT>::VisitAndRemoveFreePools(const MemVisitor &mem_visitor)
{
    // afte pygote fork, we don't change pygote space for free unused pools
    if (state_ == STATE_PYGOTE_FORKED) {
        return;
    }

    // before pygote fork, call underlying allocator to free unused pools
    runslots_alloc_.VisitAndRemoveFreePools(mem_visitor);
}

template <typename AllocConfigT>
inline void PygoteSpaceAllocator<AllocConfigT>::Collect(const GCObjectVisitor &gc_visitor)
{
    // the live bitmaps has been updated in gc process, need to do nothing here
    if (state_ == STATE_PYGOTE_FORKED) {
        return;
    }

    // before pygote fork, call underlying allocator to collect garbage
    runslots_alloc_.Collect(gc_visitor);
}

#undef LOG_PYGOTE_SPACE_ALLOCATOR

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_PYGOTE_SPACE_ALLOCATOR_INL_H_
