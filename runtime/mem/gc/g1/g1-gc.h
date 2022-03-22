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

#ifndef PANDA_RUNTIME_MEM_GC_G1_G1_GC_H_
#define PANDA_RUNTIME_MEM_GC_G1_G1_GC_H_

#include "runtime/include/mem/panda_smart_pointers.h"
#include "runtime/mem/gc/card_table.h"
#include "runtime/mem/gc/gc.h"
#include "runtime/mem/gc/lang/gc_lang.h"
#include "runtime/mem/gc/g1/g1-allocator.h"
#include "runtime/mem/gc/generational-gc-base.h"

namespace panda {
class ManagedThread;
}  // namespace panda
namespace panda::mem {

/**
 * \brief G1 alike GC
 */
template <class LanguageConfig>
class G1GC : public GenerationalGC<LanguageConfig> {
public:
    explicit G1GC(ObjectAllocatorBase *object_allocator, const GCSettings &settings);
    ~G1GC() override = default;
    DEFAULT_MOVE_SEMANTIC(G1GC);
    DEFAULT_COPY_SEMANTIC(G1GC);

    void InitGCBits(panda::ObjectHeader *obj_header) override;

    void InitGCBitsForAllocationInTLAB(panda::ObjectHeader *object) override;

    void Trigger() override;

    void MarkReferences(PandaStackTL<ObjectHeader *> *references, GCPhase gc_phase) override;

    void MarkObject(ObjectHeader *object_header) override;

    bool MarkObjectIfNotMarked(ObjectHeader *object_header) override;

    void UnMarkObject(ObjectHeader *object_header) override;

private:
    using RefUpdateInfo = std::pair<const void *, const void *>;

    void InitializeImpl() override;

    void RunPhasesImpl(const GCTask &task) override;

    void PreStartupImp() override;

    /**
     * Check if object can be in collectible set
     * @return true if object in region which is currently processed by GC for collection
     */
    bool IsInCollectibleSet(ObjectHeader *obj_header) const;

    /**
     * GC for young generation. Runs with STW.
     */
    void RunYoungGC(GCTask task);

    /**
     * GC for tenured generation.
     */
    void RunTenuredGC(GCTask task);

    /**
     * Marks objects in young generation
     */
    void MarkYoung(GCTask task);

    void MarkYoungStack(PandaStackTL<ObjectHeader *> *objects_stack);

    /**
     * Marks roots and add them to the stack
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
    NO_THREAD_SAFETY_ANALYSIS void ConcurrentMark(PandaStackTL<ObjectHeader *> *objects_stack);

    /**
     * ReMarks objects after Concurrent marking
     * @param objects_stack
     */
    void ReMark(PandaStackTL<ObjectHeader *> *objects_stack, GCTask task);

    /**
     * Mark all objects in stack recursively for Full GC.
     */
    void MarkStack(PandaStackTL<ObjectHeader *> *stack);

    /**
     * Collect dead objects in young generation and move survivors
     * @return
     */
    void CollectYoungAndMove();

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

    ALWAYS_INLINE ObjectAllocatorG1<MT_MODE_MULTI> *GetG1ObjectAllocator()
    {
        return static_cast<ObjectAllocatorG1<MT_MODE_MULTI> *>(this->GetObjectAllocator());
    }

    bool concurrent_marking_flag_ {false};  // flag indicates if we are currently in concurrent marking phase
    PandaUniquePtr<CardTable> card_table_ {nullptr};
    std::function<void(const void *, const void *)> post_queue_func_ {nullptr};  //! funciton called in the post WRB
    PandaVector<RefUpdateInfo> updated_refs_queue_ {};                           //! queue with updated refs info
    os::memory::Mutex queue_lock_;
};

template <MTModeT MTMode>
class AllocConfig<GCType::G1_GC, MTMode> {
public:
    using ObjectAllocatorType = ObjectAllocatorG1<MTMode>;
    using CodeAllocatorType = CodeAllocator;
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_GC_G1_G1_GC_H_
