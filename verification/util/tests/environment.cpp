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

#include "util/tests/environment.h"

#include "verification/debug/parser/parser.h"

#include <cstring>

namespace panda::verifier::test {

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_SIZE)
EnvOptions::EnvOptions(const char *env_var_name)
{
    using panda::parser::action;
    using panda::parser::charset;
    using panda::parser::parser;

    struct Context {
        std::string name;
        OptionValue value;
    };

    using p = parser<Context, const char, const char *>::next<EnvOptions>;

    static const auto WS = p::of_charset(" \t\r\n");  // NOLINT(readability-static-accessed-through-instance)
    static const auto DELIM = p::of_string(";");      // NOLINT(readability-static-accessed-through-instance)
    static const auto NAME_HANDLER = [](auto a, Context &c, auto s, auto e, [[maybe_unused]] auto end) {
        if (a == action::PARSED) {
            c.name = std::string {s, e};
        }
        return true;
    };
    static const auto NAME =
        WS.of_charset("abcdefghijklmnopqrstuvwxyz_")  // NOLINT(readability-static-accessed-through-instance)
        |= NAME_HANDLER;
    static const auto EQ = NAME.of_string("=");          // NOLINT(readability-static-accessed-through-instance)
    static const auto BOOL_TRUE = EQ.of_string("true");  // NOLINT(readability-static-accessed-through-instance)
    static const auto BOOL_FALSE =
        BOOL_TRUE.of_string("false");  // NOLINT(readability-static-accessed-through-instance)
    static const auto BOOL_HANDLER = [](auto a, Context &c, auto s, [[maybe_unused]] auto to,
                                        [[maybe_unused]] auto end) {
        if (a == action::PARSED) {
            if (*s == 'f') {
                c.value = false;
            } else {
                c.value = true;
            }
        }
        return true;
    };
    static const auto BOOL = BOOL_FALSE | BOOL_TRUE |= BOOL_HANDLER;
    static const auto DEC = BOOL.of_charset("0123456789");  // NOLINT(readability-static-accessed-through-instance)
    static const auto HEX = DEC.of_string("0x") >> DEC;     // NOLINT(readability-static-accessed-through-instance)
    static const auto NUM_HANDLER = [](auto a, Context &c, auto s, auto e, [[maybe_unused]] auto end) {
        if (a == action::PARSED) {
            c.value = std::stoi(std::string {s, e}, nullptr, 0);
        }
        return true;
    };
    static const auto NUM = HEX | DEC |= NUM_HANDLER;
    static const auto QUOTES = HEX.of_string("\"");  // NOLINT(readability-static-accessed-through-instance)
    static const auto NON_QUOTES =
        QUOTES.of_charset(!charset("\""));  // NOLINT(readability-static-accessed-through-instance)
    static const auto STRING_HANDLER = [](auto a, Context &c, auto s, auto e, [[maybe_unused]] auto end) {
        if (a == action::PARSED) {
            c.value = std::string {s, e};
        }
        return true;
    };
    static const auto STRING = QUOTES >> (*NON_QUOTES |= STRING_HANDLER) >> QUOTES;

    static const auto VALUE = STRING | NUM | BOOL;

    static const auto KV_PAIR_HANDLER = [this](auto a, Context &c, [[maybe_unused]] auto f, [[maybe_unused]] auto t,
                                               [[maybe_unused]] auto e) {
        if (a == action::PARSED) {
            Options_[c.name] = c.value;
        }
        return true;
    };

    static const auto KV_PAIR = ~WS >> NAME >> ~WS >> EQ >> ~WS >> VALUE >> ~WS >> DELIM |= KV_PAIR_HANDLER;
    static const auto OPTIONS = *KV_PAIR;

    const char *s = std::getenv(env_var_name);
    if (s == nullptr) {
        return;
    }

    Context c;

    OPTIONS(c, s, s + strlen(s));
}

std::optional<OptionValue> EnvOptions::operator[](const std::string &name) const
{
    auto it = Options_.find(name);
    if (it != Options_.end()) {
        return it->second;
    }
    return {};
}

}  // namespace panda::verifier::test
