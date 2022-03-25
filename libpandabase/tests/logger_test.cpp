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
#include "utils/logger.h"
#include "utils/string_helpers.h"

#include <cstdio>

#include <fstream>
#include <regex>
#include <streambuf>

#include <gtest/gtest.h>

namespace panda::test {

TEST(Logger, Initialization)
{
    EXPECT_FALSE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::ALLOC));

    testing::FLAGS_gtest_death_test_style = "fast";
    testing::internal::CaptureStderr();

    LOG(DEBUG, COMMON) << "1";
    LOG(INFO, COMMON) << "2";
    LOG(ERROR, COMMON) << "3";

    std::string err = testing::internal::GetCapturedStderr();
    EXPECT_EQ(err, "");

    EXPECT_DEATH_IF_SUPPORTED(LOG(FATAL, COMMON) << "4", "");

    Logger::InitializeStdLogging(Logger::Level::DEBUG, panda::LoggerComponentMaskAll);
    EXPECT_TRUE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::ALLOC));

    testing::internal::CaptureStderr();

    LOG(DEBUG, COMMON) << "a";
    LOG(INFO, COMMON) << "b";
    LOG(ERROR, COMMON) << "c";

    err = testing::internal::GetCapturedStderr();
    uint32_t tid = os::thread::GetCurrentThreadId();
    std::string res = helpers::string::Format(
#ifndef NDEBUG
        "[TID %06x] D/common: a\n"
#endif
        "[TID %06x] I/common: b\n"
        "[TID %06x] E/common: c\n",
        tid, tid, tid);
    EXPECT_EQ(err, res);

    EXPECT_DEATH_IF_SUPPORTED(LOG(FATAL, COMMON) << "d", "\\[TID [0-9a-f]{6}\\] F/common: d");

    Logger::Destroy();
    EXPECT_FALSE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::ALLOC));

    testing::internal::CaptureStderr();

    LOG(DEBUG, COMMON) << "1";
    LOG(INFO, COMMON) << "2";
    LOG(ERROR, COMMON) << "3";

    err = testing::internal::GetCapturedStderr();
    EXPECT_EQ(err, "");

    EXPECT_DEATH_IF_SUPPORTED(LOG(FATAL, COMMON) << "4", "");
}

TEST(Logger, FilterInfo)
{
    Logger::InitializeStdLogging(Logger::Level::INFO, panda::LoggerComponentMaskAll);
    EXPECT_TRUE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::ALLOC));

    testing::internal::CaptureStderr();

    LOG(DEBUG, COMMON) << "a";
    LOG(INFO, COMMON) << "b";
    LOG(ERROR, COMMON) << "c";

    std::string err = testing::internal::GetCapturedStderr();
    uint32_t tid = os::thread::GetCurrentThreadId();
    std::string res = helpers::string::Format(
        "[TID %06x] I/common: b\n"
        "[TID %06x] E/common: c\n",
        tid, tid);
    EXPECT_EQ(err, res);

    Logger::Destroy();
    EXPECT_FALSE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::ALLOC));
}

TEST(Logger, FilterError)
{
    Logger::InitializeStdLogging(Logger::Level::ERROR, panda::LoggerComponentMaskAll);
    EXPECT_TRUE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::ALLOC));

    testing::internal::CaptureStderr();

    LOG(DEBUG, COMMON) << "a";
    LOG(INFO, COMMON) << "b";
    LOG(ERROR, COMMON) << "c";

    std::string err = testing::internal::GetCapturedStderr();
    uint32_t tid = os::thread::GetCurrentThreadId();
    std::string res = helpers::string::Format("[TID %06x] E/common: c\n", tid);
    EXPECT_EQ(err, res);

    Logger::Destroy();
    EXPECT_FALSE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::ALLOC));
}

TEST(Logger, FilterFatal)
{
    Logger::InitializeStdLogging(Logger::Level::FATAL, panda::LoggerComponentMaskAll);
    EXPECT_TRUE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::ALLOC));

    testing::internal::CaptureStderr();

    LOG(DEBUG, COMMON) << "a";
    LOG(INFO, COMMON) << "b";
    LOG(ERROR, COMMON) << "c";

    std::string err = testing::internal::GetCapturedStderr();
    uint32_t tid = os::thread::GetCurrentThreadId();
    EXPECT_EQ(err, "");

    Logger::Destroy();
    EXPECT_FALSE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::ALLOC));
}

