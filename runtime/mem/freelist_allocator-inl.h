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

#ifndef PANDA_RUNTIME_MEM_FREELIST_ALLOCATOR_INL_H_
#define PANDA_RUNTIME_MEM_FREELIST_ALLOCATOR_INL_H_

#include "libpandabase/utils/logger.h"
#include "runtime/mem/alloc_config.h"
#include "runtime/mem/freelist_allocator.h"
#include "runtime/mem/object_helpers.h"

namespace panda::mem {

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_FREELIST_ALLOCATOR(level) LOG(level, ALLOC) << "FreeListAllocator: "

template <typename AllocConfigT, typename LockConfigT>
FreeListAllocator<AllocConfigT, LockConfigT>::FreeListAllocator(MemStatsType *mem_stats, SpaceType type_allocation)
    : type_allocation_(type_allocation), mem_stats_(mem_stats)
{
    LOG_FREELIST_ALLOCATOR(DEBUG) << "Initializing FreeListAllocator";
    ASAN_POISON_MEMORY_REGION(&segregated_list_, sizeof(segregated_list_));
    LOG_FREELIST_ALLOCATOR(INFO) << "Initializing FreeListAllocator finished";
}

template <typename AllocConfigT, typename LockConfigT>
FreeListAllocator<AllocConfigT, LockConfigT>::~FreeListAllocator()
{
    LOG_FREELIST_ALLOCATOR(DEBUG) << "Destroying FreeListAllocator";
    ASAN_UNPOISON_MEMORY_REGION(&segregated_list_, sizeof(segregated_list_));
    LOG_FREELIST_ALLOCATOR(INFO) << "Destroying FreeListAllocator finished";
}

template <typename AllocConfigT, typename LockConfigT>
void *FreeListAllocator<AllocConfigT, LockConfigT>::Alloc(size_t size, Alignment align)
{
    os::memory::WriteLockHolder wlock(alloc_free_lock_);
    LOG_FREELIST_ALLOCATOR(DEBUG) << "Try to allocate object with size " << std::dec << size;
    size_t alloc_size = size;
    if (alloc_size < FREELIST_ALLOCATOR_MIN_SIZE) {
        LOG_FREELIST_ALLOCATOR(DEBUG) << "Try to allocate an object with size less than min for this allocator";
        alloc_size = FREELIST_ALLOCATOR_MIN_SIZE;
    }
    size_t def_aligned_size = AlignUp(alloc_size, GetAlignmentInBytes(FREELIST_DEFAULT_ALIGNMENT));
    if (def_aligned_size > alloc_size) {
        alloc_size = def_aligned_size;
        LOG_FREELIST_ALLOCATOR(DEBUG) << "Align size to default alignment. New size = " << alloc_size;
    }
    if (alloc_size > FREELIST_MAX_ALLOC_SIZE) {
        LOG_FREELIST_ALLOCATOR(DEBUG) << "Try allocate too big memory for free list allocator. Return nullptr";
        return nullptr;
    }
    // Get best-fit memory piece from segregated list.
    MemoryBlockHeader *memory_block = GetFromSegregatedList(alloc_size, align);
    if (memory_block == nullptr) {
        LOG_FREELIST_ALLOCATOR(DEBUG) << "Couldn't allocate memory";
        return nullptr;
    }
    LOG_FREELIST_ALLOCATOR(DEBUG) << "Found memory block at addr = " << std::hex << memory_block << " with size "
                                  << std::dec << memory_block->GetSize();
    ASSERT(memory_block->GetSize() >= alloc_size);
    uintptr_t memory_pointer = ToUintPtr(memory_block->GetMemory());
    bool required_padding = false;
    if ((memory_pointer & (GetAlignmentInBytes(align) - 1)) != 0U) {
        LOG_FREELIST_ALLOCATOR(DEBUG) << "Raw memory is not aligned as we need. Create special header for padding";
        // Raw memory pointer is not aligned as we expected
        // We need to create extra header inside
        uintptr_t aligned_memory_pointer =
            AlignUp(memory_pointer + sizeof(MemoryBlockHeader), GetAlignmentInBytes(align));
        size_t size_with_padding = alloc_size + (aligned_memory_pointer - memory_pointer);
        ASSERT(memory_block->GetSize() >= size_with_padding);
        auto padding_header =
            static_cast<MemoryBlockHeader *>(ToVoidPtr(aligned_memory_pointer - sizeof(MemoryBlockHeader)));
        LOG_FREELIST_ALLOCATOR(DEBUG) << "Created padding header at addr " << std::hex << padding_header;
        padding_header->Initialize(alloc_size, memory_block);
        padding_header->SetAsPaddingHeader();
        // Update values
        memory_pointer = aligned_memory_pointer;
        alloc_size = size_with_padding;
        required_padding = true;
    }
    if (CanCreateNewBlockFromRemainder(memory_block, alloc_size)) {
        LOG_FREELIST_ALLOCATOR(DEBUG) << "Created new memory block from the remainder part:";
        MemoryBlockHeader *new_free_block = SplitMemoryBlocks(memory_block, alloc_size);
        LOG_FREELIST_ALLOCATOR(DEBUG) << "New block started at addr " << std::hex << new_free_block << " with size "
                                      << std::dec << new_free_block->GetSize();
        memory_block->SetUsed();
        FreeListHeader *new_free_list_element = TryToCoalescing(new_free_block);
        ASSERT(!new_free_list_element->IsUsed());
        AddToSegregatedList(new_free_list_element);
    } else {
        LOG_FREELIST_ALLOCATOR(DEBUG) << "Can't create new block from the remainder. Use full block.";
        memory_block->SetUsed();
    }
    if (required_padding) {
        // We must update some values in current memory_block
        uintptr_t padding_size = memory_pointer - ToUintPtr(memory_block->GetMemory());
        if (padding_size == sizeof(MemoryBlockHeader)) {
            LOG_FREELIST_ALLOCATOR(DEBUG) << "SetHasPaddingHeaderAfter";
            memory_block->SetPaddingHeaderStoredAfterHeader();
        } else {
            LOG_FREELIST_ALLOCATOR(DEBUG) << "SetHasPaddingSizeAfter, size = " << padding_size;
            memory_block->SetPaddingSizeStoredAfterHeader();
            memory_block->SetPaddingSize(padding_size);
        }
    }
    LOG_FREELIST_ALLOCATOR(INFO) << "Allocated memory at addr " << std::hex << ToVoidPtr(memory_pointer);
    {
        AllocConfigT::OnAlloc(memory_block->GetSize(), type_allocation_, mem_stats_);
        // It is not the object size itself, cause we can't compute it from MemoryBlockHeader structure at Free call.
        // It is an approximation.
        size_t current_size =
            ToUintPtr(memory_block) + memory_block->GetSize() + sizeof(MemoryBlockHeader) - memory_pointer;
        AllocConfigT::AddToCrossingMap(ToVoidPtr(memory_pointer), current_size);
    }
    ASAN_UNPOISON_MEMORY_REGION(ToVoidPtr(memory_pointer), size);
    AllocConfigT::MemoryInit(ToVoidPtr(memory_pointer), size);
    return ToVoidPtr(memory_pointer);
}

template <typename AllocConfigT, typename LockConfigT>
void FreeListAllocator<AllocConfigT, LockConfigT>::Free(void *mem)
{
    os::memory::WriteLockHolder wlock(alloc_free_lock_);
    FreeUnsafe(mem);
}

template <typename AllocConfigT, typename LockConfigT>
void FreeListAllocator<AllocConfigT, LockConfigT>::FreeUnsafe(void *mem)
{
    if (UNLIKELY(mem == nullptr)) {
        LOG_FREELIST_ALLOCATOR(DEBUG) << "Try to free memory at invalid addr 0";
        return;
    }
    LOG_FREELIST_ALLOCATOR(DEBUG) << "Try to free memory at addr " << std::hex << mem;
#ifndef NDEBUG
    if (!AllocatedByFreeListAllocatorUnsafe(mem)) {
        LOG_FREELIST_ALLOCATOR(DEBUG) << "Try to free memory no from this allocator";
        return;
    }
#endif  // !NDEBUG

    MemoryBlockHeader *memory_header = GetFreeListMemoryHeader(mem);
    LOG_FREELIST_ALLOCATOR(DEBUG) << "It is a memory with header " << std::hex << memory_header << " with size "
                                  << std::dec << memory_header->GetSize() << " (probably with padding)";
    {
        AllocConfigT::OnFree(memory_header->GetSize(), type_allocation_, mem_stats_);
        // It is not the object size itself, cause we can't compute it from MemoryBlockHeader structure.
        // It is an approximation.
        size_t current_size =
            ToUintPtr(memory_header) + memory_header->GetSize() + sizeof(MemoryBlockHeader) - ToUintPtr(mem);
        MemoryBlockHeader *prev_used_header = memory_header->GetPrevUsedHeader();
        void *prev_object = nullptr;
        size_t prev_size = 0;
        if (prev_used_header != nullptr) {
            prev_object = prev_used_header->GetMemory();
            prev_size = ToUintPtr(prev_used_header) + prev_used_header->GetSize() + sizeof(MemoryBlockHeader) -
                        ToUintPtr(prev_used_header->GetMemory());
        }
        MemoryBlockHeader *next_used_header = memory_header->GetNextUsedHeader();
        void *next_object = nullptr;
        if (next_used_header != nullptr) {
            next_object = next_used_header->GetMemory();
        }
        AllocConfigT::RemoveFromCrossingMap(mem, current_size, next_object, prev_object, prev_size);
    }
    memory_header->SetUnused();
    FreeListHeader *new_free_list_element = TryToCoalescing(memory_header);
    ASAN_POISON_MEMORY_REGION(new_free_list_element, new_free_list_element->GetSize() + sizeof(MemoryBlockHeader));
    AddToSegregatedList(new_free_list_element);
    LOG_FREELIST_ALLOCATOR(INFO) << "Freed memory at addr " << std::hex << mem;
}

// but it requires pool alignment restriction
// (we must compute memory pool header addr from a memory block addr stored inside it)

// During Collect method call we iterate over memory blocks in each pool.
// This iteration can cause race conditions in multithreaded mode.
// E.g. in this scenario:
// |-------|---------|-------------------|------------------------------------------------------------------|
// | time: | Thread: |    Description:   |                         Memory footprint:                        |
// |-------|---------|-------------------|------------------------------------------------------------------|
// |       |         | Thread0 starts    |  |..............Free  Block.............|...Allocated block...|  |
// |       |         | iterating         |  |                                                               |
// |   0   |    0    | over mem blocks   |  current block pointer                                           |
// |       |         | and current block |                                                                  |
// |       |         | is free block     |                                                                  |
// |-------|---------|-------------------|------------------------------------------------------------------|
// |       |         | Thread1           |  |...Allocated block...|................|...Allocated block...|  |
// |   1   |    1    | allocates memory  |                        |                                         |
// |       |         | at this block     |               Unused memory peace                                |
// |       |         |                   |                                                                  |
// |-------|---------|-------------------|------------------------------------------------------------------|
// |       |         | Thread1           |  |...Allocated block...|................|...Allocated block...|  |
// |   2   |    1    | set up values in  |  |                                                               |
// |       |         | this block header |  change size of this block                                       |
// |       |         | (set up size)     |                                                                  |
// |-------|---------|-------------------|------------------------------------------------------------------|
// |       |         | Thread0 reads     |  |...Allocated block...|................|...Allocated block...|  |
// |       |         | some garbage or   |                                                                  |
// |   3   |    0    | wrong value to    |  current block pointer - points to wrong memory                  |
// |       |         | calculate next    |                                                                  |
// |       |         | block pointer     |                                                                  |
// |-------|---------|-------------------|------------------------------------------------------------------|
//
// Therefore, we must unlock allocator's alloc/free methods only
// when we have a pointer to allocated block (i.e. IsUsed())

template <typename AllocConfigT, typename LockConfigT>
void FreeListAllocator<AllocConfigT, LockConfigT>::Collect(const GCObjectVisitor &death_checker_fn)
{
    LOG_FREELIST_ALLOCATOR(DEBUG) << "Collecting started";
    IterateOverObjects([&](ObjectHeader *mem) {
        if (death_checker_fn(mem) == ObjectStatus::DEAD_OBJECT) {
            LOG(DEBUG, GC) << "DELETE OBJECT " << GetDebugInfoAboutObject(mem);
            FreeUnsafe(mem);
        }
    });
    LOG_FREELIST_ALLOCATOR(DEBUG) << "Collecting finished";
}

template <typename AllocConfigT, typename LockConfigT>
template <typename ObjectVisitor>
void FreeListAllocator<AllocConfigT, LockConfigT>::IterateOverObjects(const ObjectVisitor &object_visitor)
{
    LOG_FREELIST_ALLOCATOR(DEBUG) << "Iterating over objects started";
    MemoryPoolHeader *current_pool = nullptr;
    {
        // Do this under lock because the pointer for mempool_tail can be changed by other threads
        // in AddMemoryPool method call.
        // NOTE: We add each new pool at the mempool_tail_. Therefore, we can read it once and iterate to head.
        os::memory::ReadLockHolder rlock(alloc_free_lock_);
        current_pool = mempool_tail_;
    }
    while (current_pool != nullptr) {
        LOG_FREELIST_ALLOCATOR(DEBUG) << "  iterate over " << std::hex << current_pool;
        MemoryBlockHeader *current_mem_header = current_pool->GetFirstMemoryHeader();
        while (current_mem_header != nullptr) {
            // Lock any possible memory headers changes in the allocator.
            os::memory::WriteLockHolder wlock(alloc_free_lock_);
            if (current_mem_header->IsUsed()) {
                // We can call Free inside mem_visitor, that's why we should lock everything
                object_visitor(static_cast<ObjectHeader *>(current_mem_header->GetMemory()));
                // At this point, current_mem_header can point to a nonexistent memory block
                // (e.g., if we coalesce it with the previous free block).
                // However, we still have valid MemoryBlockHeader class field here.
                // NOTE: Firstly, we coalesce current_mem_header block with next (if it is free)
                // and update current block size. Therefore, we have a valid pointer to next memory block.

                // After calling mem_visitor() the current_mem_header may points to free block of memory,
                // which can be modified at the alloc call in other thread.
                // Therefore, do not unlock until we have a pointer to the Used block.
                current_mem_header = current_mem_header->GetNextUsedHeader();
            } else {
                // Current header marked as Unused so it can be modify by an allocation in some thread.
                // So, read next header with in locked state.
                current_mem_header = current_mem_header->GetNextUsedHeader();
            }
            // We have a pointer to Used memory block, or to nullptr.
            // Therefore, we can unlock.
        }
        current_pool = current_pool->GetPrev();
    }
    LOG_FREELIST_ALLOCATOR(DEBUG) << "Iterating over objects finished";
}

template <typename AllocConfigT, typename LockConfigT>
bool FreeListAllocator<AllocConfigT, LockConfigT>::AddMemoryPool(void *mem, size_t size)
{
    // Lock alloc/free cause we add new block to segregated list here.
    os::memory::WriteLockHolder wlock(alloc_free_lock_);
    ASSERT(mem != nullptr);
    LOG_FREELIST_ALLOCATOR(INFO) << "Add memory pool to FreeListAllocator from  " << std::hex << mem << " with size "
                                 << std::dec << size;
    ASSERT((ToUintPtr(mem) & (sizeof(MemoryBlockHeader) - 1)) == 0U);
    auto mempool_header = static_cast<MemoryPoolHeader *>(mem);
    if (mempool_head_ == nullptr) {
        LOG_FREELIST_ALLOCATOR(DEBUG) << "Initialize mempool_head_";
        mempool_header->Initialize(size, nullptr, nullptr);
        mempool_head_ = mempool_header;
        mempool_tail_ = mempool_header;
    } else {
        LOG_FREELIST_ALLOCATOR(DEBUG) << "Add this memory pool at the tail after block " << std::hex << mempool_tail_;
        mempool_header->Initialize(size, mempool_tail_, nullptr);
        mempool_tail_->SetNext(mempool_header);
        mempool_tail_ = mempool_header;
    }
    MemoryBlockHeader *first_mem_header = mempool_header->GetFirstMemoryHeader();
    first_mem_header->Initialize(size - sizeof(MemoryBlockHeader) - sizeof(MemoryPoolHeader), nullptr);
    first_mem_header->SetLastBlockInPool();
    AddToSegregatedList(static_cast<FreeListHeader *>(first_mem_header));
    ASAN_POISON_MEMORY_REGION(mem, size);
    AllocConfigT::InitializeCrossingMapForMemory(mem, size);
    return true;
}

template <typename AllocConfigT, typename LockConfigT>
template <typename MemVisitor>
void FreeListAllocator<AllocConfigT, LockConfigT>::VisitAndRemoveAllPools(const MemVisitor &mem_visitor)
{
    // We call this method and return pools to the system.
    // Therefore, delete all objects to clear all external dependences
    LOG_FREELIST_ALLOCATOR(DEBUG) << "Clear all objects inside the allocator";
    // Lock everything to avoid race condition.
    os::memory::WriteLockHolder wlock(alloc_free_lock_);
    MemoryPoolHeader *current_pool = mempool_head_;
    while (current_pool != nullptr) {
        // Use tmp in case if visitor with side effects
        MemoryPoolHeader *tmp = current_pool->GetNext();
        AllocConfigT::RemoveCrossingMapForMemory(current_pool, current_pool->GetSize());
        mem_visitor(current_pool, current_pool->GetSize());
        current_pool = tmp;
    }
}

template <typename AllocConfigT, typename LockConfigT>
template <typename MemVisitor>
// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_DECL_PARENTHESIS_PARAM_TYPE)
void FreeListAllocator<AllocConfigT, LockConfigT>::VisitAndRemoveFreePools(const MemVisitor &mem_visitor)
{
    // Lock everything to avoid race condition.
    os::memory::WriteLockHolder wlock(alloc_free_lock_);
    MemoryPoolHeader *current_pool = mempool_head_;
    while (current_pool != nullptr) {
        // Use tmp in case if visitor with side effects
        MemoryPoolHeader *tmp = current_pool->GetNext();
        MemoryBlockHeader *first_block = current_pool->GetFirstMemoryHeader();
        if (first_block->IsLastBlockInPool() && !first_block->IsUsed()) {
            // We have only one big memory block in this pool,
            // and this block is not used
            LOG_FREELIST_ALLOCATOR(DEBUG)
                << "VisitAndRemoveFreePools: Remove free memory pool from allocator with start addr" << std::hex
                << current_pool << " and size " << std::dec << current_pool->GetSize()
                << " bytes with the first block at addr " << std::hex << first_block << " and size " << std::dec
                << first_block->GetSize();
            auto free_header = static_cast<FreeListHeader *>(first_block);
            free_header->PopFromFreeList();
            MemoryPoolHeader *next = current_pool->GetNext();
            MemoryPoolHeader *prev = current_pool->GetPrev();
            if (next != nullptr) {
                ASSERT(next->GetPrev() == current_pool);
                next->SetPrev(prev);
            } else {
                // This means that current pool is the last
                ASSERT(mempool_tail_ == current_pool);
                LOG_FREELIST_ALLOCATOR(DEBUG) << "VisitAndRemoveFreePools: Change pools tail pointer";
                mempool_tail_ = prev;
            }
            if (prev != nullptr) {
                ASSERT(prev->GetNext() == current_pool);
                prev->SetNext(next);
            } else {
                // This means that current pool is the first
                ASSERT(mempool_head_ == current_pool);
                LOG_FREELIST_ALLOCATOR(DEBUG) << "VisitAndRemoveFreePools: Change pools head pointer";
                mempool_head_ = next;
            }
            AllocConfigT::RemoveCrossingMapForMemory(current_pool, current_pool->GetSize());
            mem_visitor(current_pool, current_pool->GetSize());
        }
        current_pool = tmp;
    }
    segregated_list_.ReleaseFreeMemoryBlocks();
}

template <typename AllocConfigT, typename LockConfigT>
template <typename MemVisitor>
void FreeListAllocator<AllocConfigT, LockConfigT>::IterateOverObjectsInRange(const MemVisitor &mem_visitor,
                                                                             void *left_border, void *right_border)
{
    // NOTE: Current implementation doesn't look at PANDA_CROSSING_MAP_MANAGE_CROSSED_BORDER flag
    LOG_FREELIST_ALLOCATOR(DEBUG) << "FreeListAllocator::IterateOverObjectsInRange for range [" << std::hex
                                  << left_border << ", " << right_border << "]";
    ASSERT(ToUintPtr(right_border) >= ToUintPtr(left_border));
    // if the range crosses different allocators memory pools
    ASSERT(ToUintPtr(right_border) - ToUintPtr(left_border) ==
           (CrossingMapSingleton::GetCrossingMapGranularity() - 1U));
    ASSERT((ToUintPtr(right_border) & (~(CrossingMapSingleton::GetCrossingMapGranularity() - 1U))) ==
           (ToUintPtr(left_border) & (~(CrossingMapSingleton::GetCrossingMapGranularity() - 1U))));
    MemoryBlockHeader *first_memory_header = nullptr;
    {
        // Do this under lock because the pointer to the first object in CrossingMap can be changed during
        // CrossingMap call.
        os::memory::ReadLockHolder rlock(alloc_free_lock_);
        if (!AllocatedByFreeListAllocatorUnsafe(left_border) && !AllocatedByFreeListAllocatorUnsafe(right_border)) {
            LOG_FREELIST_ALLOCATOR(DEBUG) << "This memory range is not covered by this allocator";
            return;
        }
        void *obj_addr = AllocConfigT::FindFirstObjInCrossingMap(left_border, right_border);
        if (obj_addr == nullptr) {
            return;
        }
        ASSERT(AllocatedByFreeListAllocatorUnsafe(obj_addr));
        MemoryBlockHeader *memory_header = GetFreeListMemoryHeader(obj_addr);
        // Memory header is a pointer to an object which starts in this range or in the previous.
        // In the second case, this object may not crosses the border of the current range.
        // (but there is an object stored after it, which crosses the current range).
        ASSERT(ToUintPtr(memory_header->GetMemory()) <= ToUintPtr(right_border));
        first_memory_header = memory_header;
    }
    ASSERT(first_memory_header != nullptr);
    // Let's start iteration:
    MemoryBlockHeader *current_mem_header = first_memory_header;
    LOG_FREELIST_ALLOCATOR(DEBUG) << "FreeListAllocator::IterateOverObjectsInRange start iterating from obj with addr "
                                  << std::hex << first_memory_header->GetMemory();
    while (current_mem_header != nullptr) {
        // We don't lock allocator because we point to the used block which can't be changed
        // during the iteration in range.
        void *obj_addr = current_mem_header->GetMemory();
        if (ToUintPtr(obj_addr) > ToUintPtr(right_border)) {
            // Iteration over
            break;
        }
        LOG_FREELIST_ALLOCATOR(DEBUG)
            << "FreeListAllocator::IterateOverObjectsInRange found obj in this range with addr " << std::hex
            << obj_addr;
        mem_visitor(static_cast<ObjectHeader *>(obj_addr));
        {
            os::memory::ReadLockHolder rlock(alloc_free_lock_);
            current_mem_header = current_mem_header->GetNextUsedHeader();
        }
    }
    LOG_FREELIST_ALLOCATOR(DEBUG) << "FreeListAllocator::IterateOverObjectsInRange finished";
}

template <typename AllocConfigT, typename LockConfigT>
void FreeListAllocator<AllocConfigT, LockConfigT>::CoalesceMemoryBlocks(MemoryBlockHeader *first_block,
                                                                        MemoryBlockHeader *second_block)
{
    LOG_FREELIST_ALLOCATOR(DEBUG) << "CoalesceMemoryBlock: "
                                  << "first block = " << std::hex << first_block << " with size " << std::dec
                                  << first_block->GetSize() << " ; second block = " << std::hex << second_block
                                  << " with size " << std::dec << second_block->GetSize();
    ASSERT(first_block->GetNextHeader() == second_block);
    ASSERT(first_block->CanBeCoalescedWithNext() || second_block->CanBeCoalescedWithPrev());
    first_block->Initialize(first_block->GetSize() + second_block->GetSize() + sizeof(MemoryBlockHeader),
                            first_block->GetPrevHeader());
    if (second_block->IsLastBlockInPool()) {
        LOG_FREELIST_ALLOCATOR(DEBUG) << "CoalesceMemoryBlock: second_block was the last in a pool";
        first_block->SetLastBlockInPool();
    } else {
        first_block->GetNextHeader()->SetPrevHeader(first_block);
    }
}

template <typename AllocConfigT, typename LockConfigT>
freelist::MemoryBlockHeader *FreeListAllocator<AllocConfigT, LockConfigT>::SplitMemoryBlocks(
    MemoryBlockHeader *memory_block, size_t first_block_size)
{
    ASSERT(memory_block->GetSize() > (first_block_size + sizeof(MemoryBlockHeader)));
    ASSERT(!memory_block->IsUsed());
    auto second_block =
        static_cast<MemoryBlockHeader *>(ToVoidPtr(ToUintPtr(memory_block->GetMemory()) + first_block_size));
    size_t second_block_size = memory_block->GetSize() - first_block_size - sizeof(MemoryBlockHeader);
    second_block->Initialize(second_block_size, memory_block);
    if (memory_block->IsLastBlockInPool()) {
        second_block->SetLastBlockInPool();
    } else {
        second_block->GetNextHeader()->SetPrevHeader(second_block);
    }
    memory_block->Initialize(first_block_size, memory_block->GetPrevHeader());
    return second_block;
}

template <typename AllocConfigT, typename LockConfigT>
freelist::MemoryBlockHeader *FreeListAllocator<AllocConfigT, LockConfigT>::GetFreeListMemoryHeader(void *mem)
{
    ASSERT(mem != nullptr);
    auto memory_header = static_cast<MemoryBlockHeader *>(ToVoidPtr(ToUintPtr(mem) - sizeof(MemoryBlockHeader)));
    if (!memory_header->IsPaddingHeader()) {
        // We got correct header of this memory, just return it
        return memory_header;
    }
    // This is aligned memory with some free space before the memory pointer
    // The previous header must be a pointer to correct header of this memory block
    LOG_FREELIST_ALLOCATOR(DEBUG) << "It is a memory with padding at head";
    return memory_header->GetPrevHeader();
}

template <typename AllocConfigT, typename LockConfigT>
bool FreeListAllocator<AllocConfigT, LockConfigT>::AllocatedByFreeListAllocator(void *mem)
{
    os::memory::ReadLockHolder rlock(alloc_free_lock_);
    return AllocatedByFreeListAllocatorUnsafe(mem);
}

template <typename AllocConfigT, typename LockConfigT>
bool FreeListAllocator<AllocConfigT, LockConfigT>::AllocatedByFreeListAllocatorUnsafe(void *mem)
{
    MemoryPoolHeader *current_pool = mempool_head_;
    while (current_pool != nullptr) {
        // This assert means that we asked about memory inside MemoryPoolHeader
        ASSERT(!((ToUintPtr(current_pool->GetFirstMemoryHeader()) > ToUintPtr(mem)) &&
                 (ToUintPtr(current_pool) < ToUintPtr(mem))));
        if ((ToUintPtr(current_pool->GetFirstMemoryHeader()) < ToUintPtr(mem)) &&
            ((ToUintPtr(current_pool) + current_pool->GetSize()) > ToUintPtr(mem))) {
            return true;
        }
        current_pool = current_pool->GetNext();
    }
    return false;
}

template <typename AllocConfigT, typename LockConfigT>
freelist::FreeListHeader *FreeListAllocator<AllocConfigT, LockConfigT>::TryToCoalescing(
    MemoryBlockHeader *memory_header)
{
    ASSERT(memory_header != nullptr);
    LOG_FREELIST_ALLOCATOR(DEBUG) << "TryToCoalescing memory block";
    if (memory_header->CanBeCoalescedWithNext()) {
        ASSERT(!memory_header->GetNextHeader()->IsUsed());
        LOG_FREELIST_ALLOCATOR(DEBUG) << "Coalesce with next block";
        auto next_free_list = static_cast<FreeListHeader *>(memory_header->GetNextHeader());
        // Pop this free list element from the list
        next_free_list->PopFromFreeList();
        // Combine these two blocks together
        CoalesceMemoryBlocks(memory_header, static_cast<MemoryBlockHeader *>(next_free_list));
    }
    if (memory_header->CanBeCoalescedWithPrev()) {
        ASSERT(!memory_header->GetPrevHeader()->IsUsed());
        LOG_FREELIST_ALLOCATOR(DEBUG) << "Coalesce with prev block";
        auto prev_free_list = static_cast<FreeListHeader *>(memory_header->GetPrevHeader());
        // Pop this free list element from the list
        prev_free_list->PopFromFreeList();
        // Combine these two blocks together
        CoalesceMemoryBlocks(static_cast<MemoryBlockHeader *>(prev_free_list), memory_header);
        memory_header = static_cast<MemoryBlockHeader *>(prev_free_list);
    }
    return static_cast<FreeListHeader *>(memory_header);
}

template <typename AllocConfigT, typename LockConfigT>
void FreeListAllocator<AllocConfigT, LockConfigT>::AddToSegregatedList(FreeListHeader *free_list_element)
{
    LOG_FREELIST_ALLOCATOR(DEBUG) << "AddToSegregatedList: Add new block into segregated-list with size "
                                  << free_list_element->GetSize();
    segregated_list_.AddMemoryBlock(free_list_element);
}

template <typename AllocConfigT, typename LockConfigT>
freelist::MemoryBlockHeader *FreeListAllocator<AllocConfigT, LockConfigT>::GetFromSegregatedList(size_t size,
                                                                                                 Alignment align)
{
    LOG_FREELIST_ALLOCATOR(DEBUG) << "GetFromSegregatedList: Try to find memory for size " << size << " with alignment "
                                  << align;
    size_t aligned_size = size;
    if (align != FREELIST_DEFAULT_ALIGNMENT) {
        // Consider this:
        // (GetAlignmentInBytes(align) + sizeof(MemoryBlockHeader) - GetAlignmentInBytes(FREELIST_DEFAULT_ALIGNMENT))
        aligned_size += (GetAlignmentInBytes(align) + sizeof(MemoryBlockHeader));
    }
    FreeListHeader *mem_block = segregated_list_.FindMemoryBlock(aligned_size);
    if (mem_block != nullptr) {
        mem_block->PopFromFreeList();
        ASSERT((AlignUp(ToUintPtr(mem_block->GetMemory()), GetAlignmentInBytes(align)) -
                ToUintPtr(mem_block->GetMemory()) + size) <= mem_block->GetSize());
    }
    return static_cast<MemoryBlockHeader *>(mem_block);
}

template <typename AllocConfigT, typename LockConfigT>
ATTRIBUTE_NO_SANITIZE_ADDRESS void FreeListAllocator<AllocConfigT, LockConfigT>::MemoryPoolHeader::Initialize(
    size_t size, MemoryPoolHeader *prev, MemoryPoolHeader *next)
{
    LOG_FREELIST_ALLOCATOR(DEBUG) << "Init a new memory pool with size " << size << " with prev link = " << std::hex
                                  << prev << " and next link = " << next;
    ASAN_UNPOISON_MEMORY_REGION(this, sizeof(MemoryPoolHeader));
    size_ = size;
    prev_ = prev;
    next_ = next;
    ASAN_POISON_MEMORY_REGION(this, sizeof(MemoryPoolHeader));
}

template <typename AllocConfigT, typename LockConfigT>
void FreeListAllocator<AllocConfigT, LockConfigT>::SegregatedList::AddMemoryBlock(FreeListHeader *freelist_header)
{
    size_t size = freelist_header->GetSize();
    size_t index = GetIndex(size);
    if (SEGREGATED_LIST_FAST_INSERT) {
        free_memory_blocks_[index].InsertNext(freelist_header);
    } else {
        FreeListHeader *most_suitable = FindTheMostSuitableBlockInOrderedList(index, size);
        // The most suitable block with such must be equal to this size,
        // or the last with the bigger size in the ordered list,
        // or nullptr
        if (most_suitable == nullptr) {
            free_memory_blocks_[index].InsertNext(freelist_header);
        } else {
            most_suitable->InsertNext(freelist_header);
        }
    }
}

template <typename AllocConfigT, typename LockConfigT>
freelist::FreeListHeader *FreeListAllocator<AllocConfigT, LockConfigT>::SegregatedList::FindMemoryBlock(size_t size)
{
    size_t index = GetIndex(size);
    FreeListHeader *head = GetFirstBlock(index);
    FreeListHeader *suitable_block = nullptr;
    if (head != nullptr) {
        // We have some memory in this range. Try to find suitable block.
        if (SEGREGATED_LIST_FAST_INSERT) {
            // We don't have any order in inserting blocks,
            // and we need to iterate over the whole list
            FreeListHeader *current = head;
            while (current != nullptr) {
                if (current->GetSize() < size) {
                    current = current->GetNextFree();
                    continue;
                }

                if (SEGREGATED_LIST_FAST_EXTRACT) {
                    suitable_block = current;
                    break;
                }

                if (suitable_block != nullptr) {
                    suitable_block = current;
                } else {
                    if (suitable_block->GetSize() > current->GetSize()) {
                        suitable_block = current;
                    }
                }

                if (suitable_block->GetSize() == size) {
                    break;
                }
                current = current->GetNextFree();
            }
        } else {
            // All blocks in this list are in descending order.
            // We can check the first one to determine if we have a block
            // with this size or not.
            if (head->GetSize() >= size) {
                if (SEGREGATED_LIST_FAST_EXTRACT) {
                    // Just get the first element
                    suitable_block = head;
                } else {
                    // Try to find the mist suitable memory for this size.
                    suitable_block = FindTheMostSuitableBlockInOrderedList(index, size);
                }
            }
        }
    }

    if (suitable_block == nullptr) {
        // We didn't find the block in the head list. Try to find block in other lists.
        index++;
        while (index < SEGREGATED_LIST_SIZE) {
            if (GetFirstBlock(index) != nullptr) {
                if (SEGREGATED_LIST_FAST_INSERT || SEGREGATED_LIST_FAST_EXTRACT) {
                    // Just get the first one
                    suitable_block = GetFirstBlock(index);
                } else {
                    suitable_block = FindTheMostSuitableBlockInOrderedList(index, size);
                }
                break;
            }
            index++;
        }
    }

    if (suitable_block != nullptr) {
        ASSERT(suitable_block->GetSize() >= size);
    }

    return suitable_block;
}

template <typename AllocConfigT, typename LockConfigT>
void FreeListAllocator<AllocConfigT, LockConfigT>::SegregatedList::ReleaseFreeMemoryBlocks()
{
    for (size_t index = 0; index < SEGREGATED_LIST_SIZE; index++) {
        FreeListHeader *current = GetFirstBlock(index);
        while (current != nullptr) {
            size_t block_size = current->GetSize();
            // Start address from which we can release pages
            uintptr_t start_addr = AlignUp(ToUintPtr(current) + sizeof(FreeListHeader), os::mem::GetPageSize());
            // End address before which we can release pages
            uintptr_t end_addr =
                os::mem::AlignDownToPageSize(ToUintPtr(current) + sizeof(MemoryBlockHeader) + block_size);
            if (start_addr < end_addr) {
                os::mem::ReleasePages(start_addr, end_addr);
            }
            current = current->GetNextFree();
        }
    }
}

template <typename AllocConfigT, typename LockConfigT>
freelist::FreeListHeader *
FreeListAllocator<AllocConfigT, LockConfigT>::SegregatedList::FindTheMostSuitableBlockInOrderedList(size_t index,
                                                                                                    size_t size)
{
    static_assert(!SEGREGATED_LIST_FAST_INSERT);
    FreeListHeader *current = GetFirstBlock(index);
    if (current == nullptr) {
        return nullptr;
    }
    size_t current_size = current->GetSize();
    if (current_size < size) {
        return nullptr;
    }
    while (current_size != size) {
        FreeListHeader *next = current->GetNextFree();
        if (next == nullptr) {
            // the current free list header is the last in the list.
            break;
        }
        size_t next_size = next->GetSize();
        if (next_size < size) {
            // the next free list header is less than size,
            // so we don't need to iterate anymore.
            break;
        }
        current = next;
        current_size = next_size;
    }
    return current;
}

template <typename AllocConfigT, typename LockConfigT>
size_t FreeListAllocator<AllocConfigT, LockConfigT>::SegregatedList::GetIndex(size_t size)
{
    ASSERT(size >= FREELIST_ALLOCATOR_MIN_SIZE);
    size_t index = (size - FREELIST_ALLOCATOR_MIN_SIZE) / SEGREGATED_LIST_FREE_BLOCK_RANGE;
    return index < SEGREGATED_LIST_SIZE ? index : SEGREGATED_LIST_SIZE - 1;
}

template <typename AllocConfigT, typename LockConfigT>
bool FreeListAllocator<AllocConfigT, LockConfigT>::ContainObject(const ObjectHeader *obj)
{
    return AllocatedByFreeListAllocatorUnsafe(const_cast<ObjectHeader *>(obj));
}

template <typename AllocConfigT, typename LockConfigT>
bool FreeListAllocator<AllocConfigT, LockConfigT>::IsLive(const ObjectHeader *obj)
{
    ASSERT(ContainObject(obj));
    void *obj_mem = static_cast<void *>(const_cast<ObjectHeader *>(obj));
    // Get start address of pool via PoolManager for input object
    // for avoid iteration over all pools in allocator
    auto mem_pool_header =
        static_cast<MemoryPoolHeader *>(PoolManager::GetMmapMemPool()->GetStartAddrPoolForAddr(obj_mem));
    ASSERT(PoolManager::GetMmapMemPool()
               ->GetAllocatorInfoForAddr(static_cast<void *>(mem_pool_header))
               .GetAllocatorHeaderAddr()  // CODECHECK-NOLINT(C_RULE_ID_INDENT_CHECK)
           == static_cast<const void *>(this));
    MemoryBlockHeader *current_mem_header = mem_pool_header->GetFirstMemoryHeader();
    while (current_mem_header != nullptr) {
        if (current_mem_header->IsUsed()) {
            if (current_mem_header->GetMemory() == obj_mem) {
                return true;
            }
        }
        current_mem_header = current_mem_header->GetNextUsedHeader();
    }
    return false;
}

#undef LOG_FREELIST_ALLOCATOR

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_FREELIST_ALLOCATOR_INL_H_
