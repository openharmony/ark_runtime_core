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

#ifndef PANDA_RUNTIME_MEM_RUNSLOTS_ALLOCATOR_INL_H_
#define PANDA_RUNTIME_MEM_RUNSLOTS_ALLOCATOR_INL_H_

#include <securec.h>
#include "libpandabase/utils/asan_interface.h"
#include "runtime/mem/alloc_config.h"
#include "runtime/mem/object_helpers.h"
#include "runtime/mem/runslots_allocator.h"

namespace panda::mem {

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_RUNSLOTS_ALLOCATOR(level) LOG(level, ALLOC) << "RunSlotsAllocator: "

template <typename AllocConfigT, typename LockConfigT>
inline RunSlotsAllocator<AllocConfigT, LockConfigT>::RunSlotsAllocator(MemStatsType *mem_stats,
                                                                       SpaceType type_allocation)
    : type_allocation_(type_allocation), mem_stats_(mem_stats)
{
    LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "Initializing RunSlotsAllocator";
    LOG_RUNSLOTS_ALLOCATOR(INFO) << "Initializing RunSlotsAllocator finished";
}

template <typename AllocConfigT, typename LockConfigT>
inline RunSlotsAllocator<AllocConfigT, LockConfigT>::~RunSlotsAllocator()
{
    LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "Destroying RunSlotsAllocator";
    LOG_RUNSLOTS_ALLOCATOR(INFO) << "Destroying RunSlotsAllocator finished";
}

template <typename AllocConfigT, typename LockConfigT>
template <bool disable_use_free_runslots>
inline void *RunSlotsAllocator<AllocConfigT, LockConfigT>::Alloc(size_t size, Alignment align)
{
    LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "Try to allocate " << size << " bytes of memory with align " << align;
    if (size == 0) {
        LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "Failed to allocate - size of object is null";
        return nullptr;
    }
    size_t alignment_size = GetAlignmentInBytes(align);
    if (alignment_size > size) {
        LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "Change size of allocation to " << alignment_size
                                      << " bytes because of alignment";
        size = alignment_size;
    }
    if (size > RunSlotsType::MaxSlotSize()) {
        LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "Failed to allocate - size of object is too big";
        return nullptr;
    }
    size_t slot_size_power_of_two = RunSlotsType::ConvertToPowerOfTwoUnsafe(size);
    size_t array_index = slot_size_power_of_two;
    const size_t run_slot_size = 1UL << slot_size_power_of_two;
    RunSlotsType *runslots = nullptr;
    bool used_from_freed_runslots_list = false;
    {
        os::memory::LockHolder list_lock(*runslots_[array_index].GetLock());
        runslots = runslots_[array_index].PopFromHead();
    }
    if (runslots == nullptr) {
        LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "We don't have free RunSlots for size " << run_slot_size
                                      << ". Try to get new one.";
        if (disable_use_free_runslots) {
            return nullptr;
        }
        {
            os::memory::LockHolder list_lock(*free_runslots_.GetLock());
            runslots = free_runslots_.PopFromHead();
        }
        if (runslots != nullptr) {
            used_from_freed_runslots_list = true;
            LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "Get RunSlots from free list";
        } else {
            LOG_RUNSLOTS_ALLOCATOR(DEBUG)
                << "Failed to get new RunSlots from free list, try to allocate one from memory";
            runslots = CreateNewRunSlotsFromMemory(run_slot_size);
            if (runslots == nullptr) {
                LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "Failed to allocate an object, couldn't create RunSlots";
                return nullptr;
            }
        }
    }
    void *allocated_mem = nullptr;
    {
        os::memory::LockHolder runslots_lock(*runslots->GetLock());
        if (used_from_freed_runslots_list) {
            //                  we will have a perf issue here. Maybe it is better to delete free_runslots_?
            if (runslots->GetSlotsSize() != run_slot_size) {
                runslots->Initialize(run_slot_size, runslots->GetPoolPointer(), false);
            }
        }
        LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "Used runslots with addr " << std::hex << runslots;
        allocated_mem = static_cast<void *>(runslots->PopFreeSlot());
        if (allocated_mem == nullptr) {
            UNREACHABLE();
        }
        LOG_RUNSLOTS_ALLOCATOR(INFO) << "Allocate a memory at address " << std::hex << allocated_mem;
        if (!runslots->IsFull()) {
            os::memory::LockHolder list_lock(*runslots_[array_index].GetLock());
            // We didn't take the last free slot from this RunSlots
            runslots_[array_index].PushToTail(runslots);
        }
        ASAN_UNPOISON_MEMORY_REGION(allocated_mem, size);
        AllocConfigT::OnAlloc(run_slot_size, type_allocation_, mem_stats_);
        AllocConfigT::MemoryInit(allocated_mem, size);
    }
    return allocated_mem;
}

template <typename AllocConfigT, typename LockConfigT>
inline void RunSlotsAllocator<AllocConfigT, LockConfigT>::Free(void *mem)
{
    FreeUnsafe<true>(mem);
}

template <typename AllocConfigT, typename LockConfigT>
inline void RunSlotsAllocator<AllocConfigT, LockConfigT>::ReleaseEmptyRunSlotsPagesUnsafe()
{
    // Iterate over free_runslots list:
    RunSlotsType *cur_free_runslots = nullptr;
    {
        os::memory::LockHolder list_lock(*free_runslots_.GetLock());
        cur_free_runslots = free_runslots_.PopFromHead();
    }
    while (cur_free_runslots != nullptr) {
        memory_pool_.ReturnAndReleaseRunSlotsMemory(cur_free_runslots);

        {
            os::memory::LockHolder list_lock(*free_runslots_.GetLock());
            cur_free_runslots = free_runslots_.PopFromHead();
        }
    }
}

template <typename AllocConfigT, typename LockConfigT>
inline bool RunSlotsAllocator<AllocConfigT, LockConfigT>::FreeUnsafeInternal(RunSlotsType *runslots, void *mem)
{
    bool need_to_add_to_free_list = false;
    const size_t run_slot_size = runslots->GetSlotsSize();
    size_t array_index = RunSlotsType::ConvertToPowerOfTwoUnsafe(run_slot_size);
    bool runslots_was_full = runslots->IsFull();
    runslots->PushFreeSlot(static_cast<FreeSlot *>(mem));
    /**
     * RunSlotsAllocator doesn't know this real size which we use in slot, so we record upper bound - size of the
     * slot.
     */
    AllocConfigT::OnFree(run_slot_size, type_allocation_, mem_stats_);
    ASAN_POISON_MEMORY_REGION(mem, run_slot_size);
    ASSERT(!(runslots_was_full && runslots->IsEmpty()));  // Runslots has more that one slot inside.
    if (runslots_was_full) {
        LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "This RunSlots was full and now we must add it to the RunSlots list";

        os::memory::LockHolder list_lock(*runslots_[array_index].GetLock());
#if !defined(FAST_VERIFY)  // this assert is very expensive, takes too much time for some tests in FastVerify mode
        ASSERT(!runslots_[array_index].IsInThisList(runslots));
#endif
        runslots_[array_index].PushToTail(runslots);
    } else if (runslots->IsEmpty()) {
        os::memory::LockHolder list_lock(*runslots_[array_index].GetLock());
        // Check that we may took this runslots from list on alloc
        // and waiting for lock
        if ((runslots->GetNextRunSlots() != nullptr) || (runslots->GetPrevRunSlots() != nullptr) ||
            (runslots_[array_index].GetHead() == runslots)) {
            LOG_RUNSLOTS_ALLOCATOR(DEBUG)
                << "This RunSlots is empty. Pop it from runslots list and push it to free_runslots_";
            runslots_[array_index].PopFromList(runslots);
            need_to_add_to_free_list = true;
        }
    }

    return need_to_add_to_free_list;
}

template <typename AllocConfigT, typename LockConfigT>
template <bool LockRunSlots>
inline void RunSlotsAllocator<AllocConfigT, LockConfigT>::FreeUnsafe(void *mem)
{
    if (UNLIKELY(mem == nullptr)) {
        LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "Try to free memory at invalid addr 0";
        return;
    }
    LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "Try to free object at address " << std::hex << mem;
#ifndef NDEBUG
    if (!AllocatedByRunSlotsAllocatorUnsafe(mem)) {
        LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "This object was not allocated by this allocator";
        return;
    }
#endif  // !NDEBUG

    // Now we 100% sure that this object was allocated by RunSlots allocator.
    // We can just do alignment for this address and get a pointer to RunSlots header
    uintptr_t runslots_addr = (ToUintPtr(mem) >> RUNSLOTS_ALIGNMENT) << RUNSLOTS_ALIGNMENT;
    auto runslots = static_cast<RunSlotsType *>(ToVoidPtr(runslots_addr));
    LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "It is RunSlots with addr " << std::hex << static_cast<void *>(runslots);

    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if constexpr (LockRunSlots) {
        runslots->GetLock()->Lock();
    }

    bool need_to_add_to_free_list = FreeUnsafeInternal(runslots, mem);

    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if constexpr (LockRunSlots) {
        runslots->GetLock()->Unlock();
    }

    if (need_to_add_to_free_list) {
        os::memory::LockHolder list_lock(*free_runslots_.GetLock());
        free_runslots_.PushToTail(runslots);
    }
    LOG_RUNSLOTS_ALLOCATOR(INFO) << "Freed object at address " << std::hex << mem;
}

template <typename AllocConfigT, typename LockConfigT>
inline void RunSlotsAllocator<AllocConfigT, LockConfigT>::Collect(const GCObjectVisitor &death_checker_fn)
{
    LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "Collecting for RunSlots allocator started";
    IterateOverObjects([&](ObjectHeader *object_header) {
        LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "  iterate over " << std::hex << object_header;
        if (death_checker_fn(object_header) == ObjectStatus::DEAD_OBJECT) {
            LOG(DEBUG, GC) << "DELETE OBJECT " << GetDebugInfoAboutObject(object_header);
            FreeUnsafe<false>(object_header);
        }
    });
    LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "Collecting for RunSlots allocator finished";
}

template <typename AllocConfigT, typename LockConfigT>
template <typename ObjectVisitor>
void RunSlotsAllocator<AllocConfigT, LockConfigT>::IterateOverObjects(const ObjectVisitor &object_visitor)
{
    LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "Iteration over objects started";
    memory_pool_.IterateOverObjects(object_visitor);
    LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "Iteration over objects finished";
}

template <typename AllocConfigT, typename LockConfigT>
bool RunSlotsAllocator<AllocConfigT, LockConfigT>::AllocatedByRunSlotsAllocator(void *object)
{
    return AllocatedByRunSlotsAllocatorUnsafe(object);
}

template <typename AllocConfigT, typename LockConfigT>
bool RunSlotsAllocator<AllocConfigT, LockConfigT>::AllocatedByRunSlotsAllocatorUnsafe(void *object)
{
    return memory_pool_.IsInMemPools(object);
}

template <typename AllocConfigT, typename LockConfigT>
inline typename RunSlotsAllocator<AllocConfigT, LockConfigT>::RunSlotsType *
RunSlotsAllocator<AllocConfigT, LockConfigT>::CreateNewRunSlotsFromMemory(size_t slots_size)
{
    RunSlotsType *runslots = memory_pool_.GetNewRunSlots(slots_size);
    if (runslots != nullptr) {
        LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "Take " << RUNSLOTS_SIZE << " bytes of memory for new RunSlots instance from "
                                      << std::hex << runslots;
        return runslots;
    }
    LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "There is no free memory for RunSlots";
    return runslots;
}

template <typename AllocConfigT, typename LockConfigT>
inline bool RunSlotsAllocator<AllocConfigT, LockConfigT>::AddMemoryPool(void *mem, size_t size)
{
    LOG_RUNSLOTS_ALLOCATOR(INFO) << "Get new memory pool with size " << size << " bytes, at addr " << std::hex << mem;
    // Try to add this memory to the memory_pool_
    if (mem == nullptr) {
        LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "Failed to add memory, the memory is nullptr";
        return false;
    }
    if (size > MIN_POOL_SIZE) {
        // because it is requested for correct freed_runslots_bitmap_
        // workflow. Fix it in #4018
        LOG_RUNSLOTS_ALLOCATOR(DEBUG)
            << "Can't add new memory pool to this allocator because the memory size is equal to " << MIN_POOL_SIZE;
        return false;
    }
    if (!memory_pool_.AddNewMemoryPool(mem, size)) {
        LOG_RUNSLOTS_ALLOCATOR(DEBUG)
            << "Can't add new memory pool to this allocator. Maybe we already added too much memory pools.";
        return false;
    }
    return true;
}

template <typename AllocConfigT, typename LockConfigT>
template <typename MemVisitor>
void RunSlotsAllocator<AllocConfigT, LockConfigT>::VisitAndRemoveAllPools(const MemVisitor &mem_visitor)
{
    // We call this method and return pools to the system.
    // Therefore, delete all objects to clear all external dependences
    LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "Clear all objects inside the allocator";
    memory_pool_.VisitAllPools(mem_visitor);
}

template <typename AllocConfigT, typename LockConfigT>
template <typename MemVisitor>
// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_DECL_PARENTHESIS_PARAM_TYPE)
void RunSlotsAllocator<AllocConfigT, LockConfigT>::VisitAndRemoveFreePools([
    [maybe_unused]] const MemVisitor &mem_visitor)
{
    ReleaseEmptyRunSlotsPagesUnsafe();
    // We need to remove RunSlots from RunSlotsList
    // All of them must be inside free_runslots_ list.
    memory_pool_.VisitAndRemoveFreePools(mem_visitor);
}

template <typename AllocConfigT, typename LockConfigT>
template <typename MemVisitor>
void RunSlotsAllocator<AllocConfigT, LockConfigT>::IterateOverObjectsInRange(const MemVisitor &mem_visitor,
                                                                             void *left_border, void *right_border)
{
    LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "IterateOverObjectsInRange for range [" << std::hex << left_border << ", "
                                  << right_border << "]";
    ASSERT(ToUintPtr(right_border) >= ToUintPtr(left_border));
    if (!AllocatedByRunSlotsAllocatorUnsafe(left_border)) {
        LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "This memory range is not covered by this allocator";
        return;
    }
    // if the range crosses different allocators memory pools
    ASSERT(ToUintPtr(right_border) - ToUintPtr(left_border) ==
           (CrossingMapSingleton::GetCrossingMapGranularity() - 1U));
    ASSERT((ToUintPtr(right_border) & (~(CrossingMapSingleton::GetCrossingMapGranularity() - 1U))) ==
           (ToUintPtr(left_border) & (~(CrossingMapSingleton::GetCrossingMapGranularity() - 1U))));
    // Now we 100% sure that this left_border was allocated by RunSlots allocator.
    // We can just do alignment for this address and get a pointer to RunSlots header
    uintptr_t runslots_addr = (ToUintPtr(left_border) >> RUNSLOTS_ALIGNMENT) << RUNSLOTS_ALIGNMENT;
    while (runslots_addr < ToUintPtr(right_border)) {
        auto runslots = static_cast<RunSlotsType *>(ToVoidPtr(runslots_addr));
        os::memory::LockHolder runslots_lock(*runslots->GetLock());
        LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "IterateOverObjectsInRange, It is RunSlots with addr " << std::hex
                                      << static_cast<void *>(runslots);
        runslots->IterateOverOccupiedSlots(mem_visitor);
        runslots_addr += RUNSLOTS_SIZE;
    }
    LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "IterateOverObjectsInRange finished";
}

template <typename AllocConfigT, typename LockConfigT>
size_t RunSlotsAllocator<AllocConfigT, LockConfigT>::VerifyAllocator()
{
    size_t fail_cnt = 0;
    for (size_t i = 0; i < SLOTS_SIZES_VARIANTS; i++) {
        RunSlotsType *runslots = nullptr;
        {
            os::memory::LockHolder list_lock(*runslots_[i].GetLock());
            runslots = runslots_[i].GetHead();
        }
        if (runslots != nullptr) {
            os::memory::LockHolder runslots_lock(*runslots->GetLock());
            fail_cnt += runslots->VerifyRun();
        }
    }
    return fail_cnt;
}

template <typename AllocConfigT, typename LockConfigT>
bool RunSlotsAllocator<AllocConfigT, LockConfigT>::ContainObject(const ObjectHeader *obj)
{
    return AllocatedByRunSlotsAllocatorUnsafe(const_cast<ObjectHeader *>(obj));
}

template <typename AllocConfigT, typename LockConfigT>
bool RunSlotsAllocator<AllocConfigT, LockConfigT>::IsLive(const ObjectHeader *obj)
{
    ASSERT(ContainObject(obj));
    uintptr_t runslots_addr = ToUintPtr(obj) >> RUNSLOTS_ALIGNMENT << RUNSLOTS_ALIGNMENT;
    auto run = static_cast<RunSlotsType *>(ToVoidPtr(runslots_addr));
    if (run->IsEmpty()) {
        return false;
    }
    return run->IsLive(obj);
}

template <typename AllocConfigT, typename LockConfigT>
void RunSlotsAllocator<AllocConfigT, LockConfigT>::TrimUnsafe()
{
    // release page in free runslots list
    auto head = free_runslots_.GetHead();
    while (head != nullptr) {
        auto next = head->GetNextRunSlots();
        os::mem::ReleasePages(ToUintPtr(head), ToUintPtr(head) + RUNSLOTS_SIZE);
        head = next;
    }

    memory_pool_.VisitAllPoolsWithOccupiedSize([](void *mem, size_t used_size, size_t size) {
        uintptr_t start = AlignUp(ToUintPtr(mem) + used_size, panda::os::mem::GetPageSize());
        uintptr_t end = ToUintPtr(mem) + size;
        if (end >= start + panda::os::mem::GetPageSize()) {
            os::mem::ReleasePages(start, end);
        }
    });
}

template <typename AllocConfigT, typename LockConfigT>
inline void RunSlotsAllocator<AllocConfigT, LockConfigT>::RunSlotsList::PushToTail(RunSlotsType *runslots)
{
    LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "Push to tail RunSlots at addr " << std::hex << static_cast<void *>(runslots);
    LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "     tail_ " << std::hex << tail_;
    if (tail_ == nullptr) {
        LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "     List was empty, setup head_ and tail_";
        // this means that head_ == nullptr too
        head_ = runslots;
        tail_ = runslots;
        return;
    }
    tail_->SetNextRunSlots(runslots);
    runslots->SetPrevRunSlots(tail_);
    tail_ = runslots;
    tail_->SetNextRunSlots(nullptr);
}

template <typename AllocConfigT, typename LockConfigT>
inline typename RunSlotsAllocator<AllocConfigT, LockConfigT>::RunSlotsType *
RunSlotsAllocator<AllocConfigT, LockConfigT>::RunSlotsList::PopFromHead()
{
    LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "PopFromHead";
    if (UNLIKELY(head_ == nullptr)) {
        LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "      List is empty, nothing to pop";
        return nullptr;
    }
    RunSlotsType *head_runslots = head_;
    LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "     popped from head RunSlots " << std::hex << head_runslots;
    head_ = head_runslots->GetNextRunSlots();
    if (head_ == nullptr) {
        LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "     Now list is empty";
        // We pop the last element in the list
        tail_ = nullptr;
    } else {
        head_->SetPrevRunSlots(nullptr);
    }
    head_runslots->SetNextRunSlots(nullptr);
    return head_runslots;
}

template <typename AllocConfigT, typename LockConfigT>
inline typename RunSlotsAllocator<AllocConfigT, LockConfigT>::RunSlotsType *
RunSlotsAllocator<AllocConfigT, LockConfigT>::RunSlotsList::PopFromTail()
{
    LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "PopFromTail";
    if (UNLIKELY(tail_ == nullptr)) {
        LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "      List is empty, nothing to pop";
        return nullptr;
    }
    RunSlotsType *tail_runslots = tail_;
    LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "     popped from tail RunSlots " << std::hex << tail_runslots;
    tail_ = tail_runslots->GetPrevRunSlots();
    if (tail_ == nullptr) {
        LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "     Now list is empty";
        // We pop the last element in the list
        head_ = nullptr;
    } else {
        tail_->SetNextRunSlots(nullptr);
    }
    tail_runslots->SetPrevRunSlots(nullptr);
    return tail_runslots;
}

template <typename AllocConfigT, typename LockConfigT>
inline void RunSlotsAllocator<AllocConfigT, LockConfigT>::RunSlotsList::PopFromList(RunSlotsType *runslots)
{
    LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "PopFromList RunSlots with addr " << std::hex << runslots;
    LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "head_ = " << std::hex << head_;
    LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "tail_ = " << std::hex << tail_;

    if (runslots == head_) {
        LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "It is RunSlots from the head.";
        PopFromHead();
        return;
    }
    if (runslots == tail_) {
        LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "It is RunSlots from the tail.";
        PopFromTail();
        return;
    }
    LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "Remove RunSlots from the list.";
    ASSERT(runslots != nullptr);
    RunSlotsType *next_runslots = runslots->GetNextRunSlots();
    RunSlotsType *previous_runslots = runslots->GetPrevRunSlots();
    ASSERT(next_runslots != nullptr);
    ASSERT(previous_runslots != nullptr);

    next_runslots->SetPrevRunSlots(previous_runslots);
    previous_runslots->SetNextRunSlots(next_runslots);
    runslots->SetNextRunSlots(nullptr);
    runslots->SetPrevRunSlots(nullptr);
}

template <typename AllocConfigT, typename LockConfigT>
inline RunSlotsAllocator<AllocConfigT, LockConfigT>::MemPoolManager::MemPoolManager()
{
    occupied_tail_ = nullptr;
    free_tail_ = nullptr;
    partially_occupied_head_ = nullptr;
}

template <typename AllocConfigT, typename LockConfigT>
inline typename RunSlotsAllocator<AllocConfigT, LockConfigT>::RunSlotsType *
RunSlotsAllocator<AllocConfigT, LockConfigT>::MemPoolManager::GetNewRunSlots(size_t slots_size)
{
    os::memory::WriteLockHolder wlock(lock_);
    RunSlotsType *new_runslots = nullptr;
    if (partially_occupied_head_ != nullptr) {
        new_runslots = partially_occupied_head_->GetMemoryForRunSlots(slots_size);
        ASSERT(new_runslots != nullptr);
        if (UNLIKELY(!partially_occupied_head_->HasMemoryForRunSlots())) {
            partially_occupied_head_ = partially_occupied_head_->GetNext();
            ASSERT((partially_occupied_head_ == nullptr) || (partially_occupied_head_->HasMemoryForRunSlots()));
        }
    } else if (free_tail_ != nullptr) {
        LOG_RUNSLOTS_ALLOCATOR(DEBUG)
            << "MemPoolManager: occupied_tail_ doesn't have memory for RunSlots, get new pool from free pools";
        PoolListElement *free_element = free_tail_;
        free_tail_ = free_tail_->GetPrev();

        free_element->PopFromList();
        free_element->SetPrev(occupied_tail_);

        if (occupied_tail_ != nullptr) {
            ASSERT(occupied_tail_->GetNext() == nullptr);
            occupied_tail_->SetNext(free_element);
        }
        occupied_tail_ = free_element;

        if (partially_occupied_head_ == nullptr) {
            partially_occupied_head_ = occupied_tail_;
            ASSERT(partially_occupied_head_->HasMemoryForRunSlots());
        }

        ASSERT(occupied_tail_->GetNext() == nullptr);
        new_runslots = occupied_tail_->GetMemoryForRunSlots(slots_size);
        ASSERT(new_runslots != nullptr);
    }
    return new_runslots;
}

template <typename AllocConfigT, typename LockConfigT>
inline bool RunSlotsAllocator<AllocConfigT, LockConfigT>::MemPoolManager::AddNewMemoryPool(void *mem, size_t size)
{
    os::memory::WriteLockHolder wlock(lock_);
    PoolListElement *new_pool = PoolListElement::Create(mem, size, free_tail_);
    if (free_tail_ != nullptr) {
        ASSERT(free_tail_->GetNext() == nullptr);
        free_tail_->SetNext(new_pool);
    }
    free_tail_ = new_pool;
    ASAN_POISON_MEMORY_REGION(mem, size);
    // To not unpoison it every time at access.
    ASAN_UNPOISON_MEMORY_REGION(mem, sizeof(PoolListElement));
    return true;
}

template <typename AllocConfigT, typename LockConfigT>
inline void RunSlotsAllocator<AllocConfigT, LockConfigT>::MemPoolManager::ReturnAndReleaseRunSlotsMemory(
    RunSlotsType *runslots)
{
    os::memory::WriteLockHolder wlock(lock_);
    auto pool = static_cast<PoolListElement *>(ToVoidPtr(runslots->GetPoolPointer()));
    if (!pool->HasMemoryForRunSlots()) {
        ASSERT(partially_occupied_head_ != pool);
        // We should move this pool to the end of an occupied list
        if (pool != occupied_tail_) {
            pool->PopFromList();
            pool->SetPrev(occupied_tail_);
            if (UNLIKELY(occupied_tail_ == nullptr)) {
                UNREACHABLE();
            }
            occupied_tail_->SetNext(pool);
            occupied_tail_ = pool;
        } else {
            ASSERT(partially_occupied_head_ == nullptr);
        }
        if (partially_occupied_head_ == nullptr) {
            partially_occupied_head_ = occupied_tail_;
        }
    }

    pool->AddFreedRunSlots(runslots);
    ASSERT(partially_occupied_head_->HasMemoryForRunSlots());

    // Start address from which we can release pages
    uintptr_t start_addr = AlignUp(ToUintPtr(runslots), os::mem::GetPageSize());
    // End address before which we can release pages
    uintptr_t end_addr = os::mem::AlignDownToPageSize(ToUintPtr(runslots) + RUNSLOTS_SIZE);
    if (start_addr < end_addr) {
        os::mem::ReleasePages(start_addr, end_addr);
    }
}

template <typename AllocConfigT, typename LockConfigT>
bool RunSlotsAllocator<AllocConfigT, LockConfigT>::MemPoolManager::IsInMemPools(void *object)
{
    os::memory::ReadLockHolder rlock(lock_);
    PoolListElement *current = occupied_tail_;
    while (current != nullptr) {
        if (current->IsInUsedMemory(object)) {
            return true;
        }
        current = current->GetPrev();
    }
    return false;
}

template <typename AllocConfigT, typename LockConfigT>
template <typename ObjectVisitor>
void RunSlotsAllocator<AllocConfigT, LockConfigT>::MemPoolManager::IterateOverObjects(
    const ObjectVisitor &object_visitor)
{
    PoolListElement *current_pool = nullptr;
    {
        os::memory::ReadLockHolder rlock(lock_);
        current_pool = occupied_tail_;
    }
    while (current_pool != nullptr) {
        current_pool->IterateOverRunSlots([&](RunSlotsType *runslots) {
            os::memory::LockHolder runslots_lock(*runslots->GetLock());
            ASSERT(runslots->GetPoolPointer() == ToUintPtr(current_pool));
            runslots->IterateOverOccupiedSlots(object_visitor);
            return true;
        });
        {
            os::memory::ReadLockHolder rlock(lock_);
            current_pool = current_pool->GetPrev();
        }
    }
}

template <typename AllocConfigT, typename LockConfigT>
template <typename MemVisitor>
void RunSlotsAllocator<AllocConfigT, LockConfigT>::MemPoolManager::VisitAllPools(const MemVisitor &mem_visitor)
{
    os::memory::WriteLockHolder wlock(lock_);
    PoolListElement *current_pool = occupied_tail_;
    while (current_pool != nullptr) {
        // Use tmp in case if visitor with side effects
        PoolListElement *tmp = current_pool->GetPrev();
        mem_visitor(current_pool->GetPoolMemory(), current_pool->GetSize());
        current_pool = tmp;
    }
}

template <typename AllocConfigT, typename LockConfigT>
template <typename MemVisitor>
void RunSlotsAllocator<AllocConfigT, LockConfigT>::MemPoolManager::VisitAllPoolsWithOccupiedSize(
    const MemVisitor &mem_visitor)
{
    os::memory::WriteLockHolder wlock(lock_);
    PoolListElement *current_pool = occupied_tail_;
    while (current_pool != nullptr) {
        // Use tmp in case if visitor with side effects
        PoolListElement *tmp = current_pool->GetPrev();
        mem_visitor(current_pool->GetPoolMemory(), current_pool->GetOccupiedSize(), current_pool->GetSize());
        current_pool = tmp;
    }
}

template <typename AllocConfigT, typename LockConfigT>
template <typename MemVisitor>
void RunSlotsAllocator<AllocConfigT, LockConfigT>::MemPoolManager::VisitAndRemoveFreePools(
    const MemVisitor &mem_visitor)
{
    os::memory::WriteLockHolder wlock(lock_);
    LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "VisitAllFreePools inside RunSlotsAllocator";
    // First, iterate over totally free pools:
    PoolListElement *current_pool = free_tail_;
    while (current_pool != nullptr) {
        LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "VisitAllFreePools: Visit free pool with addr " << std::hex
                                      << current_pool->GetPoolMemory() << " and size " << std::dec
                                      << current_pool->GetSize();
        // Use tmp in case if visitor with side effects
        PoolListElement *tmp = current_pool->GetPrev();
        mem_visitor(current_pool->GetPoolMemory(), current_pool->GetSize());
        current_pool = tmp;
    }
    free_tail_ = nullptr;
    // Second, try to find free pool in occupied:
    current_pool = occupied_tail_;
    while (current_pool != nullptr) {
        // Use tmp in case if visitor with side effects
        PoolListElement *tmp = current_pool->GetPrev();
        if (!current_pool->HasUsedMemory()) {
            LOG_RUNSLOTS_ALLOCATOR(DEBUG)
                << "VisitAllFreePools: Visit occupied pool with addr " << std::hex << current_pool->GetPoolMemory()
                << " and size " << std::dec << current_pool->GetSize();
            // This Pool doesn't have any occupied memory in RunSlots
            // Therefore, we can free it
            if (occupied_tail_ == current_pool) {
                LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "VisitAllFreePools: Update occupied_tail_";
                occupied_tail_ = current_pool->GetPrev();
            }
            if (current_pool == partially_occupied_head_) {
                partially_occupied_head_ = partially_occupied_head_->GetNext();
                ASSERT((partially_occupied_head_ == nullptr) || (partially_occupied_head_->HasMemoryForRunSlots()));
            }
            current_pool->PopFromList();
            mem_visitor(current_pool->GetPoolMemory(), current_pool->GetSize());
        }
        current_pool = tmp;
    }
}

template <typename AllocConfigT, typename LockConfigT>
inline RunSlotsAllocator<AllocConfigT, LockConfigT>::MemPoolManager::PoolListElement::PoolListElement()
{
    start_mem_ = 0;
    pool_mem_ = 0;
    size_ = 0;
    free_ptr_ = 0;
    prev_pool_ = nullptr;
    next_pool_ = nullptr;
    freeded_runslots_count_ = 0;
    (void)memset_s(storage_for_bitmap_.data(), sizeof(BitMapStorageType), 0, sizeof(BitMapStorageType));
}

template <typename AllocConfigT, typename LockConfigT>
void RunSlotsAllocator<AllocConfigT, LockConfigT>::MemPoolManager::PoolListElement::PopFromList()
{
    if (next_pool_ != nullptr) {
        next_pool_->SetPrev(prev_pool_);
    }
    if (prev_pool_ != nullptr) {
        prev_pool_->SetNext(next_pool_);
    }
    next_pool_ = nullptr;
    prev_pool_ = nullptr;
}

template <typename AllocConfigT, typename LockConfigT>
uintptr_t RunSlotsAllocator<AllocConfigT, LockConfigT>::MemPoolManager::PoolListElement::GetFirstRunSlotsBlock(
    uintptr_t mem)
{
    return AlignUp(mem, 1UL << RUNSLOTS_ALIGNMENT);
}

template <typename AllocConfigT, typename LockConfigT>
inline void RunSlotsAllocator<AllocConfigT, LockConfigT>::MemPoolManager::PoolListElement::Initialize(
    void *pool_mem, uintptr_t unoccupied_mem, size_t size, PoolListElement *prev)
{
    start_mem_ = unoccupied_mem;
    pool_mem_ = ToUintPtr(pool_mem);
    size_ = size;
    free_ptr_ = GetFirstRunSlotsBlock(start_mem_);
    prev_pool_ = prev;
    next_pool_ = nullptr;
    freeded_runslots_count_ = 0;
    freed_runslots_bitmap_.ReInitializeMemoryRange(pool_mem);
    ASSERT(freed_runslots_bitmap_.FindFirstMarkedChunks() == nullptr);
    LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "PoolMemory: first free RunSlots block = " << std::hex << free_ptr_;
}

template <typename AllocConfigT, typename LockConfigT>
inline typename RunSlotsAllocator<AllocConfigT, LockConfigT>::RunSlotsType *
RunSlotsAllocator<AllocConfigT, LockConfigT>::MemPoolManager::PoolListElement::GetMemoryForRunSlots(size_t slots_size)
{
    if (!HasMemoryForRunSlots()) {
        LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "PoolMemory: There is no free memory for RunSlots";
        return nullptr;
    }
    RunSlotsType *runslots = GetFreedRunSlots(slots_size);
    if (runslots == nullptr) {
        uintptr_t old_mem = free_ptr_.load();
        ASSERT(pool_mem_ + size_ >= old_mem + RUNSLOTS_SIZE);

        // Initialize it firstly before updating free ptr
        // because it will be visible outside after that.
        runslots = static_cast<RunSlotsType *>(ToVoidPtr(old_mem));
        runslots->Initialize(slots_size, ToUintPtr(this), true);

        free_ptr_.fetch_add(RUNSLOTS_SIZE);
        ASSERT(free_ptr_.load() == (old_mem + RUNSLOTS_SIZE));
        LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "PoolMemory: Took memory for RunSlots from addr " << std::hex
                                      << ToVoidPtr(old_mem)
                                      << ". New first free RunSlots block = " << ToVoidPtr(free_ptr_.load());
    }
    ASSERT(runslots != nullptr);
    return runslots;
}

template <typename AllocConfigT, typename LockConfigT>
template <typename RunSlotsVisitor>
void RunSlotsAllocator<AllocConfigT, LockConfigT>::MemPoolManager::PoolListElement::IterateOverRunSlots(
    const RunSlotsVisitor &runslots_visitor)
{
    LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "Iterating over runslots inside pool with address" << std::hex << pool_mem_
                                  << " with size " << std::dec << size_ << " bytes";
    uintptr_t current_runslot = GetFirstRunSlotsBlock(start_mem_);
    uintptr_t last_runslot = free_ptr_.load();
    while (current_runslot < last_runslot) {
        ASSERT(start_mem_ <= current_runslot);
        if (!freed_runslots_bitmap_.AtomicTest(ToVoidPtr(current_runslot))) {
            auto cur_rs = static_cast<RunSlotsType *>(ToVoidPtr(current_runslot));
            LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "Iterating. Process RunSlots " << std::hex << cur_rs;
            if (!runslots_visitor(cur_rs)) {
                return;
            }
        }
        current_runslot += RUNSLOTS_SIZE;
    }
    LOG_RUNSLOTS_ALLOCATOR(DEBUG) << "Iterating runslots inside this pool finished";
}

template <typename AllocConfigT, typename LockConfigT>
bool RunSlotsAllocator<AllocConfigT, LockConfigT>::MemPoolManager::PoolListElement::HasUsedMemory()
{
    uintptr_t current_runslot = GetFirstRunSlotsBlock(start_mem_);
    uintptr_t last_runslot = free_ptr_.load();
    while (current_runslot < last_runslot) {
        ASSERT(start_mem_ <= current_runslot);
        if (!freed_runslots_bitmap_.AtomicTest(ToVoidPtr(current_runslot))) {
            // We have runslots instance which is in use somewhere.
            return true;
        }
        current_runslot += RUNSLOTS_SIZE;
    }
    return false;
}

template <typename AllocConfigT, typename LockConfigT>
size_t RunSlotsAllocator<AllocConfigT, LockConfigT>::MemPoolManager::PoolListElement::GetOccupiedSize()
{
    if (!IsInitialized()) {
        return 0;
    }
    return free_ptr_.load() - pool_mem_;
}

template <typename AllocConfigT, typename LockConfigT>
bool RunSlotsAllocator<AllocConfigT, LockConfigT>::MemPoolManager::PoolListElement::IsInUsedMemory(void *object)
{
    uintptr_t mem_pointer = start_mem_;
    ASSERT(!((ToUintPtr(object) < GetFirstRunSlotsBlock(mem_pointer)) && (ToUintPtr(object) >= mem_pointer)));
    bool is_in_allocated_memory =
        (ToUintPtr(object) < free_ptr_.load()) && (ToUintPtr(object) >= GetFirstRunSlotsBlock(mem_pointer));
    return is_in_allocated_memory && !IsInFreedRunSlots(object);
}

template <typename AllocConfigT, typename LockConfigT>
typename RunSlotsAllocator<AllocConfigT, LockConfigT>::RunSlotsType *
RunSlotsAllocator<AllocConfigT, LockConfigT>::MemPoolManager::PoolListElement::GetFreedRunSlots(size_t slots_size)
{
    auto slots = static_cast<RunSlotsType *>(freed_runslots_bitmap_.FindFirstMarkedChunks());
    if (slots == nullptr) {
        ASSERT(freeded_runslots_count_ == 0);
        return nullptr;
    }

    // Initialize it firstly before updating bitmap
    // because it will be visible outside after that.
    slots->Initialize(slots_size, ToUintPtr(this), true);

    ASSERT(freeded_runslots_count_ > 0);
    [[maybe_unused]] bool old_val = freed_runslots_bitmap_.AtomicTestAndClear(slots);
    ASSERT(old_val);
    freeded_runslots_count_--;

    return slots;
}

template <typename AllocConfigT, typename LockConfigT>
bool RunSlotsAllocator<AllocConfigT, LockConfigT>::MemPoolManager::PoolListElement::HasMemoryForRunSlots()
{
    if (!IsInitialized()) {
        return false;
    }
    bool has_free_memory = (free_ptr_.load() + RUNSLOTS_SIZE) <= (pool_mem_ + size_);
    bool has_freed_runslots = (freeded_runslots_count_ > 0);
    ASSERT(has_freed_runslots == (freed_runslots_bitmap_.FindFirstMarkedChunks() != nullptr));
    return has_free_memory || has_freed_runslots;
}

#undef LOG_RUNSLOTS_ALLOCATOR

}  // namespace panda::mem
#endif  // PANDA_RUNTIME_MEM_RUNSLOTS_ALLOCATOR_INL_H_
