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

#ifndef PANDA_VERIFICATION_JOB_QUEUE_JOB_QUEUE_H_
#define PANDA_VERIFICATION_JOB_QUEUE_JOB_QUEUE_H_

#include "libpandabase/os/mutex.h"
#include "libpandabase/os/thread.h"

#include "libpandafile/file.h"

#include "runtime/include/mem/panda_containers.h"

#include "runtime/include/method.h"

#include "verification/util/synchronized.h"

#include "macros.h"

#include <atomic>
#include <string>
#include <array>

namespace panda {
class Method;
}  // namespace panda

namespace panda::verifier {
class Job;
class CacheOfRuntimeThings;
class PandaTypes;

class JobQueue {
public:
    constexpr static size_t MAX_THREADS = 16U;

    static void Initialize(size_t num_threads = 1, size_t queue_size = 32) NO_THREAD_SANITIZE;
    static void Destroy() NO_THREAD_SANITIZE;
    static void Stop(bool wait_queue_empty = false);
    static PandaTypes &GetPandaTypes(size_t n);
    static CacheOfRuntimeThings &GetCache();
    static void AddJob(Job &job);
    static Job *GetJob();
    static Job &NewJob(Method &method);
    static void DisposeJob(Job *job);
    static bool IsSystemFile(const panda_file::File *file);
    static void AddSystemFile(const std::string &filename);
    template <typename Handler, typename FailureHandler>
    static void WaitForVerification(Handler &&continue_waiting, FailureHandler &&failure_handler,
                                    uint64_t quantum = 500)
    {
        if (!initialized.load()) {
            return;
        }
        panda::os::memory::LockHolder lck {*method_lock};
        while (continue_waiting()) {
            if (!initialized.load() || shutdown.load()) {
                failure_handler();
                return;
            }
            method_cond_var->TimedWait(method_lock, quantum);
        }
    }
    static void SignalMethodVerified();
    static bool IsInitialized()
    {
        return initialized.load();
    }

private:
    static size_t num_verifier_threads;
    static size_t max_jobs_in_queue;
    static std::array<PandaTypes *, MAX_THREADS> *panda_types;
    static panda::os::memory::ConditionVariable *job_get_cond_var;
    static panda::os::memory::ConditionVariable *job_put_cond_var;
    static panda::os::memory::ConditionVariable *method_cond_var;
    static Job *queue_head;
    static panda::os::memory::Mutex *queue_lock;
    static panda::os::memory::Mutex *method_lock;
    static CacheOfRuntimeThings *cache;
    static std::array<panda::os::thread::native_handle_type, MAX_THREADS> *verifier_thread_handle;
    static Synchronized<PandaUnorderedSet<uint32_t>> *system_files;
    static std::atomic<bool> shutdown;
    static std::atomic<bool> initialized;
    static size_t num_jobs;
};
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_JOB_QUEUE_JOB_QUEUE_H_
