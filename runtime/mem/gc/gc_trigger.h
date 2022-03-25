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

#ifndef PANDA_RUNTIME_MEM_GC_GC_TRIGGER_H_
#define PANDA_RUNTIME_MEM_GC_GC_TRIGGER_H_

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "libpandabase/macros.h"
#include "runtime/mem/gc/gc.h"

namespace panda {

class RuntimeOptions;

namespace mem {

enum class GCTriggerType {
    INVALID_TRIGGER,
    HEAP_TRIGGER_TEST,   // TRIGGER with low thresholds for tests
    HEAP_TRIGGER,        // Standard TRIGGER with production ready thresholds
    NO_GC_FOR_START_UP,  // A non-production strategy, TRIGGER GC after the app starts up
    DEBUG,               // Debug TRIGGER which always returns true
    GCTRIGGER_LAST = DEBUG,
};

class GCTriggerConfig {
public:
    GCTriggerConfig(std::string gc_trigger_type, uint64_t debug_start, size_t min_extra_heap_size,
                    size_t max_extra_heap_size, uint32_t skip_startup_gc_count = 0)
        : gc_trigger_type_(std::move(gc_trigger_type)),
          debug_start_(debug_start),
          min_extra_heap_size_(min_extra_heap_size),
          max_extra_heap_size_(max_extra_heap_size),
          skip_startup_gc_count_(skip_startup_gc_count)
    {
    }
    ~GCTriggerConfig() = default;
    DEFAULT_MOVE_SEMANTIC(GCTriggerConfig);
    DEFAULT_COPY_SEMANTIC(GCTriggerConfig);

    std::string_view GetGCTriggerType() const
    {
        return gc_trigger_type_;
    }

    uint64_t GetDebugStart() const
    {
        return debug_start_;
    }

    size_t GetMinExtraHeapSize() const
    {
        return min_extra_heap_size_;
    }

    size_t GetMaxExtraHeapSize() const
    {
        return max_extra_heap_size_;
    }

    uint32_t GetSkipStartupGcCount() const
    {
        return skip_startup_gc_count_;
    }

private:
    std::string gc_trigger_type_;
    uint64_t debug_start_;
    size_t min_extra_heap_size_;
    size_t max_extra_heap_size_;
    uint32_t skip_startup_gc_count_;
};

class GCTrigger : public GCListener {
public:
    GCTrigger() = default;
    ~GCTrigger() override;
    NO_COPY_SEMANTIC(GCTrigger);
    NO_MOVE_SEMANTIC(GCTrigger);

    /**
     * \brief Checks if GC required
     * @return returns true if GC should be executed
     */
    virtual bool IsGcTriggered() = 0;
    virtual size_t GetTargetFootprint() = 0;
    virtual void SetMinTargetFootprint([[maybe_unused]] size_t heap_size) {}
    virtual void RestoreMinTargetFootprint() {}

private:
    friend class GC;
};

/**
 * Triggers when heap increased by predefined %
 */
class GCTriggerHeap : public GCTrigger {
public:
    explicit GCTriggerHeap(MemStatsType *mem_stats);
    explicit GCTriggerHeap(MemStatsType *mem_stats, size_t min_heap_size, uint8_t percent_threshold,
                           size_t min_extra_size, size_t max_extra_size, uint32_t skip_gc_times = 0);
    ~GCTriggerHeap() override = default;
    NO_MOVE_SEMANTIC(GCTriggerHeap);
    NO_COPY_SEMANTIC(GCTriggerHeap);

    bool IsGcTriggered() override;

    void GCStarted(size_t heap_size) override;
    void GCFinished(const GCTask &task, size_t heap_size_before_gc, size_t heap_size) override;
    size_t GetTargetFootprint() override;
    void SetMinTargetFootprint(size_t target_size) override;
    void RestoreMinTargetFootprint() override;
    void ComputeNewTargetFootprint(const GCTask &task, size_t heap_size_before_gc, size_t heap_size);

private:
    static constexpr size_t MIN_HEAP_SIZE_FOR_TRIGGER = 512;
    static constexpr size_t DEFAULT_MIN_TARGET_FOOTPRINT = 256;
    static constexpr size_t DEFAULT_MIN_EXTRA_HEAP_SIZE = 32;      // For heap-trigger-test
    static constexpr size_t DEFAULT_MAX_EXTRA_HEAP_SIZE = 512_KB;  // For heap-trigger-test
    static constexpr uint8_t DEFAULT_PERCENTAGE_THRESHOLD = 10;

    size_t min_target_footprint_ {DEFAULT_MIN_TARGET_FOOTPRINT};
    std::atomic<size_t> target_footprint_ {MIN_HEAP_SIZE_FOR_TRIGGER};

    /**
     * We'll trigger if heap increased by delta, delta = heap_size_after_last_gc * percent_threshold_ %
     * And the constraint on delta is: min_extra_size_ <= delta <= max_extra_size_
     */
    uint8_t percent_threshold_ {DEFAULT_PERCENTAGE_THRESHOLD};
    size_t min_extra_size_ {DEFAULT_MIN_EXTRA_HEAP_SIZE};
    size_t max_extra_size_ {DEFAULT_MAX_EXTRA_HEAP_SIZE};
    MemStatsType *mem_stats_;
    uint8_t skip_gc_count_ {0};
};

/**
 * Trigger always returns true after given start
 */
class GCTriggerDebug : public GCTrigger {
public:
    GCTriggerDebug() = default;
    explicit GCTriggerDebug(uint64_t debug_start);
    ~GCTriggerDebug() override = default;
    NO_MOVE_SEMANTIC(GCTriggerDebug);
    NO_COPY_SEMANTIC(GCTriggerDebug);

    bool IsGcTriggered() override;

    void GCStarted(size_t heap_size) override;
    void GCFinished(const GCTask &task, size_t heap_size_before_gc, size_t heap_size) override;
    size_t GetTargetFootprint() override;

private:
    uint64_t debug_start_ = 0;
};

GCTrigger *CreateGCTrigger(MemStatsType *mem_stats, const GCTriggerConfig &config, InternalAllocatorPtr allocator);

}  // namespace mem
}  // namespace panda

#endif  // PANDA_RUNTIME_MEM_GC_GC_TRIGGER_H_
