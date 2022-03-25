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

#include "runtime/thread_manager.h"

#include "libpandabase/utils/logger.h"
#include "libpandabase/utils/utf.h"
#include "runtime/include/mem/allocator.h"
#include "runtime/include/runtime.h"
#include "runtime/include/panda_vm.h"
#include "runtime/include/thread_scopes.h"
#include "libpandabase/os/native_stack.h"
#include "libpandabase/os/thread.h"

namespace panda {

ThreadManager::ThreadManager(mem::InternalAllocatorPtr allocator) : threads_(allocator->Adapter())
{
    last_id_ = 0;
    pending_threads_ = 0;
}

ThreadManager::~ThreadManager()
{
    DeleteFinishedThreads();
    threads_.clear();
}

uint32_t ThreadManager::GetInternalThreadIdWithLockHeld() REQUIRES(ids_lock_)
{
    for (size_t i = 0; i < internal_thread_ids_.size(); i++) {
        last_id_ = (last_id_ + 1) % internal_thread_ids_.size();
        if (!internal_thread_ids_[last_id_]) {
            internal_thread_ids_.set(last_id_);
            return last_id_ + 1;  // 0 is reserved as uninitialized value.
        }
    }
    LOG(FATAL, RUNTIME) << "Out of internal thread ids";
    UNREACHABLE();
}

uint32_t ThreadManager::GetInternalThreadId()
{
    os::memory::LockHolder lock(ids_lock_);
    return GetInternalThreadIdWithLockHeld();
}

void ThreadManager::RemoveInternalThreadIdWithLockHeld(uint32_t id) REQUIRES(ids_lock_)
{
    id--;  // 0 is reserved as uninitialized value.
    ASSERT(internal_thread_ids_[id]);
    internal_thread_ids_.reset(id);
}

void ThreadManager::RemoveInternalThreadId(uint32_t id)
{
    os::memory::LockHolder lock(ids_lock_);
    return RemoveInternalThreadIdWithLockHeld(id);
}

bool ThreadManager::IsThreadExists(uint32_t thread_id)
{
    os::memory::LockHolder lock(thread_lock_);
    auto i = threads_.begin();
    while (i != threads_.end()) {
        MTManagedThread *thread = *i;
        if (thread->GetId() == thread_id) {
            return true;
        }
        i++;
    }
    return false;
}

uint32_t ThreadManager::GetThreadIdByInternalThreadId(uint32_t thread_id)
{
    os::memory::LockHolder lock(thread_lock_);
    auto i = threads_.begin();
    while (i != threads_.end()) {
        MTManagedThread *thread = *i;
        if (thread->GetInternalId() == thread_id) {
            return thread->GetId();
        }
        i++;
    }
    return 0;
}

MTManagedThread *ThreadManager::GetThreadByInternalThreadIdWithLockHeld(uint32_t thread_id)
{
    auto i = threads_.begin();
    while (i != threads_.end()) {
        MTManagedThread *thread = *i;
        if (thread->GetInternalId() == thread_id) {
            return thread;
        }
        i++;
    }
    return nullptr;
}

void ThreadManager::DeregisterSuspendedThreads()
{
    if (pending_threads_ != 0) {
        // There are threads, which are not completely registered
        // We can not destroy other threads, as they may use shared data (waiting mutexes)
        return;
    }

    auto current = MTManagedThread::GetCurrent();
    auto i = threads_.begin();
    while (i != threads_.end()) {
        MTManagedThread *thread = *i;
        auto status = thread->GetStatus();
        // Do not deregister current thread (which should be in status NATIVE) as HasNoActiveThreads
        // assumes it stays registered; do not deregister CREATED threads until they finish initializing
        // which requires communication with ThreadManaged
        // If thread status is not RUNNING, it's treated as suspended and we can deregister it
        // Ignore state BLOCKED as it means we are trying to acquire lock in Monitor, which was created in
        // internalAllocator
        if (thread != current && CanDeregister(status)) {
            if (thread->IsDaemon()) {
                daemon_threads_count_--;
                daemon_threads_.push_back(thread);
            }
            i = threads_.erase(i);
            // Do not delete this thread structure as it may be used by suspended thread
            threads_count_--;
            continue;
        }
        i++;
    }
}

void ThreadManager::WaitForDeregistration()
{
    trace::ScopedTrace scoped_trace(__FUNCTION__);
    {
        os::memory::LockHolder lock(thread_lock_);

        // First wait for non-daemon threads to finish
        while (true) {
            if (HasNoActiveThreads()) {
                break;
            }
            stop_var_.TimedWait(&thread_lock_, WAIT_INTERVAL);
        }

        // Then stop daemon threads
        StopDaemonThreads();

        // Finally wait until all threads are suspended
        while (true) {
            DeregisterSuspendedThreads();
            // Check for HasNoActiveThreads as new threads might be created by daemons
            if (HasNoActiveThreads() && daemon_threads_count_ == 0) {
                break;
            }
            stop_var_.TimedWait(&thread_lock_, WAIT_INTERVAL);
        }
    }
    for (const auto &thread : daemon_threads_) {
        thread->FreeInternalMemory();
    }
}

void ThreadManager::StopDaemonThreads()
{
    trace::ScopedTrace scoped_trace(__FUNCTION__);
    auto i = threads_.begin();
    while (i != threads_.end()) {
        MTManagedThread *thread = *i;
        if (thread->IsDaemon()) {
            LOG(DEBUG, RUNTIME) << "Stopping daemon thread " << thread->GetId();
            thread->StopDaemonThread();
        }
        i++;
    }
    // Suspend any future new threads
    suspend_new_count_++;
}

int ThreadManager::GetThreadsCount()
{
    return threads_count_;
}

#ifndef NDEBUG
uint32_t ThreadManager::GetAllRegisteredThreadsCount()
{
    return registered_threads_count_;
}
#endif  // NDEBUG

void ThreadManager::SuspendAllThreads()
{
    trace::ScopedTrace scoped_trace("Suspending mutator threads");
    auto cur_thread = MTManagedThread::GetCurrent();
    os::memory::LockHolder lock(thread_lock_);
    EnumerateThreadsWithLockheld(
        [cur_thread](MTManagedThread *thread) {
            if (thread != cur_thread) {
                thread->SuspendImpl(true);
            }
            return true;
        },
        static_cast<unsigned int>(EnumerationFlag::ALL));
    suspend_new_count_++;
}

void ThreadManager::ResumeAllThreads()
{
    trace::ScopedTrace scoped_trace("Resuming mutator threads");
    auto cur_thread = MTManagedThread::GetCurrent();
    os::memory::LockHolder lock(thread_lock_);
    if (suspend_new_count_ > 0) {
        suspend_new_count_--;
    }
    EnumerateThreadsWithLockheld(
        [cur_thread](MTManagedThread *thread) {
            if (thread != cur_thread) {
                thread->ResumeImpl(true);
            }
            return true;
        },
        static_cast<unsigned int>(EnumerationFlag::ALL));
}

bool ThreadManager::UnregisterExitedThread(MTManagedThread *thread)
{
    ASSERT(MTManagedThread::GetCurrent() == thread);
    os::memory::LockHolder lock(thread_lock_);

    LOG(DEBUG, RUNTIME) << "Stopping thread " << thread->GetId();
    thread->UpdateStatus(FINISHED);
    // Do not delete main thread, Runtime::GetMainThread is expected to always return valid object
    if (thread == main_thread_) {
        return false;
    }

    // While this thread is suspended, do not delete it as other thread can be accessing it.
    // TestAllFlags is required because termination request can be sent while thread_lock_ is unlocked
    while (thread->TestAllFlags()) {
        thread_lock_.Unlock();
        thread->SafepointPoll();
        thread_lock_.Lock();
    }
    // This code should happen after thread has been resumed: Both WaitSuspension and ResumeImps requires locking
    // suspend_lock_, so it acts as a memory barrier; flag clean should be visible in this thread after exit from
    // WaitSuspenion
    TSAN_ANNOTATE_HAPPENS_AFTER(&thread->stor_32_.fts_);

    threads_.remove(thread);
    if (thread->IsDaemon()) {
        daemon_threads_count_--;
    }
    threads_count_--;

    // If java_thead, its nativePeer should be 0 before
    delete thread;
    stop_var_.Signal();
    return true;
}

void ThreadManager::RegisterSensitiveThread() const
{
    LOG(INFO, RUNTIME) << __func__ << " is an empty implementation now.";
}

void ThreadManager::DumpUnattachedThreads(std::ostream &os)
{
    os::native_stack::DumpUnattachedThread dump;
    dump.InitKernelTidLists();
    os::memory::LockHolder lock(thread_lock_);
    for (const auto &thread : threads_) {
        dump.AddTid(static_cast<pid_t>(thread->GetId()));
    }
    dump.Dump(os, Runtime::GetCurrent()->IsDumpNativeCrash(), nullptr);
}

MTManagedThread *ThreadManager::SuspendAndWaitThreadByInternalThreadId(uint32_t thread_id)
{
    static constexpr uint32_t YIELD_ITERS = 500;
    // NB! Expected to be called in registered thread, change implementation if this function used elsewhere
    MTManagedThread *current = MTManagedThread::GetCurrent();
    MTManagedThread *suspended = nullptr;
    ASSERT(current->GetStatus() != ThreadStatus::RUNNING);
    for (uint32_t loop_iter = 0;; loop_iter++) {
        if (suspended == nullptr) {
            // If two threads call SuspendAndWaitThreadByInternalThreadId concurrently, one has to get suspended
            // while other waits for thread to be suspended, so thread_lock_ is required to be held until
            // SuspendImpl is called
            ScopedManagedCodeThread sa(current);
            os::memory::LockHolder lock(thread_lock_);
            auto *thread = GetThreadByInternalThreadIdWithLockHeld(thread_id);

            if (thread == nullptr) {
                // no thread found, exit
                return nullptr;
            }

            ASSERT(current != thread);
            if (current->IsSuspended()) {
                // Unsafe to suspend as other thread may be waiting for this thread to suspend;
                // Should get suspended on ScopedManagedCodeThread
                continue;
            }
            thread->SuspendImpl(true);
            suspended = thread;
        } else if (suspended->GetStatus() != ThreadStatus::RUNNING) {
            // Thread is suspended now
            return suspended;
        }
        if (loop_iter < YIELD_ITERS) {
            MTManagedThread::Yield();
        } else {
            static constexpr uint32_t SHORT_SLEEP_MS = 1;
            os::thread::NativeSleep(SHORT_SLEEP_MS);
        }
    }
    return nullptr;
}

}  // namespace panda
