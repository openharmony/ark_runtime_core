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

#ifndef PANDA_RUNTIME_MEM_GC_EPSILON_EPSILON_H_
#define PANDA_RUNTIME_MEM_GC_EPSILON_EPSILON_H_

#include "runtime/mem/gc/lang/gc_lang.h"

namespace panda::mem {

template <class LanguageConfig>
class EpsilonGC final : public GCLang<LanguageConfig> {
public:
    explicit EpsilonGC(ObjectAllocatorBase *object_allocator, const GCSettings &settings);
    ~EpsilonGC() override = default;
    DEFAULT_MOVE_SEMANTIC(EpsilonGC);
    DEFAULT_COPY_SEMANTIC(EpsilonGC);

    void RunPhases(const GCTask &task);

    void WaitForGC(const GCTask &task) override;

    void InitGCBits(panda::ObjectHeader *obj_header) override;

    void InitGCBitsForAllocationInTLAB(panda::ObjectHeader *obj_header) override;

    void Trigger() override;

private:
    void InitializeImpl() override;
    void RunPhasesImpl(const GCTask &task) override;
    void MarkReferences(PandaStackTL<ObjectHeader *> *references, GCPhase gc_phase) override;
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_GC_EPSILON_EPSILON_H_
