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

#ifndef PANDA_LIBPANDABASE_OS_UNIX_MUTEX_H_
#define PANDA_LIBPANDABASE_OS_UNIX_MUTEX_H_

#include "clang.h"
#include "macros.h"

#include <pthread.h>

namespace panda::os::unix::memory {

class ConditionVariable;

class CAPABILITY("mutex") Mutex {
public:
    explicit Mutex(bool is_init = true);

    ~Mutex();

    void Lock() ACQUIRE();

    bool TryLock() TRY_ACQUIRE(true);

    void Unlock() RELEASE();

protected:
    void Init(pthread_mutexattr_t *attrs);

private:
    pthread_mutex_t mutex_;

    NO_COPY_SEMANTIC(Mutex);
    NO_MOVE_SEMANTIC(Mutex);

    friend ConditionVariable;
};

class CAPABILITY("mutex") RecursiveMutex : public Mutex {
public:
    RecursiveMutex();

    ~RecursiveMutex() = default;

    NO_COPY_SEMANTIC(RecursiveMutex);
    NO_MOVE_SEMANTIC(RecursiveMutex);
};

class CAPABILITY("mutex") RWLock {
public:
    RWLock();

    ~RWLock();

    void ReadLock() ACQUIRE_SHARED();

    void WriteLock() ACQUIRE();

    bool TryReadLock() TRY_ACQUIRE_SHARED(true);

    bool TryWriteLock() TRY_ACQUIRE(true);

    void Unlock() RELEASE_GENERIC();

private:
    pthread_rwlock_t rwlock_;

    NO_COPY_SEMANTIC(RWLock);
    NO_MOVE_SEMANTIC(RWLock);
};

class ConditionVariable {
public:
    ConditionVariable();

    ~ConditionVariable();

    void Signal();

    void SignalAll();

    void Wait(Mutex *mutex);

    bool TimedWait(Mutex *mutex, uint64_t ms, uint64_t ns = 0, bool is_absolute = false);

private:
    pthread_cond_t cond_;

    NO_COPY_SEMANTIC(ConditionVariable);
    NO_MOVE_SEMANTIC(ConditionVariable);
};

}  // namespace panda::os::unix::memory

#endif  // PANDA_LIBPANDABASE_OS_UNIX_MUTEX_H_
