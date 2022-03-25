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

#include "runtime/mem/mem_stats.h"

#include "libpandabase/utils/utf.h"
#include "runtime/include/class.h"
#include "runtime/include/runtime.h"
#include "runtime/mem/mem_stats_additional_info.h"
#include "runtime/mem/mem_stats_default.h"
#include "runtime/mem/object_helpers.h"

namespace panda::mem {

template <typename T>
void MemStats<T>::RecordAllocateObject(size_t size, SpaceType type_mem)
{
    ASSERT(IsHeapSpace(type_mem));
    RecordAllocate(size, type_mem);
    if (type_mem == SpaceType::SPACE_TYPE_HUMONGOUS_OBJECT) {
        humongous_objects_allocated_.fetch_add(1, std::memory_order_acq_rel);
    } else {
        objects_allocated_.fetch_add(1, std::memory_order_acq_rel);
    }
}

template <typename T>
void MemStats<T>::RecordMovedObjects(size_t total_object_num, size_t size, SpaceType type_mem)
{
    ASSERT(IsHeapSpace(type_mem));
    RecordMoved(size, type_mem);
    // We can't move SPACE_TYPE_HUMONGOUS_OBJECT
    ASSERT(type_mem != SpaceType::SPACE_TYPE_HUMONGOUS_OBJECT);
    uint64_t old_val = objects_allocated_.fetch_sub(total_object_num, std::memory_order_acq_rel);
    (void)old_val;
    ASSERT(old_val >= total_object_num);
}

template <typename T>
void MemStats<T>::RecordFreeObject(size_t object_size, SpaceType type_mem)
{
    ASSERT(IsHeapSpace(type_mem));
    RecordFree(object_size, type_mem);
    if (type_mem == SpaceType::SPACE_TYPE_HUMONGOUS_OBJECT) {
        humongous_objects_freed_.fetch_add(1, std::memory_order_acq_rel);
    } else {
        objects_freed_.fetch_add(1, std::memory_order_acq_rel);
    }
}

template <typename T>
void MemStats<T>::RecordFreeObjects(size_t total_object_num, size_t total_object_size, SpaceType type_mem)
{
    ASSERT(IsHeapSpace(type_mem));
    RecordFree(total_object_size, type_mem);
    if (type_mem == SpaceType::SPACE_TYPE_HUMONGOUS_OBJECT) {
        humongous_objects_freed_.fetch_add(total_object_num, std::memory_order_acq_rel);
    } else {
        objects_freed_.fetch_add(total_object_num, std::memory_order_acq_rel);
    }
}

template <typename T>
PandaString MemStats<T>::GetStatistics(HeapManager *heap_manager)
{
    PandaStringStream statistic;
    statistic << "memory statistics:" << std::endl;
    statistic << "heap: allocated - " << GetAllocatedHeap() << ", freed - " << GetFreedHeap() << std::endl;
    statistic << "raw memory: allocated - " << GetAllocated(SpaceType::SPACE_TYPE_INTERNAL) << ", freed - "
              << GetFreed(SpaceType::SPACE_TYPE_INTERNAL) << std::endl;
    statistic << "compiler: allocated - " << GetAllocated(SpaceType::SPACE_TYPE_CODE) << std::endl;
    statistic << "ArenaAllocator: allocated - " << GetAllocated(SpaceType::SPACE_TYPE_COMPILER) << std::endl;
    statistic << "total footprint now - " << GetTotalFootprint() << std::endl;
    statistic << "total allocated object - " << GetTotalObjectsAllocated() << std::endl;
    statistic << "min GC pause time - " << GetMinGCPause() << std::endl;
    statistic << "max GC pause time - " << GetMaxGCPause() << std::endl;
    statistic << "average GC pause time - " << GetAverageGCPause() << std::endl;
    statistic << "total GC pause time - " << GetTotalGCPause() << std::endl;
    auto additional_statistics = static_cast<T *>(this)->GetAdditionalStatistics(heap_manager);
    return statistic.str() + additional_statistics;
}

template <typename T>
[[nodiscard]] uint64_t MemStats<T>::GetTotalObjectsAllocated() const
{
    return objects_allocated_.load(std::memory_order_acquire);
}

template <typename T>
[[nodiscard]] uint64_t MemStats<T>::GetTotalObjectsFreed() const
{
    return objects_freed_.load(std::memory_order_acquire);
}

template <typename T>
[[nodiscard]] uint64_t MemStats<T>::GetTotalRegularObjectsAllocated() const
{
    return GetTotalObjectsAllocated() - GetTotalHumongousObjectsAllocated();
}

template <typename T>
[[nodiscard]] uint64_t MemStats<T>::GetTotalRegularObjectsFreed() const
{
    return GetTotalObjectsFreed() - GetTotalHumongousObjectsFreed();
}

template <typename T>
[[nodiscard]] uint64_t MemStats<T>::GetTotalHumongousObjectsAllocated() const
{
    return humongous_objects_allocated_.load(std::memory_order_acquire);
}

template <typename T>
[[nodiscard]] uint64_t MemStats<T>::GetTotalHumongousObjectsFreed() const
{
    return humongous_objects_freed_.load(std::memory_order_acquire);
}

template <typename T>
[[nodiscard]] uint64_t MemStats<T>::GetObjectsCountAlive() const
{
    return GetTotalObjectsAllocated() - GetTotalObjectsFreed();
}

template <typename T>
[[nodiscard]] uint64_t MemStats<T>::GetRegularObjectsCountAlive() const
{
    return GetTotalRegularObjectsAllocated() - GetTotalRegularObjectsFreed();
}

template <typename T>
[[nodiscard]] uint64_t MemStats<T>::GetHumonguousObjectsCountAlive() const
{
    return GetTotalHumongousObjectsAllocated() - GetTotalHumongousObjectsFreed();
}

template <typename T>
void MemStats<T>::RecordGCPauseStart()
{
    pause_start_time_ = clock::now();
}

template <typename T>
void MemStats<T>::RecordGCPauseEnd()
{
    duration pause_time = clock::now() - pause_start_time_;
    if (pause_count_) {
        min_pause_ = std::min(min_pause_, pause_time);
        max_pause_ = std::max(max_pause_, pause_time);
    } else {
        min_pause_ = pause_time;
        max_pause_ = pause_time;
    }
    pause_count_++;
    sum_pause_ += pause_time;
}

template <typename T>
uint64_t MemStats<T>::GetMinGCPause() const
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(min_pause_).count();
}

template <typename T>
uint64_t MemStats<T>::GetMaxGCPause() const
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(max_pause_).count();
}

template <typename T>
uint64_t MemStats<T>::GetAverageGCPause() const
{
    return pause_count_ ? std::chrono::duration_cast<std::chrono::milliseconds>(sum_pause_).count() / pause_count_ : 0;
}

template <typename T>
uint64_t MemStats<T>::GetTotalGCPause() const
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(sum_pause_).count();
}

template class MemStats<MemStatsDefault>;
template class MemStats<MemStatsAdditionalInfo>;
}  // namespace panda::mem
