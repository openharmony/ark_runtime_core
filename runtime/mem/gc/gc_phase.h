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

#ifndef PANDA_RUNTIME_MEM_GC_GC_PHASE_H_
#define PANDA_RUNTIME_MEM_GC_GC_PHASE_H_

namespace panda::mem {

enum class GCPhase {
    GC_PHASE_IDLE,  // GC waits for trigger event
    GC_PHASE_RUNNING,
    GC_PHASE_COLLECT_ROOTS,
    GC_PHASE_INITIAL_MARK,
    GC_PHASE_MARK,
    GC_PHASE_MARK_YOUNG,
    GC_PHASE_REMARK,
    GC_PHASE_COLLECT_YOUNG_AND_MOVE,
    GC_PHASE_SWEEP_STRING_TABLE,
    GC_PHASE_SWEEP_STRING_TABLE_YOUNG,
    GC_PHASE_SWEEP,
    GC_PHASE_CLEANUP,
    GC_PHASE_LAST
};

constexpr size_t ToIndex(GCPhase phase)
{
    return static_cast<size_t>(phase);
}

constexpr GCPhase ToGCPhase(uint8_t index)
{
    return static_cast<GCPhase>(index);
}

constexpr bool IsMarking(GCPhase phase)
{
    return phase == GCPhase::GC_PHASE_MARK_YOUNG || phase == GCPhase::GC_PHASE_MARK ||
           phase == GCPhase::GC_PHASE_INITIAL_MARK || phase == GCPhase::GC_PHASE_REMARK;
}

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_GC_GC_PHASE_H_
