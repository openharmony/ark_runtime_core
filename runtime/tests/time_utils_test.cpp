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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <random>
#include <thread>

#include "runtime/include/mem/panda_string.h"
#include "runtime/include/runtime.h"
#include "runtime/include/time_utils.h"

#ifndef PANDA_NIGHTLY_TEST_ON
constexpr size_t ITERATION = 64;
#else
constexpr size_t ITERATION = 1024;
#endif

namespace panda::time::test {

class TimeTest : public testing::Test {
public:
    TimeTest()
    {
        RuntimeOptions options;
        options.SetShouldLoadBootPandaFiles(false);
        options.SetShouldInitializeIntrinsics(false);
        Runtime::Create(options);
        thread_ = panda::MTManagedThread::GetCurrent();
        thread_->ManagedCodeBegin();
    }

    ~TimeTest() override
    {
        thread_->ManagedCodeEnd();
        Runtime::Destroy();
    }

protected:
    panda::MTManagedThread *thread_ {nullptr};
};

TEST_F(TimeTest, TimerTest)
{
    uint64_t duration_ = 0;
    {
        Timer timer(&duration_);
        std::this_thread::sleep_for(std::chrono::nanoseconds(10));
    }
    ASSERT_GT(duration_, 0);

    uint64_t last_duration_ = duration_;
    {
        Timer timer(&duration_);
        std::this_thread::sleep_for(std::chrono::nanoseconds(10));
    }
    ASSERT_GT(duration_, last_duration_);

    {
        Timer timer(&duration_, true);
    }
    ASSERT_LT(duration_, last_duration_);
}

TEST_F(TimeTest, CurrentTimeStringTest)
{
    constexpr auto PATTERN =
        "(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) [0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}\\.[0-9]{3}";
    for (size_t i = 0; i < ITERATION; i++) {
        auto date = GetCurrentTimeString();
        ASSERT_EQ(date.size(), 19);
        ASSERT_THAT(date.c_str(), ::testing::MatchesRegex(PATTERN));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

}  // namespace panda::time::test
