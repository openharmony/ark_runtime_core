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

#include "runtime/mem/gc/stw-gc/stw-gc.h"

#include "libpandabase/trace/trace.h"
#include "libpandabase/utils/logger.h"
#include "runtime/include/object_header-inl.h"
#include "runtime/include/runtime.h"
#include "runtime/include/runtime_notification.h"
#include "runtime/include/hclass.h"
#include "runtime/include/panda_vm.h"
#include "runtime/mem/gc/gc_root-inl.h"
#include "runtime/mem/heap_manager.h"
#include "runtime/mem/object_helpers.h"
#include "runtime/mem/rendezvous.h"
#include "runtime/mem/refstorage/global_object_storage.h"
#include "runtime/include/coretypes/class.h"

namespace panda::mem {

template <class LanguageConfig>
StwGC<LanguageConfig>::StwGC(ObjectAllocatorBase *object_allocator, const GCSettings &settings)
    : GCLang<LanguageConfig>(object_allocator, settings)
{
    this->SetType(GCType::STW_GC);
}

template <class LanguageConfig>
void StwGC<LanguageConfig>::InitializeImpl()
{
    trace::ScopedTrace scoped_trace(__FUNCTION__);
    InternalAllocatorPtr allocator = this->GetInternalAllocator();
    auto barrier_set = allocator->New<GCDummyBarrierSet>(allocator);
    ASSERT(barrier_set != nullptr);
    this->SetGCBarrierSet(barrier_set);
    LOG_DEBUG_GC << "STW GC Initialized";
}

template <class LanguageConfig>
void StwGC<LanguageConfig>::RunPhasesImpl(const GCTask &task)
{
    trace::ScopedTrace scoped_trace(__FUNCTION__);
    GCScopedPauseStats scoped_pause_stats(this->GetPandaVm()->GetGCStats(), this->GetStats());
    [[maybe_unused]] size_t bytes_in_heap_before_gc = this->GetPandaVm()->GetMemStats()->GetFootprintHeap();
    this->BindBitmaps(true);
    Mark(task);
    SweepStringTable();
    Sweep();
    reversed_mark_ = !reversed_mark_;
    [[maybe_unused]] size_t bytes_in_heap_after_gc = this->GetPandaVm()->GetMemStats()->GetFootprintHeap();
    ASSERT(bytes_in_heap_after_gc <= bytes_in_heap_before_gc);
}

template <class LanguageConfig>
void StwGC<LanguageConfig>::Mark(const GCTask &task)
{
    trace::ScopedTrace scoped_trace(__FUNCTION__);
    GCScopedPhase scoped_phase(this->GetPandaVm()->GetMemStats(), this, GCPhase::GC_PHASE_MARK);

    // Iterate over roots and add other roots
    PandaStackTL<ObjectHeader *> objects_stack(
        this->GetInternalAllocator()->template Adapter<mem::AllocScope::LOCAL>());

    this->VisitRoots(
        [&objects_stack, this](const GCRoot &gc_root) {
            LOG_DEBUG_GC << "Handle root " << GetDebugInfoAboutObject(gc_root.GetObjectHeader());
            if (this->MarkObjectIfNotMarked(gc_root.GetObjectHeader())) {
                this->AddToStack(&objects_stack, gc_root.GetObjectHeader());
            }
            MarkStack(&objects_stack);
        },
        VisitGCRootFlags::ACCESS_ROOT_ALL);

    this->GetPandaVm()->GetStringTable()->VisitRoots(
        [this, &objects_stack](coretypes::String *str) {
            if (this->MarkObjectIfNotMarked(str)) {
                ASSERT(str != nullptr);
                this->AddToStack(&objects_stack, str);
            }
        },
        VisitGCRootFlags::ACCESS_ROOT_ALL);
    MarkStack(&objects_stack);
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    this->GetPandaVm()->HandleReferences(task);
    this->GetPandaVm()->HandleBufferData(reversed_mark_);
}

template <class LanguageConfig>
void StwGC<LanguageConfig>::MarkStack(PandaStackTL<ObjectHeader *> *stack)
{
    trace::ScopedTrace scoped_trace(__FUNCTION__);
    ASSERT(stack != nullptr);
    size_t objects_count = 0;
    while (!stack->empty()) {
        objects_count++;
        auto *object = this->PopObjectFromStack(stack);
        auto *base_class = object->template ClassAddr<BaseClass>();
        LOG_IF(base_class == nullptr, DEBUG, GC) << " object's class is nullptr: " << std::hex << object;
        ASSERT(base_class != nullptr);
        LOG_DEBUG_GC << "Current object: " << GetDebugInfoAboutObject(object);
        this->template MarkInstance<LanguageConfig::LANG_TYPE, LanguageConfig::HAS_VALUE_OBJECT_TYPES>(stack, object,
                                                                                                       base_class);
    }
    LOG_DEBUG_GC << "Iterated over " << objects_count << " objects in the stack";
}

template <class LanguageConfig>
void StwGC<LanguageConfig>::SweepStringTable()
{
    trace::ScopedTrace scoped_trace(__FUNCTION__);
    auto string_table = this->GetPandaVm()->GetStringTable();
    GCScopedPhase scoped_phase(this->GetPandaVm()->GetMemStats(), this, GCPhase::GC_PHASE_SWEEP_STRING_TABLE);

    if (!reversed_mark_) {
        LOG_DEBUG_GC << "SweepStringTable with MarkChecker";
        string_table->Sweep([this](ObjectHeader *object) { return this->marker_.MarkChecker(object); });
    } else {
        LOG_DEBUG_GC << "SweepStringTable with ReverseMarkChecker";
        string_table->Sweep([this](ObjectHeader *object) { return this->marker_.template MarkChecker<true>(object); });
    }
}

template <class LanguageConfig>
void StwGC<LanguageConfig>::Sweep()
{
    trace::ScopedTrace scoped_trace(__FUNCTION__);
    GCScopedPhase scoped_phase(this->GetPandaVm()->GetMemStats(), this, GCPhase::GC_PHASE_SWEEP);

    if (!reversed_mark_) {
        LOG_DEBUG_GC << "Sweep with MarkChecker";
        this->GetObjectAllocator()->Collect([this](ObjectHeader *object) { return this->marker_.MarkChecker(object); },
                                            GCCollectMode::GC_ALL);
        this->GetObjectAllocator()->VisitAndRemoveFreePools(
            [](void *mem, [[maybe_unused]] size_t size) { PoolManager::GetMmapMemPool()->FreePool(mem, size); });

    } else {
        LOG_DEBUG_GC << "Sweep with ReverseMarkChecker";
        this->GetObjectAllocator()->Collect(
            [this](ObjectHeader *object) { return this->marker_.template MarkChecker<true>(object); },
            GCCollectMode::GC_ALL);
        this->GetObjectAllocator()->VisitAndRemoveFreePools(
            [](void *mem, [[maybe_unused]] size_t size) { PoolManager::GetMmapMemPool()->FreePool(mem, size); });
    }
}

// NOLINTNEXTLINE(misc-unused-parameters)
template <class LanguageConfig>
void StwGC<LanguageConfig>::WaitForGC(const GCTask &task)
{
    trace::ScopedTrace scoped_trace(__FUNCTION__);
    Runtime::GetCurrent()->GetNotificationManager()->GarbageCollectorStartEvent();
    auto old_counter = this->gc_counter_.load(std::memory_order_acquire);
    this->GetPandaVm()->GetRendezvous()->SafepointBegin();

    auto new_counter = this->gc_counter_.load(std::memory_order_acquire);
    if (new_counter > old_counter && this->last_cause_.load() >= task.reason_) {
        this->GetPandaVm()->GetRendezvous()->SafepointEnd();
        return;
    }
    auto mem_stats = this->GetPandaVm()->GetMemStats();
    mem_stats->RecordGCPauseStart();
    this->RunPhases(task);
    mem_stats->RecordGCPauseEnd();
    this->GetPandaVm()->GetRendezvous()->SafepointEnd();
    Runtime::GetCurrent()->GetNotificationManager()->GarbageCollectorFinishEvent();
    this->GetPandaVm()->HandleGCFinished();
    this->GetPandaVm()->HandleEnqueueReferences();
}

template <class LanguageConfig>
void StwGC<LanguageConfig>::InitGCBits(panda::ObjectHeader *object)
{
    if (!reversed_mark_) {
        object->SetUnMarkedForGC();
        ASSERT(!object->IsMarkedForGC());
    } else {
        object->SetMarkedForGC();
        ASSERT(object->IsMarkedForGC());
    }
    LOG_DEBUG_GC << "Init gc bits for object: " << std::hex << object << " bit: " << object->IsMarkedForGC()
                 << " reversed_mark: " << reversed_mark_;
}

template <class LanguageConfig>
void StwGC<LanguageConfig>::InitGCBitsForAllocationInTLAB([[maybe_unused]] panda::ObjectHeader *obj_header)
{
    LOG(FATAL, GC) << "TLABs are not supported by this GC";
}

template <class LanguageConfig>
void StwGC<LanguageConfig>::Trigger()
{
    auto task = MakePandaUnique<GCTask>(GCTaskCause::HEAP_USAGE_THRESHOLD_CAUSE, time::GetCurrentTimeInNanos());
    this->AddGCTask(true, std::move(task), true);
}

template <class LanguageConfig>
void StwGC<LanguageConfig>::MarkObject(ObjectHeader *object)
{
    if (!reversed_mark_) {
        LOG_DEBUG_GC << "Set mark for GC " << GetDebugInfoAboutObject(object);
        this->marker_.template Mark<false>(object);
    } else {
        LOG_DEBUG_GC << "Set unmark for GC " << GetDebugInfoAboutObject(object);
        this->marker_.template Mark<true>(object);
    }
}

template <class LanguageConfig>
void StwGC<LanguageConfig>::UnMarkObject([[maybe_unused]] ObjectHeader *object_header)
{
    LOG(FATAL, GC) << "UnMarkObject for STW GC shouldn't be called";
}

template <class LanguageConfig>
void StwGC<LanguageConfig>::MarkReferences(PandaStackTL<ObjectHeader *> *references, [[maybe_unused]] GCPhase gc_phase)
{
    trace::ScopedTrace scoped_trace(__FUNCTION__);
    ASSERT(gc_phase == GCPhase::GC_PHASE_MARK);
    LOG_DEBUG_GC << "Start marking " << references->size() << " references";
    MarkStack(references);
}

template <class LanguageConfig>
bool StwGC<LanguageConfig>::IsMarked(const ObjectHeader *object) const
{
    bool marked = false;
    if (!reversed_mark_) {
        LOG_DEBUG_GC << "Get marked for GC " << GetDebugInfoAboutObject(object);
        marked = this->marker_.IsMarked(object);
    } else {
        LOG_DEBUG_GC << "Get unmarked for GC " << GetDebugInfoAboutObject(object);
        marked = this->marker_.template IsMarked<true>(object);
    }
    return marked;
}

template class StwGC<PandaAssemblyLanguageConfig>;

}  // namespace panda::mem
