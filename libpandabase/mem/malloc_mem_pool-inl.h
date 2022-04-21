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

#ifndef PANDA_LIBPANDABASE_MEM_MALLOC_MEM_POOL_INL_H_
#define PANDA_LIBPANDABASE_MEM_MALLOC_MEM_POOL_INL_H_

#include "malloc_mem_pool.h"
#include "mem.h"
#include "os/mem.h"
#include "utils/logger.h"
#include <cstdlib>
#include <memory>

namespace panda {

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_MALLOC_MEM_POOL(level) LOG(level, MEMORYPOOL) << "MallocMemPool: "

inline MallocMemPool::MallocMemPool() : MemPool("MallocMemPool")
{
    LOG_MALLOC_MEM_POOL(DEBUG) << "Successfully initialized MallocMemPool";
}

template <class ArenaT>
inline ArenaT *MallocMemPool::AllocArenaImpl(size_t size, [[maybe_unused]] SpaceType space_type,
                                             [[maybe_unused]] AllocatorType allocator_type,
                                             [[maybe_unused]] void *allocator_addr)
{
    LOG_MALLOC_MEM_POOL(DEBUG) << "Try to get new arena with size " << std::dec << size << " for "
                               << SpaceTypeToString(space_type);
    size_t max_alignment_drift = 0;
    if (DEFAULT_ALIGNMENT_IN_BYTES > alignof(ArenaT)) {
        max_alignment_drift = DEFAULT_ALIGNMENT_IN_BYTES - alignof(ArenaT);
    }
    size_t max_size = size + sizeof(ArenaT) + max_alignment_drift;
    auto ret = panda::os::mem::AlignedAlloc(std::max(DEFAULT_ALIGNMENT_IN_BYTES, alignof(ArenaT)), max_size);
    void *buff = reinterpret_cast<char *>(reinterpret_cast<std::uintptr_t>(ret) + sizeof(ArenaT));
    size_t size_for_buff = max_size - sizeof(ArenaT);
    buff = std::align(DEFAULT_ALIGNMENT_IN_BYTES, size, buff, size_for_buff);
    ASSERT(buff != nullptr);
    ASSERT(reinterpret_cast<std::uintptr_t>(buff) - reinterpret_cast<std::uintptr_t>(ret) >= sizeof(ArenaT));
    ASSERT(size_for_buff >= size);
    // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
    ret = new (ret) ArenaT(size_for_buff, buff);
    ASSERT(reinterpret_cast<std::uintptr_t>(ret) + max_size >= reinterpret_cast<std::uintptr_t>(buff) + size);
    LOG_MALLOC_MEM_POOL(DEBUG) << "Allocated new arena with size " << std::dec << size_for_buff
                               << " at addr = " << std::hex << buff << " for " << SpaceTypeToString(space_type);
    return static_cast<ArenaT *>(ret);
}

template <class ArenaT>
inline void MallocMemPool::FreeArenaImpl(ArenaT *arena)
{
    LOG_MALLOC_MEM_POOL(DEBUG) << "Try to free arena with size " << std::dec << arena->GetSize()
                               << " at addr = " << std::hex << arena;
    arena->~Arena();
    os::mem::AlignedFree(arena);
    LOG_MALLOC_MEM_POOL(DEBUG) << "Free arena call finished";
}

/* static */
inline Pool MallocMemPool::AllocPoolImpl(size_t size, [[maybe_unused]] SpaceType space_type,
                                         [[maybe_unused]] AllocatorType allocator_type,
                                         [[maybe_unused]] void *allocator_addr)
{
    LOG_MALLOC_MEM_POOL(DEBUG) << "Try to get new pool with size " << std::dec << size << " for "
                               << SpaceTypeToString(space_type);
    // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
    void *mem = std::malloc(size);  // NOLINT(cppcoreguidelines-no-malloc)
    LOG_MALLOC_MEM_POOL(DEBUG) << "Allocated new pool with size " << std::dec << size << " at addr = " << std::hex
                               << mem << " for " << SpaceTypeToString(space_type);
    return Pool(size, mem);
}

/* static */
inline void MallocMemPool::FreePoolImpl(void *mem, [[maybe_unused]] size_t size)
{
    LOG_MALLOC_MEM_POOL(DEBUG) << "Try to free pool with size " << std::dec << size << " at addr = " << std::hex << mem;
    std::free(mem);  // NOLINT(cppcoreguidelines-no-malloc)
    LOG_MALLOC_MEM_POOL(DEBUG) << "Free pool call finished";
}

/* static */
inline AllocatorInfo MallocMemPool::GetAllocatorInfoForAddrImpl([[maybe_unused]] void *addr)
{
    LOG(FATAL, ALLOC) << "Not implemented method";
    return AllocatorInfo(AllocatorType::UNDEFINED, nullptr);
}

/* static */
inline SpaceType MallocMemPool::GetSpaceTypeForAddrImpl([[maybe_unused]] void *addr)
{
    LOG(FATAL, ALLOC) << "Not implemented method";
    return SpaceType::SPACE_TYPE_UNDEFINED;
}

/* static */
inline void *MallocMemPool::GetStartAddrPoolForAddrImpl([[maybe_unused]] void *addr)
{
    LOG(FATAL, ALLOC) << "Not implemented method";
    return nullptr;
}

}  // namespace panda

#endif  // PANDA_LIBPANDABASE_MEM_MALLOC_MEM_POOL_INL_H_
