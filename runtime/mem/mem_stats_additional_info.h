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

#ifndef PANDA_RUNTIME_MEM_MEM_STATS_ADDITIONAL_INFO_H_
#define PANDA_RUNTIME_MEM_MEM_STATS_ADDITIONAL_INFO_H_

#include <array>

#include "runtime/include/mem/panda_containers.h"
#include "runtime/mem/mem_stats.h"

namespace panda {

class Class;
}  // namespace panda

namespace panda::mem {

/**
 * Implementation of MemStats with additional info about memory.
 */
class MemStatsAdditionalInfo : public MemStats<MemStatsAdditionalInfo> {
    enum STAT_TYPE { BYTES_ALLOCATED = 0, BYTES_FREED, MAX_FOOTPRINT, OBJECTS_ALLOCATED, STAT_TYPE_NUM };

public:
    NO_COPY_SEMANTIC(MemStatsAdditionalInfo);
    NO_MOVE_SEMANTIC(MemStatsAdditionalInfo);

    MemStatsAdditionalInfo() = default;

    PandaString GetAdditionalStatistics(HeapManager *heap_manager);

    void RecordGCPhaseStart(GCPhase phase);
    void RecordGCPhaseEnd();

    uint64_t GetMinGCPhaseTime(GCPhase phase);
    uint64_t GetMaxGCPhaseTime(GCPhase phase);
    uint64_t GetAverageGCPhaseTime(GCPhase phase);
    uint64_t GetTotalGCPhaseTime(GCPhase phase);

    ~MemStatsAdditionalInfo() override = default;

private:
    clock::time_point phase_start_time_ = clock::now();
    GCPhase current_phase_ = GCPhase::GC_PHASE_IDLE;
    std::array<duration, ToIndex(GCPhase::GC_PHASE_LAST)> min_phase_time_ = {};
    std::array<duration, ToIndex(GCPhase::GC_PHASE_LAST)> max_phase_time_ = {};
    std::array<duration, ToIndex(GCPhase::GC_PHASE_LAST)> sum_phase_time_ = {};
    std::array<uint, ToIndex(GCPhase::GC_PHASE_LAST)> phase_count_ = {};
    os::memory::Mutex phase_lock_;
};

extern template class MemStats<MemStatsAdditionalInfo>;

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_MEM_STATS_ADDITIONAL_INFO_H_
