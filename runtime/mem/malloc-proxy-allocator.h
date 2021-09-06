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

#ifndef PANDA_RUNTIME_MEM_MALLOC_PROXY_ALLOCATOR_H_
#define PANDA_RUNTIME_MEM_MALLOC_PROXY_ALLOCATOR_H_

#include <functional>
#include <map>
#include <memory>
#include <unordered_map>
#include <type_traits>

#include "libpandabase/mem/mem.h"
#include "libpandabase/mem/space.h"
#include "libpandabase/os/mutex.h"

namespace panda::mem {

using panda::Alignment;
class EmptyMemoryConfig;

/**
 * \brief Class-proxy to the malloc, do some logging.
 */
template <typename AllocConfigT>
class MallocProxyAllocator {
public:
    NO_COPY_SEMANTIC(MallocProxyAllocator);
    NO_MOVE_SEMANTIC(MallocProxyAllocator);

    explicit MallocProxyAllocator(MemStatsType *mem_stats, SpaceType type_allocation = SpaceType::SPACE_TYPE_INTERNAL)
        : type_allocation_(type_allocation), mem_stats_ {mem_stats}
    {
    }

    ~MallocProxyAllocator();

    [[nodiscard]] void *Alloc(size_t size, Alignment align = DEFAULT_ALIGNMENT);

    template <class T>
    T *AllocArray(size_t size)
    {
        return static_cast<T *>(this->Alloc(sizeof(T) * size));
    }

    void Free(void *mem);

private:
    static constexpr bool DUMMY_ALLOC_CONFIG = std::is_same<AllocConfigT, EmptyMemoryConfig>::value;
    std::unordered_map<void *, size_t> allocated_memory_;
    SpaceType type_allocation_;
    MemStatsType *mem_stats_;
    os::memory::Mutex lock_;
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_MALLOC_PROXY_ALLOCATOR_H_
