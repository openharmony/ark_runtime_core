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

#ifndef PANDA_RUNTIME_MEM_GC_STW_GC_STW_GC_H_
#define PANDA_RUNTIME_MEM_GC_STW_GC_STW_GC_H_

#include "runtime/include/coretypes/array-inl.h"
#include "runtime/include/mem/panda_containers.h"
#include "runtime/mem/gc/lang/gc_lang.h"
#include "runtime/mem/gc/gc_root.h"

namespace panda::mem {

/**
 * \brief Stop the world, non-concurrent GC
 */
template <class LanguageConfig>
class StwGC final : public GCLang<LanguageConfig> {
public:
    explicit StwGC(ObjectAllocatorBase *object_allocator, const GCSettings &settings);
    ~StwGC() override = default;
    DEFAULT_MOVE_SEMANTIC(StwGC);
    DEFAULT_COPY_SEMANTIC(StwGC);

    void WaitForGC(const GCTask &task) override;

    void InitGCBits(panda::ObjectHeader *object) override;

    void InitGCBitsForAllocationInTLAB(panda::ObjectHeader *obj_header) override;

    void Trigger() override;

private:
    void InitializeImpl() override;
    void RunPhasesImpl(const GCTask &task) override;
    void Mark(const GCTask &task);
    void MarkStack(PandaStackTL<ObjectHeader *> *stack);
    void SweepStringTable();
    void Sweep();

    bool IsMarked(const ObjectHeader *object) const override;
    void MarkObject(ObjectHeader *object) override;
    void UnMarkObject(ObjectHeader *object_header) override;
    void MarkReferences(PandaStackTL<ObjectHeader *> *references, GCPhase gc_phase) override;

    bool reversed_mark_ {false};  // if true - we treat marked objects as dead object
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_GC_STW_GC_STW_GC_H_
