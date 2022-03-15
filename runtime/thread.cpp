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

#include "runtime/include/thread.h"

#include "libpandabase/os/stacktrace.h"
#include "runtime/handle_base-inl.h"
#include "runtime/include/locks.h"
#include "runtime/include/object_header-inl.h"
#include "runtime/include/runtime.h"
#include "runtime/include/runtime_notification.h"
#include "runtime/include/stack_walker.h"
#include "runtime/include/thread_scopes.h"
#include "runtime/interpreter/runtime_interface.h"
#include "runtime/handle_scope-inl.h"
#include "runtime/mem/object_helpers.h"
#include "tooling/pt_thread_info.h"
#include "runtime/include/panda_vm.h"
#include "runtime/mem/runslots_allocator-inl.h"

namespace panda {
using TaggedValue = coretypes::TaggedValue;
using TaggedType = coretypes::TaggedType;

bool ManagedThread::is_initialized = false;
mem::TLAB *ManagedThread::zero_tlab = nullptr;
static const int MIN_PRIORITY = 19;

MTManagedThread::ThreadId MTManagedThread::GetInternalId()
{
    if (internal_id_ == 0) {
        internal_id_ = GetVM()->GetThreadManager()->GetInternalThreadId();
    }
    return internal_id_;
}

static thread_local Thread *s_current_thread = nullptr;

/* static */
void Thread::SetCurrent(Thread *thread)
{
    s_current_thread = thread;
}

/* static */
Thread *Thread::GetCurrent()
{
    return s_current_thread;
}

/* static */
bool ManagedThread::Initialize()
{
    ASSERT(!is_initialized);
    ASSERT(!Thread::GetCurrent());
    ASSERT(!zero_tlab);
    mem::InternalAllocatorPtr allocator = Runtime::GetCurrent()->GetInternalAllocator();
    zero_tlab = allocator->New<mem::TLAB>(nullptr, 0U);
    is_initialized = true;
    return true;
}

/* static */
bool ManagedThread::Shutdown()
{
    ASSERT(is_initialized);
    ASSERT(zero_tlab);
    is_initialized = false;
    ManagedThread::SetCurrent(nullptr);
    mem::InternalAllocatorPtr allocator = Runtime::GetCurrent()->GetInternalAllocator();
    allocator->Delete(zero_tlab);
    zero_tlab = nullptr;
    return true;
}

/* static */
void MTManagedThread::Yield()
{
    LOG(DEBUG, RUNTIME) << "Reschedule the execution of a current thread";
    os::thread::Yield();
}

/* static - creation of the initial Managed thread */
ManagedThread *ManagedThread::Create(Runtime *runtime, PandaVM *vm)
{
    trace::ScopedTrace scoped_trace("ManagedThread::Create");
    mem::InternalAllocatorPtr allocator = runtime->GetInternalAllocator();
    // Create thread structure using new, we rely on this structure to be accessible in child threads after
    // runtime is destroyed
    // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
    return new ManagedThread(os::thread::GetCurrentThreadId(), allocator, vm, Thread::ThreadType::THREAD_TYPE_MANAGED);
}

/* static - creation of the initial MT Managed thread */
MTManagedThread *MTManagedThread::Create(Runtime *runtime, PandaVM *vm)
{
    trace::ScopedTrace scoped_trace("MTManagedThread::Create");
    mem::InternalAllocatorPtr allocator = runtime->GetInternalAllocator();
    // Create thread structure using new, we rely on this structure to be accessible in child threads after
    // runtime is destroyed
    // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
    auto thread = new MTManagedThread(os::thread::GetCurrentThreadId(), allocator, vm);
    thread->ProcessCreatedThread();
    return thread;
}

static mem::InternalAllocatorPtr GetInternalAllocator(ManagedThread *thread)
{
    // WORKAROUND(v.cherkashin): EcmaScript doesn't have HeapManager, so we get internal allocator from runtime
    mem::HeapManager *heap_manager = thread->GetVM()->GetHeapManager();
    if (heap_manager != nullptr) {
        return heap_manager->GetInternalAllocator();
    }
    return Runtime::GetCurrent()->GetInternalAllocator();
}

ManagedThread::ManagedThread(ThreadId id, mem::InternalAllocatorPtr allocator, PandaVM *panda_vm,
                             Thread::ThreadType thread_type)
    : Thread(panda_vm, thread_type), id_(id), ctx_(nullptr), pt_thread_info_(allocator->New<tooling::PtThreadInfo>())
{
    ASSERT(zero_tlab != nullptr);
    stor_ptr_.tlab_ = zero_tlab;

    // WORKAROUND(v.cherkashin): EcmaScript doesn't have GC, so we skip setting barriers for this case
    mem::GC *gc = panda_vm->GetGC();
    if (gc != nullptr) {
        pre_barrier_type_ = gc->GetBarrierSet()->GetPreType();
        post_barrier_type_ = gc->GetBarrierSet()->GetPostType();
    }

    stack_frame_allocator_ = allocator->New<mem::FrameAllocator<>>();
    internal_local_allocator_ =
        mem::InternalAllocator<>::SetUpLocalInternalAllocator(static_cast<mem::Allocator *>(allocator));
    tagged_handle_storage_ = allocator->New<HandleStorage<TaggedType>>(allocator);
    tagged_global_handle_storage_ = allocator->New<GlobalHandleStorage<TaggedType>>(allocator);
    object_header_handle_storage_ = allocator->New<HandleStorage<ObjectHeader *>>(allocator);
}

ManagedThread::~ManagedThread()
{
    // ManagedThread::ShutDown() may not be called when exiting js_thread, so need set current_thread = nullptr
    // NB! ThreadManager is expected to store finished threads in separate list and GC destroys them,
    // current_thread should be nullified in Destroy()
    // (zero_tlab == nullptr means that we destroyed Runtime and do not need to register TLAB)
    if (zero_tlab != nullptr) {
        // We should register TLAB size for MemStats during thread destroy.
        GetVM()->GetHeapManager()->RegisterTLAB(GetTLAB());
    }

    mem::InternalAllocatorPtr allocator = GetInternalAllocator(this);
    allocator->Delete(object_header_handle_storage_);
    allocator->Delete(tagged_global_handle_storage_);
    allocator->Delete(tagged_handle_storage_);
    mem::InternalAllocator<>::FinalizeLocalInternalAllocator(internal_local_allocator_,
                                                             static_cast<mem::Allocator *>(allocator));
    internal_local_allocator_ = nullptr;
    allocator->Delete(stack_frame_allocator_);
    allocator->Delete(pt_thread_info_.release());
}

MTManagedThread::MTManagedThread(ThreadId id, mem::InternalAllocatorPtr allocator, PandaVM *panda_vm)
    : ManagedThread(id, allocator, panda_vm, Thread::ThreadType::THREAD_TYPE_MT_MANAGED),
      thread_frame_states_(allocator->Adapter()),
      waiting_monitor_(nullptr)
{
    internal_id_ = GetVM()->GetThreadManager()->GetInternalThreadId();

    mem::GC *gc = panda_vm->GetGC();
    auto barrier = gc->GetBarrierSet();
    if (barrier->GetPostType() != panda::mem::BarrierType::POST_WRB_NONE) {
        auto func1 = barrier->GetBarrierOperand(panda::mem::BarrierPosition::BARRIER_POSITION_POST, "MIN_ADDR");
        stor_ptr_.card_table_min_addr_ = std::get<void *>(func1.GetValue());
        auto func2 = barrier->GetBarrierOperand(panda::mem::BarrierPosition::BARRIER_POSITION_POST, "CARD_TABLE_ADDR");
        stor_ptr_.card_table_addr_ = std::get<uint8_t *>(func2.GetValue());
    }
    if (barrier->GetPreType() != panda::mem::BarrierType::PRE_WRB_NONE) {
        auto addr =
            barrier->GetBarrierOperand(panda::mem::BarrierPosition::BARRIER_POSITION_PRE, "CONCURRENT_MARKING_ADDR");
        stor_ptr_.concurrent_marking_addr_ = std::get<bool *>(addr.GetValue());
        auto func =
            barrier->GetBarrierOperand(panda::mem::BarrierPosition::BARRIER_POSITION_PRE, "STORE_IN_BUFF_TO_MARK_FUNC");
    }

    auto ext = Runtime::GetCurrent()->GetClassLinker()->GetExtension(GetLanguageContext());
    if (ext != nullptr) {
        stor_ptr_.string_class_ptr_ = ext->GetClassRoot(ClassRoot::STRING);
    }

    auto *rs = allocator->New<mem::ReferenceStorage>(panda_vm->GetGlobalObjectStorage(), allocator, false);
    LOG_IF((rs == nullptr || !rs->Init()), FATAL, RUNTIME) << "Cannot create pt reference storage";
    pt_reference_storage_ = PandaUniquePtr<mem::ReferenceStorage>(rs);
}

MTManagedThread::~MTManagedThread()
{
    ASSERT(internal_id_ != 0);
    GetVM()->GetThreadManager()->RemoveInternalThreadId(internal_id_);

    ASSERT(thread_frame_states_.empty() && "stack should be empty");
}

void MTManagedThread::SafepointPoll()
{
    if (this->TestAllFlags()) {
        trace::ScopedTrace scoped_trace("RunSafepoint");
        panda::interpreter::RuntimeInterface::Safepoint();
    }
}

void MTManagedThread::NativeCodeBegin()
{
    LOG_IF(!(thread_frame_states_.empty() || thread_frame_states_.top() != NATIVE_CODE), FATAL, RUNTIME)
        << LogThreadStack(NATIVE_CODE) << " or stack should be empty";
    thread_frame_states_.push(NATIVE_CODE);
    UpdateStatus(NATIVE);
    is_managed_scope_ = false;
}

void MTManagedThread::NativeCodeEnd()
{
    // thread_frame_states_ should not be accessed without MutatorLock (as runtime could have been destroyed)
    // If this was last frame, it should have been called from Destroy() and it should UpdateStatus to FINISHED
    // after this method
    UpdateStatus(RUNNING);
    is_managed_scope_ = true;
    LOG_IF(thread_frame_states_.empty(), FATAL, RUNTIME) << "stack should be not empty";
    LOG_IF(thread_frame_states_.top() != NATIVE_CODE, FATAL, RUNTIME) << LogThreadStack(NATIVE_CODE);
    thread_frame_states_.pop();
}

bool MTManagedThread::IsInNativeCode() const
{
    LOG_IF(HasClearStack(), FATAL, RUNTIME) << "stack should be not empty";
    return thread_frame_states_.top() == NATIVE_CODE;
}

void MTManagedThread::ManagedCodeBegin()
{
    // thread_frame_states_ should not be accessed without MutatorLock (as runtime could have been destroyed)
    UpdateStatus(RUNNING);
    is_managed_scope_ = true;
    LOG_IF(HasClearStack(), FATAL, RUNTIME) << "stack should be not empty";
    LOG_IF(thread_frame_states_.top() != NATIVE_CODE, FATAL, RUNTIME) << LogThreadStack(MANAGED_CODE);
    thread_frame_states_.push(MANAGED_CODE);
}

void MTManagedThread::ManagedCodeEnd()
{
    LOG_IF(HasClearStack(), FATAL, RUNTIME) << "stack should be not empty";
    LOG_IF(thread_frame_states_.top() != MANAGED_CODE, FATAL, RUNTIME) << LogThreadStack(MANAGED_CODE);
    thread_frame_states_.pop();
    // Should be NATIVE_CODE
    UpdateStatus(NATIVE);
    is_managed_scope_ = false;
}

bool MTManagedThread::IsManagedCode() const
{
    LOG_IF(HasClearStack(), FATAL, RUNTIME) << "stack should be not empty";
    return thread_frame_states_.top() == MANAGED_CODE;
}

// Since we don't allow two consecutive NativeCode frames, there is no managed code on stack if
// its size is 1 and last frame is Native
bool MTManagedThread::HasManagedCodeOnStack() const
{
    if (HasClearStack()) {
        return false;
    }
    if (thread_frame_states_.size() == 1 && IsInNativeCode()) {
        return false;
    }
    return true;
}

bool MTManagedThread::HasClearStack() const
{
    return thread_frame_states_.empty();
}

PandaString MTManagedThread::LogThreadStack(ThreadState new_state) const
{
    PandaStringStream debug_message;
    static std::unordered_map<ThreadState, std::string> thread_state_to_string_map = {
        {ThreadState::NATIVE_CODE, "NATIVE_CODE"}, {ThreadState::MANAGED_CODE, "MANAGED_CODE"}};
    auto new_state_it = thread_state_to_string_map.find(new_state);
    auto top_frame_it = thread_state_to_string_map.find(thread_frame_states_.top());
    ASSERT(new_state_it != thread_state_to_string_map.end());
    ASSERT(top_frame_it != thread_state_to_string_map.end());

    debug_message << "threadId: " << GetId() << " "
                  << "tried go to " << new_state_it->second << " state, but last frame is: " << top_frame_it->second
                  << ", " << thread_frame_states_.size() << " frames in stack (from up to bottom): [";

    PandaStack<ThreadState> copy_stack(thread_frame_states_);
    while (!copy_stack.empty()) {
        auto it = thread_state_to_string_map.find(copy_stack.top());
        ASSERT(it != thread_state_to_string_map.end());
        debug_message << it->second;
        if (copy_stack.size() > 1) {
            debug_message << "|";
        }
        copy_stack.pop();
    }
    debug_message << "]";
    return debug_message.str();
}

void ManagedThread::PushLocalObject(ObjectHeader **object_header)
{
    // Object handles can be created during class initialization, so check lock state only after GC is started.
    ASSERT(!ManagedThread::GetCurrent()->GetVM()->GetGC()->IsGCRunning() ||
           (Locks::mutator_lock->GetState() != MutatorLock::MutatorLockState::UNLOCKED) || this->IsJSThread());
    local_objects_.push_back(object_header);
    LOG(DEBUG, GC) << "PushLocalObject for thread " << std::hex << this << ", obj = " << *object_header;
}

void ManagedThread::PopLocalObject()
{
    // Object handles can be created during class initialization, so check lock state only after GC is started.
    ASSERT(!ManagedThread::GetCurrent()->GetVM()->GetGC()->IsGCRunning() ||
           (Locks::mutator_lock->GetState() != MutatorLock::MutatorLockState::UNLOCKED) || this->IsJSThread());
    ASSERT(!local_objects_.empty());
    LOG(DEBUG, GC) << "PopLocalObject from thread " << std::hex << this << ", obj = " << *local_objects_.back();
    local_objects_.pop_back();
}

std::unordered_set<Monitor *> &MTManagedThread::GetMonitors()
{
    return entered_monitors_;
}

void MTManagedThread::AddMonitor(Monitor *monitor)
{
    os::memory::LockHolder lock(monitor_lock_);
    entered_monitors_.insert(monitor);
    LOG(DEBUG, RUNTIME) << "Adding monitor " << monitor->GetId() << " to thread " << GetId();
}

void MTManagedThread::RemoveMonitor(Monitor *monitor)
{
    os::memory::LockHolder lock(monitor_lock_);
    entered_monitors_.erase(monitor);
    LOG(DEBUG, RUNTIME) << "Removing monitor " << monitor->GetId();
}

void MTManagedThread::ReleaseMonitors()
{
    os::memory::LockHolder lock(monitor_lock_);
    while (!entered_monitors_.empty()) {
        auto monitors = entered_monitors_;
        for (auto monitor : monitors) {
            LOG(DEBUG, RUNTIME) << "Releasing monitor " << monitor->GetId();
            monitor->Release(this);
        }
    }
}

void MTManagedThread::PushLocalObjectLocked(ObjectHeader *obj)
{
    LockedObjectInfo new_locked_obj = {obj, GetFrame()};
    local_objects_locked_.emplace_back(new_locked_obj);
}

void MTManagedThread::PopLocalObjectLocked([[maybe_unused]] ObjectHeader *out)
{
    if (LIKELY(!local_objects_locked_.empty())) {
#ifndef NDEBUG
        ObjectHeader *obj = local_objects_locked_.back().GetObject();
        if (obj != out) {
            LOG(WARNING, RUNTIME) << "Locked object is not paired";
        }
#endif  // !NDEBUG
        local_objects_locked_.pop_back();
    } else {
        LOG(WARNING, RUNTIME) << "PopLocalObjectLocked failed, current thread locked object is empty";
    }
}

const PandaVector<LockedObjectInfo> &MTManagedThread::GetLockedObjectInfos()
{
    return local_objects_locked_;
}

void ManagedThread::UpdateTLAB(mem::TLAB *tlab)
{
    ASSERT(stor_ptr_.tlab_ != nullptr);
    ASSERT(tlab != nullptr);
    stor_ptr_.tlab_ = tlab;
}

void ManagedThread::ClearTLAB()
{
    ASSERT(zero_tlab != nullptr);
    stor_ptr_.tlab_ = zero_tlab;
}

/* Common actions for creation of the thread. */
void MTManagedThread::ProcessCreatedThread()
{
    ManagedThread::SetCurrent(this);
    // Runtime takes ownership of the thread
    trace::ScopedTrace scoped_trace2("ThreadManager::RegisterThread");
    GetVM()->GetThreadManager()->RegisterThread(this);
    NativeCodeBegin();
}

void ManagedThread::UpdateGCRoots()
{
    if ((stor_ptr_.exception_ != nullptr) && (stor_ptr_.exception_->IsForwarded())) {
        stor_ptr_.exception_ = ::panda::mem::GetForwardAddress(stor_ptr_.exception_);
    }
    for (auto &&it : local_objects_) {
        if ((*it)->IsForwarded()) {
            (*it) = ::panda::mem::GetForwardAddress(*it);
        }
    }

    if (!tagged_handle_scopes_.empty()) {
        tagged_handle_storage_->UpdateHeapObject();
        tagged_global_handle_storage_->UpdateHeapObject();
    }

    if (!object_header_handle_scopes_.empty()) {
        object_header_handle_storage_->UpdateHeapObject();
    }
}

/* return true if sleep is interrupted */
bool MTManagedThread::Sleep(uint64_t ms)
{
    auto thread = MTManagedThread::GetCurrent();
    bool is_interrupted = thread->IsInterrupted();
    if (!is_interrupted) {
        thread->TimedWait(IS_SLEEPING, ms, 0);
        is_interrupted = thread->IsInterrupted();
    }
    return is_interrupted;
}

void ManagedThread::SetThreadPriority(int32_t prio)
{
    ThreadId tid = GetId();
    int res = os::thread::SetPriority(tid, prio);
    if (res == 0) {
        LOG(DEBUG, RUNTIME) << "Successfully changed priority for thread " << tid << " to " << prio;
    } else {
        LOG(DEBUG, RUNTIME) << "Cannot change priority for thread " << tid << " to " << prio;
    }
}

uint32_t ManagedThread::GetThreadPriority() const
{
    ThreadId tid = GetId();
    return os::thread::GetPriority(tid);
}

void MTManagedThread::UpdateGCRoots()
{
    ManagedThread::UpdateGCRoots();
    for (auto &it : local_objects_locked_) {
        if (it.GetObject()->IsForwarded()) {
            it.SetObject(panda::mem::GetForwardAddress(it.GetObject()));
        }
    }

    pt_reference_storage_->UpdateMovedRefs();
}

void MTManagedThread::SetDaemon()
{
    is_daemon_ = true;
    GetVM()->GetThreadManager()->AddDaemonThread();
    SetThreadPriority(MIN_PRIORITY);
}

void MTManagedThread::Interrupt(MTManagedThread *thread)
{
    os::memory::LockHolder lock(thread->cond_lock_);
    LOG(DEBUG, RUNTIME) << "Interrupt a thread " << thread->GetId();
    thread->SetInterruptedWithLockHeld(true);
    thread->SignalWithLockHeld();
    thread->InterruptPostImpl();
}

bool MTManagedThread::Interrupted()
{
    os::memory::LockHolder lock(cond_lock_);
    bool res = IsInterruptedWithLockHeld();
    SetInterruptedWithLockHeld(false);
    return res;
}

void MTManagedThread::StopDaemon0()
{
    SetRuntimeTerminated();
}

void MTManagedThread::StopDaemonThread()
{
    StopDaemon0();
    MTManagedThread::Interrupt(this);
}

// NO_THREAD_SAFETY_ANALYSIS due to TSAN not being able to determine lock status
void MTManagedThread::SuspendCheck() NO_THREAD_SAFETY_ANALYSIS
{
    // We should use internal suspension to avoid missing call of IncSuspend
    SuspendImpl(true);
    Locks::mutator_lock->Unlock();
    Locks::mutator_lock->ReadLock();
    ResumeImpl(true);
}

void MTManagedThread::SuspendImpl(bool internal_suspend)
{
    os::memory::LockHolder lock(suspend_lock_);
    LOG(DEBUG, RUNTIME) << "Suspending thread " << GetId();
    if (!internal_suspend && IsUserSuspended()) {
        LOG(DEBUG, RUNTIME) << "thread " << GetId() << " is already suspended";
        return;
    }
    IncSuspended(internal_suspend);
}

void MTManagedThread::ResumeImpl(bool internal_resume)
{
    os::memory::LockHolder lock(suspend_lock_);
    LOG(DEBUG, RUNTIME) << "Resuming thread " << GetId();
    if (!internal_resume && !IsUserSuspended()) {
        LOG(DEBUG, RUNTIME) << "thread " << GetId() << " is already resumed";
        return;
    }
    DecSuspended(internal_resume);
    // Help for UnregisterExitedThread
    TSAN_ANNOTATE_HAPPENS_BEFORE(&stor_32_.fts_);
    StopSuspension();
}

void ManagedThread::VisitGCRoots(const ObjectVisitor &cb)
{
    if (stor_ptr_.exception_ != nullptr) {
        cb(stor_ptr_.exception_);
    }
    for (auto it : local_objects_) {
        cb(*it);
    }

    if (!tagged_handle_scopes_.empty()) {
        tagged_handle_storage_->VisitGCRoots(cb);
        tagged_global_handle_storage_->VisitGCRoots(cb);
    }
    if (!object_header_handle_scopes_.empty()) {
        object_header_handle_storage_->VisitGCRoots(cb);
    }
}

void MTManagedThread::VisitGCRoots(const ObjectVisitor &cb)
{
    ManagedThread::VisitGCRoots(cb);

    pt_reference_storage_->VisitObjects([&cb](const mem::GCRoot &gc_root) { cb(gc_root.GetObjectHeader()); },
                                        mem::RootType::ROOT_PT_LOCAL);
}

void MTManagedThread::Destroy()
{
    ASSERT(this == ManagedThread::GetCurrent());
    if (GetStatus() == FINISHED) {
        return;
    }

    UpdateStatus(TERMINATING);  // Set this status to prevent runtime for destroying itself while this NATTIVE thread
                                // is trying to acquire runtime.
    ReleaseMonitors();
    Runtime *runtime = Runtime::GetCurrent();
    if (!IsDaemon()) {
        runtime->GetNotificationManager()->ThreadEndEvent(GetId());
    }

    {
        ScopedManagedCodeThread s(this);
        GetPtThreadInfo()->Destroy();
    }

    NativeCodeEnd();

    if (GetVM()->GetThreadManager()->UnregisterExitedThread(this)) {
        // Clear current_thread only if unregistration was successful
        ManagedThread::SetCurrent(nullptr);
    }
}

CustomTLSData *ManagedThread::GetCustomTLSData(const char *key)
{
    os::memory::LockHolder lock(*Locks::custom_tls_lock);
    auto it = custom_tls_cache_.find(key);
    if (it == custom_tls_cache_.end()) {
        return nullptr;
    }
    return it->second.get();
}

void ManagedThread::SetCustomTLSData(const char *key, CustomTLSData *data)
{
    os::memory::LockHolder lock(*Locks::custom_tls_lock);
    PandaUniquePtr<CustomTLSData> tls_data(data);
    auto it = custom_tls_cache_.find(key);
    if (it == custom_tls_cache_.end()) {
        custom_tls_cache_[key] = {PandaUniquePtr<CustomTLSData>()};
    }
    custom_tls_cache_[key].swap(tls_data);
}

LanguageContext ManagedThread::GetLanguageContext()
{
    return GetVM()->GetLanguageContext();
}

void MTManagedThread::FreeInternalMemory()
{
    thread_frame_states_.~PandaStack<ThreadState>();
    local_objects_locked_.~PandaVector<LockedObjectInfo>();

    ManagedThread::FreeInternalMemory();
}

void ManagedThread::FreeInternalMemory()
{
    local_objects_.~PandaVector<ObjectHeader **>();
    {
        os::memory::LockHolder lock(*Locks::custom_tls_lock);
        custom_tls_cache_.~PandaMap<const char *, PandaUniquePtr<CustomTLSData>>();
    }

    mem::InternalAllocatorPtr allocator = Runtime::GetCurrent()->GetInternalAllocator();
    allocator->Delete(stack_frame_allocator_);
    allocator->Delete(internal_local_allocator_);

    {
        ScopedManagedCodeThread smt(MTManagedThread::GetCurrent());
        pt_thread_info_->Destroy();
    }
    allocator->Delete(pt_thread_info_.release());

    tagged_handle_scopes_.~PandaVector<HandleScope<coretypes::TaggedType> *>();
    allocator->Delete(tagged_handle_storage_);
    allocator->Delete(tagged_global_handle_storage_);

    allocator->Delete(object_header_handle_storage_);
    object_header_handle_scopes_.~PandaVector<HandleScope<ObjectHeader *> *>();
}

void ManagedThread::PrintSuspensionStackIfNeeded()
{
    if (!Runtime::GetOptions().IsSafepointBacktrace()) {
        return;
    }
    PandaStringStream out;
    out << "Thread " << GetId() << " is suspended at\n";
    PrintStack(out);
    LOG(INFO, RUNTIME) << out.str();
}

}  // namespace panda
