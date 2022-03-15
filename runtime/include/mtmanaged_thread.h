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

#ifndef PANDA_RUNTIME_INCLUDE_MTMANAGED_THREAD_H_
#define PANDA_RUNTIME_INCLUDE_MTMANAGED_THREAD_H_

#include "managed_thread.h"

// See issue 4100, js thread always true
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define ASSERT_MANAGED_CODE() ASSERT(::panda::MTManagedThread::GetCurrent()->IsManagedCode())
#define ASSERT_NATIVE_CODE() ASSERT(::panda::MTManagedThread::GetCurrent()->IsInNativeCode())  // NOLINT

namespace panda {
class MTManagedThread : public ManagedThread {
public:
    enum ThreadState : uint8_t { NATIVE_CODE = 0, MANAGED_CODE = 1 };

    ThreadId GetInternalId();

    static MTManagedThread *Create(Runtime *runtime, PandaVM *vm);

    explicit MTManagedThread(ThreadId id, mem::InternalAllocatorPtr allocator, PandaVM *vm);
    ~MTManagedThread() override;

    std::unordered_set<Monitor *> &GetMonitors();
    void AddMonitor(Monitor *monitor);
    void RemoveMonitor(Monitor *monitor);
    void ReleaseMonitors();

    void PushLocalObjectLocked(ObjectHeader *obj);
    void PopLocalObjectLocked(ObjectHeader *out);
    const PandaVector<LockedObjectInfo> &GetLockedObjectInfos();

    void VisitGCRoots(const ObjectVisitor &cb) override;
    void UpdateGCRoots() override;

    ThreadStatus GetWaitingMonitorOldStatus() const
    {
        return monitor_old_status_;
    }

    void SetWaitingMonitorOldStatus(ThreadStatus status)
    {
        monitor_old_status_ = status;
    }

    static bool IsManagedScope()
    {
        auto thread = GetCurrent();
        return thread != nullptr && thread->is_managed_scope_;
    }

    void FreeInternalMemory() override;

    static bool Sleep(uint64_t ms);

    void SuspendImpl(bool internal_suspend = false);
    void ResumeImpl(bool internal_resume = false);

    Monitor *GetWaitingMonitor() const
    {
        return waiting_monitor_;
    }

    void SetWaitingMonitor(Monitor *monitor)
    {
        ASSERT(waiting_monitor_ == nullptr || monitor == nullptr);
        waiting_monitor_ = monitor;
    }

    virtual void StopDaemonThread();

    bool IsDaemon()
    {
        return is_daemon_;
    }

    void SetDaemon();

    virtual void Destroy();

    static void Yield();

    static void Interrupt(MTManagedThread *thread);

    [[nodiscard]] bool HasManagedCodeOnStack() const;
    [[nodiscard]] bool HasClearStack() const;

    /**
     * Transition to suspended and back to runnable, re-acquire share on mutator_lock_
     */
    void SuspendCheck();

    bool IsUserSuspended()
    {
        return user_code_suspend_count_ > 0;
    }

    // Need to acquire the mutex before waiting to avoid scheduling between monitor release and clond_lock acquire
    os::memory::Mutex *GetWaitingMutex() RETURN_CAPABILITY(cond_lock_)
    {
        return &cond_lock_;
    }

    void Signal()
    {
        os::memory::LockHolder lock(cond_lock_);
        cond_var_.Signal();
    }

    bool Interrupted();

    bool IsInterrupted() const
    {
        os::memory::LockHolder lock(cond_lock_);
        return is_interrupted_;
    }

    bool IsInterruptedWithLockHeld() const REQUIRES(cond_lock_)
    {
        return is_interrupted_;
    }

    void ClearInterrupted()
    {
        os::memory::LockHolder lock(cond_lock_);
        is_interrupted_ = false;
    }

    void IncSuspended(bool is_internal) REQUIRES(suspend_lock_)
    {
        if (!is_internal) {
            user_code_suspend_count_++;
        }
        // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
        auto old_count = suspend_count_++;
        if (old_count == 0) {
            SetFlag(SUSPEND_REQUEST);
        }
    }

    void DecSuspended(bool is_internal) REQUIRES(suspend_lock_)
    {
        if (!is_internal) {
            ASSERT(user_code_suspend_count_ != 0);
            user_code_suspend_count_--;
        }
        if (suspend_count_ > 0) {
            suspend_count_--;
            if (suspend_count_ == 0) {
                ClearFlag(SUSPEND_REQUEST);
            }
        }
    }

