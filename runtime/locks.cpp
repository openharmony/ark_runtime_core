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

#include "runtime/include/locks.h"
#include "libpandabase/utils/logger.h"

#include <memory>

namespace panda {

static bool is_initialized = false;

MutatorLock *Locks::mutator_lock = nullptr;
os::memory::Mutex *Locks::custom_tls_lock = nullptr;
os::memory::Mutex *Locks::user_suspension_lock = nullptr;

void Locks::Initialize()
{
    if (!is_initialized) {
        // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
        Locks::mutator_lock = new MutatorLock();
        // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
        Locks::custom_tls_lock = new os::memory::Mutex();
        // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
        Locks::user_suspension_lock = new os::memory::Mutex();
        is_initialized = true;
    }
}

#ifndef NDEBUG
thread_local MutatorLock::MutatorLockState lock_state = MutatorLock::UNLOCKED;

void MutatorLock::ReadLock()
{
    ASSERT(!HasLock());
    os::memory::RWLock::ReadLock();
    LOG(DEBUG, RUNTIME) << "MutatorLock::ReadLock";
    lock_state = RDLOCK;
}

void MutatorLock::WriteLock()
{
    ASSERT(!HasLock());
    os::memory::RWLock::WriteLock();
    LOG(DEBUG, RUNTIME) << "MutatorLock::WriteLock";
    lock_state = WRLOCK;
}

bool MutatorLock::TryReadLock()
{
    bool ret = os::memory::RWLock::TryReadLock();
    LOG(DEBUG, RUNTIME) << "MutatorLock::TryReadLock";
    if (ret) {
        lock_state = RDLOCK;
    }
    return ret;
}

bool MutatorLock::TryWriteLock()
{
    bool ret = os::memory::RWLock::TryWriteLock();
    LOG(DEBUG, RUNTIME) << "MutatorLock::TryWriteLock";
    if (ret) {
        lock_state = WRLOCK;
    }
    return ret;
}

void MutatorLock::Unlock()
{
    ASSERT(HasLock());
    os::memory::RWLock::Unlock();
    LOG(DEBUG, RUNTIME) << "MutatorLock::Unlock";
    lock_state = UNLOCKED;
}

MutatorLock::MutatorLockState MutatorLock::GetState()
{
    return lock_state;
}

bool MutatorLock::HasLock()
{
    return lock_state == RDLOCK || lock_state == WRLOCK;
}
#endif  // !NDEBUG

}  // namespace panda
