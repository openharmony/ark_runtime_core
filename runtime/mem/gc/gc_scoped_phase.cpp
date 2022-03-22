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

#include "runtime/mem/gc/gc_scoped_phase.h"

#include "runtime/mem/gc/gc.h"

namespace panda::mem {

GCScopedPhase::GCScopedPhase(MemStatsType *mem_stats, GC *gc, GCPhase new_phase) : mem_stats_(mem_stats)
{
    ASSERT(mem_stats != nullptr);
    ASSERT(gc != nullptr);
    gc_ = gc;
    gc_->BeginTracePoint(GetPhaseName(new_phase));
    phase_ = new_phase;
    old_phase_ = gc_->GetGCPhase();
    gc_->SetGCPhase(phase_);
    LOG(DEBUG, GC) << "== " << GetGCName() << "::" << GetPhaseName(phase_) << " started ==";
    mem_stats_->RecordGCPhaseStart(phase_);
}

GCScopedPhase::~GCScopedPhase()
{
    mem_stats_->RecordGCPhaseEnd();
    gc_->SetGCPhase(old_phase_);
    gc_->EndTracePoint();
    LOG(DEBUG, GC) << "== " << GetGCName() << "::" << GetPhaseName(phase_) << " finished ==";
    mem_stats_->RecordGCPhaseStart(old_phase_);
}

PandaString GCScopedPhase::GetGCName()
{
    GCType type = gc_->GetType();
    switch (type) {
        case GCType::EPSILON_GC:
            return "EpsilonGC";
        case GCType::STW_GC:
            return "StwGC";
        case GCType::GEN_GC:
            return "GenGC";
        default:
            return "GC";
    }
}

}  // namespace panda::mem
