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

#ifndef PANDA_RUNTIME_MEM_GC_GEN_GC_GEN_GC_H_
#define PANDA_RUNTIME_MEM_GC_GEN_GC_GEN_GC_H_

#include "runtime/include/mem/panda_smart_pointers.h"
#include "runtime/mem/gc/card_table.h"
#include "runtime/mem/gc/generational-gc-base.h"

namespace panda {
class ManagedThread;
}  // namespace panda
namespace panda::mem {

/**
 * \brief Generational GC
 */
template <class LanguageConfig>
class GenGC : public GenerationalGC<LanguageConfig> {
public:
    explicit GenGC(ObjectAllocatorBase *object_allocator, const GCSettings &settings);
    ~GenGC() override = default;
    DEFAULT_MOVE_SEMANTIC(GenGC);
    DEFAULT_COPY_SEMANTIC(GenGC);

    void InitGCBits(panda::ObjectHeader *obj_header) override;

    void InitGCBitsForAllocationInTLAB(panda::ObjectHeader *obj_header) override;

    void Trigger() override;

    void MarkReferences(PandaStackTL<ObjectHeader *> *references, GCPhase gc_phase) override;

    void MarkObject(ObjectHeader *object_header) override;

    bool MarkObjectIfNotMarked(ObjectHeader *object_header) override;

    void UnMarkObject(ObjectHeader *object_header) override;

    bool InGCSweepRange(uintptr_t addr) const override;

private:
    void InitializeImpl() override;

    void RunPhasesImpl(const GCTask &task) override;

    void PreStartupImp() override;

    /**
     * GC for young generation. Runs with STW.
     */
    void RunYoungGC(const GCTask &task);

    /**
     * GC for tenured generation.
     */
    void RunTenuredGC(const GCTask &task);

    /**
     * Marks objects in young generation
     */
    void MarkYoung(const GCTask &task);

    void MarkYoungStack(PandaStackTL<ObjectHeader *> *objects_stack);

    /**
     * Mark roots and add them to the stack
     * @param objects_stack
     * @param visit_class_roots
     * @param visit_card_table_roots
     */
    void MarkRoots(PandaStackTL<ObjectHeader *> *objects_stack, CardTableVisitFlag visit_card_table_roots,
                   VisitGCRootFlags flags = VisitGCRootFlags::ACCESS_ROOT_ALL);

    /**
     * Initial marks roots and fill in 1st level from roots into stack.
     * STW
     * @param objects_stack
     */
    void InitialMark(PandaStackTL<ObjectHeader *> *objects_stack);

    /**
     * Concurrently marking all objects
     * @param objects_stack
     */
    NO_THREAD_SAFETY_ANALYSIS void ConcurrentMark(PandaStackTL<ObjectHeader *> *objects_stack,
                                                  CardTableVisitFlag visit_card_table_roots);

    /**
     * ReMarks objects after Concurrent marking
     * @param objects_stack
     */
    void ReMark(PandaStackTL<ObjectHeader *> *objects_stack, const GCTask &task);

    /**
     * Mark all objects in stack recursively for Full GC.
     */
    void MarkStack(PandaStackTL<ObjectHeader *> *stack);

    /**
     * Collect dead objects in young generation and move survivors
     * @return true if moving was success, false otherwise
     */
    bool CollectYoungAndMove(const GCTask &task);

    /**
     * Sweeps string table from about to become dangled pointers to young generation
     */
    void SweepStringTableYoung();

    /**
     * Remove dead strings from string table
     */
    void SweepStringTable();

    /**
     * Update all refs to moved objects
     */
    void UpdateRefsToMovedObjects(PandaVector<ObjectHeader *> *moved_objects);

    void Sweep();

    bool IsMarked(const ObjectHeader *object) const override;

    bool ShouldRunTenuredGC(const GCTask &task) override;

    bool concurrent_marking_flag_ {false};  //! flag indicates if we currently in concurrent marking phase
    PandaUniquePtr<CardTable> card_table_ {nullptr};
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_GC_GEN_GC_GEN_GC_H_
