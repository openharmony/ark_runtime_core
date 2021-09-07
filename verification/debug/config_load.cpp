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

#ifndef PANDA_VERIF_CONFIG_LOAD_H_
#define PANDA_VERIF_CONFIG_LOAD_H_

#include "config_load.h"

#include "verification/debug/config/config_parse.h"
#include "verification/debug/config/config_process.h"
#include "verification/debug/context/context.h"
#include "verification/debug/handlers/config_handlers.h"
#include "verification/debug/breakpoint/breakpoint_private.h"
#include "verification/debug/allowlist/allowlist_private.h"

#if !PANDA_TARGET_WINDOWS
#include "securec.h"
#endif
#include "utils/logger.h"
#include "libpandabase/os/file.h"

#include "default_config.h"

namespace {

bool ProcessConfigFile(const char *text)
{
    panda::verifier::debug::RegisterConfigHandlerBreakpoints();
    panda::verifier::debug::RegisterConfigHandlerAllowlist();
    panda::verifier::debug::RegisterConfigHandlerOptions();
    panda::verifier::debug::RegisterConfigHandlerMethodOptions();
    panda::verifier::debug::RegisterConfigHandlerMethodGroups();

    panda::verifier::config::Section config;

    bool result = panda::verifier::config::ParseConfig(text, config) && panda::verifier::config::ProcessConfig(config);
    if (result) {
        LOG(DEBUG, VERIFIER) << "Verifier debug configuration: \n" << config.Image();
        panda::verifier::debug::SetDefaultMethodOptions();
    }

    return result;
}

}  // namespace

namespace panda::verifier::config {

bool LoadConfig(std::string_view filename)
{
    using panda::os::file::Mode;
    using panda::os::file::Open;

    bool result = false;

    if (filename == "default") {
        result = ProcessConfigFile(G_VERIFIER_DEBUG_DEFAULT_CONFIG);
    } else {
        do {
            auto file = Open(filename, Mode::READONLY);
            if (!file.IsValid()) {
                break;
            }
            auto size = file.GetFileSize();
            if (!size.HasValue()) {
                file.Close();
                break;
            }

            char *text = new (std::nothrow) char[1 + *size];
            if (text == nullptr) {
                file.Close();
                break;
            }

            (void)memset_s(text, 1 + *size, 0x00, 1 + *size);
            if (!file.ReadAll(text, *size)) {
                file.Close();
                delete[] text;
                break;
            }
            text[*size] = 0;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            file.Close();
            result = ProcessConfigFile(text);
            delete[] text;
        } while (false);
    }

    if (!result) {
        LOG(DEBUG, VERIFIER) << "Failed to load verifier debug config file '" << filename << "'";
    }

    return result;
}

void MethodIdCalculationHandler(uint32_t class_hash, uint32_t hash, uintptr_t id)
{
    debug::BreakpointMethodIdCalculationHandler(class_hash, hash, id);
    debug::AllowlistMethodIdCalculationHandler(class_hash, hash, id);
}

}  // namespace panda::verifier::config

#endif  //! PANDA_VERIF_CONFIG_LOAD_H_
