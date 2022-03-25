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

#include "dfx.h"
#include "logger.h"

namespace panda {
DfxController *DfxController::dfx_controller = nullptr;
os::memory::Mutex DfxController::mutex;  // NOLINT(fuchsia-statically-constructed-objects)

/* static */
void DfxController::SetDefaultOption()
{
    ASSERT(IsInitialized());
    for (auto option = DfxOptionHandler::DfxOption(0); option < DfxOptionHandler::END_FLAG;
         option = DfxOptionHandler::DfxOption(option + 1)) {
        switch (option) {
#ifdef PANDA_TARGET_UNIX
            case DfxOptionHandler::COMPILER_NULLCHECK:
                dfx_controller->option_map_[DfxOptionHandler::COMPILER_NULLCHECK] = 1;
                break;
            case DfxOptionHandler::REFERENCE_DUMP:
                dfx_controller->option_map_[DfxOptionHandler::REFERENCE_DUMP] = 1;
                break;
            case DfxOptionHandler::SIGNAL_CATCHER:
                dfx_controller->option_map_[DfxOptionHandler::SIGNAL_CATCHER] = 1;
                break;
            case DfxOptionHandler::SIGNAL_HANDLER:
                dfx_controller->option_map_[DfxOptionHandler::SIGNAL_HANDLER] = 1;
                break;
            case DfxOptionHandler::ARK_SIGQUIT:
                dfx_controller->option_map_[DfxOptionHandler::ARK_SIGQUIT] = 1;
                break;
            case DfxOptionHandler::ARK_SIGUSR1:
                dfx_controller->option_map_[DfxOptionHandler::ARK_SIGUSR1] = 1;
                break;
            case DfxOptionHandler::ARK_SIGUSR2:
                dfx_controller->option_map_[DfxOptionHandler::ARK_SIGUSR2] = 1;
                break;
            case DfxOptionHandler::MOBILE_LOG:
                dfx_controller->option_map_[DfxOptionHandler::MOBILE_LOG] = 1;
                break;
#endif
            case DfxOptionHandler::DFXLOG:
                dfx_controller->option_map_[DfxOptionHandler::DFXLOG] = 0;
                break;
            default:
                break;
        }
    }
}

/* static */
void DfxController::ResetOptionValueFromString(const std::string &s)
{
    size_t last_pos = s.find_first_not_of(';', 0);
    size_t pos = s.find(';', last_pos);
    while (last_pos != std::string::npos) {
        std::string arg = s.substr(last_pos, pos - last_pos);
        last_pos = s.find_first_not_of(';', pos);
        pos = s.find(';', last_pos);
        std::string option_str = arg.substr(0, arg.find(':'));
        uint8_t value = static_cast<uint8_t>(std::stoi(arg.substr(arg.find(':') + 1)));
        auto dfx_option = DfxOptionHandler::DfxOptionFromString(option_str);
        if (dfx_option != DfxOptionHandler::END_FLAG) {
            DfxController::SetOptionValue(dfx_option, value);
#ifdef PANDA_TARGET_UNIX
            if (dfx_option == DfxOptionHandler::MOBILE_LOG) {
                // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_NESTING_LEVEL)
                if (value == 0) {
                    Logger::SetMobileLogOpenFlag(false);
                } else {
                    Logger::SetMobileLogOpenFlag(true);
                }
            }
#endif
        } else {
            LOG(ERROR, DFX) << "Unknown Option " << option_str;
        }
    }
}

/* static */
void DfxController::PrintDfxOptionValues()
{
    ASSERT(IsInitialized());
    for (auto &iter : dfx_controller->option_map_) {
        LOG(ERROR, DFX) << "DFX option: " << DfxOptionHandler::StringFromDfxOption(iter.first)
                        << ", option values: " << std::to_string(iter.second);
    }
}

/* static */
void DfxController::Initialize(std::map<DfxOptionHandler::DfxOption, uint8_t> option_map)
{
    if (IsInitialized()) {
        dfx_controller->SetDefaultOption();
        return;
    }
    {
        os::memory::LockHolder<os::memory::Mutex> lock(mutex);
        if (IsInitialized()) {
            dfx_controller->SetDefaultOption();
            return;
        }
        // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
        dfx_controller = new DfxController(std::move(option_map));
    }
}

/* static */
void DfxController::Initialize()
{
    if (IsInitialized()) {
        dfx_controller->SetDefaultOption();
        return;
    }
    {
        os::memory::LockHolder<os::memory::Mutex> lock(mutex);
        if (IsInitialized()) {
            dfx_controller->SetDefaultOption();
            return;
        }
        std::map<DfxOptionHandler::DfxOption, uint8_t> option_map;
        for (auto option = DfxOptionHandler::DfxOption(0); option < DfxOptionHandler::END_FLAG;
             option = DfxOptionHandler::DfxOption(option + 1)) {
            switch (option) {
#ifdef PANDA_TARGET_UNIX
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
#endif
                case DfxOptionHandler::DFXLOG:
                    option_map[DfxOptionHandler::DFXLOG] = 0;
                    break;
                default:
                    break;
            }
        }
        // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
        dfx_controller = new DfxController(std::move(option_map));
    }
}

/* static */
void DfxController::Destroy()
{
    if (!IsInitialized()) {
        return;
    }
    DfxController *d = nullptr;
    {
        os::memory::LockHolder<os::memory::Mutex> lock(mutex);
        if (!IsInitialized()) {
            return;
        }
        d = dfx_controller;
        dfx_controller = nullptr;
    }
    delete d;
}

}  // namespace panda
