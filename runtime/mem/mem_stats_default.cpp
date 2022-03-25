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

#include "runtime/mem/mem_stats_default.h"

namespace panda::mem {

void MemStatsDefault::RecordAllocatedClass([[maybe_unused]] Class *cls) {}
PandaString MemStatsDefault::GetAdditionalStatistics([[maybe_unused]] HeapManager *heap_manager) const
{
    return "";
}

void MemStatsDefault::RecordGCPhaseStart([[maybe_unused]] GCPhase phase) {}

void MemStatsDefault::RecordGCPhaseEnd() {}

double MemStatsDefault::GetMinGCPhaseTime([[maybe_unused]] GCPhase phase) const
{
    return 0;
}

double MemStatsDefault::GetMaxGCPhaseTime([[maybe_unused]] GCPhase phase) const
{
    return 0;
}

double MemStatsDefault::GetAverageGCPhaseTime([[maybe_unused]] GCPhase phase) const
{
    return 0;
}

double MemStatsDefault::GetTotalGCPhaseTime([[maybe_unused]] GCPhase phase) const
{
    return 0;
}
}  // namespace panda::mem
