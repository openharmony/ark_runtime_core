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

#include "libpandabase/utils/time.h"
#include "libpandabase/utils/type_converter.h"
#include "runtime/include/runtime.h"
#include "runtime/include/panda_vm.h"
#include "runtime/mem/gc/gc_stats.h"
#include "runtime/mem/mem_stats.h"

namespace panda::mem {

PandaString GCStats::GetStatistics()
{
    PandaStringStream statistic;
    statistic << time::GetCurrentTimeString() << " ";

    statistic << GC_NAMES[ToIndex(gc_type_)] << " ";
    statistic << "freed " << objects_freed_ << "(" << helpers::MemoryConverter(objects_freed_bytes_) << "), ";
    statistic << large_objects_freed_ << "(" << helpers::MemoryConverter(large_objects_freed_bytes_)
              << ") LOS objects, ";

    constexpr uint16_t MAX_PERCENT = 100;
    size_t total_heap = mem::MemConfig::GetObjectPoolSize();
    size_t allocated_now = mem_stats_->GetFootprintHeap();
    uint16_t percent = round((1 - (allocated_now * 1.0 / total_heap)) * MAX_PERCENT);
    statistic << percent << "% free, " << helpers::MemoryConverter(allocated_now) << "/"
              << helpers::MemoryConverter(total_heap) << ", ";
    statistic << "paused " << helpers::TimeConverter(last_pause_) << " total "
              << helpers::TimeConverter(last_duration_);

    return statistic.str();
}

PandaString GCStats::GetFinalStatistics(HeapManager *heap_manager)
{
    auto total_time = ConvertTimeToPeriod(time::GetCurrentTimeInNanos() - start_time_, true);
    auto total_time_gc = helpers::TimeConverter(total_duration_);
    auto total_allocated = mem_stats_->GetAllocatedHeap();
    auto total_freed = mem_stats_->GetFreedHeap();
    auto total_objects = mem_stats_->GetTotalObjectsAllocated();

    auto current_memory = mem_stats_->GetFootprintHeap();
    auto total_memory = heap_manager->GetTotalMemory();
    auto max_memory = heap_manager->GetMaxMemory();

    Histogram<uint64_t> duration_info(all_number_durations_->begin(), all_number_durations_->end());

    if (count_gc_period_ != 0U) {
        duration_info.AddValue(count_gc_period_);
    }
    if (total_time > duration_info.GetCountDifferent()) {
        duration_info.AddValue(0, total_time - duration_info.GetCountDifferent());
    }
    PandaStringStream statistic;

    statistic << heap_manager->GetGC()->DumpStatistics() << "\n";

    statistic << "Total time spent in GC: " << total_time_gc << "\n";

    statistic << "Mean GC size throughput "
              << helpers::MemoryConverter(total_allocated / total_time_gc.GetDoubleValue()) << "/"
              << total_time_gc.GetLiteral() << "\n";
    statistic << "Mean GC object throughput: " << std::scientific << total_objects / total_time_gc.GetDoubleValue()
              << " objects/" << total_time_gc.GetLiteral() << "\n";
    statistic << "Total number of allocations " << total_objects << "\n";
    statistic << "Total bytes allocated " << helpers::MemoryConverter(total_allocated) << "\n";
    statistic << "Total bytes freed " << helpers::MemoryConverter(total_freed) << "\n\n";

    statistic << "Free memory " << helpers::MemoryConverter(helpers::UnsignedDifference(total_memory, current_memory))
              << "\n";
    statistic << "Free memory until GC " << helpers::MemoryConverter(heap_manager->GetFreeMemory()) << "\n";
    statistic << "Free memory until OOME "
              << helpers::MemoryConverter(helpers::UnsignedDifference(max_memory, total_memory)) << "\n";
    statistic << "Total memory " << helpers::MemoryConverter(total_memory) << "\n";

    {
        os::memory::LockHolder lock(mutator_stats_lock_);
        statistic << "Total mutator paused time: " << helpers::TimeConverter(total_mutator_pause_) << "\n";
    }
    statistic << "Total time waiting for GC to complete: " << helpers::TimeConverter(total_pause_) << "\n";
    statistic << "Total GC count: " << duration_info.GetSum() << "\n";
    statistic << "Total GC time: " << total_time_gc << "\n";
    statistic << "Total blocking GC count: " << duration_info.GetSum() << "\n";
    statistic << "Total blocking GC time: " << total_time_gc << "\n";
    statistic << "Histogram of GC count per 10000 ms: " << duration_info.GetTopDump() << "\n";
    statistic << "Histogram of blocking GC count per 10000 ms: " << duration_info.GetTopDump() << "\n";

    statistic << "Native bytes registered: " << heap_manager->GetGC()->GetNativeBytesRegistered() << "\n\n";

    statistic << "Max memory " << helpers::MemoryConverter(max_memory) << "\n";

    return statistic.str();
}

GCStats::GCStats(MemStatsType *mem_stats, GCType gc_type_from_runtime, InternalAllocatorPtr allocator)
    : mem_stats_(mem_stats), allocator_(allocator)
{
    start_time_ = time::GetCurrentTimeInNanos();
    all_number_durations_ = allocator_->New<PandaVector<uint64_t>>(allocator_->Adapter());
    gc_type_ = gc_type_from_runtime;
}

GCStats::~GCStats()
{
    gc_type_ = GCType::INVALID_GC;

    if (all_number_durations_ != nullptr) {
        allocator_->Delete(all_number_durations_);
    }
    all_number_durations_ = nullptr;
}

void GCStats::StartMutatorLock()
{
    os::memory::LockHolder lock(mutator_stats_lock_);
    if (count_mutator_ == 0) {
        mutator_start_time_ = time::GetCurrentTimeInNanos();
    }
    ++count_mutator_;
}

void GCStats::StopMutatorLock()
{
    os::memory::LockHolder lock(mutator_stats_lock_);
    if (count_mutator_ == 0) {
        return;
    }
    if (count_mutator_ == 1) {
        total_mutator_pause_ += time::GetCurrentTimeInNanos() - mutator_start_time_;
        mutator_start_time_ = 0;
    }
    --count_mutator_;
}

void GCStats::StartCollectStats()
{
    objects_freed_ = mem_stats_->GetTotalObjectsFreed();
    objects_freed_bytes_ = mem_stats_->GetFootprintHeap();
    large_objects_freed_ = mem_stats_->GetTotalHumongousObjectsFreed();
    large_objects_freed_bytes_ = mem_stats_->GetFootprint(SpaceType::SPACE_TYPE_HUMONGOUS_OBJECT);
}

void GCStats::StopCollectStats(GCInstanceStats *instance_stats)
{
    objects_freed_ = mem_stats_->GetTotalObjectsFreed() - objects_freed_;
    large_objects_freed_ = mem_stats_->GetTotalHumongousObjectsFreed() - large_objects_freed_;

    size_t current_objects_freed_bytes = mem_stats_->GetFootprintHeap();
    size_t current_large_objects_freed_bytes = mem_stats_->GetFootprint(SpaceType::SPACE_TYPE_HUMONGOUS_OBJECT);

    if (objects_freed_bytes_ < current_objects_freed_bytes) {
        objects_freed_bytes_ = 0;
    } else {
        objects_freed_bytes_ -= current_objects_freed_bytes;
    }

    if (large_objects_freed_bytes_ < current_large_objects_freed_bytes) {
        large_objects_freed_bytes_ = 0;
    } else {
        large_objects_freed_bytes_ -= current_large_objects_freed_bytes;
    }
    if ((instance_stats != nullptr) && (objects_freed_ > 0)) {
        instance_stats->AddMemoryValue(objects_freed_bytes_, MemoryTypeStats::ALL_FREED_BYTES);
        instance_stats->AddObjectsValue(objects_freed_, ObjectTypeStats::ALL_FREED_OBJECTS);
    }
}

uint64_t GCStats::ConvertTimeToPeriod(uint64_t time_in_nanos, bool ceil)
{
    std::chrono::nanoseconds nanos(time_in_nanos);
    if (ceil) {
        using RESULT_DURATION = std::chrono::duration<double, PERIOD>;
        return std::ceil(std::chrono::duration_cast<RESULT_DURATION>(nanos).count());
    }
    using RESULT_DURATION = std::chrono::duration<uint64_t, PERIOD>;
    return std::chrono::duration_cast<RESULT_DURATION>(nanos).count();
}

void GCStats::RecordPause(uint64_t pause, GCInstanceStats *instance_stats)
{
    if ((instance_stats != nullptr) && (pause > 0)) {
        instance_stats->AddTimeValue(pause, TimeTypeStats::ALL_PAUSED_TIME);
    }
    last_pause_ = pause;
    total_pause_ += pause;
}

void GCStats::RecordDuration(uint64_t duration, GCInstanceStats *instance_stats)
{
    uint64_t start_time_duration = ConvertTimeToPeriod(time::GetCurrentTimeInNanos() - start_time_ - duration);
    // every PERIOD
    if ((count_gc_period_ != 0U) && (last_start_duration_ != start_time_duration)) {
        all_number_durations_->push_back(count_gc_period_);
        count_gc_period_ = 0U;
    }
    last_start_duration_ = start_time_duration;
    ++count_gc_period_;
    if ((instance_stats != nullptr) && (duration > 0)) {
        instance_stats->AddTimeValue(duration, TimeTypeStats::ALL_TOTAL_TIME);
    }
    last_duration_ = duration;
    total_duration_ += duration;
}

GCScopedStats::GCScopedStats(GCStats *stats, GCInstanceStats *instance_stats)
    : start_time_(time::GetCurrentTimeInNanos()), instance_stats_(instance_stats), stats_(stats)
{
    stats_->StartCollectStats();
}

GCScopedStats::~GCScopedStats()
{
    stats_->StopCollectStats(instance_stats_);
    stats_->RecordDuration(time::GetCurrentTimeInNanos() - start_time_, instance_stats_);
}

GCScopedPauseStats::GCScopedPauseStats(GCStats *stats, GCInstanceStats *instance_stats)
    : start_time_(time::GetCurrentTimeInNanos()), instance_stats_(instance_stats), stats_(stats)
{
}

GCScopedPauseStats::~GCScopedPauseStats()
{
    stats_->RecordPause(time::GetCurrentTimeInNanos() - start_time_, instance_stats_);
}

PandaString GCInstanceStats::GetDump(GCType gc_type)
{
    PandaStringStream statistic;

    bool young_space = time_stats_[ToIndex(TimeTypeStats::YOUNG_TOTAL_TIME)].GetSum() > 0U;
    bool all_space = time_stats_[ToIndex(TimeTypeStats::ALL_TOTAL_TIME)].GetSum() > 0U;
    bool minor_gc = copied_bytes_.GetCount() > 0U;
    bool was_deleted = reclaim_bytes_.GetCount() > 0U;
    bool was_moved = memory_stats_[ToIndex(MemoryTypeStats::MOVED_BYTES)].GetCount() > 0U;

    if (young_space) {
        statistic << GetYoungSpaceDump(gc_type);
    } else if (all_space) {
        statistic << GetAllSpacesDump(gc_type);
    }

    if (was_deleted) {
        statistic << "Average GC reclaim bytes ratio " << reclaim_bytes_.GetAvg() << " over "
                  << reclaim_bytes_.GetCount() << " GC cycles \n";
    }

    if (minor_gc) {
        statistic << "Average minor GC copied live bytes ratio " << copied_bytes_.GetAvg() << " over "
                  << copied_bytes_.GetCount() << " minor GCs \n";
    }

    if (was_moved) {
        statistic << "Cumulative bytes moved "
                  << memory_stats_[ToIndex(MemoryTypeStats::MOVED_BYTES)].GetGeneralStatistic() << "\n";
        statistic << "Cumulative objects moved "
                  << objects_stats_[ToIndex(ObjectTypeStats::MOVED_OBJECTS)].GetGeneralStatistic() << "\n";
    }

    return statistic.str();
}

PandaString GCInstanceStats::GetYoungSpaceDump(GCType gc_type)
{
    PandaStringStream statistic;
    statistic << "young " << GC_NAMES[ToIndex(gc_type)]
              << " paused: " << time_stats_[ToIndex(TimeTypeStats::YOUNG_PAUSED_TIME)].GetGeneralStatistic() << "\n";

    auto &young_total_time_hist = time_stats_[ToIndex(TimeTypeStats::YOUNG_TOTAL_TIME)];
    auto young_total_time = helpers::TimeConverter(young_total_time_hist.GetSum());
    auto young_total_freed_obj = objects_stats_[ToIndex(ObjectTypeStats::YOUNG_FREED_OBJECTS)].GetSum();
    auto young_total_freed_bytes = memory_stats_[ToIndex(MemoryTypeStats::YOUNG_FREED_BYTES)].GetSum();

    statistic << "young " << GC_NAMES[ToIndex(gc_type)] << " total time: " << young_total_time
              << " mean time: " << helpers::TimeConverter(young_total_time_hist.GetAvg()) << "\n";
    statistic << "young " << GC_NAMES[ToIndex(gc_type)] << " freed: " << young_total_freed_obj << " with total size "
              << helpers::MemoryConverter(young_total_freed_bytes) << "\n";

    statistic << "young " << GC_NAMES[ToIndex(gc_type)] << " throughput: " << std::scientific
              << young_total_freed_obj / young_total_time.GetDoubleValue() << "objects/"
              << young_total_time.GetLiteral() << " / "
              << helpers::MemoryConverter(young_total_freed_bytes / young_total_time.GetDoubleValue()) << "/"
              << young_total_time.GetLiteral() << "\n";

    return statistic.str();
}

PandaString GCInstanceStats::GetAllSpacesDump(GCType gc_type)
{
    PandaStringStream statistic;

    statistic << GC_NAMES[ToIndex(gc_type)]
              << " paused: " << time_stats_[ToIndex(TimeTypeStats::ALL_PAUSED_TIME)].GetGeneralStatistic() << "\n";

    auto &total_time_hist = time_stats_[ToIndex(TimeTypeStats::ALL_TOTAL_TIME)];
    auto total_time = helpers::TimeConverter(total_time_hist.GetSum());
    auto total_freed_obj = objects_stats_[ToIndex(ObjectTypeStats::ALL_FREED_OBJECTS)].GetSum();
    auto total_freed_bytes = memory_stats_[ToIndex(MemoryTypeStats::ALL_FREED_BYTES)].GetSum();

    statistic << GC_NAMES[ToIndex(gc_type)] << " total time: " << total_time
              << " mean time: " << helpers::TimeConverter(total_time_hist.GetAvg()) << "\n";
    statistic << GC_NAMES[ToIndex(gc_type)] << " freed: " << total_freed_obj << " with total size "
              << helpers::MemoryConverter(total_freed_bytes) << "\n";

    statistic << GC_NAMES[ToIndex(gc_type)] << " throughput: " << std::scientific
              << total_freed_obj / total_time.GetDoubleValue() << "objects/" << total_time.GetLiteral() << " / "
              << helpers::MemoryConverter(total_freed_bytes / total_time.GetDoubleValue()) << "/"
              << total_time.GetLiteral() << "\n";

    return statistic.str();
}

}  // namespace panda::mem
