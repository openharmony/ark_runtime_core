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

#include "runtime/mem/gc/gen-gc/gen-gc.h"
#include "runtime/include/hclass.h"
#include "runtime/include/coretypes/array-inl.h"
#include "runtime/include/mem/panda_smart_pointers.h"
#include "runtime/include/runtime.h"
#include "runtime/include/runtime_notification.h"
#include "runtime/mem/gc/gc_root-inl.h"
#include "runtime/mem/object_helpers-inl.h"
#include "runtime/mem/refstorage/global_object_storage.h"
#include "runtime/mem/rendezvous.h"
#include "runtime/include/panda_vm.h"
#include "runtime/mem/gc/card_table-inl.h"
#include "runtime/timing.h"
#include "runtime/include/exceptions.h"

namespace panda::mem {

constexpr bool LOG_DETAILED_GC_INFO = true;

void PreStoreInBuff([[maybe_unused]] void *object_header) {}

template <class LanguageConfig>
GenGC<LanguageConfig>::GenGC(ObjectAllocatorBase *object_allocator, const GCSettings &settings)
    : GenerationalGC<LanguageConfig>(object_allocator, settings)
{
    this->SetType(GCType::GEN_GC);
    this->SetTLABsSupported();
}

template <class LanguageConfig>
void GenGC<LanguageConfig>::InitializeImpl()
{
    // GC saved the PandaVM instance, so we get allocator from the PandaVM.
    InternalAllocatorPtr allocator = this->GetInternalAllocator();
    card_table_ = MakePandaUnique<CardTable>(allocator, PoolManager::GetMmapMemPool()->GetMinObjectAddress(),
                                             PoolManager::GetMmapMemPool()->GetTotalObjectSize());
    card_table_->Initialize();
    auto barrier_set = allocator->New<GCGenBarrierSet>(allocator, &concurrent_marking_flag_, PreStoreInBuff,
                                                       PoolManager::GetMmapMemPool()->GetAddressOfMinObjectAddress(),
                                                       reinterpret_cast<uint8_t *>(*card_table_->begin()),
                                                       CardTable::GetCardBits(), CardTable::GetCardDirtyValue());
    ASSERT(barrier_set != nullptr);
    this->SetGCBarrierSet(barrier_set);
    LOG_DEBUG_GC << "GenGC initialized";
}

template <class LanguageConfig>
bool GenGC<LanguageConfig>::ShouldRunTenuredGC(const GCTask &task)
{
    return this->IsOnPygoteFork() || task.reason_ == GCTaskCause::OOM_CAUSE ||
           task.reason_ == GCTaskCause::EXPLICIT_CAUSE || task.reason_ == GCTaskCause::HEAP_USAGE_THRESHOLD_CAUSE;
}

template <class LanguageConfig>
void GenGC<LanguageConfig>::RunPhasesImpl(const GCTask &task)
{
    LOG(INFO, GC) << "GenGC start";
    LOG_DEBUG_GC << "Footprint before GC: " << this->GetPandaVm()->GetMemStats()->GetFootprintHeap();
    GCScopedPauseStats scoped_pause_stats(this->GetPandaVm()->GetGCStats());
    LOG_DEBUG_GC << "Young range: " << this->GetObjectAllocator()->GetYoungSpaceMemRange();
    uint64_t young_total_time;
    this->GetTiming()->Reset();
    {
        ScopedTiming t("Generational GC", *this->GetTiming());
        this->mem_stats_.Reset();
        {
            time::Timer timer(&young_total_time, true);
            this->GetPandaVm()->GetMemStats()->RecordGCPauseStart();
            this->BindBitmaps(false);
            RunYoungGC(task);
            this->GetPandaVm()->GetMemStats()->RecordGCPhaseEnd();
        }
        if (young_total_time > 0) {
            this->GetStats()->AddTimeValue(young_total_time, TimeTypeStats::YOUNG_TOTAL_TIME);
        }
        // we trigger a full gc at first pygote fork
        if (ShouldRunTenuredGC(task)) {
            this->BindBitmaps(true);  // clear pygote live bitmaps, we will rebuild it
            RunTenuredGC(task);
        }
    }
    LOG_DEBUG_GC << "Footprint after GC: " << this->GetPandaVm()->GetMemStats()->GetFootprintHeap();
    if (LOG_DETAILED_GC_INFO) {
        LOG(INFO, GC) << this->mem_stats_.Dump();
        LOG(INFO, GC) << this->GetTiming()->Dump();
    }
    this->GetTiming()->Reset();  // Clear records.
}

template <class LanguageConfig>
void GenGC<LanguageConfig>::PreStartupImp()
{
    GenerationalGC<LanguageConfig>::DisableTenuredGC();
}

template <class LanguageConfig>
void GenGC<LanguageConfig>::InitGCBits(panda::ObjectHeader *obj_header)
{
    if (UNLIKELY(this->GetGCPhase() == GCPhase::GC_PHASE_SWEEP) &&
        (!this->GetObjectAllocator()->IsAddressInYoungSpace(ToUintPtr(obj_header)))) {
        obj_header->SetMarkedForGC();
        // Do unmark if out of sweep phase otherwise we may miss it in sweep
        if (UNLIKELY(this->GetGCPhase() != GCPhase::GC_PHASE_SWEEP)) {
            obj_header->SetUnMarkedForGC();
        }
    } else {
        obj_header->SetUnMarkedForGC();
    }
    LOG_DEBUG_GC << "Init gc bits for object: " << std::hex << obj_header << " bit: " << obj_header->IsMarkedForGC()
                 << ", is marked = " << IsMarked(obj_header);
}

template <class LanguageConfig>
void GenGC<LanguageConfig>::InitGCBitsForAllocationInTLAB(panda::ObjectHeader *obj_header)
{
    // Compiler will allocate objects in TLABs only in young space
    // Therefore, set unmarked for GC here.
    obj_header->SetUnMarkedForGC();
}

template <class LanguageConfig>
void GenGC<LanguageConfig>::Trigger()
{
    // Check current heap size.
    // Collect Young gen.
    // If threshold for tenured gen - collect tenured gen.
    auto task = MakePandaUnique<GCTask>(GCTaskCause::HEAP_USAGE_THRESHOLD_CAUSE, time::GetCurrentTimeInNanos());
    this->AddGCTask(true, std::move(task), true);
}

template <class LanguageConfig>
void GenGC<LanguageConfig>::RunYoungGC(const GCTask &task)
{
    trace::ScopedTrace scoped_trace(__FUNCTION__);
    LOG_DEBUG_GC << "GenGC RunYoungGC start";
    ScopedTiming t(__FUNCTION__, *this->GetTiming());
    uint64_t young_pause_time;
    {
        NoAtomicGCMarkerScope scope(&this->marker_);
        time::Timer timer(&young_pause_time, true);
        // NOLINTNEXTLINE(performance-unnecessary-value-param)
        MarkYoung(task);
        bool moved = CollectYoungAndMove(task);
        if (moved) {
            card_table_->ClearAll();
        }
    }
    if (young_pause_time > 0) {
        this->GetStats()->AddTimeValue(young_pause_time, TimeTypeStats::YOUNG_PAUSED_TIME);
    }
    LOG_DEBUG_GC << "GenGC RunYoungGC end";
}

template <class LanguageConfig>
void GenGC<LanguageConfig>::MarkYoung(const GCTask &task)
{
    trace::ScopedTrace s_trace(__FUNCTION__);
    GCScopedPhase s_phase(this->GetPandaVm()->GetMemStats(), this, GCPhase::GC_PHASE_MARK_YOUNG);

    ScopedTiming s_timing(__FUNCTION__, *this->GetTiming());
    // Iterate over roots and add other roots
    PandaStackTL<ObjectHeader *> objects_stack(
        this->GetInternalAllocator()->template Adapter<mem::AllocScope::LOCAL>());
    auto young_mr = this->GetObjectAllocator()->GetYoungSpaceMemRange();
    GCRootVisitor gc_mark_young = [&objects_stack, &young_mr, this](const GCRoot &gc_root) {
        // Skip non-young roots
        auto root_object_ptr = gc_root.GetObjectHeader();
        ASSERT(root_object_ptr != nullptr);
        if (!young_mr.IsAddressInRange(ToUintPtr(root_object_ptr))) {
            LOG_DEBUG_GC << "Skip root for young mark: " << std::hex << root_object_ptr;
            return;
        }
        LOG(DEBUG, GC) << "root " << GetDebugInfoAboutObject(root_object_ptr);
        if (MarkObjectIfNotMarked(root_object_ptr)) {
            this->AddToStack(&objects_stack, root_object_ptr);
            MarkYoungStack(&objects_stack);
        }
    };
    {
        trace::ScopedTrace s_trace2("Marking roots young");
        ScopedTiming s_timing2("VisitRoots", *this->GetTiming());
        this->VisitRoots(gc_mark_young,
                         VisitGCRootFlags::ACCESS_ROOT_NONE | VisitGCRootFlags::ACCESS_ROOT_AOT_STRINGS_ONLY_YOUNG);
    }
    {
        ScopedTiming s_timing2("VisitCardTableRoots", *this->GetTiming());
        LOG_DEBUG_GC << "START Marking tenured -> young roots";
        MemRangeChecker tenured_range_checker = [&young_mr](MemRange &mem_range) -> bool {
            return !young_mr.IsIntersect(mem_range);
        };
        ObjectChecker tenured_range_young_object_checker = [&young_mr](const ObjectHeader *object_header) -> bool {
            return young_mr.IsAddressInRange(ToUintPtr(object_header));
        };

        ObjectChecker from_object_checker = []([[maybe_unused]] const ObjectHeader *object_header) -> bool {
            return true;
        };

        this->VisitCardTableRoots(card_table_.get(), gc_mark_young, tenured_range_checker,
                                  tenured_range_young_object_checker, from_object_checker,
                                  CardTableProcessedFlag::VISIT_MARKED | CardTableProcessedFlag::VISIT_PROCESSED);
    }
    // reference-processor in VisitCardTableRoots can add new objects to stack
    MarkYoungStack(&objects_stack);
    ASSERT(objects_stack.empty());
    LOG_DEBUG_GC << "END Marking tenured -> young roots";
    this->GetPandaVm()->HandleReferences(task);
}

template <class LanguageConfig>
void GenGC<LanguageConfig>::MarkYoungStack(PandaStackTL<ObjectHeader *> *stack)
{
    trace::ScopedTrace scoped_trace(__FUNCTION__);
    ASSERT(stack != nullptr);
    auto allocator = this->GetObjectAllocator();
    auto young_mem_range = allocator->GetYoungSpaceMemRange();
    while (!stack->empty()) {
        auto *object = this->PopObjectFromStack(stack);
        auto *cls = object->template ClassAddr<Class>();
        LOG_IF(cls == nullptr, DEBUG, GC) << " object's class is nullptr: " << std::hex << object;
        ASSERT(cls != nullptr);
        LOG_DEBUG_GC << "current object " << GetDebugInfoAboutObject(object);
        if (young_mem_range.IsAddressInRange(ToUintPtr(object))) {
            this->template MarkInstance<LanguageConfig::LANG_TYPE, LanguageConfig::HAS_VALUE_OBJECT_TYPES>(stack,
                                                                                                           object, cls);
        }
    }
}

// NOLINTNEXTLINE(readability-function-size)
template <class LanguageConfig>
bool GenGC<LanguageConfig>::CollectYoungAndMove(const GCTask &task)
{
    trace::ScopedTrace s_trace(__FUNCTION__);
    GCScopedPhase s_phase(this->GetPandaVm()->GetMemStats(), this, GCPhase::GC_PHASE_COLLECT_YOUNG_AND_MOVE);
    LOG_DEBUG_GC << "== GenGC CollectYoungAndMove start ==";

    ScopedTiming s_timing("CollectYoungAndMove", *this->GetTiming());
    PandaVector<ObjectHeader *> moved_objects;
    size_t young_move_size = 0;
    size_t young_move_count = 0;
    size_t young_delete_size = 0;
    size_t young_delete_count = 0;
    size_t bytes_in_heap_before_move = this->GetPandaVm()->GetMemStats()->GetFootprintHeap();

    // Hack for pools cause we have 2 types of pools in tenures space, in bad cases objects can be moved to different
    // spaces - so it would require x2 memory.
    auto need_memory = this->GetSettings()->young_space_size * 2;
    // Move to genObjAllocator
    auto free_bytes_in_pools =
        this->GetPandaVm()->GetHeapManager()->GetObjectAllocator().AsObjectAllocator()->GetObjectSpaceFreeBytes();
    if (need_memory > free_bytes_in_pools) {
        auto *caller_thread = task.caller_thread_;
        if (caller_thread != nullptr) {
            caller_thread->SetException(this->GetPandaVm()->GetOOMErrorObject());
        }
        // We just exited from moving, if gc was triggered in managed-thread then it would throw OOM, otherwise we don't
        // clean young-space so next allocation will throw OOM
        return false;
    }

    auto object_allocator = this->GetObjectAllocator();
    std::function<void(ObjectHeader * object_header)> move_visitor(
        [this, &object_allocator, &moved_objects, &young_move_size, &young_move_count, &young_delete_size,
         &young_delete_count](ObjectHeader *object_header) -> void {
            size_t size = GetObjectSize(object_header);
            ASSERT(size <= ObjectAllocatorGen<>::GetYoungAllocMaxSize());
            // Use aligned size here, because we need to proceed MemStats correctly.
            size_t aligned_size = GetAlignedObjectSize(size);
            if (IsMarked(object_header)) {
                auto dst = reinterpret_cast<ObjectHeader *>(object_allocator->AllocateTenured(size));
                ASSERT(dst != nullptr);
                (void)memcpy_s(dst, size, object_header, size);
                young_move_size += aligned_size;
                young_move_count++;
                LOG_DEBUG_GC << "object MOVED from " << std::hex << object_header << " to " << dst
                             << ", size = " << std::dec << size;
                moved_objects.push_back(dst);
                // Set unmarked dst
                ASSERT(IsMarked(object_header));
                UnMarkObject(dst);
                this->SetForwardAddress(object_header, dst);
            } else {
                LOG_DEBUG_GC << "DELETE OBJECT young:" << GetDebugInfoAboutObject(object_header);
                ++young_delete_count;
                young_delete_size += aligned_size;
            }
            // We will record all object in MemStats as SPACE_TYPE_OBJECT, so check it
            ASSERT(PoolManager::GetMmapMemPool()->GetSpaceTypeForAddr(object_header) == SpaceType::SPACE_TYPE_OBJECT);
        });
    {
        ScopedTiming s_timing2("Move", *this->GetTiming());
        object_allocator->IterateOverYoungObjects(move_visitor);
    }
    if (young_move_size > 0) {
        this->GetStats()->AddMemoryValue(young_move_size, MemoryTypeStats::MOVED_BYTES);
        this->GetStats()->AddObjectsValue(moved_objects.size(), ObjectTypeStats::MOVED_OBJECTS);
        this->mem_stats_.RecordSizeMovedYoung(young_move_size);
        this->mem_stats_.RecordCountMovedYoung(moved_objects.size());
    }
    if (bytes_in_heap_before_move > 0) {
        this->GetStats()->AddCopiedRatioValue(static_cast<double>(young_move_size) / bytes_in_heap_before_move);
    }
    if (young_delete_size > 0) {
        this->GetStats()->AddMemoryValue(young_delete_size, MemoryTypeStats::YOUNG_FREED_BYTES);
        this->GetStats()->AddObjectsValue(young_delete_count, ObjectTypeStats::YOUNG_FREED_OBJECTS);
        this->mem_stats_.RecordSizeFreedYoung(young_delete_size);
        this->mem_stats_.RecordCountMovedYoung(young_delete_count);
    }
    UpdateRefsToMovedObjects(&moved_objects);
    // Sweep string table here to avoid dangling references
    SweepStringTableYoung();
    // Remove young
    object_allocator->ResetYoungAllocator();

    // We need to record freed and moved objects:
    this->GetPandaVm()->GetMemStats()->RecordFreeObjects(young_delete_count, young_delete_size,
                                                         SpaceType::SPACE_TYPE_OBJECT);
    this->GetPandaVm()->GetMemStats()->RecordMovedObjects(young_move_count, young_move_size,
                                                          SpaceType::SPACE_TYPE_OBJECT);

    LOG_DEBUG_GC << "== GenGC CollectYoungAndMove end ==";
    return true;
}

template <class LanguageConfig>
void GenGC<LanguageConfig>::SweepStringTableYoung()
{
    trace::ScopedTrace scoped_trace(__FUNCTION__);
    auto string_table = this->GetPandaVm()->GetStringTable();

    GCScopedPhase scoped_phase(this->GetPandaVm()->GetMemStats(), this, GCPhase::GC_PHASE_SWEEP_STRING_TABLE_YOUNG);

    auto young_mem_range = this->GetObjectAllocator()->GetYoungSpaceMemRange();
    string_table->Sweep(static_cast<GCObjectVisitor>([&young_mem_range](ObjectHeader *object_header) {
        if (young_mem_range.IsAddressInRange(ToUintPtr(object_header))) {
            return ObjectStatus::DEAD_OBJECT;
        }
        return ObjectStatus::ALIVE_OBJECT;
    }));
}

template <class LanguageConfig>
void GenGC<LanguageConfig>::SweepStringTable()
{
    trace::ScopedTrace scoped_trace(__FUNCTION__);
    GCScopedPhase scoped_phase(this->GetPandaVm()->GetMemStats(), this, GCPhase::GC_PHASE_SWEEP_STRING_TABLE);

    // New strings may be created in young space during tenured gc, we shouldn't collect them
    auto young_mem_range = this->GetObjectAllocator()->GetYoungSpaceMemRange();
    this->GetPandaVm()->GetStringTable()->Sweep([this, &young_mem_range](ObjectHeader *object) {
        if (young_mem_range.IsAddressInRange(ToUintPtr(object))) {
            return ObjectStatus::ALIVE_OBJECT;
        }
        return this->marker_.MarkChecker(object);
    });
}

template <class LanguageConfig>
void GenGC<LanguageConfig>::UpdateRefsToMovedObjects(PandaVector<ObjectHeader *> *moved_objects)
{
    trace::ScopedTrace scoped_trace(__FUNCTION__);

    ScopedTiming t("UpdateRefsToMovedObjects", *this->GetTiming());
    auto obj_allocator = this->GetObjectAllocator();
    this->CommonUpdateRefsToMovedObjects([&](const UpdateRefInObject &update_refs_in_object) {
        // Update references exyoung -> young
        LOG_DEBUG_GC << "process moved objects cnt = " << std::dec << moved_objects->size();
        LOG_DEBUG_GC << "=== Update exyoung -> young references. START. ===";
        for (auto obj : *moved_objects) {
            update_refs_in_object(obj);
        }

        LOG_DEBUG_GC << "=== Update exyoung -> young references. END. ===";
        // Update references tenured -> young
        LOG_DEBUG_GC << "=== Update tenured -> young references. START. ===";
        auto young_space = obj_allocator->GetYoungSpaceMemRange();
        card_table_->VisitMarked(
            [&update_refs_in_object, &obj_allocator, &young_space](const MemRange &mem_range) {
                if (!young_space.Contains(mem_range)) {
                    obj_allocator->IterateOverObjectsInRange(mem_range, update_refs_in_object);
                }
            },
            CardTableProcessedFlag::VISIT_MARKED | CardTableProcessedFlag::VISIT_PROCESSED);
        LOG_DEBUG_GC << "=== Update tenured -> young references. END. ===";
    });
}

template <class LanguageConfig>
void GenGC<LanguageConfig>::RunTenuredGC(const GCTask &task)
{
    trace::ScopedTrace scoped_trace(__FUNCTION__);
    LOG_DEBUG_GC << "GC tenured start";
    ScopedTiming t(__FUNCTION__, *this->GetTiming());
    this->GetPandaVm()->GetMemStats()->RecordGCPauseStart();
    // Unmark all because no filter out tenured when mark young
    this->GetObjectAllocator()->IterateOverObjects([this](ObjectHeader *obj) { this->marker_.UnMark(obj); });
    PandaStackTL<ObjectHeader *> objects_stack(
        this->GetInternalAllocator()->template Adapter<mem::AllocScope::LOCAL>());
    InitialMark(&objects_stack);
    this->GetPandaVm()->GetMemStats()->RecordGCPauseEnd();
    ConcurrentMark(&objects_stack, CardTableVisitFlag::VISIT_ENABLED);
    this->GetPandaVm()->GetMemStats()->RecordGCPauseStart();
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    ReMark(&objects_stack, task);
    ASSERT(objects_stack.empty());
    this->GetObjectAllocator()->IterateOverYoungObjects([this](ObjectHeader *obj) { this->marker_.UnMark(obj); });
    SweepStringTable();
    Sweep();
    this->GetPandaVm()->GetMemStats()->RecordGCPauseEnd();
    LOG_DEBUG_GC << "GC tenured end";
}

template <class LanguageConfig>
void GenGC<LanguageConfig>::MarkRoots(PandaStackTL<ObjectHeader *> *objects_stack,
                                      CardTableVisitFlag visit_card_table_roots, VisitGCRootFlags flags)
{
    trace::ScopedTrace scoped_trace(__FUNCTION__);
    GCRootVisitor gc_mark_roots = [this, &objects_stack](const GCRoot &gc_root) {
        ObjectHeader *root_object = gc_root.GetObjectHeader();
        ObjectHeader *from_object = gc_root.GetFromObjectHeader();
        LOG_DEBUG_GC << "Handle root " << GetDebugInfoAboutObject(root_object);
        if (UNLIKELY(from_object != nullptr) && this->IsReference(from_object->ClassAddr<BaseClass>(), from_object)) {
            LOG_DEBUG_GC << "Add reference: " << GetDebugInfoAboutObject(from_object) << " to stack";
            MarkObject(from_object);
            this->ProcessReference(objects_stack, from_object->ClassAddr<BaseClass>(), from_object);
        } else {
            // We should always add this object to the stack, cause we could mark this object in InitialMark, but write
            // to some fields in ConcurrentMark - need to iterate over all fields again, MarkObjectIfNotMarked can't be
            // used here
            MarkObject(root_object);
            this->AddToStack(objects_stack, root_object);
        }
    };
    this->VisitRoots(gc_mark_roots, flags);
    if (visit_card_table_roots == CardTableVisitFlag::VISIT_ENABLED) {
        auto allocator = this->GetObjectAllocator();
        MemRange young_mr = allocator->GetYoungSpaceMemRange();
        MemRangeChecker young_range_checker = []([[maybe_unused]] MemRange &mem_range) -> bool { return true; };
        ObjectChecker young_range_tenured_object_checker = [&young_mr](const ObjectHeader *object_header) -> bool {
            return !young_mr.IsAddressInRange(ToUintPtr(object_header));
        };
        ObjectChecker from_object_checker = [&young_mr, this](const ObjectHeader *object_header) -> bool {
            // Don't visit objects which are in tenured and not marked.
            return young_mr.IsAddressInRange(ToUintPtr(object_header)) || IsMarked(object_header);
        };
        this->VisitCardTableRoots(card_table_.get(), gc_mark_roots, young_range_checker,
                                  young_range_tenured_object_checker, from_object_checker,
                                  CardTableProcessedFlag::VISIT_MARKED);
    }
}

template <class LanguageConfig>
void GenGC<LanguageConfig>::InitialMark(PandaStackTL<ObjectHeader *> *objects_stack)
{
    trace::ScopedTrace scoped_trace(__FUNCTION__);
    GCScopedPhase scoped_phase(this->GetPandaVm()->GetMemStats(), this, GCPhase::GC_PHASE_INITIAL_MARK);
    ScopedTiming t(__FUNCTION__, *this->GetTiming());

    {
        NoAtomicGCMarkerScope scope(&this->marker_);
        MarkRoots(objects_stack, CardTableVisitFlag::VISIT_DISABLED,
                  VisitGCRootFlags::ACCESS_ROOT_NONE | VisitGCRootFlags::START_RECORDING_NEW_ROOT);
    }
}

template <class LanguageConfig>
NO_THREAD_SAFETY_ANALYSIS void GenGC<LanguageConfig>::ConcurrentMark(PandaStackTL<ObjectHeader *> *objects_stack,
                                                                     CardTableVisitFlag visit_card_table_roots)
{
    trace::ScopedTrace s_trace(__FUNCTION__);
    ScopedTiming s_timing(__FUNCTION__, *this->GetTiming());
    GCScopedPhase scoped_phase(this->GetPandaVm()->GetMemStats(), this, GCPhase::GC_PHASE_MARK);
    ConcurrentScope concurrent_scope(this);
    // Concurrently visit class roots
    this->VisitClassRoots([this, &objects_stack](const GCRoot &gc_root) {
        if (MarkObjectIfNotMarked(gc_root.GetObjectHeader())) {
            ASSERT(gc_root.GetObjectHeader() != nullptr);
            this->AddToStack(objects_stack, gc_root.GetObjectHeader());
        }
    });
    MarkStack(objects_stack);
    ScopedTiming s_timing2("VisitInternalStringTable", *this->GetTiming());
    this->GetPandaVm()->GetStringTable()->VisitRoots(
        [this, &objects_stack](coretypes::String *str) {
            if (this->MarkObjectIfNotMarked(str)) {
                ASSERT(str != nullptr);
                this->AddToStack(objects_stack, str);
            }
        },
        VisitGCRootFlags::ACCESS_ROOT_ALL | VisitGCRootFlags::START_RECORDING_NEW_ROOT);
    MarkStack(objects_stack);

    // Concurrently visit card table
    if (visit_card_table_roots == CardTableVisitFlag::VISIT_ENABLED) {
        GCRootVisitor gc_mark_roots = [this, &objects_stack](const GCRoot &gc_root) {
            ObjectHeader *from_object = gc_root.GetFromObjectHeader();
            if (UNLIKELY(from_object != nullptr) &&
                this->IsReference(from_object->ClassAddr<BaseClass>(), from_object)) {
                LOG_DEBUG_GC << "Add reference: " << GetDebugInfoAboutObject(from_object) << " to stack";
                MarkObject(from_object);
                this->ProcessReference(objects_stack, from_object->ClassAddr<BaseClass>(), from_object);
            } else {
                objects_stack->push(gc_root.GetObjectHeader());
                MarkObject(gc_root.GetObjectHeader());
            }
        };

        auto allocator = this->GetObjectAllocator();
        MemRange young_mr = allocator->GetYoungSpaceMemRange();
        MemRangeChecker range_checker = [&young_mr]([[maybe_unused]] MemRange &mem_range) -> bool {
            return !young_mr.IsIntersect(mem_range);
        };
        ObjectChecker tenured_object_checker = [&young_mr](const ObjectHeader *object_header) -> bool {
            return !young_mr.IsAddressInRange(ToUintPtr(object_header));
        };
        ObjectChecker from_object_checker = [this](const ObjectHeader *object_header) -> bool {
            return IsMarked(object_header);
        };
        this->VisitCardTableRoots(card_table_.get(), gc_mark_roots, range_checker, tenured_object_checker,
                                  from_object_checker,
                                  CardTableProcessedFlag::VISIT_MARKED | CardTableProcessedFlag::VISIT_PROCESSED |
                                      CardTableProcessedFlag::SET_PROCESSED);
    }
    MarkStack(objects_stack);
}

template <class LanguageConfig>
void GenGC<LanguageConfig>::ReMark(PandaStackTL<ObjectHeader *> *objects_stack, const GCTask &task)
{
    trace::ScopedTrace scoped_trace(__FUNCTION__);
    GCScopedPhase scoped_phase(this->GetPandaVm()->GetMemStats(), this, GCPhase::GC_PHASE_REMARK);
    ScopedTiming t(__FUNCTION__, *this->GetTiming());

    {
        NoAtomicGCMarkerScope scope(&this->marker_);
        MarkRoots(objects_stack, CardTableVisitFlag::VISIT_ENABLED,
                  VisitGCRootFlags::ACCESS_ROOT_ONLY_NEW | VisitGCRootFlags::END_RECORDING_NEW_ROOT);
        MarkStack(objects_stack);
        {
            ScopedTiming t1("VisitInternalStringTable", *this->GetTiming());
            this->GetPandaVm()->GetStringTable()->VisitRoots(
                [this, &objects_stack](coretypes::String *str) {
                    if (this->MarkObjectIfNotMarked(str)) {
                        ASSERT(str != nullptr);
                        this->AddToStack(objects_stack, str);
                    }
                },
                VisitGCRootFlags::ACCESS_ROOT_ONLY_NEW | VisitGCRootFlags::END_RECORDING_NEW_ROOT);
            MarkStack(objects_stack);
        }
        // NOLINTNEXTLINE(performance-unnecessary-value-param)
        this->GetPandaVm()->HandleReferences(task);
        this->GetPandaVm()->HandleBufferData(false);
    }
}

template <class LanguageConfig>
void GenGC<LanguageConfig>::MarkStack(PandaStackTL<ObjectHeader *> *stack)
{
    trace::ScopedTrace scoped_trace(__FUNCTION__);
    ASSERT(stack != nullptr);
    while (!stack->empty()) {
        auto *object = this->PopObjectFromStack(stack);
        auto *object_class = object->template ClassAddr<Class>();
        LOG_IF(object_class == nullptr, DEBUG, GC) << " object's class is nullptr: " << std::hex << object;
        ASSERT(object_class != nullptr);
        LOG_DEBUG_GC << "Current object: " << GetDebugInfoAboutObject(object);

        ASSERT(!object->IsForwarded());
        this->template MarkInstance<LanguageConfig::LANG_TYPE, LanguageConfig::HAS_VALUE_OBJECT_TYPES>(stack, object,
                                                                                                       object_class);
    }
}

template <class LanguageConfig>
void GenGC<LanguageConfig>::MarkReferences(PandaStackTL<ObjectHeader *> *references, GCPhase gc_phase)
{
    trace::ScopedTrace scoped_trace(__FUNCTION__);
    LOG_DEBUG_GC << "Start marking " << references->size() << " references";
    if (gc_phase == GCPhase::GC_PHASE_MARK_YOUNG) {
        MarkYoungStack(references);
    } else if (gc_phase == GCPhase::GC_PHASE_INITIAL_MARK || gc_phase == GCPhase::GC_PHASE_MARK ||
               gc_phase == GCPhase::GC_PHASE_REMARK) {
        MarkStack(references);
    } else {
        UNREACHABLE();
    }
}

template <class LanguageConfig>
void GenGC<LanguageConfig>::MarkObject(ObjectHeader *object_header)
{
    LOG_DEBUG_GC << "Set mark for GC " << GetDebugInfoAboutObject(object_header);
    this->marker_.Mark(object_header);
}

template <class LanguageConfig>
bool GenGC<LanguageConfig>::MarkObjectIfNotMarked(ObjectHeader *object_header)
{
    if (!this->marker_.MarkIfNotMarked(object_header)) {
        return false;
    }
    LOG_DEBUG_GC << "Set mark for GC " << GetDebugInfoAboutObject(object_header);
    return true;
}

template <class LanguageConfig>
void GenGC<LanguageConfig>::UnMarkObject(ObjectHeader *object_header)
{
    LOG_DEBUG_GC << "Set unmark for GC " << GetDebugInfoAboutObject(object_header);
    this->marker_.UnMark(object_header);
}

template <class LanguageConfig>
bool GenGC<LanguageConfig>::IsMarked(const ObjectHeader *object) const
{
    return this->marker_.IsMarked(object);
}

// NO_THREAD_SAFETY_ANALYSIS because clang thread safety analysis
template <class LanguageConfig>
NO_THREAD_SAFETY_ANALYSIS void GenGC<LanguageConfig>::Sweep()
{
    trace::ScopedTrace scoped_trace(__FUNCTION__);
    ScopedTiming t(__FUNCTION__, *this->GetTiming());
    ConcurrentScope concurrent_scope(this, false);
    size_t freed_object_size = 0U;
    size_t freed_object_count = 0U;

    // NB! We can't move block out of brace, and we need to make sure GC_PHASE_SWEEP cleared
    {
        GCScopedPhase scoped_phase(this->GetPandaVm()->GetMemStats(), this, GCPhase::GC_PHASE_SWEEP);
        concurrent_scope.Start();  // enable concurrent after GC_PHASE_SWEEP has been set

        // Run monitor deflation again, to avoid object was reclaimed before monitor deflate.
        auto young_mr = this->GetObjectAllocator()->GetYoungSpaceMemRange();
        this->GetPandaVm()->GetMonitorPool()->DeflateMonitorsWithCallBack([&young_mr, this](Monitor *monitor) {
            ObjectHeader *object_header = monitor->GetObject();
            return (!IsMarked(object_header)) && (!young_mr.IsAddressInRange(ToUintPtr(object_header)));
        });

        this->GetObjectAllocator()->Collect(
            [this, &freed_object_size, &freed_object_count](ObjectHeader *object) {
                auto status = this->marker_.MarkChecker(object);
                if (status == ObjectStatus::DEAD_OBJECT) {
                    freed_object_size += GetAlignedObjectSize(GetObjectSize(object));
                    freed_object_count++;
                }
                return status;
            },
            GCCollectMode::GC_ALL);
        this->GetObjectAllocator()->VisitAndRemoveFreePools([this](void *mem, size_t size) {
            card_table_->ClearCardRange(ToUintPtr(mem), ToUintPtr(mem) + size);
            PoolManager::GetMmapMemPool()->FreePool(mem, size);
        });
    }

    this->mem_stats_.RecordSizeFreedTenured(freed_object_size);
    this->mem_stats_.RecordCountFreedTenured(freed_object_count);

    // In concurrent sweep phase, the new created objects may being marked in InitGCBits,
    // so we need to wait for that done, then we can safely unmark objects concurrent with mutator.
    ASSERT(this->GetGCPhase() != GCPhase::GC_PHASE_SWEEP);  // Make sure we are out of sweep scope
    this->GetObjectAllocator()->IterateOverTenuredObjects([this](ObjectHeader *obj) { this->marker_.UnMark(obj); });
}

template <class LanguageConfig>
bool GenGC<LanguageConfig>::InGCSweepRange(uintptr_t addr) const
{
    bool in_young_space = this->GetObjectAllocator()->IsAddressInYoungSpace(addr);
    auto phase = this->GetGCPhase();

    // Do young GC and the object is in the young space
    if (phase == GCPhase::GC_PHASE_MARK_YOUNG && in_young_space) {
        return true;
    }

    // Do tenured GC and the object is in the tenured space
    if (phase != GCPhase::GC_PHASE_MARK_YOUNG && !in_young_space) {
        return true;
    }

    return false;
}

template class GenGC<PandaAssemblyLanguageConfig>;

}  // namespace panda::mem
