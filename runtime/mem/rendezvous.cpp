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

#include "runtime/mem/rendezvous.h"

#include "runtime/include/runtime.h"
#include "runtime/include/thread.h"
#include "runtime/include/panda_vm.h"
#include "runtime/thread_manager.h"

namespace panda {

void Rendezvous::SafepointBegin()
{
    ASSERT(!Locks::mutator_lock->HasLock());
    LOG(DEBUG, GC) << "Rendezvous: SafepointBegin";
    // Suspend all threads
    Thread::GetCurrent()->GetVM()->GetThreadManager()->SuspendAllThreads();
    // Acquire write MutatorLock
    Locks::mutator_lock->WriteLock();
}

void Rendezvous::SafepointEnd()
{
    ASSERT(Locks::mutator_lock->HasLock());
    LOG(DEBUG, GC) << "Rendezvous: SafepointEnd";
    // Release write MutatorLock
    Locks::mutator_lock->Unlock();
    // Resume all threads
    Thread::GetCurrent()->GetVM()->GetThreadManager()->ResumeAllThreads();
    LOG(DEBUG, GC) << "Rendezvous: SafepointEnd exit";
}

ScopedSuspendAllThreadsRunning::ScopedSuspendAllThreadsRunning(Rendezvous *rendezvous) : rendezvous_(rendezvous)
{
    ASSERT(rendezvous_ != nullptr);
    ASSERT(Locks::mutator_lock->HasLock());
    Locks::mutator_lock->Unlock();
    rendezvous_->SafepointBegin();
}

ScopedSuspendAllThreadsRunning::~ScopedSuspendAllThreadsRunning()
{
    rendezvous_->SafepointEnd();
    Locks::mutator_lock->ReadLock();
}

}  // namespace panda
