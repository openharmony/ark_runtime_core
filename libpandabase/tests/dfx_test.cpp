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

#include "os/thread.h"
#include "utils/dfx.h"
#include "utils/logger.h"
#include "utils/string_helpers.h"

#include <gtest/gtest.h>

namespace panda::test {

TEST(DfxController, Initialization)
{
    if (DfxController::IsInitialized()) {
        DfxController::Destroy();
    }
    EXPECT_FALSE(DfxController::IsInitialized());

    DfxController::Initialize();
    EXPECT_TRUE(DfxController::IsInitialized());

    DfxController::Destroy();
    EXPECT_FALSE(DfxController::IsInitialized());

    std::map<DfxOptionHandler::DfxOption, uint8_t> option_map;
    for (auto option = DfxOptionHandler::DfxOption(0); option < DfxOptionHandler::END_FLAG;
         option = DfxOptionHandler::DfxOption(option + 1)) {
        switch (option) {
            case DfxOptionHandler::COMPILER_NULLCHECK:
                option_map[DfxOptionHandler::COMPILER_NULLCHECK] = 1;
                break;
            case DfxOptionHandler::REFERENCE_DUMP:
                option_map[DfxOptionHandler::REFERENCE_DUMP] = 1;
                break;
            case DfxOptionHandler::SIGNAL_CATCHER:
                option_map[DfxOptionHandler::SIGNAL_CATCHER] = 1;
                break;
            case DfxOptionHandler::SIGNAL_HANDLER:
                option_map[DfxOptionHandler::SIGNAL_HANDLER] = 1;
                break;
            case DfxOptionHandler::ARK_SIGQUIT:
                option_map[DfxOptionHandler::ARK_SIGQUIT] = 1;
                break;
            case DfxOptionHandler::ARK_SIGUSR1:
                option_map[DfxOptionHandler::ARK_SIGUSR1] = 1;
                break;
            case DfxOptionHandler::ARK_SIGUSR2:
                option_map[DfxOptionHandler::ARK_SIGUSR2] = 1;
                break;
            case DfxOptionHandler::MOBILE_LOG:
                option_map[DfxOptionHandler::MOBILE_LOG] = 1;
                break;
            case DfxOptionHandler::DFXLOG:
                option_map[DfxOptionHandler::DFXLOG] = 0;
                break;
            default:
                break;
        }
    }

    DfxController::Initialize(option_map);
    EXPECT_TRUE(DfxController::IsInitialized());

    DfxController::Destroy();
    EXPECT_FALSE(DfxController::IsInitialized());
}

TEST(DfxController, TestResetOptionValueFromString)
{
    if (DfxController::IsInitialized()) {
        DfxController::Destroy();
    }
    EXPECT_FALSE(DfxController::IsInitialized());

    DfxController::Initialize();
    EXPECT_TRUE(DfxController::IsInitialized());

    DfxController::ResetOptionValueFromString("dfx-log:1");
    EXPECT_EQ(DfxController::GetOptionValue(DfxOptionHandler::DFXLOG), 1);

    DfxController::Destroy();
    EXPECT_FALSE(DfxController::IsInitialized());
}

TEST(DfxController, TestPrintDfxOptionValues)
{
    if (DfxController::IsInitialized()) {
        DfxController::Destroy();
    }
    EXPECT_FALSE(DfxController::IsInitialized());

    Logger::InitializeStdLogging(Logger::Level::INFO, panda::LoggerComponentMaskAll);
    EXPECT_TRUE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::DFX));

    DfxController::Initialize();
    EXPECT_TRUE(DfxController::IsInitialized());

    testing::internal::CaptureStderr();

    DfxController::PrintDfxOptionValues();

    std::string err = testing::internal::GetCapturedStderr();
    uint32_t tid = os::thread::GetCurrentThreadId();
#ifdef PANDA_TARGET_UNIX
    std::string res = helpers::string::Format(
        "[TID %06x] E/dfx: DFX option: compiler-nullcheck, option values: 1\n"
        "[TID %06x] E/dfx: DFX option: reference-dump, option values: 1\n"
        "[TID %06x] E/dfx: DFX option: signal-catcher, option values: 1\n"
        "[TID %06x] E/dfx: DFX option: signal-handler, option values: 1\n"
        "[TID %06x] E/dfx: DFX option: sigquit, option values: 1\n"
        "[TID %06x] E/dfx: DFX option: sigusr1, option values: 1\n"
        "[TID %06x] E/dfx: DFX option: sigusr2, option values: 1\n"
        "[TID %06x] E/dfx: DFX option: mobile-log, option values: 1\n"
        "[TID %06x] E/dfx: DFX option: dfx-log, option values: 0\n",
        tid, tid, tid, tid, tid, tid, tid, tid, tid, tid);
#else
    std::string res = helpers::string::Format("[TID %06x] E/dfx: DFX option: dfx-log, option values: 0\n", tid, tid);
#endif
    EXPECT_EQ(err, res);

    Logger::Destroy();
    EXPECT_FALSE(Logger::IsLoggingOn(Logger::Level::FATAL, Logger::Component::DFX));

    DfxController::Destroy();
    EXPECT_FALSE(DfxController::IsInitialized());
}

}  // namespace panda::test
