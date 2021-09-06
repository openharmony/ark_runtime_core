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

#include "runtime/mem/mem_stats_additional_info.h"

#include "runtime/include/class-inl.h"
#include "runtime/include/class_linker.h"
#include "runtime/include/runtime.h"

namespace panda::mem {

PandaString MemStatsAdditionalInfo::GetAdditionalStatistics(HeapManager *heap_manager)
{
    PandaVector<Class *> classes;
    auto class_linker = Runtime::GetCurrent()->GetClassLinker();
    class_linker->EnumerateClasses([&classes](Class *cls) {
        classes.push_back(cls);
        return true;
    });

    PandaVector<uint64_t> footprint_of_classes(classes.size(), 0U);
    heap_manager->CountInstances(classes, true, footprint_of_classes.data());

    PandaMultiMap<uint64_t, Class *> footprint_to_class;
    for (size_t index = 0; index < classes.size(); ++index) {
        footprint_to_class.insert({footprint_of_classes[index], classes[index]});
    }

    PandaStringStream statistic;
    PandaMultiMap<uint64_t, Class *>::reverse_iterator rit;
    for (rit = footprint_to_class.rbegin(); rit != footprint_to_class.rend(); ++rit) {
        if (rit->first == 0U) {
            break;
        }
        auto clazz = rit->second;
        statistic << "class: " << clazz->GetName() << ", footprint - " << rit->first << std::endl;
    }
    return statistic.str();
}

void MemStatsAdditionalInfo::RecordGCPhaseStart(GCPhase phase)
{
    os::memory::LockHolder lk(phase_lock_);
    if (current_phase_ != GCPhase::GC_PHASE_LAST) {
        RecordGCPauseEnd();
    }
    phase_start_time_ = clock::now();
    current_phase_ = phase;
}

void MemStatsAdditionalInfo::RecordGCPhaseEnd()
{
    os::memory::LockHolder lk(phase_lock_);
    ASSERT(current_phase_ != GCPhase::GC_PHASE_LAST);

    uint phase_index = ToIndex(current_phase_);
    duration phase_time = clock::now() - phase_start_time_;
    if (phase_count_[phase_index] != 0) {
        min_phase_time_[phase_index] = std::min(min_phase_time_[phase_index], phase_time);
        max_phase_time_[phase_index] = std::max(max_phase_time_[phase_index], phase_time);
    } else {
        min_phase_time_[phase_index] = phase_time;
        max_phase_time_[phase_index] = phase_time;
    }
    phase_count_[phase_index]++;
    sum_phase_time_[phase_index] += phase_time;

    current_phase_ = GCPhase::GC_PHASE_LAST;
}

uint64_t MemStatsAdditionalInfo::GetMinGCPhaseTime(GCPhase phase)
{
    os::memory::LockHolder lk(phase_lock_);
    return std::chrono::duration_cast<std::chrono::milliseconds>(min_phase_time_[ToIndex(phase)]).count();
}

uint64_t MemStatsAdditionalInfo::GetMaxGCPhaseTime(GCPhase phase)
{
    os::memory::LockHolder lk(phase_lock_);
    return std::chrono::duration_cast<std::chrono::milliseconds>(max_phase_time_[ToIndex(phase)]).count();
}

uint64_t MemStatsAdditionalInfo::GetAverageGCPhaseTime(GCPhase phase)
{
    os::memory::LockHolder lk(phase_lock_);
    return phase_count_[ToIndex(phase)] != 0
               ? std::chrono::duration_cast<std::chrono::milliseconds>(sum_phase_time_[ToIndex(phase)]).count() /
                     phase_count_[ToIndex(phase)]
               : 0;
}

uint64_t MemStatsAdditionalInfo::GetTotalGCPhaseTime(GCPhase phase)
{
    os::memory::LockHolder lk(phase_lock_);
    return std::chrono::duration_cast<std::chrono::milliseconds>(sum_phase_time_[ToIndex(phase)]).count();
}

}  // namespace panda::mem
