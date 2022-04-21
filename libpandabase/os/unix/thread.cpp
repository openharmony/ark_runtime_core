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

#include "os/thread.h"

#include <cstdint>
#ifdef PANDA_TARGET_UNIX
#include <sys/syscall.h>
#include <sys/resource.h>
#endif
#include <unistd.h>

namespace panda::os::thread {
ThreadId GetCurrentThreadId()
{
#if defined(HAVE_GETTID)
    static_assert(sizeof(decltype(gettid())) == sizeof(ThreadId), "Incorrect alias for ThreadID");
    return static_cast<ThreadId>(gettid());
#elif defined(PANDA_TARGET_MACOS)
    uint64_t tid64;
    pthread_threadid_np(NULL, &tid64);
    return static_cast<ThreadId>(tid64);
#else
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    return static_cast<ThreadId>(syscall(SYS_gettid));
#endif
}

int SetPriority(int thread_id, int prio)
{
    return setpriority(PRIO_PROCESS, thread_id, prio);
}

int GetPriority(int thread_id)
{
    return getpriority(PRIO_PROCESS, thread_id);
}

int SetThreadName(native_handle_type pthread_id, const char *name)
{
    ASSERT(pthread_id != 0);
#if defined(PANDA_TARGET_MACOS)
    return pthread_setname_np(name);
#else
    return pthread_setname_np(pthread_id, name);
#endif
}

native_handle_type GetNativeHandle()
{
    return pthread_self();
}

void Yield()
{
    std::this_thread::yield();
}

void NativeSleep(unsigned int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void ThreadDetach(native_handle_type pthread_id)
{
    pthread_detach(pthread_id);
}

void ThreadExit(void *retval)
{
    pthread_exit(retval);
}

void ThreadJoin(native_handle_type pthread_id, void **retval)
{
    pthread_join(pthread_id, retval);
}
}  // namespace panda::os::thread
