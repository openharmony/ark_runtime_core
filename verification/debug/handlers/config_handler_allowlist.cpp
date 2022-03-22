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

#include "verification/debug/config/config_process.h"
#include "verification/debug/breakpoint/breakpoint_private.h"
#include "verification/debug/allowlist/allowlist_private.h"
#include "verification/debug/parser/parser.h"

#include "runtime/include/method.h"

#include "runtime/include/mem/panda_string.h"

#include "utils/logger.h"

#include <string>
#include <cstring>
#include <cstdint>

namespace panda::verifier::debug {

using panda::Method;
using panda::parser::action;
using panda::parser::parser;
using panda::verifier::config::Section;
using panda::verifier::debug::AllowlistKind;

const auto &AllowlistMethodParser()
{
    struct Allowlist;

    using panda::parser::charset;
    using p = parser<PandaString, const char, const char *>::next<Allowlist>;
    using p1 = p::p;

    static const auto WS = p::of_charset(" \t");

    static const auto METHOD_NAME_HANDLER = [](action a, PandaString &c, auto from, auto to) {
        if (a == action::PARSED) {
            c = PandaString {from, to};
        }
        return true;
    };

    static const auto METHOD_NAME = p1::of_charset(!charset {" \t,"}) |= METHOD_NAME_HANDLER;  // NOLINT

    static const auto ALLOWLIST_METHOD = ~WS >> METHOD_NAME >> ~WS >> p::end() | ~WS >> p::end();

    return ALLOWLIST_METHOD;
}

void RegisterConfigHandlerAllowlist()
{
    static const auto CONFIG_DEBUG_ALLOWLIST_VERIFIER = [](const Section &section) {
        for (const auto &s : section.sections) {
            AllowlistKind kind;
            if (s.name == "class") {
                kind = AllowlistKind::CLASS;
            } else if (s.name == "method") {
                kind = AllowlistKind::METHOD;
            } else if (s.name == "method_call") {
                kind = AllowlistKind::METHOD_CALL;
            } else {
                LOG(DEBUG, VERIFIER) << "Wrong debug verifier allowlist section: '" << s.name << "'";
                return false;
            }
            for (const auto &i : s.items) {
                PandaString c;
                const char *start = i.c_str();
                const char *end = i.c_str() + i.length();  // NOLINT
                if (!AllowlistMethodParser()(c, start, end)) {
                    LOG(DEBUG, VERIFIER) << "Wrong allowlist line: '" << i << "'";
                    return false;
                }
                if (!c.empty()) {
                    uint32_t hash;
                    if (kind == AllowlistKind::CLASS) {
                        hash = Method::GetClassNameHashFromString(reinterpret_cast<const uint8_t *>(c.c_str()));
                        LOG(DEBUG, VERIFIER) << "Added to allowlist config '" << s.name << "' methods from class '" << c
                                             << "', hash 0x" << std::hex << hash;
                    } else {
                        hash = Method::GetFullNameHashFromString(reinterpret_cast<const uint8_t *>(c.c_str()));
                        LOG(DEBUG, VERIFIER) << "Added to allowlist config '" << s.name << "' method '" << c
                                             << "', hash 0x" << std::hex << hash;
                    }
                    AddAllowlistMethodConfig(kind, hash);
                }
            }
        }
        return true;
    };

    config::RegisterConfigHandler("config.debug.allowlist.verifier", CONFIG_DEBUG_ALLOWLIST_VERIFIER);
}

}  // namespace panda::verifier::debug
