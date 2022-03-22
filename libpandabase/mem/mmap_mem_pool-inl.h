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

#ifndef PANDA_LIBPANDABASE_MEM_MMAP_MEM_POOL_INL_H_
#define PANDA_LIBPANDABASE_MEM_MMAP_MEM_POOL_INL_H_

#include "mmap_mem_pool.h"
#include "mem.h"
#include "os/mem.h"
#include "utils/logger.h"
#include "mem/arena.h"
#include "mem/mem_config.h"
#include "utils/asan_interface.h"

namespace panda {

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_MMAP_MEM_POOL(level) LOG(level, MEMORYPOOL) << "MmapMemPool: "

inline Pool MmapPoolMap::PopFreePool(size_t size)
{
    auto element = free_pools_.lower_bound(size);
    if (element == free_pools_.end()) {
        return NULLPOOL;
    }
    auto mmap_pool = element->second;
    ASSERT(!mmap_pool->IsUsed(free_pools_.end()));
    auto element_size = element->first;
    ASSERT(element_size == mmap_pool->GetSize());
    auto element_mem = mmap_pool->GetMem();

    mmap_pool->SetFreePoolsIter(free_pools_.end());
    Pool pool(size, element_mem);
    free_pools_.erase(element);
    if (size < element_size) {
        Pool new_pool(element_size - size, ToVoidPtr(ToUintPtr(element_mem) + size));
        mmap_pool->SetSize(size);
        // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
        auto new_mmap_pool = new MmapPool(new_pool, free_pools_.end());
        pool_map_.insert(std::pair<void *, MmapPool *>(new_pool.GetMem(), new_mmap_pool));
        auto new_free_pools_iter = free_pools_.insert(std::pair<size_t, MmapPool *>(new_pool.GetSize(), new_mmap_pool));
        new_mmap_pool->SetFreePoolsIter(new_free_pools_iter);
    }
    return pool;
}

inline void MmapPoolMap::PushFreePool(Pool pool)
{
    auto mmap_pool_element = pool_map_.find(pool.GetMem());
    if (UNLIKELY(mmap_pool_element == pool_map_.end())) {
        LOG_MMAP_MEM_POOL(FATAL) << "can't find mmap pool in the pool map when PushFreePool";
    }

    auto mmap_pool = mmap_pool_element->second;
    ASSERT(mmap_pool->IsUsed(free_pools_.end()));

    auto prev_pool = mmap_pool_element != pool_map_.begin() ? prev(mmap_pool_element, 1)->second : nullptr;
    if (prev_pool != nullptr && !prev_pool->IsUsed(free_pools_.end())) {
        ASSERT(ToUintPtr(prev_pool->GetMem()) + prev_pool->GetSize() == ToUintPtr(mmap_pool->GetMem()));
        free_pools_.erase(prev_pool->GetFreePoolsIter());
        prev_pool->SetSize(prev_pool->GetSize() + mmap_pool->GetSize());
        delete mmap_pool;
        pool_map_.erase(mmap_pool_element--);
        mmap_pool = prev_pool;
    }

    auto next_pool = mmap_pool_element != prev(pool_map_.end(), 1) ? next(mmap_pool_element, 1)->second : nullptr;
    if (next_pool != nullptr && !next_pool->IsUsed(free_pools_.end())) {
        ASSERT(ToUintPtr(mmap_pool->GetMem()) + mmap_pool->GetSize() == ToUintPtr(next_pool->GetMem()));
        free_pools_.erase(next_pool->GetFreePoolsIter());
        mmap_pool->SetSize(next_pool->GetSize() + mmap_pool->GetSize());
        delete next_pool;
        pool_map_.erase(++mmap_pool_element);
    }

    auto res = free_pools_.insert(std::pair<size_t, MmapPool *>(mmap_pool->GetSize(), mmap_pool));
    mmap_pool->SetFreePoolsIter(res);
}

inline void MmapPoolMap::AddNewPool(Pool pool)
{
    // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
    auto new_mmap_pool = new MmapPool(pool, free_pools_.end());
    pool_map_.insert(std::pair<void *, MmapPool *>(pool.GetMem(), new_mmap_pool));
}

inline size_t MmapPoolMap::GetAllSize() const
{
    size_t bytes = 0;
    for (const auto &pool : free_pools_) {
        bytes += pool.first;
    }
    return bytes;
}

inline MmapMemPool::MmapMemPool() : MemPool("MmapMemPool")
{
    ASSERT(static_cast<uint64_t>(mem::MemConfig::GetObjectPoolSize()) <= PANDA_MAX_HEAP_SIZE);
    uint64_t object_space_size = mem::MemConfig::GetObjectPoolSize();
    if (PANDA_MAX_HEAP_SIZE < object_space_size) {
        LOG_MMAP_MEM_POOL(FATAL) << "The memory limits is too high. We can't allocate so much memory from the system";
    }
    ASSERT(object_space_size <= PANDA_MAX_HEAP_SIZE);
#if defined(PANDA_USE_32_BIT_POINTER) && !defined(PANDA_TARGET_WINDOWS)
    void *mem = panda::os::mem::MapRWAnonymousFixedRaw(ToVoidPtr(PANDA_32BITS_HEAP_START_ADDRESS), object_space_size);
    ASSERT((ToUintPtr(mem) == PANDA_32BITS_HEAP_START_ADDRESS) || (object_space_size == 0));
    ASSERT(ToUintPtr(mem) + object_space_size <= PANDA_32BITS_HEAP_END_OBJECTS_ADDRESS);
#else
    // We should get aligned to PANDA_POOL_ALIGNMENT_IN_BYTES size
    void *mem = panda::os::mem::MapRWAnonymousWithAlignmentRaw(object_space_size, PANDA_POOL_ALIGNMENT_IN_BYTES);
#endif
    LOG_IF(((mem == nullptr) && (object_space_size != 0)), FATAL, MEMORYPOOL)
        << "MmapMemPool: couldn't mmap " << object_space_size << " bytes of memory for the system";
    ASSERT(AlignUp(ToUintPtr(mem), PANDA_POOL_ALIGNMENT_IN_BYTES) == ToUintPtr(mem));
    min_object_memory_addr_ = ToUintPtr(mem);
    mmaped_object_memory_size_ = object_space_size;
    common_space_.Initialize(min_object_memory_addr_, object_space_size);
    code_space_max_size_ = mem::MemConfig::GetCodePoolSize();
    compiler_space_max_size_ = mem::MemConfig::GetCompilerPoolSize();
    internal_space_max_size_ = mem::MemConfig::GetInternalPoolSize();
    LOG_MMAP_MEM_POOL(DEBUG) << "Successfully initialized MMapMemPool. Object memory start from addr "
                             << ToVoidPtr(min_object_memory_addr_) << " Preallocated size is equal to "
                             << object_space_size;
}

inline MmapMemPool::~MmapMemPool()
{
    for (auto i : non_object_mmaped_pools_) {
        Pool pool = std::get<0>(i.second);
        FreeRawMemImpl(pool.GetMem(), pool.GetSize());
    }
    void *mmaped_mem_addr = ToVoidPtr(min_object_memory_addr_);
    if (mmaped_mem_addr == nullptr) {
        ASSERT(mmaped_object_memory_size_ == 0);
        return;
    }
    if (auto unmap_res = panda::os::mem::UnmapRaw(mmaped_mem_addr, mmaped_object_memory_size_)) {
        LOG_MMAP_MEM_POOL(FATAL) << "Destructor unnmap  error: " << unmap_res->ToString();
    }
}

template <class ArenaT>
inline ArenaT *MmapMemPool::AllocArenaImpl(size_t size, SpaceType space_type, AllocatorType allocator_type,
                                           void *allocator_addr)
{
    os::memory::LockHolder lk(lock_);
    LOG_MMAP_MEM_POOL(DEBUG) << "Try to get new arena with size " << std::dec << size << " for "
                             << SpaceTypeToString(space_type);
    Pool pool_for_arena = AllocPoolUnsafe(size, space_type, allocator_type, allocator_addr);
    void *mem = pool_for_arena.GetMem();
    if (UNLIKELY(mem == nullptr)) {
        LOG_MMAP_MEM_POOL(ERROR) << "Failed to allocated new arena"
                                 << " for " << SpaceTypeToString(space_type);
        return nullptr;
    }
    ASSERT(pool_for_arena.GetSize() == size);
    // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
    mem = new (mem) ArenaT(size - sizeof(ArenaT), ToVoidPtr(ToUintPtr(mem) + sizeof(ArenaT)));
    LOG_MMAP_MEM_POOL(DEBUG) << "Allocated new arena with size " << std::dec << pool_for_arena.GetSize()
                             << " at addr = " << std::hex << pool_for_arena.GetMem() << " for "
                             << SpaceTypeToString(space_type);
    return static_cast<ArenaT *>(mem);
}

template <class ArenaT>
inline void MmapMemPool::FreeArenaImpl(ArenaT *arena)
{
    os::memory::LockHolder lk(lock_);
    arena->ClearNextLink();
    size_t size = arena->GetSize();
    size = size + sizeof(ArenaT);
    ASSERT(size == AlignUp(size, panda::os::mem::GetPageSize()));
    LOG_MMAP_MEM_POOL(DEBUG) << "Try to free arena with size " << std::dec << size << " at addr = " << std::hex
                             << arena;
    FreePoolUnsafe(arena, size);
    LOG_MMAP_MEM_POOL(DEBUG) << "Free arena call finished";
}

inline void *MmapMemPool::AllocRawMemCompilerImpl(size_t size)
{
    void *mem = nullptr;
    if (LIKELY(compiler_space_max_size_ >= compiler_space_current_size_ + size)) {
        mem = panda::os::mem::MapRWAnonymousWithAlignmentRaw(size, PANDA_POOL_ALIGNMENT_IN_BYTES);
        if (mem != nullptr) {
            compiler_space_current_size_ += size;
        }
    }
    LOG_MMAP_MEM_POOL(DEBUG) << "Occupied memory for " << SpaceTypeToString(SpaceType::SPACE_TYPE_COMPILER) << " - "
                             << std::dec << compiler_space_current_size_;
    return mem;
}

inline void *MmapMemPool::AllocRawMemInternalImpl(size_t size)
{
    void *mem = nullptr;
    if (LIKELY(internal_space_max_size_ >= internal_space_current_size_ + size)) {
        mem = panda::os::mem::MapRWAnonymousWithAlignmentRaw(size, PANDA_POOL_ALIGNMENT_IN_BYTES);
        if (mem != nullptr) {
            internal_space_current_size_ += size;
        }
    }
    LOG_MMAP_MEM_POOL(DEBUG) << "Occupied memory for " << SpaceTypeToString(SpaceType::SPACE_TYPE_INTERNAL) << " - "
                             << std::dec << internal_space_current_size_;
    return mem;
}

inline void *MmapMemPool::AllocRawMemCodeImpl(size_t size)
{
    void *mem = nullptr;
    if (LIKELY(code_space_max_size_ >= code_space_current_size_ + size)) {
        mem = panda::os::mem::MapRWAnonymousWithAlignmentRaw(size, PANDA_POOL_ALIGNMENT_IN_BYTES);
        if (mem != nullptr) {
            code_space_current_size_ += size;
        }
    }
    LOG_MMAP_MEM_POOL(DEBUG) << "Occupied memory for " << SpaceTypeToString(SpaceType::SPACE_TYPE_CODE) << " - "
                             << std::dec << code_space_current_size_;
    return mem;
}

inline void *MmapMemPool::AllocRawMemObjectImpl(size_t size, SpaceType type)
{
    void *mem = common_space_.AllocRawMem(size, &common_space_pools_);
    LOG_MMAP_MEM_POOL(DEBUG) << "Occupied memory for " << SpaceTypeToString(type) << " - " << std::dec
                             << common_space_.GetOccupiedMemorySize();
    return mem;
}

inline void *MmapMemPool::AllocRawMemImpl(size_t size, SpaceType type)
{
    os::memory::LockHolder lk(lock_);
    ASSERT(size % panda::os::mem::GetPageSize() == 0);
    // NOTE: We need this check because we use this memory for Pools too
    // which require PANDA_POOL_ALIGNMENT_IN_BYTES alignment
    ASSERT(size == AlignUp(size, PANDA_POOL_ALIGNMENT_IN_BYTES));
    void *mem = nullptr;
    switch (type) {
        // Internal spaces
        case SpaceType::SPACE_TYPE_COMPILER:
            mem = AllocRawMemCompilerImpl(size);
            break;
        case SpaceType::SPACE_TYPE_INTERNAL:
            mem = AllocRawMemInternalImpl(size);
            break;
        case SpaceType::SPACE_TYPE_CODE:
            mem = AllocRawMemCodeImpl(size);
            break;
        // Heap spaces
        case SpaceType::SPACE_TYPE_HUMONGOUS_OBJECT:
        case SpaceType::SPACE_TYPE_NON_MOVABLE_OBJECT:
        case SpaceType::SPACE_TYPE_OBJECT:
            mem = AllocRawMemObjectImpl(size, type);
            break;
        default:
            LOG_MMAP_MEM_POOL(FATAL) << "Try to use incorrect " << SpaceTypeToString(type) << " for AllocRawMem.";
    }
    if (UNLIKELY(mem == nullptr)) {
        LOG_MMAP_MEM_POOL(DEBUG) << "OOM when trying to allocate " << size << " bytes for " << SpaceTypeToString(type);
        // We have OOM and must return nullptr
        mem = nullptr;
    } else {
        LOG_MMAP_MEM_POOL(DEBUG) << "Allocate raw memory with size " << size << " at addr = " << mem << " for "
                                 << SpaceTypeToString(type);
    }
    return mem;
}

/* static */
inline void MmapMemPool::FreeRawMemImpl(void *mem, size_t size)
{
    if (auto unmap_res = panda::os::mem::UnmapRaw(mem, size)) {
        LOG_MMAP_MEM_POOL(FATAL) << "Destructor unnmap  error: " << unmap_res->ToString();
    }
    LOG_MMAP_MEM_POOL(DEBUG) << "Deallocated raw memory with size " << size << " at addr = " << mem;
}

inline Pool MmapMemPool::AllocPoolUnsafe(size_t size, SpaceType space_type, AllocatorType allocator_type,
                                         void *allocator_addr)
{
    ASSERT(size == AlignUp(size, panda::os::mem::GetPageSize()));
    ASSERT(size == AlignUp(size, PANDA_POOL_ALIGNMENT_IN_BYTES));
    Pool pool = NULLPOOL;
    bool add_to_pool_map = false;
    // Try to find free pool from the early allocated memory
    switch (space_type) {
        case SpaceType::SPACE_TYPE_CODE:
        case SpaceType::SPACE_TYPE_COMPILER:
        case SpaceType::SPACE_TYPE_INTERNAL:
            // We always use mmap for these space types
            break;
        case SpaceType::SPACE_TYPE_HUMONGOUS_OBJECT:
        case SpaceType::SPACE_TYPE_NON_MOVABLE_OBJECT:
        case SpaceType::SPACE_TYPE_OBJECT:
            add_to_pool_map = true;
            pool = common_space_pools_.PopFreePool(size);
            break;
        default:
            LOG_MMAP_MEM_POOL(FATAL) << "Try to use incorrect " << SpaceTypeToString(space_type)
                                     << " for AllocPoolUnsafe.";
            break;
    }
    if (pool.GetMem() != nullptr) {
        LOG_MMAP_MEM_POOL(DEBUG) << "Reuse pool with size " << pool.GetSize() << " at addr = " << pool.GetMem()
                                 << " for " << SpaceTypeToString(space_type);
    }
    if (pool.GetMem() == nullptr) {
        void *mem = AllocRawMemImpl(size, space_type);
        if (mem != nullptr) {
            pool = Pool(size, mem);
        }
    }
    if (pool.GetMem() == nullptr) {
        return pool;
    }
    ASAN_UNPOISON_MEMORY_REGION(pool.GetMem(), pool.GetSize());
    if (UNLIKELY(allocator_addr == nullptr)) {
        // Save the pointer to the first byte of a Pool
        allocator_addr = pool.GetMem();
    }
    if (add_to_pool_map) {
        pool_map_.AddPoolToMap(ToVoidPtr(ToUintPtr(pool.GetMem()) - GetMinObjectAddress()), pool.GetSize(), space_type,
                               allocator_type, allocator_addr);
    } else {
        AddToNonObjectPoolsMap(std::make_tuple(pool, AllocatorInfo(allocator_type, allocator_addr), space_type));
    }
    os::mem::TagAnonymousMemory(pool.GetMem(), pool.GetSize(), SpaceTypeToString(space_type));
    ASSERT(AlignUp(ToUintPtr(pool.GetMem()), PANDA_POOL_ALIGNMENT_IN_BYTES) == ToUintPtr(pool.GetMem()));
    return pool;
}

inline void MmapMemPool::FreePoolUnsafe(void *mem, size_t size)
{
    ASSERT(size == AlignUp(size, panda::os::mem::GetPageSize()));
    ASAN_POISON_MEMORY_REGION(mem, size);
    SpaceType pool_space_type = GetSpaceTypeForAddrImpl(mem);
    bool remove_from_pool_map = false;
    switch (pool_space_type) {
        case SpaceType::SPACE_TYPE_HUMONGOUS_OBJECT:
        case SpaceType::SPACE_TYPE_NON_MOVABLE_OBJECT:
        case SpaceType::SPACE_TYPE_OBJECT:
            remove_from_pool_map = true;
            common_space_pools_.PushFreePool(Pool(size, mem));
            break;
        case SpaceType::SPACE_TYPE_COMPILER:
            compiler_space_current_size_ -= size;
            FreeRawMemImpl(mem, size);
            break;
        case SpaceType::SPACE_TYPE_INTERNAL:
            internal_space_current_size_ -= size;
            FreeRawMemImpl(mem, size);
            break;
        case SpaceType::SPACE_TYPE_CODE:
            code_space_current_size_ -= size;
            FreeRawMemImpl(mem, size);
            break;
        default:
            LOG_MMAP_MEM_POOL(FATAL) << "Try to use incorrect " << SpaceTypeToString(pool_space_type)
                                     << " for FreePoolUnsafe.";
            break;
    }
    os::mem::TagAnonymousMemory(mem, size, nullptr);
    if (remove_from_pool_map) {
        pool_map_.RemovePoolFromMap(ToVoidPtr(ToUintPtr(mem) - GetMinObjectAddress()), size);
        os::mem::ReleasePages(ToUintPtr(mem), ToUintPtr(mem) + size);
    } else {
        RemoveFromNonObjectPoolsMap(mem);
    }
    LOG_MMAP_MEM_POOL(DEBUG) << "Freed " << std::dec << size << " memory for " << SpaceTypeToString(pool_space_type);
}

inline Pool MmapMemPool::AllocPoolImpl(size_t size, SpaceType space_type, AllocatorType allocator_type,
                                       void *allocator_addr)
{
    os::memory::LockHolder lk(lock_);
    LOG_MMAP_MEM_POOL(DEBUG) << "Try to get new pool with size " << std::dec << size << " for "
                             << SpaceTypeToString(space_type);
    Pool pool = AllocPoolUnsafe(size, space_type, allocator_type, allocator_addr);
    LOG_MMAP_MEM_POOL(DEBUG) << "Allocated new pool with size " << std::dec << pool.GetSize()
                             << " at addr = " << std::hex << pool.GetMem() << " for " << SpaceTypeToString(space_type);
    return pool;
}

inline void MmapMemPool::FreePoolImpl(void *mem, size_t size)
{
    os::memory::LockHolder lk(lock_);
    LOG_MMAP_MEM_POOL(DEBUG) << "Try to free pool with size " << std::dec << size << " at addr = " << std::hex << mem;
    FreePoolUnsafe(mem, size);
    LOG_MMAP_MEM_POOL(DEBUG) << "Free pool call finished";
}

inline void MmapMemPool::AddToNonObjectPoolsMap(std::tuple<Pool, AllocatorInfo, SpaceType> pool_info)
{
    void *pool_addr = std::get<0>(pool_info).GetMem();
    ASSERT(non_object_mmaped_pools_.find(pool_addr) == non_object_mmaped_pools_.end());
    non_object_mmaped_pools_.insert({pool_addr, pool_info});
}

inline void MmapMemPool::RemoveFromNonObjectPoolsMap(void *pool_addr)
{
    auto element = non_object_mmaped_pools_.find(pool_addr);
    ASSERT(element != non_object_mmaped_pools_.end());
    non_object_mmaped_pools_.erase(element);
}

inline std::tuple<Pool, AllocatorInfo, SpaceType> MmapMemPool::FindAddrInNonObjectPoolsMap(void *addr)
{
    auto element = non_object_mmaped_pools_.lower_bound(addr);
    uintptr_t pool_start =
        element != non_object_mmaped_pools_.end() ? ToUintPtr(element->first) : std::numeric_limits<uintptr_t>::max();
    if (ToUintPtr(addr) < pool_start) {
        ASSERT(element != non_object_mmaped_pools_.begin());
        element = std::prev(element);
        pool_start = ToUintPtr(element->first);
    }
    ASSERT(element != non_object_mmaped_pools_.end());
    [[maybe_unused]] uintptr_t pool_end = pool_start + std::get<0>(element->second).GetSize();
    ASSERT(ToUintPtr(addr) >= pool_start);
    ASSERT(ToUintPtr(addr) < pool_end);
    return element->second;
}

inline AllocatorInfo MmapMemPool::GetAllocatorInfoForAddrImpl(void *addr)
{
    os::memory::LockHolder lk(lock_);
    if ((ToUintPtr(addr) < GetMinObjectAddress()) || (ToUintPtr(addr) >= GetMaxObjectAddress())) {
        return std::get<1>(FindAddrInNonObjectPoolsMap(addr));
    }
    AllocatorInfo info = pool_map_.GetAllocatorInfo(ToVoidPtr(ToUintPtr(addr) - GetMinObjectAddress()));
    ASSERT(info.GetType() != AllocatorType::UNDEFINED);
    ASSERT(info.GetAllocatorHeaderAddr() != nullptr);
    return info;
}

inline SpaceType MmapMemPool::GetSpaceTypeForAddrImpl(void *addr)
{
    os::memory::LockHolder lk(lock_);
    if ((ToUintPtr(addr) < GetMinObjectAddress()) || (ToUintPtr(addr) >= GetMaxObjectAddress())) {
        // <2> is a pointer to SpaceType
        return std::get<2>(FindAddrInNonObjectPoolsMap(addr));
    }
    SpaceType space_type = pool_map_.GetSpaceType(ToVoidPtr(ToUintPtr(addr) - GetMinObjectAddress()));
    ASSERT(space_type != SpaceType::SPACE_TYPE_UNDEFINED);
    return space_type;
}

inline void *MmapMemPool::GetStartAddrPoolForAddrImpl(void *addr)
{
    os::memory::LockHolder lk(lock_);
    if ((ToUintPtr(addr) < GetMinObjectAddress()) || (ToUintPtr(addr) >= GetMaxObjectAddress())) {
        return std::get<0>(FindAddrInNonObjectPoolsMap(addr)).GetMem();
    }
    void *pool_start_addr = pool_map_.GetFirstByteOfPoolForAddr(ToVoidPtr(ToUintPtr(addr) - GetMinObjectAddress()));
    return ToVoidPtr(ToUintPtr(pool_start_addr) + GetMinObjectAddress());
}

inline size_t MmapMemPool::GetObjectSpaceFreeBytes()
{
    os::memory::LockHolder lk(lock_);

    size_t unused_bytes = common_space_.GetFreeSpace();
    size_t freed_bytes = common_space_pools_.GetAllSize();
    ASSERT(unused_bytes + freed_bytes <= common_space_.GetMaxSize());
    return unused_bytes + freed_bytes;
}

#undef LOG_MMAP_MEM_POOL

}  // namespace panda

#endif  // PANDA_LIBPANDABASE_MEM_MMAP_MEM_POOL_INL_H_
