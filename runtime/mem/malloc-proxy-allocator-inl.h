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

#ifndef PANDA_RUNTIME_MEM_MALLOC_PROXY_ALLOCATOR_INL_H_
#define PANDA_RUNTIME_MEM_MALLOC_PROXY_ALLOCATOR_INL_H_

#include <cstring>

#include "libpandabase/utils/logger.h"
#include "libpandabase/os/mem.h"
#include "runtime/mem/alloc_config.h"
#include "libpandabase/utils/asan_interface.h"
#include "runtime/mem/malloc-proxy-allocator.h"

namespace panda::mem {

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_MALLOCPROXY_ALLOCATOR(level) LOG(level, ALLOC) << "MallocProxyAllocator: "

template <typename AllocConfigT>
MallocProxyAllocator<AllocConfigT>::~MallocProxyAllocator()
{
    LOG_MALLOCPROXY_ALLOCATOR(INFO) << "Destroying MallocProxyAllocator";
}

template <typename AllocConfigT>
void *MallocProxyAllocator<AllocConfigT>::Alloc(size_t size, Alignment align)
{
    if (size == 0) {
        return nullptr;
    }

    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if constexpr (!DUMMY_ALLOC_CONFIG) {
        lock_.Lock();
    }
    size_t alignment_in_bytes = GetAlignmentInBytes(align);
    void *ret = os::mem::AlignedAlloc(alignment_in_bytes, size);
    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if constexpr (!DUMMY_ALLOC_CONFIG) {
        ASSERT(allocated_memory_.find(ret) == allocated_memory_.end());
        allocated_memory_.insert({ret, size});
        AllocConfigT::OnAlloc(size, type_allocation_, mem_stats_);
        AllocConfigT::MemoryInit(ret, size);
        lock_.Unlock();
    }
    LOG_MALLOCPROXY_ALLOCATOR(DEBUG) << "Allocate memory with size " << std::dec << size << " at addr " << std::hex
                                     << ret;
    return ret;
}

template <typename AllocConfigT>
void MallocProxyAllocator<AllocConfigT>::Free(void *mem)
{
    if (mem == nullptr) {
        return;
    }
    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if constexpr (!DUMMY_ALLOC_CONFIG) {
        lock_.Lock();
    }
    os::mem::AlignedFree(mem);
    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if constexpr (!DUMMY_ALLOC_CONFIG) {
        auto iterator = allocated_memory_.find(mem);
        ASSERT(iterator != allocated_memory_.end());
        size_t size = iterator->second;
        allocated_memory_.erase(mem);
        AllocConfigT::OnFree(size, type_allocation_, mem_stats_);
        lock_.Unlock();
    }
    LOG_MALLOCPROXY_ALLOCATOR(DEBUG) << "Free memory at " << mem;
}

#undef LOG_MALLOCPROXY_ALLOCATOR

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_MALLOC_PROXY_ALLOCATOR_INL_H_
