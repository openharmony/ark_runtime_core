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

#include "dfx_option.h"

namespace panda::os::dfx_option {

/* static */
bool DfxOptionHandler::IsInOptionList(const std::string &s)
{
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define D(e, v, str) \
    if (s == str) {  \
        return true; \
    }
    DFX_OPTION_LIST(D)
#undef D
    return false;
}

/* static */
DfxOptionHandler::DfxOption DfxOptionHandler::DfxOptionFromString(const std::string &s)
{
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define D(e, v, str)                           \
    if (s == str) {                            \
        return DfxOptionHandler::DfxOption::e; \
    }
    DFX_OPTION_LIST(D)
#undef D
    UNREACHABLE();
}

/* static */
std::string DfxOptionHandler::StringFromDfxOption(DfxOptionHandler::DfxOption dfx_option)
{
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define D(e, v, str)                                    \
    if (dfx_option == DfxOptionHandler::DfxOption::e) { \
        return str;                                     \
    }
    DFX_OPTION_LIST(D)
#undef D
    UNREACHABLE();
}

}  // namespace panda::os::dfx_option
