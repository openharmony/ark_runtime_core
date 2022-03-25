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

#ifndef PANDA_VERIFICATION_DEBUG_OPTIONS_METHOD_GROUP_PARSER_H_
#define PANDA_VERIFICATION_DEBUG_OPTIONS_METHOD_GROUP_PARSER_H_

#include "verification/debug/parser/parser.h"

template <typename Parser, typename Str, typename RegexHandler>
const auto &MethodGroupParser(RegexHandler &regex_handler)
{
    using panda::parser::action;
    using panda::parser::charset;
    using panda::parser::parser;

    struct MethodGroup;

    using p = typename Parser::template next<MethodGroup>;
    using p1 = typename p::p;

    static const auto QUOTE = p::of_string("'");
    static const auto NON_QUOTES = p1::of_charset(!charset("'"));

    static const auto REGEX_HANDLER = [&](action a, typename p::Ctx &c, auto from, auto to) {
        if (a == action::PARSED) {
            auto *start = from;
            ++start;
            auto *end = to;
            --end;
            return regex_handler(c, Str {start, end});
        }
        return true;
    };

    static const auto REGEX = (((QUOTE >> NON_QUOTES) >> QUOTE) |= REGEX_HANDLER);

    return REGEX;
}

#endif  // PANDA_VERIFICATION_DEBUG_OPTIONS_METHOD_GROUP_PARSER_H_
