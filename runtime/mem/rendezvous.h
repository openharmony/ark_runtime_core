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

#ifndef PANDA_RUNTIME_MEM_RENDEZVOUS_H_
#define PANDA_RUNTIME_MEM_RENDEZVOUS_H_

#include <atomic>

#include "libpandabase/os/mutex.h"
#include "macros.h"
#include "runtime/include/thread.h"
#include "include/locks.h"

namespace panda {

/** Meeting point for all java threads.
 * High level plan:
 *      * Check if there's already a main thread (running_)
 *      * If there is, wait until we get woken up
 *      * Otherwise, acquire write global Mutator lock and set field running_
 */
class Rendezvous {
public:
    explicit Rendezvous() = default;
    virtual ~Rendezvous() = default;

    // Wait until all threads release Mutator lock and acquires it for write;
    virtual void SafepointBegin() ACQUIRE(*Locks::mutator_lock);
    // Ends safepoint (wakes up waiting threads, releases Mutator lock);
    virtual void SafepointEnd() RELEASE(*Locks::mutator_lock);

private:
    NO_MOVE_SEMANTIC(Rendezvous);
    NO_COPY_SEMANTIC(Rendezvous);
};

class EmptyRendezvous final : public Rendezvous {
public:
    explicit EmptyRendezvous() = default;
    ~EmptyRendezvous() override = default;

    void SafepointBegin() override {}
    void SafepointEnd() override {}

private:
    NO_MOVE_SEMANTIC(EmptyRendezvous);
    NO_COPY_SEMANTIC(EmptyRendezvous);
};

class PANDA_PUBLIC_API ScopedSuspendAllThreads {
public:
    explicit ScopedSuspendAllThreads(Rendezvous *rendezvous) ACQUIRE(*Locks::mutator_lock);
    ~ScopedSuspendAllThreads() RELEASE(*Locks::mutator_lock);

    NO_COPY_SEMANTIC(ScopedSuspendAllThreads);
    NO_MOVE_SEMANTIC(ScopedSuspendAllThreads);

private:
    Rendezvous *rendezvous_;
};

class ScopedSuspendAllThreadsRunning {
public:
    explicit ScopedSuspendAllThreadsRunning(Rendezvous *rendezvous) ACQUIRE(*Locks::mutator_lock);
    ~ScopedSuspendAllThreadsRunning() RELEASE(*Locks::mutator_lock);

    NO_COPY_SEMANTIC(ScopedSuspendAllThreadsRunning);
    NO_MOVE_SEMANTIC(ScopedSuspendAllThreadsRunning);

private:
    Rendezvous *rendezvous_;
};

}  // namespace panda

#endif  // PANDA_RUNTIME_MEM_RENDEZVOUS_H_
