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

#include "mutex.h"
#include "utils/logger.h"
#include "utils/type_helpers.h"

#include <cstring>
#include <cerrno>
#include <ctime>

#include <sched.h>

namespace panda::os::unix::memory::futex {

// Avoid repeatedly calling GetCurrentThreadId by storing tid locally
thread_local thread::ThreadId current_tid {0};

void PostFork()
{
    current_tid = os::thread::GetCurrentThreadId();
}

// Spin for small arguments and yield for longer ones.
static void BackOff(uint32_t i)
{
    static constexpr uint32_t SPIN_MAX = 10;
    if (i <= SPIN_MAX) {
        volatile uint32_t x = 0;  // Volatile to make sure loop is not optimized out.
        const uint32_t spin_count = 10 * i;
        for (uint32_t spin = 0; spin < spin_count; spin++) {
            ++x;
        }
    } else {
        thread::Yield();
    }
}

// Wait until pred is true, or until timeout is reached.
// Return true if the predicate test succeeded, false if we timed out.
template <typename Pred>
static inline bool WaitBrieflyFor(std::atomic_int *addr, Pred pred)
{
    // We probably don't want to do syscall (switch context) when we use WaitBrieflyFor
    static constexpr uint32_t MAX_BACK_OFF = 10;
    static constexpr uint32_t MAX_ITER = 50;
    for (uint32_t i = 1; i <= MAX_ITER; i++) {
        BackOff(std::min(i, MAX_BACK_OFF));
        if (pred(addr->load(std::memory_order_relaxed))) {
            return true;
        }
    }
    return false;
}

Mutex::~Mutex()
{
    if (state_and_waiters_.load(std::memory_order_relaxed) != 0) {
        LOG(FATAL, COMMON) << "Mutex destruction failed; state_and_waiters_ is non zero!";
    } else if (exclusive_owner_.load(std::memory_order_relaxed) != 0) {
        LOG(FATAL, COMMON) << "Mutex destruction failed; mutex has an owner!";
    }
}

void Mutex::Lock()
{
    if (current_tid == 0) {
        current_tid = os::thread::GetCurrentThreadId();
    }
    if (recursive_mutex_) {
        if (IsHeld(current_tid)) {
            recursive_count_++;
            return;
        }
    }

    ASSERT(!IsHeld(current_tid));
    bool done = false;
    while (!done) {
        auto cur_state = state_and_waiters_.load(std::memory_order_relaxed);
        if (LIKELY((helpers::ToUnsigned(cur_state) & helpers::ToUnsigned(HELD_MASK)) == 0)) {
            // Lock not held, try acquiring it.
            auto new_state = static_cast<int32_t>(helpers::ToUnsigned(cur_state) | helpers::ToUnsigned(HELD_MASK));
            done = state_and_waiters_.compare_exchange_weak(cur_state, new_state, std::memory_order_acquire);
        } else {
            // Failed to acquire, wait for unlock
            auto res = WaitBrieflyFor(&state_and_waiters_, [](int32_t state) {
                return (helpers::ToUnsigned(state) & helpers::ToUnsigned(HELD_MASK)) == 0;
            });
            if (!res) {
                // WaitBrieflyFor failed, go to futex wait
                // Increment waiters count.
                IncrementWaiters();
                // Update cur_state to be equal to current expected state_and_waiters_.
                cur_state += WAITER_INCREMENT;
                // Retry wait until lock is not held. In heavy contention situations cur_state check can fail
                // immediately due to repeatedly decrementing and incrementing waiters.
                // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_NESTING_LEVEL)
                while ((helpers::ToUnsigned(cur_state) & helpers::ToUnsigned(HELD_MASK)) != 0) {
                    // NOLINTNEXTLINE(hicpp-signed-bitwise), CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_NESTING_LEVEL)
                    if (futex(GetStateAddr(), FUTEX_WAIT_PRIVATE, cur_state, nullptr, nullptr, 0) != 0) {
                        // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_NESTING_LEVEL)
                        if ((errno != EAGAIN) && (errno != EINTR)) {
                            LOG(FATAL, COMMON) << "Futex wait failed!";
                        }
                    }
                    cur_state = state_and_waiters_.load(std::memory_order_relaxed);
                }
                DecrementWaiters();
            }
        }
    }
    // Mutex is held now
    ASSERT((helpers::ToUnsigned(state_and_waiters_.load(std::memory_order_relaxed)) & helpers::ToUnsigned(HELD_MASK)) !=
           0);
    ASSERT(exclusive_owner_.load(std::memory_order_relaxed) == 0);
    exclusive_owner_.store(current_tid, std::memory_order_relaxed);
    recursive_count_++;
    ASSERT(recursive_count_ == 1);  // should be 1 here, there's a separate path for recursive mutex above
}

bool Mutex::TryLock()
{
    if (current_tid == 0) {
        current_tid = os::thread::GetCurrentThreadId();
    }
    if (recursive_mutex_) {
        if (IsHeld(current_tid)) {
            recursive_count_++;
            return true;
        }
    }

    ASSERT(!IsHeld(current_tid));
    bool done = false;
    auto cur_state = state_and_waiters_.load(std::memory_order_relaxed);
    while (!done) {
        if (LIKELY((helpers::ToUnsigned(cur_state) & helpers::ToUnsigned(HELD_MASK)) == 0)) {
            // Lock not held, retry acquiring it until it's held.
            auto new_state = static_cast<int32_t>(helpers::ToUnsigned(cur_state) | helpers::ToUnsigned(HELD_MASK));
            // cur_state should be updated with fetched value on fail
            done = state_and_waiters_.compare_exchange_weak(cur_state, new_state, std::memory_order_acquire);
        } else {
            // Lock is held by someone, exit
            return false;
        }
    }
    // Mutex is held now
    ASSERT((helpers::ToUnsigned(state_and_waiters_.load(std::memory_order_relaxed)) & helpers::ToUnsigned(HELD_MASK)) !=
           0);
    ASSERT(exclusive_owner_.load(std::memory_order_relaxed) == 0);
    exclusive_owner_.store(current_tid, std::memory_order_relaxed);
    recursive_count_++;
    ASSERT(recursive_count_ == 1);  // should be 1 here, there's a separate path for recursive mutex above
    return true;
}

bool Mutex::TryLockWithSpinning()
{
    const int MAX_ITER = 10;
    for (int i = 0; i < MAX_ITER; i++) {
        if (TryLock()) {
            return true;
        }
        auto res = WaitBrieflyFor(&state_and_waiters_, [](int32_t state) {
            return (helpers::ToUnsigned(state) & helpers::ToUnsigned(HELD_MASK)) == 0;
        });
        if (!res) {
            // WaitBrieflyFor failed, means lock is held
            return false;
        }
    }
    return false;
}

void Mutex::Unlock()
{
    if (current_tid == 0) {
        current_tid = os::thread::GetCurrentThreadId();
    }
    if (!IsHeld(current_tid)) {
        LOG(FATAL, COMMON) << "Trying to unlock mutex which is not held by current thread";
    }
    recursive_count_--;
    if (recursive_mutex_) {
        if (recursive_count_ > 0) {
            return;
        }
    }

    ASSERT(recursive_count_ == 0);  // should be 0 here, there's a separate path for recursive mutex above
    bool done = false;
    auto cur_state = state_and_waiters_.load(std::memory_order_relaxed);
    // Retry CAS until success
    while (!done) {
        auto new_state = helpers::ToUnsigned(cur_state) & ~helpers::ToUnsigned(HELD_MASK);  // State without holding bit
        if ((helpers::ToUnsigned(cur_state) & helpers::ToUnsigned(HELD_MASK)) == 0) {
            LOG(FATAL, COMMON) << "Mutex unlock got unexpected state, maybe mutex is unlocked?";
        }
        // Reset exclusive owner before changing state to avoid check failures if other thread sees UNLOCKED
        exclusive_owner_.store(0, std::memory_order_relaxed);
        // cur_state should be updated with fetched value on fail
        done = state_and_waiters_.compare_exchange_weak(cur_state, new_state, std::memory_order_release);
        if (LIKELY(done)) {
            // If we had waiters, we need to do futex call
            if (UNLIKELY(new_state != 0)) {
                // NOLINTNEXTLINE(hicpp-signed-bitwise)
                futex(GetStateAddr(), FUTEX_WAKE_PRIVATE, WAKE_ONE, nullptr, nullptr, 0);
            }
        }
    }
}

void Mutex::LockForOther(thread::ThreadId thread)
{
    ASSERT(state_and_waiters_.load() == 0);
    state_and_waiters_.store(HELD_MASK, std::memory_order_relaxed);
    recursive_count_ = 1;
    exclusive_owner_.store(thread, std::memory_order_relaxed);
}

void Mutex::UnlockForOther(thread::ThreadId thread)
{
    if (!IsHeld(thread)) {
        LOG(FATAL, COMMON) << "Unlocking for thread which doesn't own this mutex";
    }
    ASSERT(state_and_waiters_.load() == HELD_MASK);
    state_and_waiters_.store(0, std::memory_order_relaxed);
    recursive_count_ = 0;
    exclusive_owner_.store(0, std::memory_order_relaxed);
}

RWLock::~RWLock()
{
    if (state_.load(std::memory_order_relaxed) != 0) {
        LOG(FATAL, COMMON) << "RWLock destruction failed; state_ is non zero!";
    } else if (exclusive_owner_.load(std::memory_order_relaxed) != 0) {
        LOG(FATAL, COMMON) << "RWLock destruction failed; RWLock has an owner!";
    } else if (waiters_.load(std::memory_order_relaxed) != 0) {
        LOG(FATAL, COMMON) << "RWLock destruction failed; RWLock has waiters!";
    }
}

void RWLock::WriteLock()
{
    if (current_tid == 0) {
        current_tid = os::thread::GetCurrentThreadId();
    }
    bool done = false;
    while (!done) {
        auto cur_state = state_.load(std::memory_order_relaxed);
        if (LIKELY(cur_state == UNLOCKED)) {
            // Unlocked, can acquire writelock
            // Do CAS in case other thread beats us and acquires readlock first
            done = state_.compare_exchange_weak(cur_state, WRITE_LOCKED, std::memory_order_acquire);
        } else {
            // Wait until RWLock is unlocked
            if (!WaitBrieflyFor(&state_, [](int32_t state) { return state == UNLOCKED; })) {
                // WaitBrieflyFor failed, go to futex wait
                // Increment waiters count.
                IncrementWaiters();
                // Retry wait until lock not held. If we have more than one reader, cur_state check failure
                // doesn't mean this lock is unlocked.
                // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_NESTING_LEVEL)
                while (cur_state != UNLOCKED) {
                    // NOLINTNEXTLINE(hicpp-signed-bitwise), CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_NESTING_LEVEL)
                    if (futex(GetStateAddr(), FUTEX_WAIT_PRIVATE, cur_state, nullptr, nullptr, 0) != 0) {
                        // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_NESTING_LEVEL)
                        if ((errno != EAGAIN) && (errno != EINTR)) {
                            LOG(FATAL, COMMON) << "Futex wait failed!";
                        }
                    }
                    cur_state = state_.load(std::memory_order_relaxed);
                }
                DecrementWaiters();
            }
        }
    }
    // RWLock is held now
    ASSERT(state_.load(std::memory_order_relaxed) == WRITE_LOCKED);
    ASSERT(exclusive_owner_.load(std::memory_order_relaxed) == 0);
    exclusive_owner_.store(current_tid, std::memory_order_relaxed);
}

void RWLock::HandleReadLockWait(int32_t cur_state)
{
    // Wait until RWLock WriteLock is unlocked
    if (!WaitBrieflyFor(&state_, [](int32_t state) { return state >= UNLOCKED; })) {
        // WaitBrieflyFor failed, go to futex wait
        IncrementWaiters();
        // Retry wait until WriteLock is not held
        while (cur_state == WRITE_LOCKED) {
            // NOLINTNEXTLINE(hicpp-signed-bitwise), CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_NESTING_LEVEL)
            if (futex(GetStateAddr(), FUTEX_WAIT_PRIVATE, cur_state, nullptr, nullptr, 0) != 0) {
                // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_NESTING_LEVEL)
                if ((errno != EAGAIN) && (errno != EINTR)) {
                    LOG(FATAL, COMMON) << "Futex wait failed!";
                }
            }
            cur_state = state_.load(std::memory_order_relaxed);
        }
        DecrementWaiters();
    }
}

bool RWLock::TryReadLock()
{
    bool done = false;
    auto cur_state = state_.load(std::memory_order_relaxed);
    while (!done) {
        if (cur_state >= UNLOCKED) {
            auto new_state = cur_state + READ_INCREMENT;
            // cur_state should be updated with fetched value on fail
            done = state_.compare_exchange_weak(cur_state, new_state, std::memory_order_acquire);
        } else {
            // RWLock is Write held, trylock failed.
            return false;
        }
    }
    ASSERT(!HasExclusiveHolder());
    return true;
}

bool RWLock::TryWriteLock()
{
    if (current_tid == 0) {
        current_tid = os::thread::GetCurrentThreadId();
    }
    bool done = false;
    auto cur_state = state_.load(std::memory_order_relaxed);
    while (!done) {
        if (LIKELY(cur_state == UNLOCKED)) {
            // Unlocked, can acquire writelock
            // Do CAS in case other thread beats us and acquires readlock first
            // cur_state should be updated with fetched value on fail
            done = state_.compare_exchange_weak(cur_state, WRITE_LOCKED, std::memory_order_acquire);
        } else {
            // RWLock is held, trylock failed.
            return false;
        }
    }
    // RWLock is held now
    ASSERT(state_.load(std::memory_order_relaxed) == WRITE_LOCKED);
    ASSERT(exclusive_owner_.load(std::memory_order_relaxed) == 0);
    exclusive_owner_.store(current_tid, std::memory_order_relaxed);
    return true;
}

void RWLock::WriteUnlock()
{
    if (current_tid == 0) {
        current_tid = os::thread::GetCurrentThreadId();
    }
    ASSERT(IsExclusiveHeld(current_tid));

    bool done = false;
    int32_t cur_state = state_.load(std::memory_order_relaxed);
    // CAS is weak and might fail, do in loop
    while (!done) {
        if (LIKELY(cur_state == WRITE_LOCKED)) {
            // Reset exclusive owner before changing state to avoid check failures if other thread sees UNLOCKED
            exclusive_owner_.store(0, std::memory_order_relaxed);
            // Change state to unlocked and do release store.
            // waiters_ load should not be reordered before state_, so it's done with seq cst.
            // cur_state should be updated with fetched value on fail
            done = state_.compare_exchange_weak(cur_state, UNLOCKED, std::memory_order_seq_cst);
            if (LIKELY(done)) {
                // We are doing write unlock. All waiters could be ReadLocks so we need to wake all.
                // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_NESTING_LEVEL)
                if (waiters_.load(std::memory_order_seq_cst) > 0) {
                    // NOLINTNEXTLINE(hicpp-signed-bitwise)
                    futex(GetStateAddr(), FUTEX_WAKE_PRIVATE, WAKE_ALL, nullptr, nullptr, 0);
                }
            }
        } else {
            LOG(FATAL, COMMON) << "RWLock WriteUnlock got unexpected state, maybe RWLock is not writelocked?";
        }
    }
}

ConditionVariable::~ConditionVariable()
{
    if (waiters_.load(std::memory_order_relaxed) != 0) {
        LOG(FATAL, COMMON) << "CondVar destruction failed; waiters_ is non zero!";
    }
}

void ConditionVariable::Wait(Mutex *mutex)
{
    if (current_tid == 0) {
        current_tid = os::thread::GetCurrentThreadId();
    }
    if (!mutex->IsHeld(current_tid)) {
        LOG(FATAL, COMMON) << "CondVar Wait failed; provided mutex is not held by current thread";
    }

    // It's undefined behavior to call Wait with different mutexes on the same condvar
    Mutex *old_mutex = nullptr;
    while (!mutex_ptr_.compare_exchange_weak(old_mutex, mutex, std::memory_order_relaxed)) {
        // CAS failed, either it was spurious fail and old val is nullptr, or make sure mutex ptr equals to current
        if (old_mutex != mutex && old_mutex != nullptr) {
            LOG(FATAL, COMMON) << "CondVar Wait failed; mutex_ptr_ doesn't equal to provided mutex";
        }
    }

    waiters_.fetch_add(1, std::memory_order_relaxed);
    mutex->IncrementWaiters();
    auto old_count = mutex->recursive_count_;
    mutex->recursive_count_ = 1;
    auto cur_cond = cond_.load(std::memory_order_relaxed);
    mutex->Unlock();
    // NOLINTNEXTLINE(hicpp-signed-bitwise)
    if (futex(GetCondAddr(), FUTEX_WAIT_PRIVATE, cur_cond, nullptr, nullptr, 0) != 0) {
        if ((errno != EAGAIN) && (errno != EINTR)) {
            LOG(FATAL, COMMON) << "Futex wait failed!";
        }
    }
    mutex->Lock();
    mutex->recursive_count_ = old_count;
    mutex->DecrementWaiters();
    waiters_.fetch_sub(1, std::memory_order_relaxed);
}

const int64_t MILLISECONDS_PER_SEC = 1000;
const int64_t NANOSECONDS_PER_MILLISEC = 1000000;
const int64_t NANOSECONDS_PER_SEC = 1000000000;

struct timespec ConvertTime(uint64_t ms, uint64_t ns)
{
    struct timespec time = {0, 0};
    auto seconds = static_cast<time_t>(ms / MILLISECONDS_PER_SEC);
    auto nanoseconds = static_cast<time_t>((ms % MILLISECONDS_PER_SEC) * NANOSECONDS_PER_MILLISEC + ns);
    time.tv_sec += seconds;
    time.tv_nsec += nanoseconds;
    if (time.tv_nsec >= NANOSECONDS_PER_SEC) {
        time.tv_nsec -= NANOSECONDS_PER_SEC;
        time.tv_sec++;
    }
    return time;
}

bool ConditionVariable::TimedWait(Mutex *mutex, uint64_t ms, uint64_t ns, bool is_absolute)
{
    if (current_tid == 0) {
        current_tid = os::thread::GetCurrentThreadId();
    }
    if (!mutex->IsHeld(current_tid)) {
        LOG(FATAL, COMMON) << "CondVar Wait failed; provided mutex is not held by current thread";
    }

    // It's undefined behavior to call Wait with different mutexes on the same condvar
    Mutex *old_mutex = nullptr;
    while (!mutex_ptr_.compare_exchange_weak(old_mutex, mutex, std::memory_order_relaxed)) {
        // CAS failed, either it was spurious fail and old val is nullptr, or make sure mutex ptr equals to current
        if (old_mutex != mutex && old_mutex != nullptr) {
            LOG(FATAL, COMMON) << "CondVar Wait failed; mutex_ptr_ doesn't equal to provided mutex";
        }
    }

    bool timeout = false;
    struct timespec time = ConvertTime(ms, ns);
    waiters_.fetch_add(1, std::memory_order_relaxed);
    mutex->IncrementWaiters();
    auto old_count = mutex->recursive_count_;
    mutex->recursive_count_ = 1;
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_REDUNDANT_INIT)
    auto cur_cond = cond_.load(std::memory_order_relaxed);
    mutex->Unlock();

