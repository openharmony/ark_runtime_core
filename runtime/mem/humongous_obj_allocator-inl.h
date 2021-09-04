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

#ifndef PANDA_RUNTIME_MEM_HUMONGOUS_OBJ_ALLOCATOR_INL_H_
#define PANDA_RUNTIME_MEM_HUMONGOUS_OBJ_ALLOCATOR_INL_H_

#include "runtime/mem/alloc_config.h"
#include "runtime/mem/humongous_obj_allocator.h"
#include "runtime/mem/object_helpers.h"

namespace panda::mem {

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_HUMONGOUS_OBJ_ALLOCATOR(level) LOG(level, ALLOC) << "HumongousObjAllocator: "

template <typename AllocConfigT, typename LockConfigT>
HumongousObjAllocator<AllocConfigT, LockConfigT>::HumongousObjAllocator(MemStatsType *mem_stats,
                                                                        SpaceType type_allocation)
    : type_allocation_(type_allocation), mem_stats_(mem_stats)
{
    LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "Initializing HumongousObjAllocator";
    LOG_HUMONGOUS_OBJ_ALLOCATOR(INFO) << "Initializing HumongousObjAllocator finished";
}

template <typename AllocConfigT, typename LockConfigT>
HumongousObjAllocator<AllocConfigT, LockConfigT>::~HumongousObjAllocator()
{
    LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "Destroying HumongousObjAllocator";
    LOG_HUMONGOUS_OBJ_ALLOCATOR(INFO) << "Destroying HumongousObjAllocator finished";
}

template <typename AllocConfigT, typename LockConfigT>
void *HumongousObjAllocator<AllocConfigT, LockConfigT>::Alloc(const size_t size, const Alignment align)
{
    os::memory::WriteLockHolder wlock(alloc_free_lock_);
    LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "Try to allocate memory with size " << size;

    // Check that we can get a memory header for the memory pointer by using PAGE_SIZE_MASK mask
    if (UNLIKELY(PAGE_SIZE <= sizeof(MemoryPoolHeader) + GetAlignmentInBytes(align))) {
        ASSERT(PAGE_SIZE > sizeof(MemoryPoolHeader) + GetAlignmentInBytes(align));
        LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "The align is too big for this allocator. Return nullptr.";
        return nullptr;
    }

    // We can save about sizeof(MemoryPoolHeader) / 2 bytes here
    // (BTW, it is not so much for MB allocations)
    size_t aligned_size = size + sizeof(MemoryPoolHeader) + GetAlignmentInBytes(align);

    void *mem = nullptr;

    if (UNLIKELY(aligned_size > HUMONGOUS_OBJ_ALLOCATOR_MAX_SIZE)) {
        // the size is too big
        LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "The size is too big for this allocator. Return nullptr.";
        return nullptr;
    }

    // First try to find suitable block in Reserved pools
    MemoryPoolHeader *mem_header = reserved_pools_list_.FindSuitablePool(aligned_size);
    if (mem_header != nullptr) {
        LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "Find reserved memory block with size " << mem_header->GetPoolSize();
        reserved_pools_list_.Pop(mem_header);
        mem_header->Alloc(size, align);
        mem = mem_header->GetMemory();
    } else {
        mem_header = free_pools_list_.FindSuitablePool(aligned_size);
        if (mem_header != nullptr) {
            LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "Find free memory block with size " << mem_header->GetPoolSize();
            free_pools_list_.Pop(mem_header);
            mem_header->Alloc(size, align);
            mem = mem_header->GetMemory();
        } else {
            LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "Can't find memory for this size";
            return nullptr;
        }
    }
    occupied_pools_list_.Insert(mem_header);
    LOG_HUMONGOUS_OBJ_ALLOCATOR(INFO) << "Allocated memory at addr " << std::hex << mem;
    AllocConfigT::OnAlloc(mem_header->GetPoolSize(), type_allocation_, mem_stats_);
    ASAN_UNPOISON_MEMORY_REGION(mem, size);
    AllocConfigT::MemoryInit(mem, size);
    ReleaseUnusedPagesOnAlloc(mem_header, size);
    return mem;
}

template <typename AllocConfigT, typename LockConfigT>
void HumongousObjAllocator<AllocConfigT, LockConfigT>::Free(void *mem)
{
    os::memory::WriteLockHolder wlock(alloc_free_lock_);
    FreeUnsafe(mem);
}

template <typename AllocConfigT, typename LockConfigT>
void HumongousObjAllocator<AllocConfigT, LockConfigT>::FreeUnsafe(void *mem)
{
    if (UNLIKELY(mem == nullptr)) {
        LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "Try to free memory at invalid addr 0";
        return;
    }
    LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "Try to free memory at addr " << std::hex << mem;
#ifndef NDEBUG
    if (!AllocatedByHumongousObjAllocatorUnsafe(mem)) {
        LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "Try to free memory not from this allocator";
        return;
    }
#endif  // !NDEBUG

    // Each memory pool is PAGE_SIZE aligned, so to get a header we need just to align a pointer
    auto mem_header = static_cast<MemoryPoolHeader *>(ToVoidPtr(ToUintPtr(mem) & PAGE_SIZE_MASK));
    LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "It is a MemoryPoolHeader with addr " << std::hex << mem_header
                                       << " and size " << std::dec << mem_header->GetPoolSize();
    occupied_pools_list_.Pop(mem_header);
    AllocConfigT::OnFree(mem_header->GetPoolSize(), type_allocation_, mem_stats_);
    ASAN_POISON_MEMORY_REGION(mem_header, mem_header->GetPoolSize());
    InsertPool(mem_header);
    LOG_HUMONGOUS_OBJ_ALLOCATOR(INFO) << "Freed memory at addr " << std::hex << mem;
}

template <typename AllocConfigT, typename LockConfigT>
void HumongousObjAllocator<AllocConfigT, LockConfigT>::Collect(const GCObjectVisitor &death_checker_fn)
{
    LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "Collecting started";
    IterateOverObjects([&](ObjectHeader *object_header) {
        if (death_checker_fn(object_header) == ObjectStatus::DEAD_OBJECT) {
            LOG(DEBUG, GC) << "DELETE OBJECT " << GetDebugInfoAboutObject(object_header);
            FreeUnsafe(object_header);
        }
    });
    LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "Collecting finished";
}

template <typename AllocConfigT, typename LockConfigT>
template <typename ObjectVisitor>
void HumongousObjAllocator<AllocConfigT, LockConfigT>::IterateOverObjects(const ObjectVisitor &object_visitor)
{
    LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "Iterating over objects started";
    MemoryPoolHeader *current_pool = nullptr;
    {
        os::memory::ReadLockHolder rlock(alloc_free_lock_);
        current_pool = occupied_pools_list_.GetListHead();
    }
    while (current_pool != nullptr) {
        os::memory::WriteLockHolder wlock(alloc_free_lock_);
        LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "  check pool at addr " << std::hex << current_pool;
        MemoryPoolHeader *next = current_pool->GetNext();
        object_visitor(static_cast<ObjectHeader *>(current_pool->GetMemory()));
        current_pool = next;
    }
    LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "Iterating over objects finished";
}

template <typename AllocConfigT, typename LockConfigT>
bool HumongousObjAllocator<AllocConfigT, LockConfigT>::AddMemoryPool(void *mem, size_t size)
{
    os::memory::WriteLockHolder wlock(alloc_free_lock_);
    ASSERT(mem != nullptr);
    LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "Add memory pool to HumongousObjAllocator from  " << std::hex << mem
                                       << " with size " << std::dec << size;
    if (AlignUp(ToUintPtr(mem), PAGE_SIZE) != ToUintPtr(mem)) {
        return false;
    }
    auto mempool_header = static_cast<MemoryPoolHeader *>(mem);
    mempool_header->Initialize(size, nullptr, nullptr);
    InsertPool(mempool_header);
    ASAN_POISON_MEMORY_REGION(mem, size);
    return true;
}

template <typename AllocConfigT, typename LockConfigT>
void HumongousObjAllocator<AllocConfigT, LockConfigT>::ReleaseUnusedPagesOnAlloc(MemoryPoolHeader *memory_pool,
                                                                                 size_t alloc_size)
{
    ASSERT(memory_pool != nullptr);
    uintptr_t alloc_addr = ToUintPtr(memory_pool->GetMemory());
    uintptr_t pool_addr = ToUintPtr(memory_pool);
    size_t pool_size = memory_pool->GetPoolSize();
    uintptr_t first_free_page = AlignUp(alloc_addr + alloc_size, os::mem::GetPageSize());
    uintptr_t end_of_last_free_page = os::mem::AlignDownToPageSize(pool_addr + pool_size);
    if (first_free_page < end_of_last_free_page) {
        os::mem::ReleasePages(first_free_page, end_of_last_free_page);
    }
}

template <typename AllocConfigT, typename LockConfigT>
void HumongousObjAllocator<AllocConfigT, LockConfigT>::InsertPool(MemoryPoolHeader *header)
{
    LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "Try to insert pool with size " << header->GetPoolSize()
                                       << " in Reserved memory";
    // Try to insert it into ReservedMemoryPools
    MemoryPoolHeader *mem_header = reserved_pools_list_.TryToInsert(header);
    if (mem_header == nullptr) {
        LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "Successfully inserted in Reserved memory";
        // We successfully insert header into ReservedMemoryPools
        return;
    }
    // We have a crowded out pool or the "header" argument in mem_header
    // Insert it into free_pools
    LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "Couldn't insert into Reserved memory. Insert in free pools";
    free_pools_list_.Insert(mem_header);
}

template <typename AllocConfigT, typename LockConfigT>
template <typename MemVisitor>
void HumongousObjAllocator<AllocConfigT, LockConfigT>::VisitAndRemoveAllPools(const MemVisitor &mem_visitor)
{
    // We call this method and return pools to the system.
    // Therefore, delete all objects to clear all external dependences
    LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "Clear all objects inside the allocator";
    os::memory::WriteLockHolder wlock(alloc_free_lock_);
    occupied_pools_list_.IterateAndPopOverPools(mem_visitor);
    reserved_pools_list_.IterateAndPopOverPools(mem_visitor);
    free_pools_list_.IterateAndPopOverPools(mem_visitor);
}

template <typename AllocConfigT, typename LockConfigT>
template <typename MemVisitor>
void HumongousObjAllocator<AllocConfigT, LockConfigT>::VisitAndRemoveFreePools(const MemVisitor &mem_visitor)
{
    os::memory::WriteLockHolder wlock(alloc_free_lock_);
    free_pools_list_.IterateAndPopOverPools(mem_visitor);
}

template <typename AllocConfigT, typename LockConfigT>
template <typename MemVisitor>
void HumongousObjAllocator<AllocConfigT, LockConfigT>::IterateOverObjectsInRange(const MemVisitor &mem_visitor,
                                                                                 void *left_border, void *right_border)
{
    // NOTE: Current implementation doesn't look at PANDA_CROSSING_MAP_MANAGE_CROSSED_BORDER flag
    LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "HumongousObjAllocator::IterateOverObjectsInRange for range [" << std::hex
                                       << left_border << ", " << right_border << "]";
    ASSERT(ToUintPtr(right_border) >= ToUintPtr(left_border));
    // if the range crosses different allocators memory pools
    ASSERT(ToUintPtr(right_border) - ToUintPtr(left_border) ==
           (CrossingMapSingleton::GetCrossingMapGranularity() - 1U));
    ASSERT((ToUintPtr(right_border) & (~(CrossingMapSingleton::GetCrossingMapGranularity() - 1U))) ==
           (ToUintPtr(left_border) & (~(CrossingMapSingleton::GetCrossingMapGranularity() - 1U))));

    // Try to find a pool with this range
    MemoryPoolHeader *discovered_pool = nullptr;
    MemoryPoolHeader *current_pool = nullptr;
    {
        os::memory::ReadLockHolder rlock(alloc_free_lock_);
        current_pool = occupied_pools_list_.GetListHead();
    }
    while (current_pool != nullptr) {
        // Use current pool here because it is page aligned
        uintptr_t current_pool_start = ToUintPtr(current_pool);
        uintptr_t current_pool_end = ToUintPtr(current_pool->GetMemory()) + current_pool->GetPoolSize();
        if (current_pool_start <= ToUintPtr(left_border)) {
            // Check that this range is located in the same pool
            if (current_pool_end >= ToUintPtr(right_border)) {
                discovered_pool = current_pool;
                break;
            }
        }
        {
            os::memory::ReadLockHolder rlock(alloc_free_lock_);
            current_pool = current_pool->GetNext();
        }
    }

    if (discovered_pool != nullptr) {
        LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG)
            << "HumongousObjAllocator: It is a MemoryPoolHeader with addr " << std::hex << discovered_pool
            << " and size " << std::dec << discovered_pool->GetPoolSize();
        mem_visitor(static_cast<ObjectHeader *>(discovered_pool->GetMemory()));
    } else {
        LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG)
            << "HumongousObjAllocator This memory range is not covered by this allocator";
    }
    LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "HumongousObjAllocator::IterateOverObjectsInRange finished";
}