TEST(Logger, ComponentFilter)
{
    panda::Logger::ComponentMask component_mask;
    component_mask.set(Logger::Component::CLASS_LINKER);
    component_mask.set(Logger::Component::GC);

    Logger::InitializeStdLogging(Logger::Level::INFO, component_mask);
    EXPECT_FALSE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::ALLOC));
    EXPECT_TRUE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::CLASS_LINKER));
    EXPECT_TRUE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::GC));

    testing::internal::CaptureStderr();

    LOG(INFO, COMMON) << "a";
    LOG(INFO, CLASS_LINKER) << "b";
    LOG(INFO, RUNTIME) << "c";
    LOG(INFO, GC) << "d";

    std::string err = testing::internal::GetCapturedStderr();
    uint32_t tid = os::thread::GetCurrentThreadId();
    std::string res = helpers::string::Format(
        "[TID %06x] I/classlinker: b\n"
        "[TID %06x] I/gc: d\n",
        tid, tid);
    EXPECT_EQ(err, res);

    Logger::Destroy();
    EXPECT_FALSE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::ALLOC));
}

TEST(Logger, FileLogging)
{
    uint32_t tid = os::thread::GetCurrentThreadId();
    std::string log_filename = helpers::string::Format("/tmp/gtest_panda_logger_file_%06x", tid);

    Logger::InitializeFileLogging(log_filename, Logger::Level::INFO,
                                  panda::Logger::ComponentMask().set(Logger::Component::COMMON));
    EXPECT_FALSE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::ALLOC));
    EXPECT_TRUE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::COMMON));

    LOG(DEBUG, COMMON) << "a";
    LOG(INFO, COMMON) << "b";
    LOG(ERROR, CLASS_LINKER) << "c";
    LOG(ERROR, COMMON) << "d";

#if GTEST_HAS_DEATH_TEST
    testing::FLAGS_gtest_death_test_style = "fast";

    EXPECT_DEATH(LOG(FATAL, COMMON) << "e", "");

    std::string res = helpers::string::Format(
        "\\[TID %06x\\] I/common: b\n"
        "\\[TID %06x\\] E/common: d\n"
        "\\[TID [0-9a-f]{6}\\] F/common: e\n",
        tid, tid);
    std::regex e(res);
    {
        std::ifstream log_file_stream(log_filename);
        std::string log_file_content((std::istreambuf_iterator<char>(log_file_stream)),
                                     std::istreambuf_iterator<char>());
        EXPECT_TRUE(std::regex_match(log_file_content, e));
    }
#endif  // GTEST_HAS_DEATH_TEST

    EXPECT_EQ(std::remove(log_filename.c_str()), 0);

    Logger::Destroy();
    EXPECT_FALSE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::ALLOC));
}

TEST(Logger, Multiline)
{
    Logger::InitializeStdLogging(Logger::Level::INFO, panda::Logger::ComponentMask().set(Logger::Component::COMMON));
    EXPECT_FALSE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::ALLOC));
    EXPECT_TRUE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::COMMON));

    testing::internal::CaptureStderr();

    LOG(INFO, COMMON) << "a\nb\nc\n\nd\n";

    std::string err = testing::internal::GetCapturedStderr();
    uint32_t tid = os::thread::GetCurrentThreadId();
    std::string res = helpers::string::Format(
        "[TID %06x] I/common: a\n"
        "[TID %06x] I/common: b\n"
        "[TID %06x] I/common: c\n"
        "[TID %06x] I/common: \n"
        "[TID %06x] I/common: d\n"
        "[TID %06x] I/common: \n",
        tid, tid, tid, tid, tid, tid);
    EXPECT_EQ(err, res);

    Logger::Destroy();
    EXPECT_FALSE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::ALLOC));
}

