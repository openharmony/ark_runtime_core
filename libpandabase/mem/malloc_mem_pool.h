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

#ifndef PANDA_LIBPANDABASE_MEM_MALLOC_MEM_POOL_H_
#define PANDA_LIBPANDABASE_MEM_MALLOC_MEM_POOL_H_

#include "mem_pool.h"

namespace panda {

// Simple Mem Pool without cache
class MallocMemPool : public MemPool<MallocMemPool> {
private:
    template <class ArenaT = Arena>
    ArenaT *AllocArenaImpl(size_t size, SpaceType space_type, AllocatorType allocator_type, void *allocator_addr);

    template <class ArenaT = Arena>
    void FreeArenaImpl(ArenaT *arena);

    static Pool AllocPoolImpl(size_t size, SpaceType space_type, AllocatorType allocator_type, void *allocator_addr);

    static void FreePoolImpl(void *mem, size_t size);

    static AllocatorInfo GetAllocatorInfoForAddrImpl(void *addr);

    static SpaceType GetSpaceTypeForAddrImpl(void *addr);

    static void *GetStartAddrPoolForAddrImpl(void *addr);

    MallocMemPool();

    ~MallocMemPool() override = default;

    NO_COPY_SEMANTIC(MallocMemPool);
    NO_MOVE_SEMANTIC(MallocMemPool);

    friend class PoolManager;
    friend class MemPool<MallocMemPool>;
};

}  // namespace panda

#endif  // PANDA_LIBPANDABASE_MEM_MALLOC_MEM_POOL_H_
