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

#include "verification/thread/verifier_thread.h"

#include "verification/job_queue/job.h"
#include "verification/job_queue/cache.h"
#include "verification/job_queue/job_queue.h"

#include "verification/absint/panda_types.h"

#include "runtime/include/mem/allocator.h"
#include "runtime/include/mem/panda_containers.h"
#include "runtime/include/runtime.h"

#include "libpandabase/os/thread.h"

#include "macros.h"

namespace panda::verifier {

size_t JobQueue::num_verifier_threads = 1;
std::array<PandaTypes *, JobQueue::MAX_THREADS> *JobQueue::panda_types = nullptr;
os::memory::ConditionVariable *JobQueue::job_get_cond_var = nullptr;
os::memory::ConditionVariable *JobQueue::job_put_cond_var = nullptr;
os::memory::ConditionVariable *JobQueue::method_cond_var = nullptr;
Job *JobQueue::queue_head = nullptr;
os::memory::Mutex *JobQueue::queue_lock = nullptr;
os::memory::Mutex *JobQueue::method_lock = nullptr;
CacheOfRuntimeThings *JobQueue::cache = nullptr;
std::array<panda::os::thread::native_handle_type, JobQueue::MAX_THREADS> *JobQueue::verifier_thread_handle = nullptr;
Synchronized<PandaUnorderedSet<uint32_t>> *JobQueue::system_files = nullptr;
std::atomic<bool> JobQueue::shutdown {false};
std::atomic<bool> JobQueue::initialized {false};
size_t JobQueue::num_jobs {0};
size_t JobQueue::max_jobs_in_queue {0};

void JobQueue::Initialize(size_t num_threads, size_t queue_size)
{
    if (initialized.load()) {
        return;
    }

    max_jobs_in_queue = queue_size;

    if (queue_lock == nullptr) {
        queue_lock = new os::memory::Mutex;
    }

    if (method_lock == nullptr) {
        method_lock = new os::memory::Mutex;
    }

    num_verifier_threads = num_threads;

    panda_types = new std::array<PandaTypes *, JobQueue::MAX_THREADS> {};

    for (size_t n = 0; n < num_threads; ++n) {
        (*panda_types)[n] = new (mem::AllocatorAdapter<PandaTypes>().allocate(1)) PandaTypes {n};
        (*panda_types)[n]->Init();
    }

    cache = new (mem::AllocatorAdapter<CacheOfRuntimeThings>().allocate(1)) CacheOfRuntimeThings {};

    cache->FastAPI().InitializePandaAssemblyRootClasses();

    job_get_cond_var =
        new (mem::AllocatorAdapter<os::memory::ConditionVariable>().allocate(1)) os::memory::ConditionVariable {};

    job_put_cond_var =
        new (mem::AllocatorAdapter<os::memory::ConditionVariable>().allocate(1)) os::memory::ConditionVariable {};

    method_cond_var =
        new (mem::AllocatorAdapter<os::memory::ConditionVariable>().allocate(1)) os::memory::ConditionVariable {};

    system_files = new Synchronized<PandaUnorderedSet<uint32_t>> {};

    verifier_thread_handle = new std::array<panda::os::thread::native_handle_type, JobQueue::MAX_THREADS> {};

    {
        panda::os::memory::LockHolder lck {*JobQueue::queue_lock};
        initialized.store(true);
        for (size_t n = 0; n < num_threads; ++n) {
            (*verifier_thread_handle)[n] = panda::os::thread::ThreadStart(VerifierThread, n);
        }
    }
}

void JobQueue::Stop(bool wait_queue_empty)
{
    if (!initialized.load()) {
        return;
    }
    if (wait_queue_empty) {
        panda::os::memory::LockHolder lck {*JobQueue::queue_lock};
        while (num_jobs > 0) {
            constexpr uint64_t QUANTUM = 100;
            job_put_cond_var->TimedWait(queue_lock, QUANTUM);
        }
    } else {
        panda::os::memory::LockHolder lck {*JobQueue::queue_lock};
        while (queue_head != nullptr) {
            auto next_job = queue_head->TakeNext();
            JobQueue::DisposeJob(queue_head);
            queue_head = next_job;
        }
    }

    JobQueue::shutdown.store(true);
    job_get_cond_var->SignalAll();

    void *retval = nullptr;
    for (size_t n = 0; n < num_verifier_threads; ++n) {
        os::thread::ThreadJoin((*verifier_thread_handle)[n], &retval);
    }
}

void JobQueue::Destroy()
{
    if (!initialized.load()) {
        return;
    }

    cache->~CacheOfRuntimeThings();
    mem::AllocatorAdapter<CacheOfRuntimeThings>().deallocate(cache, 1);

    for (size_t n = 0; n < num_verifier_threads; ++n) {
        (*panda_types)[n]->~PandaTypes();
        mem::AllocatorAdapter<PandaTypes>().deallocate((*panda_types)[n], 1);
        (*panda_types)[n] = nullptr;
    }

    mem::AllocatorAdapter<os::memory::ConditionVariable>().deallocate(job_get_cond_var, 1);
    mem::AllocatorAdapter<os::memory::ConditionVariable>().deallocate(job_put_cond_var, 1);
    mem::AllocatorAdapter<os::memory::ConditionVariable>().deallocate(method_cond_var, 1);

    delete system_files;
    system_files = nullptr;

    delete panda_types;
    panda_types = nullptr;

    delete verifier_thread_handle;
    verifier_thread_handle = nullptr;

    delete queue_lock;
    queue_lock = nullptr;

    delete method_lock;
    method_lock = nullptr;

    initialized.store(false);
}

PandaTypes &JobQueue::GetPandaTypes(size_t n)
{
    ASSERT(initialized.load());
    return *(*panda_types)[n];
}

CacheOfRuntimeThings &JobQueue::GetCache()
{
    ASSERT(initialized);
    return *cache;
}

void JobQueue::AddJob(Job &job)
{
    ASSERT(initialized.load());
    {
        panda::os::memory::LockHolder lck {*JobQueue::queue_lock};
        while (num_jobs >= max_jobs_in_queue) {
            constexpr uint64_t QUANTUM = 100;
            job_put_cond_var->TimedWait(queue_lock, QUANTUM);
        }
        job.SetNext(queue_head);
        queue_head = &job;
        ++num_jobs;
    }
    job_get_cond_var->SignalAll();
}

Job *JobQueue::GetJob()
{
    ASSERT(initialized.load());
    auto check_job = []() -> Job * {
        if (JobQueue::queue_head != nullptr) {
            Job *result = JobQueue::queue_head;
            JobQueue::queue_head = result->TakeNext();
            --num_jobs;
            return result;
        }
        return nullptr;
    };
    Job *job = nullptr;
    panda::os::memory::LockHolder lck {*queue_lock};
    if (JobQueue::shutdown.load()) {
        return nullptr;
    }
    while ((job = check_job()) == nullptr) {
        constexpr uint64_t QUANTUM = 100;
        job_get_cond_var->TimedWait(queue_lock, QUANTUM);
        if (JobQueue::shutdown.load()) {
            return nullptr;
        }
    }
    job_put_cond_var->SignalAll();
    return job;
}

Job &JobQueue::NewJob(Method &method)
{
    ASSERT(initialized.load());
    auto id = method.GetUniqId();
    auto &cached_method =
        JobQueue::GetCache().GetFromCache<CacheOfRuntimeThings::CachedMethod>(method.GetClass()->GetSourceLang(), id);
    if (Invalid(cached_method)) {
        return Invalid<Job>();
    }
    auto &runtime = *Runtime::GetCurrent();
    auto &verif_options = runtime.GetVerificationOptions();
    auto method_name = method.GetFullName();
    auto config = verif_options.Debug.GetMethodOptions()[method_name];
    Job *job = nullptr;
    if (!config) {
        if (!verif_options.Debug.GetMethodOptions().IsOptionsPresent("default")) {
            LOG(FATAL, VERIFIER) << "Cannot load default options";
            UNREACHABLE();
        }
        job = new (mem::AllocatorAdapter<Job>().allocate(1))
            Job {method, cached_method, verif_options.Debug.GetMethodOptions().GetOptions("default")};
        LOG(DEBUG, VERIFIER) << "Verification config for '" << method_name << "'"
                             << " : 'default'";
    } else {
        job = new (mem::AllocatorAdapter<Job>().allocate(1)) Job {method, cached_method, config->get()};
        LOG(DEBUG, VERIFIER) << "Verification config for '" << method_name << "'"
                             << " : '" << config->get().GetName() << "'";
    }
    if (job == nullptr) {
        return Invalid<Job>();
    }
    return *job;
}

void JobQueue::DisposeJob(Job *job)
{
    ASSERT(initialized.load());
    ASSERT(job != nullptr);
    job->~Job();
    mem::AllocatorAdapter<Job>().deallocate(job, 1);
}

bool JobQueue::IsSystemFile(const panda_file::File *file)
{
    ASSERT(initialized.load());
    ASSERT(file != nullptr);
    const auto hash = file->GetFilenameHash();
    return (*system_files)->count(hash) > 0;
}

void JobQueue::AddSystemFile(const std::string &filename)
{
    ASSERT(initialized.load());
    const auto hash = panda_file::File::CalcFilenameHash(filename);
    (*system_files)->insert(hash);
}

void JobQueue::SignalMethodVerified()
{
    if (!initialized.load()) {
        return;
    }
    method_cond_var->SignalAll();
}

}  // namespace panda::verifier
