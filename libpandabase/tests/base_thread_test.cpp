/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include <condition_variable>
#include "gtest/gtest.h"
#include "os/thread.h"

namespace panda::os::thread {
class ThreadTest : public testing::Test {};

uint32_t thread_id = 0;
bool updated = false;
bool operated = false;
std::mutex mu;
std::condition_variable cv;

#ifdef PANDA_TARGET_UNIX
// On linux, the priority can be set within range [-20, 19], and 19 is the lowest priority.
constexpr int LOWER_PRIOIRITY = 1;
constexpr int LOWEST_PRIORITY = 19;
#elif PANDA_TARGET_WINDOWS
// On Windows, the priority can be set within range [-2, 2], and -2 is the lowest priority.
constexpr int LOWER_PRIOIRITY = -1;
constexpr int LOWEST_PRIORITY = -2;
#endif

void ThreadFunc()
{
    thread_id = GetCurrentThreadId();
    {
        std::lock_guard lk(mu);
        updated = true;
    }
    cv.notify_one();
    {
        // wait for the main thread to Set/GetPriority
        std::unique_lock lk(mu);
        cv.wait(lk, [] { return operated; });
    }
}

TEST_F(ThreadTest, SetCurrentThreadPriorityTest)
{
    // Since setting higher priority needs "sudo" right, we only test lower one here.
    auto ret1 = SetPriority(GetCurrentThreadId(), LOWER_PRIOIRITY);
    auto prio1 = GetPriority(GetCurrentThreadId());
    ASSERT_EQ(prio1, LOWER_PRIOIRITY);

    auto ret2 = SetPriority(GetCurrentThreadId(), LOWEST_PRIORITY);
    auto prio2 = GetPriority(GetCurrentThreadId());
    ASSERT_EQ(prio2, LOWEST_PRIORITY);

#ifdef PANDA_TARGET_UNIX
    ASSERT_EQ(ret1, 0);
    ASSERT_EQ(ret2, 0);
#elif PANDA_TARGET_WINDOWS
    ASSERT_NE(ret1, 0);
    ASSERT_NE(ret2, 0);
#endif
}

TEST_F(ThreadTest, SetOtherThreadPriorityTest)
{
    auto parent_pid = GetCurrentThreadId();
    auto parent_prio_before = GetPriority(parent_pid);

    auto new_thread = ThreadStart(ThreadFunc);
    // wait for the new_thread to update thread_id
    std::unique_lock lk(mu);
    cv.wait(lk, [] { return updated; });
    auto child_pid = thread_id;

    auto child_prio_before = GetPriority(child_pid);
    auto ret = SetPriority(child_pid, LOWEST_PRIORITY);

    auto child_prio_after = GetPriority(child_pid);
    auto parent_prio_after = GetPriority(parent_pid);

    operated = true;
    lk.unlock();
    cv.notify_one();
    void *res;
    ThreadJoin(new_thread, &res);

    ASSERT_EQ(parent_prio_before, parent_prio_after);
#ifdef PANDA_TARGET_UNIX
    ASSERT_EQ(ret, 0);
    ASSERT(child_prio_before <= child_prio_after);
#elif PANDA_TARGET_WINDOWS
    ASSERT_NE(ret, 0);
    ASSERT(child_prio_after <= child_prio_before);
#endif
}
}  // namespace panda::os::thread
