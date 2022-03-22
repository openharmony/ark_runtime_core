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

#include "runtime/mem/gc/gc_trigger.h"

#include <atomic>

#include "libpandabase/macros.h"
#include "runtime/include/runtime.h"
#include "runtime/include/runtime_options.h"
#include "runtime/include/panda_vm.h"
#include "utils/logger.h"

namespace panda::mem {

static constexpr size_t PERCENT_100 = 100;

GCTrigger::~GCTrigger() = default;

GCTriggerHeap::GCTriggerHeap(MemStatsType *mem_stats) : mem_stats_(mem_stats) {}

GCTriggerHeap::GCTriggerHeap(MemStatsType *mem_stats, size_t min_heap_size, uint8_t percent_threshold,
                             size_t min_extra_size, size_t max_extra_size, uint32_t skip_gc_times)
    : mem_stats_(mem_stats), skip_gc_count_(skip_gc_times)
{
    percent_threshold_ = percent_threshold;
    min_extra_size_ = min_extra_size;
    max_extra_size_ = max_extra_size;
    // If we have min_heap_size < 100, we get false positives in IsGcTriggered, since we divide by 100 first
    ASSERT(min_heap_size >= 100);
    target_footprint_.store((min_heap_size / PERCENT_100) * percent_threshold_, std::memory_order_relaxed);
    LOG(DEBUG, GC_TRIGGER) << "GCTriggerHeap created, min heap size " << min_heap_size << ", percent threshold "
                           << percent_threshold << ", min_extra_size " << min_extra_size << ", max_extra_size "
                           << max_extra_size;
}

void GCTriggerHeap::SetMinTargetFootprint(size_t target_size)
{
    LOG(DEBUG, GC_TRIGGER) << "SetTempTargetFootprint target_footprint = " << target_size;
    min_target_footprint_ = target_size;
    target_footprint_.store(target_size, std::memory_order_relaxed);
}

void GCTriggerHeap::RestoreMinTargetFootprint()
{
    min_target_footprint_ = DEFAULT_MIN_TARGET_FOOTPRINT;
}

void GCTriggerHeap::ComputeNewTargetFootprint(const GCTask &task, size_t heap_size_before_gc, size_t heap_size)
{
    GC *gc = Thread::GetCurrent()->GetVM()->GetGC();
    if (gc->IsGenerational() && task.reason_ == GCTaskCause::YOUNG_GC_CAUSE) {
        // we don't want to update heap-trigger on young-gc
        return;
    }
    // Note: divide by 100 first to avoid overflow
    size_t delta = (heap_size / PERCENT_100) * percent_threshold_;

    if (heap_size > heap_size_before_gc) {  // heap increased corresponding with previous gc
        delta = std::min(delta, max_extra_size_);
    } else {
        // if heap was squeeze from 200mb to 100mb we want to set a target to 150mb, not just 100mb*percent_threshold_
        delta = std::max(delta, (heap_size_before_gc - heap_size) / 2);
    }
    delta = std::max(delta, min_extra_size_);
    size_t target = heap_size + delta;

    target_footprint_.store(target, std::memory_order_relaxed);

    LOG(DEBUG, GC_TRIGGER) << "ComputeNewTargetFootprint target_footprint = " << target;
}

bool GCTriggerHeap::IsGcTriggered()
{
    if (skip_gc_count_ > 0) {
        skip_gc_count_--;
        return false;
    }
    size_t bytes_in_heap = mem_stats_->GetFootprintHeap();
    if (UNLIKELY(bytes_in_heap >= target_footprint_.load(std::memory_order_relaxed))) {
        LOG(DEBUG, GC_TRIGGER) << "GCTriggerHeap triggered";
        return true;
    }
    return false;
}

GCTrigger *CreateGCTrigger(MemStatsType *mem_stats, const GCTriggerConfig &config, InternalAllocatorPtr allocator)
{
    std::string_view gc_trigger_type = config.GetGCTriggerType();
    uint32_t skip_gc_times = config.GetSkipStartupGcCount();

    constexpr size_t DEFAULT_HEAP_SIZE = 8_MB;
    constexpr uint8_t DEFAULT_PERCENT_THRESHOLD = 10;
    auto trigger_type = GCTriggerType::INVALID_TRIGGER;
    if (gc_trigger_type == "heap-trigger-test") {
        trigger_type = GCTriggerType::HEAP_TRIGGER_TEST;
    } else if (gc_trigger_type == "heap-trigger") {
        trigger_type = GCTriggerType::HEAP_TRIGGER;
    } else if (gc_trigger_type == "debug") {
        trigger_type = GCTriggerType::DEBUG;
    } else if (gc_trigger_type == "no-gc-for-start-up") {
        trigger_type = GCTriggerType::NO_GC_FOR_START_UP;
    }
    GCTrigger *ret {nullptr};
    switch (trigger_type) {  // NOLINT(hicpp-multiway-paths-covered)
        case GCTriggerType::HEAP_TRIGGER_TEST:
            ret = allocator->New<GCTriggerHeap>(mem_stats);
            break;
        case GCTriggerType::HEAP_TRIGGER:
            ret = allocator->New<GCTriggerHeap>(mem_stats, DEFAULT_HEAP_SIZE, DEFAULT_PERCENT_THRESHOLD,
                                                config.GetMinExtraHeapSize(), config.GetMaxExtraHeapSize());
            break;
        case GCTriggerType::NO_GC_FOR_START_UP:
            ret = allocator->New<GCTriggerHeap>(mem_stats, DEFAULT_HEAP_SIZE, DEFAULT_PERCENT_THRESHOLD,
                                                config.GetMinExtraHeapSize(), config.GetMaxExtraHeapSize(),
                                                skip_gc_times);
            break;
        case GCTriggerType::DEBUG:
            ret = allocator->New<GCTriggerDebug>(config.GetDebugStart());
            break;
        default:
            LOG(FATAL, GC) << "Wrong GCTrigger type";
            break;
    }
    return ret;
}

void GCTriggerHeap::GCStarted([[maybe_unused]] size_t heap_size) {}

void GCTriggerHeap::GCFinished(const GCTask &task, size_t heap_size_before_gc, size_t heap_size)
{
    ComputeNewTargetFootprint(task, heap_size_before_gc, heap_size);
}

size_t GCTriggerHeap::GetTargetFootprint()
{
    return target_footprint_.load(std::memory_order_relaxed);
}

GCTriggerDebug::GCTriggerDebug(uint64_t debug_start) : debug_start_(debug_start)
{
    LOG(DEBUG, GC_TRIGGER) << "GCTriggerDebug created";
}

bool GCTriggerDebug::IsGcTriggered()
{
    bool ret = false;
    static std::atomic<uint64_t> counter = 0;
    LOG(DEBUG, GC_TRIGGER) << "GCTriggerDebug counter " << counter;
    if (counter >= debug_start_) {
        LOG(DEBUG, GC_TRIGGER) << "GCTriggerDebug triggered";
        ret = true;
    }
    counter++;
    return ret;
}

void GCTriggerDebug::GCStarted([[maybe_unused]] size_t heap_size) {}

void GCTriggerDebug::GCFinished([[maybe_unused]] const GCTask &task, [[maybe_unused]] size_t heap_size_before_gc,
                                [[maybe_unused]] size_t heap_size)
{
}

size_t GCTriggerDebug::GetTargetFootprint()
{
    return 0;
}

}  // namespace panda::mem
