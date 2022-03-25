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

#include "config_parse.h"
#include "verification/debug/parser/parser.h"
#include "runtime/include/mem/panda_string.h"

#include <vector>

namespace panda::verifier::config {

using panda::parser::action;
using panda::parser::parser;
using panda::verifier::config::Section;

namespace {

struct Context {
    Section current;
    std::vector<Section> sections;
};

}  // namespace

using p = panda::parser::parser<Context, const char, const char *>;

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_SIZE)
bool ParseConfig(const char *str, Section &cfg)
{
    using panda::parser::charset;
    using p1 = p::p;
    using p2 = p1::p;
    using p3 = p2::p;
    using p4 = p3::p;
    using p5 = p4::p;
    using p6 = p5::p;

    static const auto WS = p::of_charset(" \t\r\n");
    static const auto NL = p1::of_charset("\r\n");
    static const auto SP = p2::of_charset(" \t");
    static const auto NAME_HANDLER = [](auto a, Context &c, auto from, auto to) {
        if (a == action::PARSED) {
            c.current.name = PandaString {from, to};
        }
        return true;
    };
    static const auto NAME = p3::of_charset("abcdefghijklmnopqrstuvwxyz_") |= NAME_HANDLER;

    static const auto LCURL = p4::of_string("{");
    static const auto RCURL = p5::of_string("}");

    static const auto LINE_HANDLER = [](auto a, Context &c, auto from, auto to) {
        if (a == action::PARSED) {
            c.current.items.push_back(PandaString {from, to});
        }
        return true;
    };

    static const auto LINE = p6::of_charset(!charset {"\r\n"}) |= LINE_HANDLER;

    static const auto SECTION_END = ~SP >> RCURL >> ~SP >> NL;
    static const auto SECTION_START = ~SP >> NAME >> ~SP >> LCURL >> ~SP >> NL;
    static const auto ITEM = (!SECTION_END) & (~SP >> LINE >> NL);

    static const auto SECTION_HANDLER = [](auto a, Context &c) {
        if (a == action::START) {
            c.sections.push_back(c.current);
            c.current.sections.clear();
        }
        if (a == action::CANCEL) {
            c.current = c.sections.back();
            c.sections.pop_back();
        }
        if (a == action::PARSED) {
            c.sections.back().sections.push_back(c.current);
            c.current = c.sections.back();
            c.sections.pop_back();
        }
        return true;
    };

    static p::p section_rec;

    static const auto SECTION = ~WS >> SECTION_START >> ~WS >> *section_rec >> *ITEM >> SECTION_END >> ~WS |=
        SECTION_HANDLER;  // NOLINT

    section_rec = SECTION;

    Context context;

    context.current.name = "config";

    if (SECTION(context, str, &str[std::strlen(str)])) {  // NOLINT
        cfg = context.current;
        return true;
    }

    return false;
}

}  // namespace panda::verifier::config