template <typename AllocConfigT, typename LockConfigT>
bool HumongousObjAllocator<AllocConfigT, LockConfigT>::AllocatedByHumongousObjAllocator(void *mem)
{
    os::memory::ReadLockHolder rlock(alloc_free_lock_);
    return AllocatedByHumongousObjAllocatorUnsafe(mem);
}

template <typename AllocConfigT, typename LockConfigT>
bool HumongousObjAllocator<AllocConfigT, LockConfigT>::AllocatedByHumongousObjAllocatorUnsafe(void *mem)
{
    MemoryPoolHeader *current_pool = occupied_pools_list_.GetListHead();
    while (current_pool != nullptr) {
        if (current_pool->GetMemory() == mem) {
            return true;
        }
        current_pool = current_pool->GetNext();
    }
    return false;
}

template <typename AllocConfigT, typename LockConfigT>
ATTRIBUTE_NO_SANITIZE_ADDRESS void HumongousObjAllocator<AllocConfigT, LockConfigT>::MemoryPoolHeader::Initialize(
    size_t size, MemoryPoolHeader *prev, MemoryPoolHeader *next)
{
    ASAN_UNPOISON_MEMORY_REGION(this, sizeof(MemoryPoolHeader));
    pool_size_ = size;
    prev_ = prev;
    next_ = next;
    mem_addr_ = nullptr;
    ASAN_POISON_MEMORY_REGION(this, sizeof(MemoryPoolHeader));
}