    static bool ThreadIsMTManagedThread(Thread *thread)
    {
        ASSERT(thread != nullptr);
        return thread->GetThreadType() == Thread::ThreadType::THREAD_TYPE_MT_MANAGED;
    }

    static MTManagedThread *CastFromThread(Thread *thread)
    {
        ASSERT(thread != nullptr);
        ASSERT(ThreadIsMTManagedThread(thread));
        return static_cast<MTManagedThread *>(thread);
    }

    /**
     * @brief GetCurrentRaw Unsafe method to get current MTManagedThread.
     * It can be used in hotspots to get the best performance.
     * We can only use this method in places where the MTManagedThread exists.
     * @return pointer to MTManagedThread
     */
    static MTManagedThread *GetCurrentRaw()
    {
        return CastFromThread(Thread::GetCurrent());
    }

    /**
     * @brief GetCurrent Safe method to gets current MTManagedThread.
     * @return pointer to MTManagedThread or nullptr (if current thread is not a managed thread)
     */
    static MTManagedThread *GetCurrent()
    {
        Thread *thread = Thread::GetCurrent();
        ASSERT(thread != nullptr);
        if (ThreadIsMTManagedThread(thread)) {
            return CastFromThread(thread);
        }
        // no guarantee that we will return nullptr here in the future
        return nullptr;
    }

    void SafepointPoll();

    /**
     * From NativeCode you can call ManagedCodeBegin.
     * From ManagedCode you can call NativeCodeBegin.
     * Call the same type is forbidden.
     */
    virtual void NativeCodeBegin();
    virtual void NativeCodeEnd();
    [[nodiscard]] virtual bool IsInNativeCode() const;

    virtual void ManagedCodeBegin();
    virtual void ManagedCodeEnd();
    [[nodiscard]] virtual bool IsManagedCode() const;

    void WaitWithLockHeld(ThreadStatus wait_status) REQUIRES(cond_lock_)
    {
        ASSERT(wait_status == IS_WAITING);
        // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
        auto old_status = GetStatus();
        // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
        UpdateStatus(wait_status);
        WaitWithLockHeldInternal();
        // Unlock before setting status RUNNING to handle MutatorReadLock without inversed lock order.
        cond_lock_.Unlock();
        UpdateStatus(old_status);
        cond_lock_.Lock();
    }

    static void WaitForSuspension(ManagedThread *thread)
    {
        static constexpr uint32_t YIELD_ITERS = 500;
        uint32_t loop_iter = 0;
        while (thread->GetStatus() == RUNNING) {
            if (!thread->IsSuspended()) {
                LOG(WARNING, RUNTIME) << "No request for suspension, do not wait thread " << thread->GetId();
                break;
            }

            loop_iter++;
            if (loop_iter < YIELD_ITERS) {
                MTManagedThread::Yield();
            } else {
                // Use native sleep over ManagedThread::Sleep to prevent potentially time consuming
                // mutator_lock locking and unlocking
                static constexpr uint32_t SHORT_SLEEP_MS = 1;
                os::thread::NativeSleep(SHORT_SLEEP_MS);
            }
        }
    }

    void Wait(ThreadStatus wait_status)
    {
        ASSERT(wait_status == IS_WAITING);
        auto old_status = GetStatus();
        {
            os::memory::LockHolder lock(cond_lock_);
            UpdateStatus(wait_status);
            WaitWithLockHeldInternal();
        }
        UpdateStatus(old_status);
    }

    bool TimedWaitWithLockHeld(ThreadStatus wait_status, uint64_t timeout, uint64_t nanos, bool is_absolute = false)
        REQUIRES(cond_lock_)
    {
        ASSERT(wait_status == IS_TIMED_WAITING || wait_status == IS_SLEEPING || wait_status == IS_BLOCKED ||
               wait_status == IS_SUSPENDED || wait_status == IS_COMPILER_WAITING ||
               wait_status == IS_WAITING_INFLATION);
        // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
        auto old_status = GetStatus();
        // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
        UpdateStatus(wait_status);
        // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
        bool res = TimedWaitWithLockHeldInternal(timeout, nanos, is_absolute);
        // Unlock before setting status RUNNING to handle MutatorReadLock without inversed lock order.
        cond_lock_.Unlock();
        UpdateStatus(old_status);
        cond_lock_.Lock();
        return res;
    }

