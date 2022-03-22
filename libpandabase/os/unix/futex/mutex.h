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

#ifndef PANDA_LIBPANDABASE_OS_UNIX_FUTEX_MUTEX_H_
#define PANDA_LIBPANDABASE_OS_UNIX_FUTEX_MUTEX_H_

#include "clang.h"
#include "macros.h"
#include "os/thread.h"

#include <atomic>
#include <limits>
#include <iostream>

#include <unistd.h>
#include <linux/futex.h>
#include <sys/syscall.h>

namespace panda::os::unix::memory::futex {

static inline int futex(volatile int *uaddr, int op, int val, const struct timespec *timeout, volatile int *uaddr2,
                        int val3)
{
    // NOLINTNEXTLINE
    return syscall(SYS_futex, uaddr, op, val, timeout, uaddr2, val3);
}

// We need to update TLS current tid after fork
void PostFork();

class ConditionVariable;

static constexpr int WAKE_ONE = 1;
static constexpr int WAKE_ALL = std::numeric_limits<int>::max();

class CAPABILITY("mutex") Mutex {
public:
    Mutex() = default;

    ~Mutex();

    void Lock() ACQUIRE();

    bool TryLock() TRY_ACQUIRE(true);

    bool TryLockWithSpinning() TRY_ACQUIRE(true);

    void Unlock() RELEASE();

    // Should be used only in monitor. Intended to be used with just created mutexes which aren't in use yet
    // Register `thread` as mutex's owner and locks it
    void LockForOther(thread::ThreadId thread);

    // Should be used only in monitor. Intended to be used with just created mutexes which aren't in use yet
    // Unregister `thread` as mutex's owner and unlocks it
    void UnlockForOther(thread::ThreadId thread);

private:
    // Lowest bit: 0 - unlocked, 1 - locked.
    // Other bits: Number of waiters.
    // Unified lock state and waiters count to avoid requirement of double seq_cst memory order on mutex unlock
    // as it's done in RWLock::WriteUnlock
    std::atomic_int32_t state_and_waiters_ {0};

    int *GetStateAddr()
    {
        return reinterpret_cast<int *>(&state_and_waiters_);
    }

    static constexpr int32_t HELD_MASK = 1;
    static constexpr int32_t WAITER_SHIFT = 1;
    // NOLINTNEXTLINE(hicpp-signed-bitwise, cppcoreguidelines-narrowing-conversions, bugprone-narrowing-conversions)
    static constexpr int32_t WAITER_INCREMENT = 1 << WAITER_SHIFT;

    void IncrementWaiters()
    {
        state_and_waiters_.fetch_add(WAITER_INCREMENT, std::memory_order_relaxed);
    }
    void DecrementWaiters()
    {
        state_and_waiters_.fetch_sub(WAITER_INCREMENT, std::memory_order_relaxed);
    }
    int32_t GetWaiters()
    {
        // NOLINTNEXTLINE(hicpp-signed-bitwise)
        return static_cast<uint32_t>(state_and_waiters_.load(std::memory_order_relaxed)) >> WAITER_SHIFT;
    }

    // Exclusive owner.
    alignas(alignof(uint32_t)) std::atomic<thread::ThreadId> exclusive_owner_ {0};
    static_assert(std::atomic<thread::ThreadId>::is_always_lock_free);

    bool IsHeld(thread::ThreadId thread)
    {
        return exclusive_owner_.load(std::memory_order_relaxed) == thread;
    }

    uint32_t recursive_count_ {0};
    alignas(alignof(uint32_t)) const bool recursive_mutex_ {false};

    NO_COPY_SEMANTIC(Mutex);
    NO_MOVE_SEMANTIC(Mutex);

    friend ConditionVariable;

protected:
    explicit Mutex(bool recursive) : recursive_mutex_(recursive) {}
};

class CAPABILITY("mutex") RecursiveMutex : public Mutex {
public:
    RecursiveMutex() : Mutex(true) {}

    ~RecursiveMutex() = default;

private:
    NO_COPY_SEMANTIC(RecursiveMutex);
    NO_MOVE_SEMANTIC(RecursiveMutex);
};

class SHARED_CAPABILITY("mutex") RWLock {
public:
    RWLock() = default;

    ~RWLock();

    // ReadLock and ReadUnlock are used in mutator lock often, which prefers inlining over call to libpandabase
    ALWAYS_INLINE void ReadLock() ACQUIRE_SHARED()
    {
        bool done = false;
        while (!done) {
            auto cur_state = state_.load(std::memory_order_relaxed);
            if (LIKELY(cur_state >= UNLOCKED)) {
                auto new_state = cur_state + READ_INCREMENT;
                done = state_.compare_exchange_weak(cur_state, new_state, std::memory_order_acquire);
            } else {
                HandleReadLockWait(cur_state);
            }
        }
        ASSERT(!HasExclusiveHolder());
    }