template <typename AllocConfigT, typename LockConfigT>
ATTRIBUTE_NO_SANITIZE_ADDRESS void HumongousObjAllocator<AllocConfigT, LockConfigT>::MemoryPoolHeader::Alloc(
    size_t size, Alignment align)
{
    (void)size;
    ASAN_UNPOISON_MEMORY_REGION(this, sizeof(MemoryPoolHeader));
    mem_addr_ = ToVoidPtr(AlignUp(ToUintPtr(this) + sizeof(MemoryPoolHeader), GetAlignmentInBytes(align)));
    ASSERT(ToUintPtr(mem_addr_) + size <= ToUintPtr(this) + pool_size_);
    ASAN_POISON_MEMORY_REGION(this, sizeof(MemoryPoolHeader));
}

template <typename AllocConfigT, typename LockConfigT>
ATTRIBUTE_NO_SANITIZE_ADDRESS void HumongousObjAllocator<AllocConfigT, LockConfigT>::MemoryPoolHeader::PopHeader()
{
    ASAN_UNPOISON_MEMORY_REGION(this, sizeof(MemoryPoolHeader));
    if (prev_ != nullptr) {
        ASAN_UNPOISON_MEMORY_REGION(prev_, sizeof(MemoryPoolHeader));
        prev_->SetNext(next_);
        ASAN_POISON_MEMORY_REGION(prev_, sizeof(MemoryPoolHeader));
    }
    if (next_ != nullptr) {
        ASAN_UNPOISON_MEMORY_REGION(next_, sizeof(MemoryPoolHeader));
        next_->SetPrev(prev_);
        ASAN_POISON_MEMORY_REGION(next_, sizeof(MemoryPoolHeader));
    }
    next_ = nullptr;
    prev_ = nullptr;
    ASAN_POISON_MEMORY_REGION(this, sizeof(MemoryPoolHeader));
}

