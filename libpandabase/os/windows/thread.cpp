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

#include <thread>
#include <processthreadsapi.h>

namespace panda::os::thread {

ThreadId GetCurrentThreadId()
{
    return static_cast<ThreadId>(std::hash<std::thread::id>()(std::this_thread::get_id()));
}

int SetPriority([[maybe_unused]] int thread_id, int prio)
{
    return SetThreadPriority(GetCurrentThread(), prio);
}

int GetPriority([[maybe_unused]] int thread_id)
{
    return GetThreadPriority(GetCurrentThread());
}

int SetThreadName([[maybe_unused]] native_handle_type pthread_id, const char *name)
{
    ASSERT(pthread_id != 0);
    return pthread_setname_np(pthread_self(), name);
}

void Yield()
{
    std::this_thread::yield();
}

void NativeSleep(unsigned int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void ThreadJoin(native_handle_type pthread_id, void **retval)
{
    pthread_join(reinterpret_cast<pthread_t>(pthread_id), retval);
}

}  // namespace panda::os::thread
