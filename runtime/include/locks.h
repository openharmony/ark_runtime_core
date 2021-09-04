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

#ifndef PANDA_RUNTIME_INCLUDE_LOCKS_H_
#define PANDA_RUNTIME_INCLUDE_LOCKS_H_

#include "libpandabase/os/mutex.h"

#include <memory>

namespace panda {

class MutatorLock : public os::memory::RWLock {
#ifndef NDEBUG
public:
    enum MutatorLockState { UNLOCKED, RDLOCK, WRLOCK };

    void ReadLock() ACQUIRE_SHARED();

    void WriteLock() ACQUIRE();

    bool TryReadLock() TRY_ACQUIRE_SHARED(true);

    bool TryWriteLock() TRY_ACQUIRE(true);

    void Unlock() RELEASE_GENERIC();

    MutatorLockState GetState();

    bool HasLock();
#endif  // !NDEBUG
};

class Locks {
public:
    static void Initialize();

    /**
     * Lock used for preventing object heap modifications (for example at GC<->JIT,ManagedCode interaction during STW)
     */
    static MutatorLock *mutator_lock;  // NOLINT(misc-non-private-member-variables-in-classes)
    /**
     * Lock used for preventing custom_tls_cache_ modifications
     */
    static os::memory::Mutex *custom_tls_lock;  // NOLINT(misc-non-private-member-variables-in-classes)

    /**
     * The lock is a specific lock for exclusive suspension process,
     * It is static for access from JVMTI interface
     */
    static os::memory::Mutex *user_suspension_lock;
};

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_LOCKS_H_