template <typename AllocConfigT, typename LockConfigT>
void HumongousObjAllocator<AllocConfigT, LockConfigT>::MemoryPoolList::Pop(MemoryPoolHeader *pool)
{
    LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "Pop a pool with addr " << std::hex << pool << " from the pool list";
    ASSERT(IsInThisList(pool));
    if (head_ == pool) {
        head_ = pool->GetNext();
        LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "It was a pointer to list head. Change head to " << std::hex << head_;
    }
    pool->PopHeader();
}

template <typename AllocConfigT, typename LockConfigT>
void HumongousObjAllocator<AllocConfigT, LockConfigT>::MemoryPoolList::Insert(MemoryPoolHeader *pool)
{
    LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "Insert a pool with addr " << std::hex << pool << " into the pool list";
    if (head_ != nullptr) {
        head_->SetPrev(pool);
    } else {
        LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "The head was not initialized. Set it up.";
    }
    pool->SetNext(head_);
    pool->SetPrev(nullptr);
    head_ = pool;
}

template <typename AllocConfigT, typename LockConfigT>
typename HumongousObjAllocator<AllocConfigT, LockConfigT>::MemoryPoolHeader *
HumongousObjAllocator<AllocConfigT, LockConfigT>::MemoryPoolList::FindSuitablePool(size_t size)
{
    LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "Try to find suitable pool for memory with size " << size;
    MemoryPoolHeader *cur_pool = head_;
    while (cur_pool != nullptr) {
        if (cur_pool->GetPoolSize() >= size) {
            break;
        }
        cur_pool = cur_pool->GetNext();
    }
    LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "Found a pool with addr " << std::hex << cur_pool;
    return cur_pool;
}

