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

#ifndef PANDA_RUNTIME_MEM_MEM_STATS_DEFAULT_H_
#define PANDA_RUNTIME_MEM_MEM_STATS_DEFAULT_H_

#include <cstdio>

#include "runtime/mem/mem_stats.h"

namespace panda {
class Class;
}  // namespace panda

namespace panda::mem {

/**
 * Default implementation of MemStats in release mode.
 */
class MemStatsDefault : public MemStats<MemStatsDefault> {
public:
    NO_COPY_SEMANTIC(MemStatsDefault);
    NO_MOVE_SEMANTIC(MemStatsDefault);

    MemStatsDefault() = default;

    void RecordAllocatedClass(Class *cls);

    PandaString GetAdditionalStatistics(HeapManager *heap_manager) const;

    void RecordGCPhaseStart(GCPhase phase);
    void RecordGCPhaseEnd();

    double GetMinGCPhaseTime(GCPhase phase) const;
    double GetMaxGCPhaseTime(GCPhase phase) const;
    double GetAverageGCPhaseTime(GCPhase phase) const;
    double GetTotalGCPhaseTime(GCPhase phase) const;

    ~MemStatsDefault() override = default;
};

extern template class MemStats<MemStatsDefault>;

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_MEM_STATS_DEFAULT_H_
