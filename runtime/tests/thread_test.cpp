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

#include "gtest/gtest.h"
#include "runtime/include/runtime.h"
#include "runtime/include/thread.h"

namespace panda::test {

class ThreadTest : public testing::Test {
public:
    MTManagedThread *thread;
    ThreadTest() : thread(nullptr)
    {
        RuntimeOptions options;
        options.SetShouldLoadBootPandaFiles(false);
        options.SetShouldInitializeIntrinsics(false);
        /*
         * gtest ASSERT_DEATH doesn't work with multiple threads:
         * "In particular, death tests don't like having multiple threads in the parent process"
         * turn off gc-thread & compiler-thread here, because we use a lot of ASSERT_DEATH and test can hang.
         */
        options.SetCompilerEnableJit(false);
        options.SetGcType("epsilon");
        Runtime::Create(options);
        thread = MTManagedThread::GetCurrent();
    }

    ~ThreadTest() override
    {
        Runtime::Destroy();
    }

    void AssertNative() const
    {
        ASSERT_TRUE(thread->IsInNativeCode());
        ASSERT_FALSE(thread->IsManagedCode());
    }

    void AssertManaged() const
    {
        ASSERT_FALSE(thread->IsInNativeCode());
        ASSERT_TRUE(thread->IsManagedCode());
    }

    void BeginToStateAndEnd(MTManagedThread::ThreadState state) const
    {
        if (state == MTManagedThread::ThreadState::NATIVE_CODE) {
            thread->NativeCodeBegin();
            AssertNative();
            thread->NativeCodeEnd();
        } else if (state == MTManagedThread::ThreadState::MANAGED_CODE) {
            thread->ManagedCodeBegin();
            AssertManaged();
            thread->ManagedCodeEnd();
        } else {
            UNREACHABLE();
        }
    }
};

/**
 * call stack:
 *
 * native #0
 *   managed #1
 *      native #2
 *          access #3
 *   access #4
 *
 */
TEST_F(ThreadTest, LegalThreadStatesTest)
{
    AssertNative();
    thread->ManagedCodeBegin();  // #1
    AssertManaged();
    thread->NativeCodeBegin();  // #2
    AssertNative();

    thread->NativeCodeEnd();  // #2
    AssertManaged();
    thread->ManagedCodeEnd();  // #1
    AssertNative();
}

TEST_F(ThreadTest, BeginForbiddenStatesFromNativeFrame)
{
    testing::FLAGS_gtest_death_test_style = "fast";

    AssertNative();
#ifndef NDEBUG
    ASSERT_DEATH(thread->NativeCodeBegin(), "last frame is: NATIVE_CODE");
#endif
    AssertNative();
}

TEST_F(ThreadTest, BeginForbiddenStatesFromManagedFrame)
{
    testing::FLAGS_gtest_death_test_style = "fast";

    AssertNative();
    thread->ManagedCodeBegin();
    AssertManaged();
#ifndef NDEBUG
    ASSERT_DEATH(thread->ManagedCodeBegin(), "last frame is: MANAGED_CODE");
#endif
    AssertManaged();
    thread->ManagedCodeEnd();
    AssertNative();
}

TEST_F(ThreadTest, EndNativeStateByOtherStates)
{
    testing::FLAGS_gtest_death_test_style = "fast";

    AssertNative();

#ifndef NDEBUG
    ASSERT_DEATH(thread->ManagedCodeEnd(), "last frame is: NATIVE_CODE");
    ASSERT_DEATH(thread->ManagedCodeEnd(), "last frame is: NATIVE_CODE");
#endif
}

TEST_F(ThreadTest, EndManagedStateByOtherStates)
{
    testing::FLAGS_gtest_death_test_style = "fast";

    AssertNative();
    thread->ManagedCodeBegin();

#ifndef NDEBUG
    ASSERT_DEATH(thread->NativeCodeEnd(), "last frame is: MANAGED_CODE");
#endif
    thread->ManagedCodeEnd();
}

TEST_F(ThreadTest, TestAllConversions)
{
    testing::FLAGS_gtest_death_test_style = "fast";

    // from NATIVE_CODE
    AssertNative();
#ifndef NDEBUG
    ASSERT_DEATH(BeginToStateAndEnd(MTManagedThread::ThreadState::NATIVE_CODE), "last frame is: NATIVE_CODE");
#endif
    BeginToStateAndEnd(MTManagedThread::ThreadState::MANAGED_CODE);

    // from MANAGED_CODE
    thread->ManagedCodeBegin();
    AssertManaged();

    BeginToStateAndEnd(MTManagedThread::ThreadState::NATIVE_CODE);
#ifndef NDEBUG
    ASSERT_DEATH(BeginToStateAndEnd(MTManagedThread::ThreadState::MANAGED_CODE), "last frame is: MANAGED_CODE");
#endif
    thread->ManagedCodeEnd();
    AssertNative();
}
}  // namespace panda::test