template <typename AllocConfigT, typename LockConfigT>
bool HumongousObjAllocator<AllocConfigT, LockConfigT>::MemoryPoolList::IsInThisList(MemoryPoolHeader *pool)
{
    MemoryPoolHeader *cur_pool = head_;
    while (cur_pool != nullptr) {
        if (cur_pool == pool) {
            break;
        }
        cur_pool = cur_pool->GetNext();
    }
    return cur_pool != nullptr;
}

template <typename AllocConfigT, typename LockConfigT>
template <typename MemVisitor>
void HumongousObjAllocator<AllocConfigT, LockConfigT>::MemoryPoolList::IterateAndPopOverPools(
    const MemVisitor &mem_visitor)
{
    MemoryPoolHeader *current_pool = head_;
    while (current_pool != nullptr) {
        MemoryPoolHeader *tmp = current_pool->GetNext();
        this->Pop(current_pool);
        mem_visitor(current_pool, current_pool->GetPoolSize());
        current_pool = tmp;
    }
}

template <typename AllocConfigT, typename LockConfigT>
typename HumongousObjAllocator<AllocConfigT, LockConfigT>::MemoryPoolHeader *
HumongousObjAllocator<AllocConfigT, LockConfigT>::ReservedMemoryPools::TryToInsert(MemoryPoolHeader *pool)
{
    LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "Try to insert a pool in Reserved memory with addr " << std::hex << pool;
    if (pool->GetPoolSize() > MAX_POOL_SIZE) {
        // This pool is too big for inserting in Reserved
        LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "It is too big for Reserved memory";
        return pool;
    }
    if (elements_count_ < MAX_POOLS_AMOUNT) {
        // We can insert the memory pool to Reserved
        SortedInsert(pool);
        elements_count_++;
        LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "We don't have max amount of elements in Reserved list. Just insert.";
        return nullptr;
    }
    // We have the max amount of elements in the Reserved pools list
    // Try to swap the smallest pool (which is the first because it is ordered list)
    LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "We have max amount of elements in Reserved list.";
    MemoryPoolHeader *smallest_pool = this->GetListHead();
    if (smallest_pool == nullptr) {
        // It is the only variant when smallest_pool can be equal to nullptr.
        ASSERT(MAX_POOLS_AMOUNT == 0);
        LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "MAX_POOLS_AMOUNT for Reserved list is equal to zero. Do nothing";
        return pool;
    }
    ASSERT(smallest_pool != nullptr);
    if (smallest_pool->GetPoolSize() >= pool->GetPoolSize()) {
        LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "The pool is too small. Do not insert it";
        return pool;
    }
    // Just pop this element from the list. Do not update elements_count_ value
    MemoryPoolList::Pop(smallest_pool);
    SortedInsert(pool);
    LOG_HUMONGOUS_OBJ_ALLOCATOR(DEBUG) << "Swap the smallest element in Reserved list with addr " << std::hex
                                       << smallest_pool;
    return smallest_pool;
}