    ALWAYS_INLINE void Unlock() RELEASE_GENERIC()
    {
        if (HasExclusiveHolder()) {
            WriteUnlock();
        } else {
            ReadUnlock();
        }
    }

    void WriteLock() ACQUIRE();

    bool TryReadLock() TRY_ACQUIRE_SHARED(true);

    bool TryWriteLock() TRY_ACQUIRE(true);

private:
    ALWAYS_INLINE void ReadUnlock() RELEASE_SHARED()
    {
        ASSERT(!HasExclusiveHolder());
        bool done = false;
        auto cur_state = state_.load(std::memory_order_relaxed);
        while (!done) {
            if (LIKELY(cur_state > 0)) {
                // Reduce state by 1 and do release store.
                // waiters_ load should not be reordered before state_, so it's done with seq cst.
                auto new_state = cur_state - READ_INCREMENT;
                // cur_state should be updated with fetched value on fail
                done = state_.compare_exchange_weak(cur_state, new_state, std::memory_order_seq_cst);
                if (done && new_state == UNLOCKED) {
                    if (waiters_.load(std::memory_order_seq_cst) > 0) {
                        // Wake one exclusive waiter as there are now no readers.
                        // NOLINTNEXTLINE(hicpp-signed-bitwise)
                        futex(GetStateAddr(), FUTEX_WAKE_PRIVATE, WAKE_ALL, nullptr, nullptr, 0);
                    }
                }
            } else {
                // Cannot use logger in header
                std::cout << "RWLock ReadUnlock got unexpected state, RWLock is unlocked?" << std::endl;
                std::abort();
            }
        }
    }

    void WriteUnlock() RELEASE();

    // Non-inline path for handling waiting.
    void HandleReadLockWait(int32_t cur_state);

    static constexpr int32_t WRITE_LOCKED = -1;
    static constexpr int32_t UNLOCKED = 0;
    static constexpr int32_t READ_INCREMENT = 1;
    // -1 - write locked; 0 - unlocked; > 0 - read locked by state_ owners.
    std::atomic_int32_t state_ {0};

    int *GetStateAddr()
    {
        return reinterpret_cast<int *>(&state_);
    }

    // Exclusive owner.
    alignas(alignof(uint32_t)) std::atomic<thread::ThreadId> exclusive_owner_ {0};
    static_assert(std::atomic<thread::ThreadId>::is_always_lock_free);

    bool HasExclusiveHolder()
    {
        return exclusive_owner_.load(std::memory_order_relaxed) != 0;
    }
    bool IsExclusiveHeld(thread::ThreadId thread)
    {
        return exclusive_owner_.load(std::memory_order_relaxed) == thread;
    }

    // Number of waiters both for read and write locks.
    std::atomic_uint32_t waiters_ {0};

    void IncrementWaiters()
    {
        waiters_.fetch_add(1, std::memory_order_relaxed);
    }
    void DecrementWaiters()
    {
        waiters_.fetch_sub(1, std::memory_order_relaxed);
    }

    // Extra padding to make RWLock 16 bytes long
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    static constexpr size_t PADDING_SIZE = 1;
    std::array<uint32_t, PADDING_SIZE> padding_ = {0};
    // [[maybe_unused]] causes issues, dummy accessor for `padding_` as workaround
    uint32_t dummy_access_padding()
    {
        return padding_[0];
    }

    NO_COPY_SEMANTIC(RWLock);
    NO_MOVE_SEMANTIC(RWLock);
};

class ConditionVariable {
public:
    ConditionVariable() = default;

    ~ConditionVariable();

    void Signal()
    {
        SignalCount(WAKE_ONE);
    }

    void SignalAll()
    {
        SignalCount(WAKE_ALL);
    }

    void Wait(Mutex *mutex) NO_THREAD_SAFETY_ANALYSIS;

    bool TimedWait(Mutex *mutex, uint64_t ms, uint64_t ns = 0, bool is_absolute = false) NO_THREAD_SAFETY_ANALYSIS;

private:
    alignas(alignof(uint64_t)) std::atomic<Mutex *> mutex_ptr_ {nullptr};
    std::atomic_int32_t cond_ {0};
    std::atomic_int32_t waiters_ {0};
    static_assert(std::atomic<Mutex *>::is_always_lock_free);

    void SignalCount(int32_t to_wake);

    int *GetCondAddr()
    {
        return reinterpret_cast<int *>(&cond_);
    }

    NO_COPY_SEMANTIC(ConditionVariable);
    NO_MOVE_SEMANTIC(ConditionVariable);
};

static constexpr size_t ALL_STRUCTURES_SIZE = 16U;
static_assert(sizeof(ConditionVariable) == ALL_STRUCTURES_SIZE);
static_assert(sizeof(Mutex) == ALL_STRUCTURES_SIZE);
static_assert(sizeof(RWLock) == ALL_STRUCTURES_SIZE);

}  // namespace panda::os::unix::memory::futex

#endif  // PANDA_LIBPANDABASE_OS_UNIX_FUTEX_MUTEX_H_
