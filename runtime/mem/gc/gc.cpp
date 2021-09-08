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

#include <memory>

#include "libpandabase/os/mem.h"
#include "libpandabase/os/thread.h"
#include "libpandabase/utils/time.h"
#include "runtime/assert_gc_scope.h"
#include "runtime/include/class.h"
#include "runtime/include/coretypes/dyn_objects.h"
#include "runtime/include/locks.h"
#include "runtime/include/runtime.h"
#include "runtime/include/runtime_notification.h"
#include "runtime/include/stack_walker-inl.h"
#include "runtime/mem/gc/epsilon/epsilon.h"
#include "runtime/mem/gc/gc.h"
#include "runtime/mem/gc/gc_root-inl.h"
#include "runtime/mem/gc/gc_queue.h"
#include "runtime/mem/gc/g1/g1-gc.h"
#include "runtime/mem/gc/gen-gc/gen-gc.h"
#include "runtime/mem/gc/stw-gc/stw-gc.h"
#include "runtime/mem/pygote_space_allocator-inl.h"
#include "runtime/mem/heap_manager.h"
#include "runtime/mem/gc/reference-processor/reference_processor.h"
#include "runtime/include/panda_vm.h"
#include "runtime/assert_gc_scope.h"
#include "runtime/include/object_accessor-inl.h"
#include "runtime/include/coretypes/class.h"
#include "runtime/thread_manager.h"