template <typename AllocConfigT, typename LockConfigT>
void HumongousObjAllocator<AllocConfigT, LockConfigT>::ReservedMemoryPools::SortedInsert(MemoryPoolHeader *pool)
{
    size_t pool_size = pool->GetPoolSize();
    MemoryPoolHeader *list_head = this->GetListHead();
    if (list_head == nullptr) {
        this->Insert(pool);
        return;
    }
    if (list_head->GetPoolSize() >= pool_size) {
        // Do this comparison to not update head_ in this method
        this->Insert(pool);
        return;
    }
    MemoryPoolHeader *cur = list_head;
    while (cur != nullptr) {
        if (cur->GetPoolSize() >= pool_size) {
            pool->SetNext(cur);
            pool->SetPrev(cur->GetPrev());
            cur->GetPrev()->SetNext(pool);
            cur->SetPrev(pool);
            return;
        }
        MemoryPoolHeader *next = cur->GetNext();
        if (next == nullptr) {
            cur->SetNext(pool);
            pool->SetNext(nullptr);
            pool->SetPrev(cur);
            return;
        }
        cur = next;
    }
}

template <typename AllocConfigT, typename LockConfigT>
bool HumongousObjAllocator<AllocConfigT, LockConfigT>::ContainObject(const ObjectHeader *obj)
{
    return AllocatedByHumongousObjAllocatorUnsafe(const_cast<ObjectHeader *>(obj));
}

template <typename AllocConfigT, typename LockConfigT>
bool HumongousObjAllocator<AllocConfigT, LockConfigT>::IsLive(const ObjectHeader *obj)
{
    ASSERT(ContainObject(obj));
    auto *mem_header = static_cast<MemoryPoolHeader *>(ToVoidPtr(ToUintPtr(obj) & PAGE_SIZE_MASK));
    ASSERT(PoolManager::GetMmapMemPool()->GetStartAddrPoolForAddr(
               // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_PARAM_ALIGN)
               static_cast<void *>(const_cast<ObjectHeader *>(obj))) == static_cast<void *>(mem_header));
    return mem_header->GetMemory() == static_cast<void *>(const_cast<ObjectHeader *>(obj));
}

#undef LOG_HUMONGOUS_OBJ_ALLOCATOR

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_HUMONGOUS_OBJ_ALLOCATOR_INL_H_
