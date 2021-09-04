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

#include "runtime/mem/heap_manager.h"

#include <string>

#include "heap_manager.h"
#include "include/runtime.h"
#include "include/locks.h"
#include "include/thread.h"
#include "libpandabase/mem/mmap_mem_pool-inl.h"
#include "libpandabase/mem/pool_manager.h"
#include "libpandabase/utils/logger.h"
#include "mem/pool_manager.h"
#include "mem/mmap_mem_pool-inl.h"
#include "mem/internal_allocator-inl.h"
#include "mem/gc/hybrid-gc/hybrid_object_allocator.h"
#include "runtime/include/locks.h"
#include "runtime/include/runtime.h"
#include "runtime/include/runtime_notification.h"
#include "runtime/include/thread.h"
#include "runtime/include/thread_scopes.h"
#include "runtime/mem/internal_allocator-inl.h"
#include "runtime/handle_base-inl.h"
#include "runtime/include/panda_vm.h"
#include "runtime/mem/gc/g1/g1-gc.h"

namespace panda::mem {

bool HeapManager::Initialize(GCType gc_type, bool single_threaded, bool use_tlab, MemStatsType *mem_stats,
                             InternalAllocatorPtr internal_allocator, bool create_pygote_space)
{
    trace::ScopedTrace scoped_trace("HeapManager::Initialize");
    bool ret = false;
    mem_stats_ = mem_stats;
    internalAllocator_ = internal_allocator;
    // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define FWD_GC_INIT(type, mem_stats)                                                \
    case type:                                                                      \
        if (single_threaded) {                                                      \
            ret = Initialize<type, MT_MODE_SINGLE>(mem_stats, create_pygote_space); \
        } else {                                                                    \
            ret = Initialize<type, MT_MODE_MULTI>(mem_stats, create_pygote_space);  \
        }                                                                           \
        break

    switch (gc_type) {
        FWD_GC_INIT(GCType::EPSILON_GC, mem_stats);
        FWD_GC_INIT(GCType::STW_GC, mem_stats);
        FWD_GC_INIT(GCType::GEN_GC, mem_stats);
        FWD_GC_INIT(GCType::HYBRID_GC, mem_stats);
        FWD_GC_INIT(GCType::G1_GC, mem_stats);
        default:
            LOG(FATAL, GC) << "Invalid init for gc_type = " << static_cast<int>(gc_type);
            break;
    }
#undef FWD_GC_INIT
    if (!objectAllocator_.AsObjectAllocator()->IsTLABSupported() || single_threaded) {
        use_tlab = false;
    }
    use_tlab_for_allocations_ = use_tlab;
    // Now, USE_TLAB_FOR_ALLOCATIONS option is supported only for Generational GCs
    ASSERT(IsGenerationalGCType(gc_type) || (!use_tlab_for_allocations_));
    return ret;
}

void HeapManager::SetPandaVM(PandaVM *vm)
{
    vm_ = vm;
    gc_ = vm_->GetGC();
    notification_manager_ = Runtime::GetCurrent()->GetNotificationManager();
}

bool HeapManager::Finalize()
{
    delete codeAllocator_;
    objectAllocator_->VisitAndRemoveAllPools(
        [](void *mem, [[maybe_unused]] size_t size) { PoolManager::GetMmapMemPool()->FreePool(mem, size); });
    delete static_cast<Allocator *>(objectAllocator_);
    objectAllocator_ = nullptr;

    return true;
}

ObjectHeader *HeapManager::AllocateObject(BaseClass *cls, size_t size, Alignment align, MTManagedThread *thread)
{
    ASSERT(vm_->GetLanguageContext().GetLanguage() == panda_file::SourceLang::ECMASCRIPT || !GetGC()->IsGCRunning() ||
           Locks::mutator_lock->HasLock());
    TriggerGCIfNeeded();
    if (thread == nullptr) {
        thread = MTManagedThread::GetCurrent();
        ASSERT(thread != nullptr);
    }
    void *mem = AllocateMemoryForObject(size, align, thread);
    if (UNLIKELY(mem == nullptr)) {
        mem = TryGCAndAlloc(size, align, thread);
        if (UNLIKELY(mem == nullptr)) {
            ThrowOutOfMemoryError("AllocateObject failed");
            return nullptr;
        }
    }
    LOG(DEBUG, ALLOC_OBJECT) << "Alloc object at " << std::hex << mem << " size: " << size;
    ObjectHeader *object = InitObjectHeaderAtMem(cls, mem);
    bool is_object_finalizable = IsObjectFinalized(cls);
    if (UNLIKELY(is_object_finalizable || GetNotificationManager()->HasAllocationListeners())) {
        // Use object handle here as RegisterFinalizedObject can trigger GC
        [[maybe_unused]] HandleScope<ObjectHeader *> scope(thread);
        VMHandle<ObjectHeader> handle(thread, object);
        RegisterFinalizedObject(handle.GetPtr(), cls, is_object_finalizable);
        GetNotificationManager()->ObjectAllocEvent(cls, handle.GetPtr(), thread, size);
        object = handle.GetPtr();
    }
    return object;
}

void *HeapManager::TryGCAndAlloc(size_t size, Alignment align, panda::MTManagedThread *thread)
{
    // do not try many times in case of OOM scenarios.
    constexpr size_t ALLOC_RETRY = 4;
    size_t alloc_try_cnt = 0;
    void *mem = nullptr;
    bool is_generational = GetGC()->IsGenerational();
    ASSERT(!thread->HasPendingException());

    while (mem == nullptr && alloc_try_cnt++ < ALLOC_RETRY) {
        GCTaskCause cause;
        // add comment why -1
        if (alloc_try_cnt == ALLOC_RETRY - 1 || !is_generational) {
            cause = GCTaskCause::OOM_CAUSE;
        } else {
            cause = GCTaskCause::YOUNG_GC_CAUSE;
        }
        GetGC()->WaitForGCInManaged(GCTask(cause, thread));
        mem = AllocateMemoryForObject(size, align, thread);
        if (mem != nullptr) {
            // we could set OOM in gc, but we need to clear it if next gc was successful and we allocated memory
            thread->ClearException();
        } else {
            auto reclaimed_bytes = GetGC()->GetLastGCReclaimedBytes();
            // if last GC reclaimed some bytes - it means that we have a progress in JVM, just this thread was unlucky
            // to get some memory. We reset alloc_try_cnt to try again.
            if (reclaimed_bytes != 0) {
                alloc_try_cnt = 0;
            }
        }
    }
    return mem;
}

void *HeapManager::AllocateMemoryForObject(size_t size, Alignment align, ManagedThread *thread)
{
    void *mem = nullptr;
    if (UseTLABForAllocations() && size <= GetTLABMaxAllocSize()) {
        ASSERT(thread != nullptr);
        ASSERT(GetGC()->IsTLABsSupported());
        // Try to allocate an object via TLAB
        TLAB *current_tlab = thread->GetTLAB();
        ASSERT(current_tlab != nullptr);  // A thread's TLAB must be initialized at least via some ZERO tlab values.
        mem = current_tlab->Alloc(size);
        if (mem == nullptr) {
            // We couldn't allocate an object via current TLAB,
            // Therefore, create a new one and allocate in it.
            if (CreateNewTLAB(thread)) {
                current_tlab = thread->GetTLAB();
                mem = current_tlab->Alloc(size);
            }
        }
        if (PANDA_TRACK_TLAB_ALLOCATIONS && (mem != nullptr)) {
            mem_stats_->RecordAllocateObject(GetAlignedObjectSize(size), SpaceType::SPACE_TYPE_OBJECT);
        }
    }
    if (mem == nullptr) {  // if mem == nullptr, try to use common allocate scenario
        mem = objectAllocator_->Allocate(size, align, thread);
    }
    return mem;
}

template <bool IsFirstClassClass>
ObjectHeader *HeapManager::AllocateNonMovableObject(BaseClass *cls, size_t size, Alignment align, ManagedThread *thread)
{
    ASSERT(vm_->GetLanguageContext().GetLanguage() == panda_file::SourceLang::ECMASCRIPT || !GetGC()->IsGCRunning() ||
           Locks::mutator_lock->HasLock());
    TriggerGCIfNeeded();
    void *mem = objectAllocator_->AllocateNonMovable(size, align, thread);
    if (UNLIKELY(mem == nullptr)) {
        GCTaskCause cause = GCTaskCause::OOM_CAUSE;
        GetGC()->WaitForGCInManaged(GCTask(cause, thread));
        mem = objectAllocator_->AllocateNonMovable(size, align, thread);
    }
    if (UNLIKELY(mem == nullptr)) {
        ThrowOutOfMemoryError("AllocateNonMovableObject failed");
        return nullptr;
    }
    LOG(DEBUG, ALLOC_OBJECT) << "Alloc non-movable object at " << std::hex << mem;
    auto *object = InitObjectHeaderAtMem(cls, mem);
    // cls can be null for first class creation, when we create ClassRoot::Class
    // NOLINTNEXTLINE(readability-braces-around-statements, readability-misleading-indentation)
    if constexpr (IsFirstClassClass) {
        ASSERT(cls == nullptr);
        // NOLINTNEXTLINE(readability-braces-around-statements, readability-misleading-indentation)
    } else {
        ASSERT(cls != nullptr);
        bool is_object_finalizable = IsObjectFinalized(cls);
        RegisterFinalizedObject(object, cls, is_object_finalizable);
        GetNotificationManager()->ObjectAllocEvent(cls, object, thread, size);
    }
    return object;
}

ObjectHeader *HeapManager::InitObjectHeaderAtMem(BaseClass *cls, void *mem)
{
    ASSERT(mem != nullptr);
    ASSERT(vm_->GetLanguageContext().GetLanguage() == panda_file::SourceLang::ECMASCRIPT || !GetGC()->IsGCRunning() ||
           Locks::mutator_lock->HasLock());

    auto object = static_cast<ObjectHeader *>(mem);
    // we need zeroed memory here according to ISA
    ASSERT(object->AtomicGetMark().GetValue() == 0);
    ASSERT(object->AtomicClassAddr<BaseClass *>() == nullptr);
    // The order is crucial here - we need to have 0 class word to avoid data race with concurrent sweep.
    // Otherwise we can remove not initialized object.
    GetGC()->InitGCBits(object);
    object->SetClass(cls);
    return object;
}

void HeapManager::TriggerGCIfNeeded()
{
    if (vm_->GetGCTrigger()->IsGcTriggered()) {
        GetGC()->Trigger();
    }
}

Frame *HeapManager::AllocateFrame(size_t size)
{
    ASSERT(vm_->GetLanguageContext().GetLanguage() == panda_file::SourceLang::ECMASCRIPT || !GetGC()->IsGCRunning() ||
           Locks::mutator_lock->HasLock());
    StackFrameAllocator *frame_allocator = GetCurrentStackFrameAllocator();
    return static_cast<Frame *>(frame_allocator->Alloc(size));
}

bool HeapManager::CreateNewTLAB(ManagedThread *thread)
{
    ASSERT(vm_->GetLanguageContext().GetLanguage() == panda_file::SourceLang::ECMASCRIPT || !GetGC()->IsGCRunning() ||
           Locks::mutator_lock->HasLock());
    ASSERT(thread != nullptr);
    TLAB *new_tlab = objectAllocator_.AsObjectAllocator()->CreateNewTLAB(thread);
    if (new_tlab != nullptr) {
        RegisterTLAB(thread->GetTLAB());
        thread->UpdateTLAB(new_tlab);
        return true;
    }
    return false;
}

void HeapManager::RegisterTLAB(TLAB *tlab)
{
    ASSERT(tlab != nullptr);
    if (!PANDA_TRACK_TLAB_ALLOCATIONS && (tlab->GetOccupiedSize() != 0)) {
        mem_stats_->RecordAllocateObject(tlab->GetOccupiedSize(), SpaceType::SPACE_TYPE_OBJECT);
    }
}

void HeapManager::FreeFrame(Frame *frame_ptr)
{
    ASSERT(vm_->GetLanguageContext().GetLanguage() == panda_file::SourceLang::ECMASCRIPT || !GetGC()->IsGCRunning() ||
           Locks::mutator_lock->HasLock());
    StackFrameAllocator *frame_allocator = GetCurrentStackFrameAllocator();
    frame_allocator->Free(frame_ptr);
}

CodeAllocator *HeapManager::GetCodeAllocator() const
{
    return codeAllocator_;
}

InternalAllocatorPtr HeapManager::GetInternalAllocator()
{
    return internalAllocator_;
}

ObjectAllocatorPtr HeapManager::GetObjectAllocator()
{
    return objectAllocator_;
}

StackFrameAllocator *HeapManager::GetCurrentStackFrameAllocator()
{
    return ManagedThread::GetCurrent()->GetStackFrameAllocator();
}

void HeapManager::PreZygoteFork()
{
    GetGC()->WaitForGCOnPygoteFork(GCTask(GCTaskCause::PYGOTE_FORK_CAUSE));
}

float HeapManager::GetTargetHeapUtilization() const
{
    return target_utilization_;
}

void HeapManager::SetTargetHeapUtilization(float target)
{
    ASSERT_PRINT(target > 0.0F && target < 1.0F, "Target heap utilization should be in the range (0,1)");
    target_utilization_ = target;
}

size_t HeapManager::GetTotalMemory() const
{
    return vm_->GetGCTrigger()->GetTargetFootprint();
}

size_t HeapManager::GetFreeMemory() const
{
    return helpers::UnsignedDifference(GetTotalMemory(), vm_->GetMemStats()->GetFootprintHeap());
}

void HeapManager::DumpHeap(PandaOStringStream *o_string_stream)
{
    size_t obj_cnt = 0;
    *o_string_stream << "Dumping heap" << std::endl;
    objectAllocator_->IterateOverObjects([&obj_cnt, &o_string_stream](ObjectHeader *mem) {
        DumpObject(static_cast<ObjectHeader *>(mem), o_string_stream);
        obj_cnt++;
    });
    *o_string_stream << "Total dumped " << obj_cnt << std::endl;
}

/**
 * \brief Check whether the given object is an instance of the given class.
 * @param obj - ObjectHeader pointer
 * @param h_class - Class pointer
 * @param assignable - whether the subclass of h_class counts
 * @return true if obj is instanceOf h_class, otherwise false
 */
static bool MatchesClass(ObjectHeader *obj, Class *h_class, bool assignable)
{
    if (assignable) {
        return obj->IsInstanceOf(h_class);
    }
    return obj->ClassAddr<Class>() == h_class;
}

void HeapManager::CountInstances(const PandaVector<Class *> &classes, bool assignable, uint64_t *counts)
{
    auto objects_checker = [&](ObjectHeader *obj) {
        for (size_t i = 0; i < classes.size(); ++i) {
            if (classes[i] == nullptr) {
                continue;
            }
            if (MatchesClass(obj, classes[i], assignable)) {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                ++counts[i];
            }
        }
    };
    {
        MTManagedThread *thread = MTManagedThread::GetCurrent();
        ASSERT(thread != nullptr);
        ScopedChangeThreadStatus sts(thread, ThreadStatus::RUNNING);
        ScopedSuspendAllThreadsRunning ssatr(Runtime::GetCurrent()->GetPandaVM()->GetRendezvous());
        GetObjectAllocator().AsObjectAllocator()->IterateOverObjects(objects_checker);
    }
}

void HeapManager::SetIsFinalizableFunc(IsObjectFinalizebleFunc func)
{
    IsObjectFinalizebleFunc_ = func;
}

void HeapManager::SetRegisterFinalizeReferenceFunc(RegisterFinalizeReferenceFunc func)
{
    RegisterFinalizeReferenceFunc_ = func;
}

bool HeapManager::IsObjectFinalized(BaseClass *cls)
{
    return IsObjectFinalizebleFunc_ != nullptr && IsObjectFinalizebleFunc_(cls);
}

void HeapManager::RegisterFinalizedObject(ObjectHeader *object, BaseClass *cls, bool is_object_finalizable)
{
    if (is_object_finalizable) {
        ASSERT(RegisterFinalizeReferenceFunc_ != nullptr);
        RegisterFinalizeReferenceFunc_(object, cls);
    }
}

template ObjectHeader *HeapManager::AllocateNonMovableObject<true>(BaseClass *cls, size_t size, Alignment align,
                                                                   ManagedThread *thread);

template ObjectHeader *HeapManager::AllocateNonMovableObject<false>(BaseClass *cls, size_t size, Alignment align,
                                                                    ManagedThread *thread);
}  // namespace panda::mem
