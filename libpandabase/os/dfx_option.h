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

#ifndef PANDA_LIBPANDABASE_OS_DFX_OPTION_H_
#define PANDA_LIBPANDABASE_OS_DFX_OPTION_H_

#include "macros.h"

#include <cstdint>
#include <string>

namespace panda::os::dfx_option {

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DFX_OPTION_ELEM(D, NAME, STR) D(NAME, DfxOptionHandler::DfxOptionId::NAME##_ID, STR)

#ifdef PANDA_TARGET_UNIX
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DFX_OPTION_LIST(D)                                       \
    DFX_OPTION_ELEM(D, COMPILER_NULLCHECK, "compiler-nullcheck") \
    DFX_OPTION_ELEM(D, REFERENCE_DUMP, "reference-dump")         \
    DFX_OPTION_ELEM(D, SIGNAL_CATCHER, "signal-catcher")         \
    DFX_OPTION_ELEM(D, SIGNAL_HANDLER, "signal-handler")         \
    DFX_OPTION_ELEM(D, ARK_SIGQUIT, "sigquit")                   \
    DFX_OPTION_ELEM(D, ARK_SIGUSR1, "sigusr1")                   \
    DFX_OPTION_ELEM(D, ARK_SIGUSR2, "sigusr2")                   \
    DFX_OPTION_ELEM(D, MOBILE_LOG, "mobile-log")                 \
    DFX_OPTION_ELEM(D, DFXLOG, "dfx-log")                        \
    DFX_OPTION_ELEM(D, END_FLAG, "end-flag")
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DFX_OPTION_LIST(D)                \
    DFX_OPTION_ELEM(D, DFXLOG, "dfx-log") \
    DFX_OPTION_ELEM(D, END_FLAG, "end-flag")
#endif  // PANDA_TARGET_UNIX

class DfxOptionHandler {
public:
    enum DfxOptionId : uint8_t {
#ifdef PANDA_TARGET_UNIX
        COMPILER_NULLCHECK_ID,
        REFERENCE_DUMP_ID,
        SIGNAL_CATCHER_ID,
        SIGNAL_HANDLER_ID,
        ARK_SIGQUIT_ID,
        ARK_SIGUSR1_ID,
        ARK_SIGUSR2_ID,
        MOBILE_LOG_ID,
#endif  // PANDA_TARGET_UNIX
        DFXLOG_ID,
        END_FLAG_ID,
    };

    enum DfxOption : uint8_t {
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define D(e, v, str) e = v,
        DFX_OPTION_LIST(D)
#undef D
    };

    static bool IsInOptionList(const std::string &s);

    static DfxOption DfxOptionFromString(const std::string &s);

    static std::string StringFromDfxOption(DfxOptionHandler::DfxOption dfx_option);
};

}  // namespace panda::os::dfx_option

#endif  // PANDA_LIBPANDABASE_OS_DFX_OPTION_H_