namespace panda::mem {
using TaggedValue = coretypes::TaggedValue;
using TaggedType = coretypes::TaggedType;
using DynClass = coretypes::DynClass;

GCListener::~GCListener() = default;

GC::GC(ObjectAllocatorBase *object_allocator, const GCSettings &settings)
    : gc_settings_(settings),
      object_allocator_(object_allocator),
      internal_allocator_(InternalAllocator<>::GetInternalAllocatorFromRuntime())
{
}

GC::~GC()
{
    if (gc_queue_ != nullptr) {
        InternalAllocatorPtr allocator = GetInternalAllocator();
        allocator->Delete(gc_queue_);
    }
    if (gc_listeners_ptr_ != nullptr) {
        InternalAllocatorPtr allocator = GetInternalAllocator();
        allocator->Delete(gc_listeners_ptr_);
    }
    if (gc_barrier_set_ != nullptr) {
        InternalAllocatorPtr allocator = GetInternalAllocator();
        allocator->Delete(gc_barrier_set_);
    }
    if (cleared_references_ != nullptr) {
        InternalAllocatorPtr allocator = GetInternalAllocator();
        allocator->Delete(cleared_references_);
    }
    if (cleared_references_lock_ != nullptr) {
        InternalAllocatorPtr allocator = GetInternalAllocator();
        allocator->Delete(cleared_references_lock_);
    }
}

GCType GC::GetType()
{
    return gc_type_;
}

void GC::SetPandaVM(PandaVM *vm)
{
    vm_ = vm;
    reference_processor_ = vm->GetReferenceProcessor();
}

NativeGcTriggerType GC::GetNativeGcTriggerType()
{
    return gc_settings_.native_gc_trigger_type;
}

size_t GC::SimpleNativeAllocationGcWatermark()
{
    return GetPandaVm()->GetOptions().GetMaxFree();
}

NO_THREAD_SAFETY_ANALYSIS void GC::WaitForIdleGC()
{
    while (!CASGCPhase(GCPhase::GC_PHASE_IDLE, GCPhase::GC_PHASE_RUNNING)) {
        GetPandaVm()->GetRendezvous()->SafepointEnd();
        constexpr uint64_t WAIT_FINISHED = 10;
        // Use NativeSleep for all threads, as this thread shouldn't hold Mutator lock here
        os::thread::NativeSleep(WAIT_FINISHED);
        GetPandaVm()->GetRendezvous()->SafepointBegin();
    }
}

inline void GC::TriggerGCForNative()
{
    auto native_gc_trigger_type = GetNativeGcTriggerType();
    ASSERT_PRINT((native_gc_trigger_type == NativeGcTriggerType::NO_NATIVE_GC_TRIGGER) ||
                     (native_gc_trigger_type == NativeGcTriggerType::SIMPLE_STRATEGY),
                 "Unknown Native GC Trigger type");
    switch (native_gc_trigger_type) {
        case NativeGcTriggerType::NO_NATIVE_GC_TRIGGER:
            break;
        case NativeGcTriggerType::SIMPLE_STRATEGY:
            if (native_bytes_registered_ > SimpleNativeAllocationGcWatermark()) {
                auto task = MakePandaUnique<GCTask>(GCTaskCause::NATIVE_ALLOC_CAUSE, time::GetCurrentTimeInNanos());
                AddGCTask(false, std::move(task), true);
                MTManagedThread::GetCurrent()->SafepointPoll();
            }
            break;
        default:
            LOG(FATAL, GC) << "Unknown Native GC Trigger type";
            break;
    }
}

void GC::Initialize()
{
    trace::ScopedTrace scoped_trace(__PRETTY_FUNCTION__);
    // GC saved the PandaVM instance, so we get allocator from the PandaVM.
    auto allocator = GetInternalAllocator();
    gc_listeners_ptr_ = allocator->template New<PandaVector<GCListener *>>(allocator->Adapter());
    cleared_references_lock_ = allocator->New<os::memory::Mutex>();
    os::memory::LockHolder holder(*cleared_references_lock_);
    cleared_references_ = allocator->New<PandaVector<panda::mem::Reference *>>(allocator->Adapter());
    gc_queue_ = allocator->New<GCQueueWithTime>(this);
    InitializeImpl();
}

void GC::StartGC()
{
    CreateWorker();
}

void GC::StopGC()
{
    JoinWorker();
    ASSERT(gc_queue_ != nullptr);
    gc_queue_->Finalize();
}

void GC::BindBitmaps(bool clear_pygote_space_bitmaps)
{
    // Set marking bitmaps
    marker_.ClearMarkBitMaps();
    auto pygote_space_allocator = object_allocator_->GetPygoteSpaceAllocator();
    if (pygote_space_allocator != nullptr) {
        // clear live bitmaps if we decide to rebuild it in full gc,
        // it will be used as marked bitmaps and updated at the end of gc
        if (clear_pygote_space_bitmaps) {
            pygote_space_allocator->ClearLiveBitmaps();
        }
        auto &bitmaps = pygote_space_allocator->GetLiveBitmaps();
        marker_.AddMarkBitMaps(bitmaps.begin(), bitmaps.end());
    }
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
void GC::RunPhases(const GCTask &task)
{
    DCHECK_ALLOW_GARBAGE_COLLECTION;
    trace::ScopedTrace s_trace(__FUNCTION__);
    auto old_counter = gc_counter_.load(std::memory_order_acquire);
    WaitForIdleGC();
    auto new_counter = gc_counter_.load(std::memory_order_acquire);
    if (new_counter > old_counter) {
        SetGCPhase(GCPhase::GC_PHASE_IDLE);
        return;
    }
    last_cause_ = task.reason_;
    if (gc_settings_.pre_gc_heap_verification) {
        trace::ScopedTrace s_trace2("PreGCHeapVeriFier");
        size_t fail_count = VerifyHeap();
        if (gc_settings_.fail_on_heap_verification && fail_count > 0) {
            LOG(FATAL, GC) << "Heap corrupted before GC, HeapVerifier found " << fail_count << " corruptions";
        }
    }
    gc_counter_.fetch_add(1, std::memory_order_acq_rel);
    if (gc_settings_.is_dump_heap) {
        PandaOStringStream os;
        os << "Heap dump before GC" << std::endl;
        GetPandaVm()->GetHeapManager()->DumpHeap(&os);
        std::cerr << os.str() << std::endl;
    }
    size_t bytes_in_heap_before_gc = GetPandaVm()->GetMemStats()->GetFootprintHeap();
    LOG_DEBUG_GC << "Bytes in heap before GC " << std::dec << bytes_in_heap_before_gc;
    {
        GCScopedStats scoped_stats(GetPandaVm()->GetGCStats(), gc_type_ == GCType::STW_GC ? GetStats() : nullptr);
        for (auto listener : *gc_listeners_ptr_) {
            listener->GCStarted(bytes_in_heap_before_gc);
        }

        PreRunPhasesImpl();
        // NOLINTNEXTLINE(performance-unnecessary-value-param)
        RunPhasesImpl(task);

        // Clear Internal allocator unused pools (must do it on pause to avoid race conditions):
        // - Clear global part:
        InternalAllocator<>::GetInternalAllocatorFromRuntime()->VisitAndRemoveFreePools(
            [](void *mem, [[maybe_unused]] size_t size) { PoolManager::GetMmapMemPool()->FreePool(mem, size); });
        // - Clear local part:
        GetPandaVm()->GetThreadManager()->EnumerateThreads(
            [](ManagedThread *thread) {
                InternalAllocator<>::RemoveFreePoolsForLocalInternalAllocator(thread->GetLocalInternalAllocator());
                return true;
            },
            static_cast<unsigned int>(EnumerationFlag::ALL));

        size_t bytes_in_heap_after_gc = GetPandaVm()->GetMemStats()->GetFootprintHeap();
        // There is case than bytes_in_heap_after_gc > 0 and bytes_in_heap_before_gc == 0.
        // Because TLABs are registered during GC
        if (bytes_in_heap_after_gc > 0 && bytes_in_heap_before_gc > 0) {
            GetStats()->AddReclaimRatioValue(1 - static_cast<double>(bytes_in_heap_after_gc) / bytes_in_heap_before_gc);
        }
        LOG_DEBUG_GC << "Bytes in heap after GC " << std::dec << bytes_in_heap_after_gc;
        for (auto listener : *gc_listeners_ptr_) {
            listener->GCFinished(task, bytes_in_heap_before_gc, bytes_in_heap_after_gc);
        }
    }
    last_gc_reclaimed_bytes.store(vm_->GetGCStats()->GetObjectsFreedBytes());

    LOG(INFO, GC) << task.reason_ << " " << GetPandaVm()->GetGCStats()->GetStatistics();
    if (gc_settings_.is_dump_heap) {
        PandaOStringStream os;
        os << "Heap dump after GC" << std::endl;
        GetPandaVm()->GetHeapManager()->DumpHeap(&os);
        std::cerr << os.str() << std::endl;
    }

    if (gc_settings_.post_gc_heap_verification) {
        trace::ScopedTrace s_trace2("PostGCHeapVeriFier");
        size_t fail_count = VerifyHeap();
        if (gc_settings_.fail_on_heap_verification && fail_count > 0) {
            LOG(FATAL, GC) << "Heap corrupted after GC, HeapVerifier found " << fail_count << " corruptions";
        }
    }

    SetGCPhase(GCPhase::GC_PHASE_IDLE);
}

template <class LanguageConfig>
GC *CreateGC(GCType gc_type, ObjectAllocatorBase *object_allocator, const GCSettings &settings)
{
    GC *ret = nullptr;
    ASSERT_PRINT((gc_type == GCType::EPSILON_GC) || (gc_type == GCType::STW_GC) || (gc_type == GCType::GEN_GC) ||
                     (gc_type == GCType::G1_GC),
                 "Unknown GC type");
    InternalAllocatorPtr allocator {InternalAllocator<>::GetInternalAllocatorFromRuntime()};

    switch (gc_type) {
        case GCType::EPSILON_GC:
            ret = allocator->New<EpsilonGC<LanguageConfig>>(object_allocator, settings);
            break;
        case GCType::STW_GC:
            ret = allocator->New<StwGC<LanguageConfig>>(object_allocator, settings);
            break;
        case GCType::GEN_GC:
            ret = allocator->New<GenGC<LanguageConfig>>(object_allocator, settings);
            break;
        case GCType::G1_GC:
            ret = allocator->New<G1GC<LanguageConfig>>(object_allocator, settings);
            break;
        default:
            LOG(FATAL, GC) << "Unknown GC type";
            break;
    }
    return ret;
}

void GC::MarkObject(ObjectHeader *object_header)
{
    marker_.Mark(object_header);
}

bool GC::MarkObjectIfNotMarked(ObjectHeader *object_header)
{
    ASSERT(object_header != nullptr);
    if (IsMarked(object_header)) {
        return false;
    }
    MarkObject(object_header);
    return true;
}

void GC::UnMarkObject(ObjectHeader *object_header)
{
    marker_.UnMark(object_header);
}

void GC::ProcessReference(PandaStackTL<ObjectHeader *> *objects_stack, BaseClass *cls, const ObjectHeader *object)
{
    ASSERT(reference_processor_ != nullptr);
    reference_processor_->DelayReferenceProcessing(cls, object);
    reference_processor_->HandleReference(this, objects_stack, cls, object);
}

bool GC::IsMarked(const ObjectHeader *object) const
{
    return marker_.IsMarked(object);
}

void GC::AddReference(ObjectHeader *object)
{
    ASSERT(IsMarked(object));
    PandaStackTL<ObjectHeader *> references;
    AddToStack(&references, object);
    MarkReferences(&references, phase_);
    if (gc_type_ != GCType::EPSILON_GC) {
        ASSERT(references.empty());
    }
}

/* static */
// NOLINTNEXTLINE(performance-unnecessary-value-param)
void GC::ProcessReferences(GCPhase gc_phase, const GCTask &task)
{
    LOG(DEBUG, REF_PROC) << "Start processing cleared references";
    ASSERT(reference_processor_ != nullptr);
    bool clear_soft_references = task.reason_ == GCTaskCause::OOM_CAUSE || task.reason_ == GCTaskCause::EXPLICIT_CAUSE;
    reference_processor_->ProcessReferences(false, clear_soft_references, gc_phase);
    Reference *processed_ref = reference_processor_->CollectClearedReferences();

    if (processed_ref != nullptr) {
        os::memory::LockHolder holder(*cleared_references_lock_);
        cleared_references_->push_back(processed_ref);
    }
}

void GC::GCWorkerEntry(GC *gc, PandaVM *vm)
{
    // We need to set VM to current_thread, since GC can call ObjectAccessor::GetBarrierSet() methods
    Thread gc_thread(vm, Thread::ThreadType::THREAD_TYPE_GC);
    ScopedCurrentThread sct(&gc_thread);
    while (true) {
        auto task = gc->gc_queue_->GetTask();
        if (!gc->IsGCRunning()) {
            LOG(DEBUG, GC) << "Stopping GC thread";
            if (task != nullptr) {
                task->Release(Runtime::GetCurrent()->GetInternalAllocator());
            }
            break;
        }
        if (task == nullptr) {
            continue;
        }
        if (task->reason_ == GCTaskCause::INVALID_CAUSE) {
            task->Release(Runtime::GetCurrent()->GetInternalAllocator());
            continue;
        }
        LOG(DEBUG, GC) << "Running GC task, reason " << task->reason_;
        task->Run(*gc);
        task->Release(Runtime::GetCurrent()->GetInternalAllocator());
    }
}

void GC::JoinWorker()
{
    gc_running_.store(false);
    if (!gc_settings_.run_gc_in_place) {
        ASSERT(worker_ != nullptr);
    }
    if (worker_ != nullptr && !gc_settings_.run_gc_in_place) {
        ASSERT(gc_queue_ != nullptr);
        gc_queue_->Signal();
        worker_->join();
        InternalAllocatorPtr allocator = GetInternalAllocator();
        allocator->Delete(worker_);
        worker_ = nullptr;
    }
}

void GC::CreateWorker()
{
    gc_running_.store(true);
    ASSERT(worker_ == nullptr);
    if (worker_ == nullptr && !gc_settings_.run_gc_in_place) {
        InternalAllocatorPtr allocator = GetInternalAllocator();
        worker_ = allocator->New<std::thread>(GC::GCWorkerEntry, this, this->GetPandaVm());
        if (worker_ == nullptr) {
            LOG(FATAL, RUNTIME) << "Cannot create a GC thread";
        }
        int res = os::thread::SetThreadName(worker_->native_handle(), "GCThread");
        if (res != 0) {
            LOG(ERROR, RUNTIME) << "Failed to set a name for the gc thread";
        }
        ASSERT(gc_queue_ != nullptr);
    }
}

class GC::PostForkGCTask : public GCTask {
public:
    PostForkGCTask(GCTaskCause reason, uint64_t target_time) : GCTask(reason, target_time) {}

    void Run(mem::GC &gc) override
    {
        LOG(INFO, GC) << "Running PostForkGCTask";
        gc.GetPandaVm()->GetGCTrigger()->RestoreMinTargetFootprint();
        gc.PostForkCallback();
        GCTask::Run(gc);
    }

    ~PostForkGCTask() override = default;

    NO_COPY_SEMANTIC(PostForkGCTask);
    NO_MOVE_SEMANTIC(PostForkGCTask);
};

void GC::PreStartup()
{
    // Add a delay GCTask.
    if ((!Runtime::GetCurrent()->IsZygote()) && (!gc_settings_.run_gc_in_place)) {
        // divide 2 to temporarily set target footprint to a high value to disable GC during App startup.
        GetPandaVm()->GetGCTrigger()->SetMinTargetFootprint(Runtime::GetOptions().GetHeapSizeLimit() / 2);
        PreStartupImp();
        constexpr uint64_t DISABLE_GC_DURATION_NS = 2000 * 1000 * 1000;
        auto task = MakePandaUnique<PostForkGCTask>(GCTaskCause::STARTUP_COMPLETE_CAUSE,
                                                    time::GetCurrentTimeInNanos() + DISABLE_GC_DURATION_NS);
        AddGCTask(true, std::move(task), false);
        LOG(INFO, GC) << "Add PostForkGCTask";
    }
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
void GC::AddGCTask(bool is_managed, PandaUniquePtr<GCTask> task, bool triggered_by_threshold)
{
    if (gc_settings_.run_gc_in_place) {
        auto *gc_task = task.release();
        if (IsGCRunning()) {
            if (is_managed) {
                WaitForGCInManaged(*gc_task);
            } else {
                WaitForGC(*gc_task);
            }
        }
        gc_task->Release(Runtime::GetCurrent()->GetInternalAllocator());
    } else {
        if (triggered_by_threshold) {
            bool expect = true;
            if (can_add_gc_task_.compare_exchange_strong(expect, false, std::memory_order_seq_cst)) {
                gc_queue_->AddTask(task.release());
            }
        } else {
            gc_queue_->AddTask(task.release());
        }
    }
}

bool GC::IsReference(BaseClass *cls, const ObjectHeader *ref)
{
    ASSERT(reference_processor_ != nullptr);
    return reference_processor_->IsReference(cls, ref);
}

void GC::EnqueueReferences()
{
    while (true) {
        panda::mem::Reference *ref = nullptr;
        {
            os::memory::LockHolder holder(*cleared_references_lock_);
            if (cleared_references_->empty()) {
                break;
            }
            ref = cleared_references_->back();
            cleared_references_->pop_back();
        }
        ASSERT(ref != nullptr);
        ASSERT(reference_processor_ != nullptr);
        reference_processor_->ScheduleForEnqueue(ref);
    }
}

void GC::NotifyNativeAllocations()
{
    native_objects_notified_.fetch_add(NOTIFY_NATIVE_INTERVAL, std::memory_order_relaxed);
    TriggerGCForNative();
}

void GC::RegisterNativeAllocation(size_t bytes)
{
    size_t allocated;
    do {
        allocated = native_bytes_registered_.load(std::memory_order_relaxed);
    } while (!native_bytes_registered_.compare_exchange_weak(allocated, allocated + bytes));
    if (allocated > std::numeric_limits<size_t>::max() - bytes) {
        native_bytes_registered_.store(std::numeric_limits<size_t>::max(), std::memory_order_relaxed);
    }
    TriggerGCForNative();
}

void GC::RegisterNativeFree(size_t bytes)
{
    size_t allocated;
    size_t new_freed_bytes;
    do {
        allocated = native_bytes_registered_.load(std::memory_order_relaxed);
        new_freed_bytes = std::min(allocated, bytes);
    } while (!native_bytes_registered_.compare_exchange_weak(allocated, allocated - new_freed_bytes));
}

size_t GC::GetNativeBytesFromMallinfoAndRegister() const
{
    size_t mallinfo_bytes = panda::os::mem::GetNativeBytesFromMallinfo();
    size_t all_bytes = mallinfo_bytes + native_bytes_registered_.load(std::memory_order_relaxed);
    return all_bytes;
}

void GC::WaitForGCInManaged(const GCTask &task)
{
    MTManagedThread *thread = MTManagedThread::GetCurrent();
    if (thread != nullptr) {
        ASSERT(Locks::mutator_lock->HasLock());
        ASSERT(!thread->IsDaemon() || thread->GetStatus() == ThreadStatus::RUNNING);
        Locks::mutator_lock->Unlock();
        thread->PrintSuspensionStackIfNeeded();
        WaitForGC(task);
        Locks::mutator_lock->ReadLock();
        ASSERT(Locks::mutator_lock->HasLock());
    }
}

ConcurrentScope::ConcurrentScope(GC *gc, bool auto_start)
{
    gc_ = gc;
    if (auto_start) {
        Start();
    }
}

ConcurrentScope::~ConcurrentScope()
{
    if (started_ && gc_->IsConcurrencyAllowed()) {
        gc_->GetPandaVm()->GetRendezvous()->SafepointBegin();
        gc_->GetPandaVm()->GetMemStats()->RecordGCPauseStart();
    }
}

NO_THREAD_SAFETY_ANALYSIS void ConcurrentScope::Start()
{
    if (!started_ && gc_->IsConcurrencyAllowed()) {
        gc_->GetPandaVm()->GetRendezvous()->SafepointEnd();
        gc_->GetPandaVm()->GetMemStats()->RecordGCPauseEnd();
        started_ = true;
    }
}

void GC::WaitForGCOnPygoteFork(const GCTask &task)
{
    // do nothing if no pygote space
    auto pygote_space_allocator = object_allocator_->GetPygoteSpaceAllocator();
    if (pygote_space_allocator == nullptr) {
        return;
    }

    // do nothing if not at first pygote fork
    if (pygote_space_allocator->GetState() != PygoteSpaceState::STATE_PYGOTE_INIT) {
        return;
    }

    LOG(INFO, GC) << "== GC WaitForGCOnPygoteFork Start ==";

    // do we need a lock?
    // looks all other threads have been stopped before pygote fork

    // 0. indicate that we're rebuilding pygote space
    pygote_space_allocator->SetState(PygoteSpaceState::STATE_PYGOTE_FORKING);

    // 1. trigger gc
    WaitForGC(task);

    // 2. move other space to pygote space
    MoveObjectsToPygoteSpace();

    // 3. indicate that we have done
    pygote_space_allocator->SetState(PygoteSpaceState::STATE_PYGOTE_FORKED);

    // 4. disable pygote for allocation
    object_allocator_->DisablePygoteAlloc();

    LOG(INFO, GC) << "== GC WaitForGCOnPygoteFork End ==";
}

bool GC::IsOnPygoteFork()
{
    auto pygote_space_allocator = object_allocator_->GetPygoteSpaceAllocator();
    return pygote_space_allocator != nullptr &&
           pygote_space_allocator->GetState() == PygoteSpaceState::STATE_PYGOTE_FORKING;
}

void GC::MoveObjectsToPygoteSpace()
{
    trace::ScopedTrace scoped_trace(__FUNCTION__);
    LOG(INFO, GC) << "MoveObjectsToPygoteSpace: start";

    size_t all_size_move = 0;
    size_t moved_objects_num = 0;
    size_t bytes_in_heap_before_move = GetPandaVm()->GetMemStats()->GetFootprintHeap();
    auto pygote_space_allocator = object_allocator_->GetPygoteSpaceAllocator();
    ObjectVisitor move_visitor(
        [this, &pygote_space_allocator, &moved_objects_num, &all_size_move](ObjectHeader *src) -> void {
            size_t size = GetObjectSize(src);
            auto dst = reinterpret_cast<ObjectHeader *>(pygote_space_allocator->Alloc(size));
            ASSERT(dst != nullptr);
            (void)memcpy_s(dst, size, src, size);
            all_size_move += size;
            moved_objects_num++;
            SetForwardAddress(src, dst);
            LOG_DEBUG_GC << "object MOVED from " << std::hex << src << " to " << dst << ", size = " << std::dec << size;
        });

    // Move all small movable objects to pygote space
    object_allocator_->IterateRegularSizeObjects(move_visitor);

    LOG(INFO, GC) << "MoveObjectsToPygoteSpace: move_num = " << moved_objects_num << ", move_size = " << all_size_move;

    if (all_size_move > 0) {
        GetStats()->AddMemoryValue(all_size_move, MemoryTypeStats::MOVED_BYTES);
        GetStats()->AddObjectsValue(moved_objects_num, ObjectTypeStats::MOVED_OBJECTS);
    }
    if (bytes_in_heap_before_move > 0) {
        GetStats()->AddCopiedRatioValue(static_cast<double>(all_size_move) / bytes_in_heap_before_move);
    }

    // Update because we moved objects from object_allocator -> pygote space
    CommonUpdateRefsToMovedObjects([this](const UpdateRefInObject &update_refs_in_object) {
        object_allocator_->IterateNonRegularSizeObjects(update_refs_in_object);
    });

    // Clear the moved objects in old space
    object_allocator_->FreeObjectsMovedToPygoteSpace();

    LOG(INFO, GC) << "MoveObjectsToPygoteSpace: finish";
}

void GC::SetForwardAddress(ObjectHeader *src, ObjectHeader *dst)
{
    auto base_cls = src->ClassAddr<BaseClass>();
    if (base_cls->IsDynamicClass()) {
        auto cls = static_cast<HClass *>(base_cls);
        // Note: During moving phase, 'src => dst'. Consider the src is a DynClass,
        //       since 'dst' is not in GC-status the 'manage-object' inside 'dst' won't be updated to
        //       'dst'. To fix it, we update 'manage-object' here rather than upating phase.
        if (cls->IsHClass()) {
            size_t offset = ObjectHeader::ObjectHeaderSize() + HClass::OffsetOfManageObject();
            dst->SetFieldObject<false, false, true>(GetPandaVm()->GetAssociatedThread(), offset, dst);
        }
    }

    // Set fwd address in src
    bool update_res = false;
    do {
        MarkWord mark_word = src->AtomicGetMark();
        MarkWord fwd_mark_word =
            mark_word.DecodeFromForwardingAddress(static_cast<MarkWord::markWordSize>(ToUintPtr(dst)));
        update_res = src->AtomicSetMark(mark_word, fwd_mark_word);
    } while (!update_res);
}

void GC::UpdateRefsInVRegs(ManagedThread *thread)
{
    LOG_DEBUG_GC << "Update frames for thread: " << thread->GetId();
    for (StackWalker pframe(thread); pframe.HasFrame(); pframe.NextFrame()) {
        LOG_DEBUG_GC << "Frame for method " << pframe.GetMethod()->GetFullName();
        pframe.IterateObjectsWithInfo([&pframe, this](auto &reg_info, auto &vreg) {
            ObjectHeader *object_header = vreg.GetReference();
            if (object_header == nullptr) {
                return true;
            }

            MarkWord mark_word = object_header->AtomicGetMark();
            if (mark_word.GetState() != MarkWord::ObjectState::STATE_GC) {
                return true;
            }

            MarkWord::markWordSize addr = mark_word.GetForwardingAddress();
            LOG_DEBUG_GC << "Update vreg, vreg old val = " << std::hex << object_header << ", new val = 0x" << addr;
            LOG_IF(reg_info.IsAccumulator(), DEBUG, GC) << "^ acc reg";
            if (!pframe.IsCFrame() && reg_info.IsAccumulator()) {
                LOG_DEBUG_GC << "^ acc updated";
                vreg.SetReference(reinterpret_cast<ObjectHeader *>(addr));
            } else {
                pframe.SetVRegValue(reg_info, reinterpret_cast<ObjectHeader *>(addr));
            }
            return true;
        });
    }
}

void GC::AddToStack(PandaStackTL<ObjectHeader *> *objects_stack, ObjectHeader *object)
{
    ASSERT(IsMarked(object));
    ASSERT(object != nullptr);
    LOG_DEBUG_GC << "Add object to stack: " << GetDebugInfoAboutObject(object);
    objects_stack->push(object);
}

ObjectHeader *GC::PopObjectFromStack(PandaStackTL<ObjectHeader *> *objects_stack)
{
    LOG_DEBUG_GC << "stack size is: " << objects_stack->size() << " pop object";
    auto *object = objects_stack->top();
    ASSERT(object != nullptr);
    objects_stack->pop();
    return object;
}

bool GC::IsGenerational() const
{
    return IsGenerationalGCType(gc_type_);
}

uint64_t GC::GetLastGCReclaimedBytes()
{
    return last_gc_reclaimed_bytes.load();
}

template GC *CreateGC<PandaAssemblyLanguageConfig>(GCType gc_type, ObjectAllocatorBase *object_allocator,
                                                   const GCSettings &settings);

}  // namespace panda::mem
