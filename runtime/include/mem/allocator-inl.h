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

#ifndef PANDA_RUNTIME_INCLUDE_MEM_ALLOCATOR_INL_H_
#define PANDA_RUNTIME_INCLUDE_MEM_ALLOCATOR_INL_H_

#include "runtime/include/mem/allocator.h"
namespace panda::mem {

template <typename AllocT>
inline void *ObjectAllocatorBase::AllocateSafe(size_t size, Alignment align, AllocT *object_allocator, size_t pool_size,
                                               SpaceType space_type)
{
    void *mem = object_allocator->Alloc(size, align);
    if (UNLIKELY(mem == nullptr)) {
        return AddPoolsAndAlloc(size, align, object_allocator, pool_size, space_type);
    }
    return mem;
}

template <typename AllocT>
inline void *ObjectAllocatorBase::AddPoolsAndAlloc(size_t size, Alignment align, AllocT *object_allocator,
                                                   size_t pool_size, SpaceType space_type)
{
    void *mem = nullptr;
    static os::memory::Mutex pool_lock;
    os::memory::LockHolder lock(pool_lock);
    while (true) {
        auto pool = PoolManager::GetMmapMemPool()->AllocPool(pool_size, space_type, AllocT::GetAllocatorType(),
                                                             object_allocator);
        if (UNLIKELY(pool.GetMem() == nullptr)) {
            return nullptr;
        }
        bool added_memory_pool = object_allocator->AddMemoryPool(pool.GetMem(), pool.GetSize());
        if (!added_memory_pool) {
            LOG(FATAL, ALLOC) << "ObjectAllocator: couldn't add memory pool to object allocator";
        }
        mem = object_allocator->Alloc(size, align);
        if (mem != nullptr) {
            break;
        }
    }
    return mem;
}

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_INCLUDE_MEM_ALLOCATOR_INL_H_
