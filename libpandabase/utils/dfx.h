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

// DfxController is designed to help developers enable/disable or set level of some DFX capabilities.
//
// Developers can change the option value of each capability included in DfxOptionHandler::DfxOption.
//
// DFX Capabilities Supported:
// |-----------------------------------------------------------------------------------------------------------------|
// |     Dfx Option     |     Ark Option     | Prop `ark.dfx.options` |          How option values work              |
// |--------------------|--------------------|------------------------|----------------------------------------------|
// | COMPILER_NULLCHECK | compiler-nullcheck |   compiler-nullcheck   |   0/1(default) compiler implicit null check  |
// |   REFERENCE_DUMP   |   reference-dump   |     reference-dump     |     0/1(default) dump refs statistic info    |
// |   SIGNAL_CATCHER   |   signal-catcher   |     signal-catcher     |         0/1(default) signal catcher          |
// |   SIGNAL_HANDLER   |   signal-handler   |     signal-handler     |         0/1(default) signal handler          |
// |     ARKSIGQUIT     |    sigquit-flag    |         sigquit        |         0/1(default) sigquit action          |
// |     ARKSIGUSR1     |    sigusr1-flag    |         sigusr1        |         0/1(default) sigusr1 action          |
// |     ARKSIGUSR2     |    sigusr2-flag    |         sigusr2        |         0/1(default) sigusr2 action          |
// |       MOBILE_LOG       |     mlog-flag    |         mlog         |    0/1(default) ark log printed in mlog    |
// |       DFXLOG       |       dfx-log      |         dfx-log        |          0(default)/1 ark dfx log            |
// |--------------------|--------------------|------------------------|----------------------------------------------|
// Note: if option value can only be set to 0/1, it means disable/enable, else enable in multiple levels.
//
// If you want to control your own DFX capability, please add your dfx option in DfxOptionHandler::DfxOption,
// and update code related to DfxController initialize and runtime options.

#ifndef PANDA_LIBPANDABASE_UTILS_DFX_H_
#define PANDA_LIBPANDABASE_UTILS_DFX_H_

#include "os/mutex.h"
#include "os/dfx_option.h"

#include <cstdint>
#include <map>

#include <atomic>

namespace panda {

using DfxOptionHandler = os::dfx_option::DfxOptionHandler;

class DfxController {
public:
    static void Initialize(std::map<DfxOptionHandler::DfxOption, uint8_t> option_map);

    static void Initialize();

    static bool IsInitialized()
    {
        return dfx_controller != nullptr;
    }

    static void Destroy();

    static uint8_t GetOptionValue(DfxOptionHandler::DfxOption dfx_option)
    {
        ASSERT(IsInitialized());
        return dfx_controller->option_map_[dfx_option];
    }

    static void SetOptionValue(DfxOptionHandler::DfxOption dfx_option, uint8_t value)
    {
        ASSERT(IsInitialized());
        dfx_controller->option_map_[dfx_option] = value;
    }

    static void PrintDfxOptionValues();

    static void ResetOptionValueFromString(const std::string &s);

private:
    static void SetDefaultOption();

    explicit DfxController(std::map<DfxOptionHandler::DfxOption, uint8_t> option_map)
        : option_map_(std::move(option_map))
    {
    }

    ~DfxController() = default;

    std::map<DfxOptionHandler::DfxOption, uint8_t> option_map_;
    static DfxController *dfx_controller;

    static os::memory::Mutex mutex;

    NO_COPY_SEMANTIC(DfxController);
    NO_MOVE_SEMANTIC(DfxController);
};

}  // namespace panda

#endif  // PANDA_LIBPANDABASE_UTILS_DFX_H_
