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

#include "base_mem_stats.h"

#include "os/mutex.h"
#include "utils/logger.h"
#include "utils/type_helpers.h"

#include <numeric>
namespace panda {

void BaseMemStats::RecordAllocateRaw(size_t size, SpaceType type_mem)
{
    ASSERT(!IsHeapSpace(type_mem));
    RecordAllocate(size, type_mem);
}

void BaseMemStats::RecordAllocate(size_t size, SpaceType type_mem)
{
    auto index = helpers::ToUnderlying(type_mem);
    allocated_[index].fetch_add(size, std::memory_order_acq_rel);
}

void BaseMemStats::RecordMoved(size_t size, SpaceType type_mem)
{
    auto index = helpers::ToUnderlying(type_mem);
    uint64_t old_value = allocated_[index].fetch_sub(size, std::memory_order_acq_rel);
    (void)old_value;
    ASSERT(old_value >= size);
}

void BaseMemStats::RecordFreeRaw(size_t size, SpaceType type_mem)
{
    ASSERT(!IsHeapSpace(type_mem));
    RecordFree(size, type_mem);
}

void BaseMemStats::RecordFree(size_t size, SpaceType type_mem)
{
    auto index = helpers::ToUnderlying(type_mem);
    freed_[index].fetch_add(size, std::memory_order_acq_rel);
}

uint64_t BaseMemStats::GetAllocated(SpaceType type_mem) const
{
    return allocated_[helpers::ToUnderlying(type_mem)].load(std::memory_order_acquire);
}

uint64_t BaseMemStats::GetFreed(SpaceType type_mem) const
{
    return freed_[helpers::ToUnderlying(type_mem)].load(std::memory_order_acquire);
}

uint64_t BaseMemStats::GetAllocatedHeap() const
{
    uint64_t result = 0;
    for (size_t index = 0; index < SPACE_TYPE_SIZE; index++) {
        SpaceType type = ToSpaceType(index);
        if (IsHeapSpace(type)) {
            result += allocated_[index].load(std::memory_order_acquire);
        }
    }
    return result;
}

uint64_t BaseMemStats::GetFreedHeap() const
{
    uint64_t result = 0;
    for (size_t index = 0; index < SPACE_TYPE_SIZE; index++) {
        SpaceType type = ToSpaceType(index);
        if (IsHeapSpace(type)) {
            result += freed_[index].load(std::memory_order_acquire);
        }
    }
    return result;
}

uint64_t BaseMemStats::GetFootprintHeap() const
{
    return helpers::UnsignedDifferenceUint64(GetAllocatedHeap(), GetFreedHeap());
}

uint64_t BaseMemStats::GetFootprint(SpaceType type_mem) const
{
    auto index = helpers::ToUnderlying(type_mem);
    LOG_IF(allocated_[index].load(std::memory_order_acquire) < freed_[index].load(std::memory_order_acquire), FATAL, GC)
        << "Allocated < Freed (mem type = " << std::dec << static_cast<size_t>(index)
        << "): " << allocated_[index].load(std::memory_order_acquire) << " < "
        << freed_[index].load(std::memory_order_acquire);
    return allocated_[index].load(std::memory_order_acquire) - freed_[index].load(std::memory_order_acquire);
}

uint64_t BaseMemStats::GetTotalFootprint() const
{
    return std::accumulate(begin(allocated_), end(allocated_), 0UL) - std::accumulate(begin(freed_), end(freed_), 0UL);
}

}  // namespace panda
