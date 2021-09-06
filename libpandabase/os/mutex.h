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

#ifndef PANDA_LIBPANDABASE_OS_MUTEX_H_
#define PANDA_LIBPANDABASE_OS_MUTEX_H_

#if defined(PANDA_USE_FUTEX)
#include "os/unix/futex/mutex.h"
#elif defined(PANDA_TARGET_UNIX) || defined(PANDA_TARGET_WINDOWS)
#include "os/unix/mutex.h"
#else
#error "Unsupported platform"
#endif

#include "clang.h"
#include "macros.h"

namespace panda::os::memory {

// Dummy lock which locks nothing
// but has the same methods as RWLock and Mutex.
// Can be used in Locks Holders.
class DummyLock {
public:
    void Lock() {}
    void Unlock() {}
    void ReadLock() {}
    void WriteLock() {}
};

#if defined(PANDA_USE_FUTEX)
using Mutex = panda::os::unix::memory::futex::Mutex;
using RecursiveMutex = panda::os::unix::memory::futex::RecursiveMutex;
using RWLock = panda::os::unix::memory::futex::RWLock;
using ConditionVariable = panda::os::unix::memory::futex::ConditionVariable;
#else
using Mutex = panda::os::unix::memory::Mutex;
using RecursiveMutex = panda::os::unix::memory::RecursiveMutex;
using RWLock = panda::os::unix::memory::RWLock;
// Some RTOS could not have support for condition variables, so this primitive should be used carefully
using ConditionVariable = panda::os::unix::memory::ConditionVariable;
#endif

using PandaThreadKey = pthread_key_t;
const auto PandaGetspecific = pthread_getspecific;     // NOLINT(readability-identifier-naming)
const auto PandaSetspecific = pthread_setspecific;     // NOLINT(readability-identifier-naming)
const auto PandaThreadKeyCreate = pthread_key_create;  // NOLINT(readability-identifier-naming)

template <class T>
class SCOPED_CAPABILITY LockHolder {
public:
    explicit LockHolder(T &lock) ACQUIRE(lock) : lock_(lock)
    {
        lock_.Lock();
    }

    ~LockHolder() RELEASE()
    {
        lock_.Unlock();
    }

private:
    T &lock_;

    NO_COPY_SEMANTIC(LockHolder);
    NO_MOVE_SEMANTIC(LockHolder);
};

template <class T>
class SCOPED_CAPABILITY ReadLockHolder {
public:
    explicit ReadLockHolder(T &lock) ACQUIRE_SHARED(lock) : lock_(lock)
    {
        lock_.ReadLock();
    }

    ~ReadLockHolder() RELEASE()
    {
        lock_.Unlock();
    }

private:
    T &lock_;

    NO_COPY_SEMANTIC(ReadLockHolder);
    NO_MOVE_SEMANTIC(ReadLockHolder);
};

template <class T>
class SCOPED_CAPABILITY WriteLockHolder {
public:
    explicit WriteLockHolder(T &lock) ACQUIRE(lock) : lock_(lock)
    {
        lock_.WriteLock();
    }

    ~WriteLockHolder() RELEASE()
    {
        lock_.Unlock();
    }

private:
    T &lock_;

    NO_COPY_SEMANTIC(WriteLockHolder);
    NO_MOVE_SEMANTIC(WriteLockHolder);
};

}  // namespace panda::os::memory

#endif  // PANDA_LIBPANDABASE_OS_MUTEX_H_
