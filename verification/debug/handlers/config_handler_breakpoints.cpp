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

#include "verifier_messages.h"

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

namespace {

struct Context {
    PandaString Method;
    std::vector<uint32_t> Offsets;
};

}  // namespace

const auto &BreakpointParser()
{
    struct Breakpoint;
    using panda::parser::charset;
    using p = parser<Context, const char, const char *>::next<Breakpoint>;
    using p1 = p::p;
    using p2 = p1::p;
    using p3 = p2::p;
    using p4 = p3::p;
    using p5 = p4::p;

    static const auto WS = p::of_charset(" \t");
    static const auto COMMA = p1::of_string(",");
    static const auto DEC = p2::of_charset("0123456789");
    static const auto HEX = p3::of_charset("0123456789abcdefABCDEF");

    static const auto OFFSET_HANDLER = [](action a, Context &c, auto from) {
        if (a == action::PARSED) {
            c.Offsets.push_back(std::strtol(from, nullptr, 0));
        }
        return true;
    };

    static const auto OFFSET = p4::of_string("0x") >> HEX | DEC |= OFFSET_HANDLER;

    static const auto METHOD_NAME_HANDLER = [](action a, Context &c, auto from, auto to) {
        if (a == action::PARSED) {
            c.Method = PandaString {from, to};
        }
        return true;
    };

    static const auto BREAKPOINT_HANDLER = [](action a, Context &c) {
        if (a == action::START) {
            c.Method.clear();
            c.Offsets.clear();
        }
        return true;
    };

    static const auto METHOD_NAME = p5::of_charset(!charset {" \t,"}) |= METHOD_NAME_HANDLER;
    static const auto BREAKPOINT = ~WS >> METHOD_NAME >> *(~WS >> COMMA >> ~WS >> OFFSET) >> ~WS >> p::end() |
                                   ~WS >> p::end() |= BREAKPOINT_HANDLER;  // NOLINT
    return BREAKPOINT;
}

void RegisterConfigHandlerBreakpoints()
{
    static const auto CONFIG_DEBUG_BREAKPOINTS = [](const Section &section) {
        for (const auto &s : section.sections) {
            if (s.name == "verifier") {
                for (const auto &i : s.items) {
                    Context c;
                    const char *start = i.c_str();
                    const char *end = i.c_str() + i.length();  // NOLINT
                    if (!BreakpointParser()(c, start, end)) {
                        LOG_VERIFIER_DEBUG_BREAKPOINT_WRONG_CFG_LINE(i);
                        return false;
                    }
                    if (!c.Method.empty()) {
                        uint32_t hash =
                            Method::GetFullNameHashFromString(reinterpret_cast<const uint8_t *>(c.Method.c_str()));
                        if (c.Offsets.empty()) {
                            c.Offsets.push_back(0);
                        }
                        for (auto o : c.Offsets) {
                            LOG_VERIFIER_DEBUG_BREAKPOINT_ADDED_INFO(c.Method, hash, o);
                            AddBreakpointConfig({Component::VERIFIER, hash, o});
                        }
                    }
                }
            } else {
                LOG_VERIFIER_DEBUG_BREAKPOINT_WRONG_SECTION(s.name);
                return false;
            }
        }
        return true;
    };

    config::RegisterConfigHandler("config.debug.breakpoints", CONFIG_DEBUG_BREAKPOINTS);
}

}  // namespace panda::verifier::debug
