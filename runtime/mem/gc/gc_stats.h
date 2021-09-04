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

#ifndef PANDA_RUNTIME_MEM_GC_GC_STATS_H_
#define PANDA_RUNTIME_MEM_GC_GC_STATS_H_

#include "libpandabase/os/mutex.h"
#include "runtime/include/histogram-inl.h"
#include "runtime/include/mem/panda_containers.h"
#include "runtime/include/mem/panda_string.h"
#include "runtime/include/time_utils.h"
#include "runtime/mem/gc/gc_types.h"

#include <algorithm>
#include <atomic>
#include <ratio>

namespace panda::mem {

class GCStats;
class HeapManager;

enum class ObjectTypeStats : size_t {
    YOUNG_FREED_OBJECTS = 0,
    MOVED_OBJECTS,
    ALL_FREED_OBJECTS,

    OBJECT_TYPE_STATS_LAST
};

constexpr size_t ToIndex(ObjectTypeStats type)
{
    return static_cast<size_t>(type);
}

constexpr size_t OBJECT_TYPE_STATS_SIZE = static_cast<size_t>(ObjectTypeStats::OBJECT_TYPE_STATS_LAST);

enum class MemoryTypeStats : size_t {
    YOUNG_FREED_BYTES = 0,
    MOVED_BYTES,
    ALL_FREED_BYTES,

    MEMORY_TYPE_STATS_LAST
};

constexpr size_t ToIndex(MemoryTypeStats type)
{
    return static_cast<size_t>(type);
}

constexpr size_t MEMORY_TYPE_STATS_SIZE = static_cast<size_t>(MemoryTypeStats::MEMORY_TYPE_STATS_LAST);

enum class TimeTypeStats : size_t {
    YOUNG_PAUSED_TIME = 0,
    YOUNG_TOTAL_TIME,
    ALL_PAUSED_TIME,
    ALL_TOTAL_TIME,

    TIME_TYPE_STATS_LAST
};

constexpr size_t ToIndex(TimeTypeStats type)
{
    return static_cast<size_t>(type);
}

constexpr size_t TIME_TYPE_STATS_SIZE = static_cast<size_t>(TimeTypeStats::TIME_TYPE_STATS_LAST);

// scoped specific for GC Stats
class GCInstanceStats {
public:
    GCInstanceStats()
    {
        std::fill(begin(objects_stats_), end(objects_stats_),
                  SimpleHistogram<uint64_t>(helpers::ValueType::VALUE_TYPE_OBJECT));
        std::fill(begin(memory_stats_), end(memory_stats_),
                  SimpleHistogram<uint64_t>(helpers::ValueType::VALUE_TYPE_MEMORY));
        std::fill(begin(time_stats_), end(time_stats_), SimpleHistogram<uint64_t>(helpers::ValueType::VALUE_TYPE_TIME));
    }

    void AddObjectsValue(uint64_t value, ObjectTypeStats memory_type)
    {
        auto index = static_cast<size_t>(memory_type);
        objects_stats_[index].AddValue(value);
    }

    void AddMemoryValue(uint64_t value, MemoryTypeStats memory_type)
    {
        auto index = static_cast<size_t>(memory_type);
        memory_stats_[index].AddValue(value);
    }

    void AddTimeValue(uint64_t value, TimeTypeStats time_type)
    {
        auto index = static_cast<size_t>(time_type);
        time_stats_[index].AddValue(value);
    }

    void AddReclaimRatioValue(double value)
    {
        reclaim_bytes_.AddValue(value);
    }

    void AddCopiedRatioValue(double value)
    {
        copied_bytes_.AddValue(value);
    }

    PandaString GetDump(GCType gc_type);

    virtual ~GCInstanceStats() = default;

    NO_COPY_SEMANTIC(GCInstanceStats);
    NO_MOVE_SEMANTIC(GCInstanceStats);

private:
    PandaString GetYoungSpaceDump(GCType gc_type);
    PandaString GetAllSpacesDump(GCType gc_type);
    std::array<SimpleHistogram<uint64_t>, OBJECT_TYPE_STATS_SIZE> objects_stats_;
    std::array<SimpleHistogram<uint64_t>, MEMORY_TYPE_STATS_SIZE> memory_stats_;
    std::array<SimpleHistogram<uint64_t>, TIME_TYPE_STATS_SIZE> time_stats_;
    SimpleHistogram<double> reclaim_bytes_;
    SimpleHistogram<double> copied_bytes_;
};

// scoped all field GCStats except pause_
class GCScopedStats {
public:
    explicit GCScopedStats(GCStats *stats, GCInstanceStats *instance_stats = nullptr);

    NO_COPY_SEMANTIC(GCScopedStats);
    NO_MOVE_SEMANTIC(GCScopedStats);

    ~GCScopedStats();

private:
    uint64_t start_time_;
    GCInstanceStats *instance_stats_;
    GCStats *stats_;
};

// scoped field GCStats while GC in pause
class GCScopedPauseStats {
public:
    explicit GCScopedPauseStats(GCStats *stats, GCInstanceStats *instance_stats = nullptr);

    NO_COPY_SEMANTIC(GCScopedPauseStats);
    NO_MOVE_SEMANTIC(GCScopedPauseStats);

    ~GCScopedPauseStats();

private:
    uint64_t start_time_;
    GCInstanceStats *instance_stats_;
    GCStats *stats_;
};

class GCStats {
public:
    explicit GCStats(MemStatsType *mem_stats, GCType gc_type_from_runtime, InternalAllocatorPtr allocator);
    ~GCStats();

    NO_COPY_SEMANTIC(GCStats);
    NO_MOVE_SEMANTIC(GCStats);

    PandaString GetStatistics();

    PandaString GetFinalStatistics(HeapManager *heap_manager);

    uint64_t GetObjectsFreedBytes()
    {
        return objects_freed_bytes_;
    }

    void StartMutatorLock();
    void StopMutatorLock();

private:
    // For convert from nano to 10 seconds
    using PERIOD = std::deca;
    GCType gc_type_ {GCType::INVALID_GC};
    size_t objects_freed_ {0};
    size_t objects_freed_bytes_ {0};
    size_t large_objects_freed_ {0};
    size_t large_objects_freed_bytes_ {0};
    uint64_t start_time_ {0};
    size_t count_mutator_ GUARDED_BY(mutator_stats_lock_) {0};
    uint64_t mutator_start_time_ GUARDED_BY(mutator_stats_lock_) {0};

    uint64_t last_duration_ {0};
    uint64_t total_duration_ {0};
    uint64_t last_pause_ {0};
    uint64_t total_pause_ {0};
    uint64_t total_mutator_pause_ GUARDED_BY(mutator_stats_lock_) {0};

    uint64_t last_start_duration_ {0};
    // GC in the last PERIOD
    uint64_t count_gc_period_ {0};
    // GC number of times every PERIOD
    PandaVector<uint64_t> *all_number_durations_ {nullptr};

    os::memory::Mutex mutator_stats_lock_;
    MemStatsType *mem_stats_;

    void StartCollectStats();
    void StopCollectStats(GCInstanceStats *instance_stats);

    void RecordPause(uint64_t pause, GCInstanceStats *instance_stats);
    void RecordDuration(uint64_t duration, GCInstanceStats *instance_stats);

    uint64_t ConvertTimeToPeriod(uint64_t time_in_nanos, bool ceil = false);

    InternalAllocatorPtr allocator_ {nullptr};

    friend GCScopedPauseStats;
    friend GCScopedStats;
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_GC_GC_STATS_H_