    bool TimedWait(ThreadStatus wait_status, uint64_t timeout, uint64_t nanos = 0, bool is_absolute = false)
    {
        ASSERT(wait_status == IS_TIMED_WAITING || wait_status == IS_SLEEPING || wait_status == IS_BLOCKED ||
               wait_status == IS_SUSPENDED || wait_status == IS_COMPILER_WAITING ||
               wait_status == IS_WAITING_INFLATION);
        auto old_status = GetStatus();
        bool res = false;
        {
            os::memory::LockHolder lock(cond_lock_);
            UpdateStatus(wait_status);
            res = TimedWaitWithLockHeldInternal(timeout, nanos, is_absolute);
        }
        UpdateStatus(old_status);
        return res;
    }

    void WaitSuspension()
    {
        constexpr int TIMEOUT = 100;
        auto old_status = GetStatus();
        UpdateStatus(IS_SUSPENDED);
        {
            PrintSuspensionStackIfNeeded();
            os::memory::LockHolder lock(suspend_lock_);
            while (suspend_count_ > 0) {
                suspend_var_.TimedWait(&suspend_lock_, TIMEOUT);
                // In case runtime is being terminated, we should abort suspension and release monitors
                if (UNLIKELY(IsRuntimeTerminated())) {
                    suspend_lock_.Unlock();
                    TerminationLoop();
                }
            }
            ASSERT(!IsSuspended());
        }
        UpdateStatus(old_status);
    }

    void TerminationLoop()
    {
        ASSERT(IsRuntimeTerminated());
        // Free all monitors first in case we are suspending in status IS_BLOCKED
        ReleaseMonitors();
        UpdateStatus(IS_TERMINATED_LOOP);
        while (true) {
            static constexpr unsigned int LONG_SLEEP_MS = 1000000;
            os::thread::NativeSleep(LONG_SLEEP_MS);
        }
    }

    // NO_THREAD_SAFETY_ANALYSIS due to TSAN not being able to determine lock status
    void TransitionFromRunningToSuspended(enum ThreadStatus status) NO_THREAD_SAFETY_ANALYSIS
    {
        // Workaround: We masked the assert for 'ManagedThread::GetCurrent() == null' condition,
        //             because JSThread updates status_ not from current thread.
        //             (Remove it when issue 5183 is resolved)
        ASSERT(ManagedThread::GetCurrent() == this || ManagedThread::GetCurrent() == nullptr);

        Locks::mutator_lock->Unlock();
        StoreStatus(status);
    }

    // NO_THREAD_SAFETY_ANALYSIS due to TSAN not being able to determine lock status
    void TransitionFromSuspendedToRunning(enum ThreadStatus status) NO_THREAD_SAFETY_ANALYSIS
    {
        // Workaround: We masked the assert for 'ManagedThread::GetCurrent() == null' condition,
        //             because JSThread updates status_ not from current thread.
        //             (Remove it when issue 5183 is resolved)
        ASSERT(ManagedThread::GetCurrent() == this || ManagedThread::GetCurrent() == nullptr);

        // NB! This thread is treated as suspended so when we transition from suspended state to
        // running we need to check suspension flag and counter so SafepointPoll has to be done before
        // acquiring mutator_lock.
        StoreStatusWithSafepoint(status);
        Locks::mutator_lock->ReadLock();
    }

    void UpdateStatus(enum ThreadStatus status)
    {
        // Workaround: We masked the assert for 'ManagedThread::GetCurrent() == null' condition,
        //             because JSThread updates status_ not from current thread.
        //             (Remove it when issue 5183 is resolved)
        ASSERT(ManagedThread::GetCurrent() == this || ManagedThread::GetCurrent() == nullptr);

        ThreadStatus old_status = GetStatus();
        if (old_status == RUNNING && status != RUNNING) {
            TransitionFromRunningToSuspended(status);
        } else if (old_status != RUNNING && status == RUNNING) {
            TransitionFromSuspendedToRunning(status);
        } else if (status == TERMINATING) {
            // Using Store with safepoint to be sure that main thread didn't suspend us while trying to update status
            StoreStatusWithSafepoint(status);
        } else {
            // NB! Status is not a simple bit, without atomics it can produce faulty GetStatus.
            StoreStatus(status);
        }
    }

    MTManagedThread *GetNextWait() const
    {
        return next_;
    }

    void SetWaitNext(MTManagedThread *next)
    {
        next_ = next;
    }

    mem::ReferenceStorage *GetPtReferenceStorage() const
    {
        return pt_reference_storage_.get();
    }

protected:
    virtual void ProcessCreatedThread();