    int futex_call_res;
    if (is_absolute) {
        // FUTEX_WAIT_BITSET uses absolute time
        // NOLINTNEXTLINE(hicpp-signed-bitwise)
        static constexpr int WAIT_BITSET = FUTEX_WAIT_BITSET_PRIVATE;
        // NOLINTNEXTLINE(hicpp-signed-bitwise)
        static constexpr int MATCH_ANY = FUTEX_BITSET_MATCH_ANY;
        // NOLINTNEXTLINE(hicpp-signed-bitwise)
        futex_call_res = futex(GetCondAddr(), WAIT_BITSET, cur_cond, &time, nullptr, MATCH_ANY);
    } else {
        // FUTEX_WAIT uses relative time
        // NOLINTNEXTLINE(hicpp-signed-bitwise)
        futex_call_res = futex(GetCondAddr(), FUTEX_WAIT_PRIVATE, cur_cond, &time, nullptr, 0);
    }
    if (futex_call_res != 0) {
        if (errno == ETIMEDOUT) {
            timeout = true;
        } else if ((errno != EAGAIN) && (errno != EINTR)) {
            LOG(FATAL, COMMON) << "Futex wait failed!";
        }
    }
    mutex->Lock();
    mutex->recursive_count_ = old_count;
    mutex->DecrementWaiters();
    waiters_.fetch_sub(1, std::memory_order_relaxed);
    return timeout;
}

