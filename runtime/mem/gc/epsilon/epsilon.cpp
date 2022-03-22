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

#include "runtime/mem/gc/epsilon/epsilon.h"

#include "libpandabase/utils/logger.h"
#include "runtime/include/runtime.h"
#include "runtime/include/panda_vm.h"

namespace panda::mem {
template <class LanguageConfig>
EpsilonGC<LanguageConfig>::EpsilonGC(ObjectAllocatorBase *object_allocator, const GCSettings &settings)
    : GCLang<LanguageConfig>(object_allocator, settings)
{
    this->SetType(GCType::EPSILON_GC);
}

template <class LanguageConfig>
void EpsilonGC<LanguageConfig>::InitializeImpl()
{
    InternalAllocatorPtr allocator = this->GetInternalAllocator();
    auto barrier_set = allocator->New<GCDummyBarrierSet>(allocator);
    ASSERT(barrier_set != nullptr);
    this->SetGCBarrierSet(barrier_set);
    LOG(DEBUG, GC) << "Epsilon GC initialized...";
}

template <class LanguageConfig>
void EpsilonGC<LanguageConfig>::RunPhasesImpl([[maybe_unused]] const GCTask &task)
{
    LOG(DEBUG, GC) << "Epsilon GC RunPhases...";
    GCScopedPauseStats scoped_pause_stats(this->GetPandaVm()->GetGCStats());
}

// NOLINTNEXTLINE(misc-unused-parameters)
template <class LanguageConfig>
void EpsilonGC<LanguageConfig>::WaitForGC([[maybe_unused]] const GCTask &task)
{
}

template <class LanguageConfig>
void EpsilonGC<LanguageConfig>::InitGCBits([[maybe_unused]] panda::ObjectHeader *obj_header)
{
}

template <class LanguageConfig>
void EpsilonGC<LanguageConfig>::InitGCBitsForAllocationInTLAB([[maybe_unused]] panda::ObjectHeader *obj_header)
{
    LOG(FATAL, GC) << "TLABs are not supported by this GC";
}

template <class LanguageConfig>
void EpsilonGC<LanguageConfig>::Trigger()
{
}

template <class LanguageConfig>
void EpsilonGC<LanguageConfig>::MarkReferences([[maybe_unused]] PandaStackTL<ObjectHeader *> *references,
                                               [[maybe_unused]] GCPhase gc_phase)
{
}

template class EpsilonGC<PandaAssemblyLanguageConfig>;

}  // namespace panda::mem
