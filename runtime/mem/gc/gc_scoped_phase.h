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

#ifndef PANDA_RUNTIME_MEM_GC_GC_SCOPED_PHASE_H_
#define PANDA_RUNTIME_MEM_GC_GC_SCOPED_PHASE_H_

#include "libpandabase/macros.h"
#include "libpandabase/utils/logger.h"
#include "runtime/include/mem/panda_string.h"
#include "runtime/mem/gc/gc_phase.h"
#include "runtime/mem/mem_stats_additional_info.h"
#include "runtime/mem/mem_stats_default.h"

namespace panda::mem {

// forward declarations:
class GC;

class GCScopedPhase {
public:
    GCScopedPhase(MemStatsType *mem_stats, GC *gc, GCPhase new_phase);
    NO_COPY_SEMANTIC(GCScopedPhase);
    NO_MOVE_SEMANTIC(GCScopedPhase);

    ~GCScopedPhase();

    static PandaString GetPhaseName(GCPhase phase)
    {
        switch (phase) {
            case GCPhase::GC_PHASE_IDLE:
                return "Idle";
            case GCPhase::GC_PHASE_RUNNING:
                return "RunPhases()";
            case GCPhase::GC_PHASE_COLLECT_ROOTS:
                return "CollectRoots()";
            case GCPhase::GC_PHASE_INITIAL_MARK:
                return "InitialMark";
            case GCPhase::GC_PHASE_MARK:
                return "MarkAll()";
            case GCPhase::GC_PHASE_MARK_YOUNG:
                return "MarkYoung()";
            case GCPhase::GC_PHASE_REMARK:
                return "YoungRemark()";
            case GCPhase::GC_PHASE_COLLECT_YOUNG_AND_MOVE:
                return "CollectYoungAndMove()";
            case GCPhase::GC_PHASE_SWEEP_STRING_TABLE:
                return "SweepStringTable()";
            case GCPhase::GC_PHASE_SWEEP_STRING_TABLE_YOUNG:
                return "SweepStringTableYoung()";
            case GCPhase::GC_PHASE_SWEEP:
                return "Sweep()";
            case GCPhase::GC_PHASE_CLEANUP:
                return "Cleanup()";
            default:
                return "UnknownPhase";
        }
    }

    static PandaString GetPhaseAbbr(GCPhase phase)
    {
        switch (phase) {
            case GCPhase::GC_PHASE_IDLE:
                return "Idle";
            case GCPhase::GC_PHASE_RUNNING:
                return "RunPhases";
            case GCPhase::GC_PHASE_COLLECT_ROOTS:
                return "ColRoots";
            case GCPhase::GC_PHASE_INITIAL_MARK:
                return "InitMark";
            case GCPhase::GC_PHASE_MARK:
                return "Mark";
            case GCPhase::GC_PHASE_MARK_YOUNG:
                return "MarkY";
            case GCPhase::GC_PHASE_REMARK:
                return "YRemark";
            case GCPhase::GC_PHASE_COLLECT_YOUNG_AND_MOVE:
                return "ColYAndMove";
            case GCPhase::GC_PHASE_SWEEP_STRING_TABLE:
                return "SweepStrT";
            case GCPhase::GC_PHASE_SWEEP_STRING_TABLE_YOUNG:
                return "SweepStrTY()";
            case GCPhase::GC_PHASE_SWEEP:
                return "Sweep";
            case GCPhase::GC_PHASE_CLEANUP:
                return "Cleanup";
            default:
                return "UnknownPhase";
        }
    }

private:
    GCPhase phase_;
    GCPhase old_phase_;
    GC *gc_;
    MemStatsType *mem_stats_;

    PandaString GetGCName();
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_GC_GC_SCOPED_PHASE_H_