void ConditionVariable::SignalCount(int32_t to_wake)
{
    if (waiters_.load(std::memory_order_relaxed) == 0) {
        // No waiters, do nothing
        return;
    }

    if (current_tid == 0) {
        current_tid = os::thread::GetCurrentThreadId();
    }
    auto mutex = mutex_ptr_.load(std::memory_order_relaxed);
    // If this condvar has waiters, mutex_ptr_ should be set
    ASSERT(mutex != nullptr);
    cond_.fetch_add(1, std::memory_order_relaxed);
    if (mutex->IsHeld(current_tid)) {
        // This thread is owner of current mutex, do requeue to mutex waitqueue
        // NOLINTNEXTLINE(hicpp-signed-bitwise)
        bool success = futex(GetCondAddr(), FUTEX_REQUEUE_PRIVATE, 0, reinterpret_cast<const timespec *>(to_wake),
                             mutex->GetStateAddr(), 0) != -1;
        if (!success) {
            LOG(FATAL, COMMON) << "Futex requeue failed!";
        }
    } else {
        // Mutex is not held by this thread, do wake
        // NOLINTNEXTLINE(hicpp-signed-bitwise)
        futex(GetCondAddr(), FUTEX_WAKE_PRIVATE, to_wake, nullptr, nullptr, 0);
    }
}

}  // namespace panda::os::unix::memory::futex