    virtual void StopDaemon0();

    void StopSuspension() REQUIRES(suspend_lock_)
    {
        // Lock before this call.
        suspend_var_.Signal();
    }

    os::memory::Mutex *GetSuspendMutex() RETURN_CAPABILITY(suspend_lock_)
    {
        return &suspend_lock_;
    }

    void WaitInternal()
    {
        os::memory::LockHolder lock(cond_lock_);
        WaitWithLockHeldInternal();
    }

    void WaitWithLockHeldInternal() REQUIRES(cond_lock_)
    {
        ASSERT(this == ManagedThread::GetCurrent());
        cond_var_.Wait(&cond_lock_);
    }

    bool TimedWaitInternal(uint64_t timeout, uint64_t nanos, bool is_absolute = false)
    {
        os::memory::LockHolder lock(cond_lock_);
        return TimedWaitWithLockHeldInternal(timeout, nanos, is_absolute);
    }

    bool TimedWaitWithLockHeldInternal(uint64_t timeout, uint64_t nanos, bool is_absolute = false) REQUIRES(cond_lock_)
    {
        ASSERT(this == ManagedThread::GetCurrent());
        return cond_var_.TimedWait(&cond_lock_, timeout, nanos, is_absolute);
    }

    void SignalWithLockHeld() REQUIRES(cond_lock_)
    {
        cond_var_.Signal();
    }

    void SetInterruptedWithLockHeld(bool interrupted) REQUIRES(cond_lock_)
    {
        is_interrupted_ = interrupted;
    }

private:
    PandaString LogThreadStack(ThreadState new_state) const;

    void StoreStatusWithSafepoint(ThreadStatus status)
    {
        while (true) {
            SafepointPoll();
            union FlagsAndThreadStatus old_fts {
            };
            union FlagsAndThreadStatus new_fts {
            };
            old_fts.as_int = ReadFlagsAndThreadStatusUnsafe();      // NOLINT(cppcoreguidelines-pro-type-union-access)
            new_fts.as_struct.flags = old_fts.as_struct.flags;      // NOLINT(cppcoreguidelines-pro-type-union-access)
            new_fts.as_struct.status = status;                      // NOLINT(cppcoreguidelines-pro-type-union-access)
            bool no_flags = (old_fts.as_struct.flags == NO_FLAGS);  // NOLINT(cppcoreguidelines-pro-type-union-access)

            // clang-format conflicts with CodeCheckAgent, so disable it here
            // clang-format off
            if (no_flags && stor_32_.fts_.as_atomic.compare_exchange_weak(
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
                old_fts.as_nonvolatile_int, new_fts.as_nonvolatile_int, std::memory_order_release)) {
                // If CAS succeeded, we set new status and no request occurred here, safe to proceed.
                break;
            }
            // clang-format on
        }
    }

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    MTManagedThread *next_ {nullptr};

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    ThreadId internal_id_ {0};

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    PandaStack<ThreadState> thread_frame_states_;

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    PandaVector<LockedObjectInfo> local_objects_locked_;

    // Implementation of Wait/Notify
    os::memory::ConditionVariable cond_var_ GUARDED_BY(cond_lock_);
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    mutable os::memory::Mutex cond_lock_;

    bool is_interrupted_ GUARDED_BY(cond_lock_) = false;

    os::memory::ConditionVariable suspend_var_ GUARDED_BY(suspend_lock_);
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    os::memory::Mutex suspend_lock_;
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    uint32_t suspend_count_ GUARDED_BY(suspend_lock_) = 0;
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    std::atomic_uint32_t user_code_suspend_count_ {0};

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    bool is_daemon_ = false;

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    Monitor *waiting_monitor_;

    // Monitor lock is required for multithreaded AddMonitor; RecursiveMutex to allow calling RemoveMonitor
    // in ReleaseMonitors
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    os::memory::RecursiveMutex monitor_lock_;
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    std::unordered_set<Monitor *> entered_monitors_ GUARDED_BY(monitor_lock_);
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    ThreadStatus monitor_old_status_ = FINISHED;

    // Boolean which is safe to access after runtime is destroyed
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    bool is_managed_scope_ {false};

    PandaUniquePtr<mem::ReferenceStorage> pt_reference_storage_ {nullptr};

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    NO_COPY_SEMANTIC(MTManagedThread);
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    NO_MOVE_SEMANTIC(MTManagedThread);
};

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_MTMANAGED_THREAD_H_