TEST(Logger, PLog)
{
    Logger::InitializeStdLogging(Logger::Level::INFO, panda::LoggerComponentMaskAll);
    EXPECT_TRUE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::ALLOC));

    testing::internal::CaptureStderr();

    int errnum = errno;

    errno = EEXIST;
    PLOG(ERROR, COMMON) << "a";
    errno = EACCES;
    PLOG(INFO, CLASS_LINKER) << "b";
    errno = errnum;

    std::string err = testing::internal::GetCapturedStderr();
    uint32_t tid = os::thread::GetCurrentThreadId();
    std::string res = helpers::string::Format(
        "[TID %06x] E/common: a: File exists\n"
        "[TID %06x] I/classlinker: b: Permission denied\n",
        tid, tid, tid, tid);
    EXPECT_EQ(err, res);

    Logger::Destroy();
    EXPECT_FALSE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::ALLOC));
}

TEST(Logger, LogIf)
{
    Logger::InitializeStdLogging(Logger::Level::INFO, panda::LoggerComponentMaskAll);
    EXPECT_TRUE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::ALLOC));

    testing::internal::CaptureStderr();

    LOG_IF(true, INFO, COMMON) << "a";
    LOG_IF(false, INFO, COMMON) << "b";

    std::string err = testing::internal::GetCapturedStderr();
    uint32_t tid = os::thread::GetCurrentThreadId();
    std::string res = helpers::string::Format("[TID %06x] I/common: a\n", tid);
    EXPECT_EQ(err, res);

    Logger::Destroy();
    EXPECT_FALSE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::ALLOC));
}

TEST(Logger, LogOnce)
{
    Logger::InitializeStdLogging(Logger::Level::INFO, panda::LoggerComponentMaskAll);
    EXPECT_TRUE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::ALLOC));

    testing::internal::CaptureStderr();

    LOG_ONCE(INFO, COMMON) << "a";
    for (int i = 0; i < 10; ++i) {
        LOG_ONCE(INFO, COMMON) << "b";
    }
    LOG_ONCE(INFO, COMMON) << "c";

    std::string err = testing::internal::GetCapturedStderr();
    uint32_t tid = os::thread::GetCurrentThreadId();
    std::string res = helpers::string::Format(
        "[TID %06x] I/common: a\n"
        "[TID %06x] I/common: b\n"
        "[TID %06x] I/common: c\n",
        tid, tid, tid);
    EXPECT_EQ(err, res);

    Logger::Destroy();
    EXPECT_FALSE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::ALLOC));
}

TEST(Logger, LogDfx)
{
    Logger::InitializeStdLogging(Logger::Level::ERROR, panda::LoggerComponentMaskAll);
    EXPECT_TRUE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::ALLOC));

    DfxController::Initialize();
    EXPECT_TRUE(DfxController::IsInitialized());
    EXPECT_EQ(DfxController::GetOptionValue(DfxOptionHandler::DFXLOG), 0);

    testing::internal::CaptureStderr();

    LOG_DFX(COMMON) << "a";
    LOG_DFX(COMMON) << "b";
    LOG_DFX(COMMON) << "c";

    std::string err = testing::internal::GetCapturedStderr();
    EXPECT_EQ(err, "");

    DfxController::ResetOptionValueFromString("dfx-log:1");
    EXPECT_EQ(DfxController::GetOptionValue(DfxOptionHandler::DFXLOG), 1);

    testing::internal::CaptureStderr();

    LOG_DFX(COMMON) << "a";
    LOG_DFX(COMMON) << "b";
    LOG_DFX(COMMON) << "c";

    err = testing::internal::GetCapturedStderr();
    uint32_t tid = os::thread::GetCurrentThreadId();
    std::string res = helpers::string::Format(
        "[TID %06x] E/dfx: common log:a\n"
        "[TID %06x] E/dfx: common log:b\n"
        "[TID %06x] E/dfx: common log:c\n",
        tid, tid, tid);
    EXPECT_EQ(err, res);

    Logger::Destroy();
    EXPECT_FALSE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::ALLOC));

    DfxController::Destroy();
    EXPECT_FALSE(DfxController::IsInitialized());
}

}  // namespace panda::test
