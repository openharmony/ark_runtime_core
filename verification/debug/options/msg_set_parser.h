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

#ifndef PANDA_VERIFICATION_DEBUG_OPTIONS_MSG_SET_PARSER_H_
#define PANDA_VERIFICATION_DEBUG_OPTIONS_MSG_SET_PARSER_H_

#include "verification/debug/parser/parser.h"
#include "verifier_messages.h"

namespace panda::verifier::debug {

template <typename Context, typename String>
const auto &NameHandler()
{
    using panda::parser::action;

    static const auto NAME_HANDLER = [](action a, Context &c, auto from, auto to) {
        if (a == action::PARSED) {
            auto name = String {from, to};
            size_t num = static_cast<size_t>(panda::verifier::StringToVerifierMessage(name));
            c.stack.push_back(std::make_pair(num, num));
        }
        return true;
    };

    return NAME_HANDLER;
}

template <typename Context>
const auto &NumHandler()
{
    using panda::parser::action;

    static const auto NUM_HANDLER = [](action a, Context &c, auto from) {
        if (a == action::PARSED) {
            size_t num = std::strtol(from, nullptr, 0);
            c.stack.push_back(std::make_pair(num, num));
        }
        return true;
    };

    return NUM_HANDLER;
}

template <typename Context>
const auto &RangeHandler()
{
    using panda::parser::action;

    static const auto RANGE_HANDLER = [](action a, Context &c) {
        if (a == action::PARSED) {
            auto num_end = c.stack.back();
            c.stack.pop_back();
            auto num_start = c.stack.back();
            c.stack.pop_back();

            c.stack.push_back(std::make_pair(num_start.first, num_end.first));
        }
        return true;
    };

    return RANGE_HANDLER;
}

template <typename Context>
const auto &ItemHandler()
{
    using panda::parser::action;

    static const auto ITEM_HANDLER = [](action a, Context &c) {
        if (a == action::START) {
            c.stack.clear();
        }
        if (a == action::PARSED) {
            auto range = c.stack.back();
            c.stack.pop_back();

            for (auto i = range.first; i <= range.second; ++i) {
                c.nums.insert(i);
            }
        }
        return true;
    };

    return ITEM_HANDLER;
}

template <typename Context, typename String>
const auto &MessageSetParser()
{
    using panda::parser::action;
    using panda::parser::charset;
    using panda::parser::parser;

    using p = parser<Context, const char, const char *>;
    using p1 = typename p::p;
    using p2 = typename p1::p;
    using p3 = typename p2::p;
    using p4 = typename p3::p;

    static const auto WS = p::of_charset(" \t\r\n");
    static const auto COMMA = p1::of_charset(",");
    static const auto DEC = p2::of_charset("0123456789");

    static const auto NAME = p3::of_charset("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789") |=
        NameHandler<Context, String>();

    static const auto NUM = DEC |= NumHandler<Context>();

    static const auto MSG = NUM | NAME;

    static const auto RANGE_DELIM = ~WS >> p4::of_string("-") >> ~WS;

    static const auto MSG_RANGE = MSG >> RANGE_DELIM >> MSG |= RangeHandler<Context>();

    static const auto ITEM = (~WS >> MSG_RANGE >> ~WS | ~WS >> MSG >> ~WS |= ItemHandler<Context>()) >> ~COMMA;

    // here should be ITEMS = *ITEM, but due to clang-tidy bug, used lambda instead
    static const auto ITEMS = [](Context &c, const char *&start, const char *end) {
        while (true) {
            auto saved = start;
            if (!ITEM(c, start, end)) {
                start = saved;
                break;
            }
        }
        return true;
    };

    return ITEMS;
}

}  // namespace panda::verifier::debug

#endif  // PANDA_VERIFICATION_DEBUG_OPTIONS_MSG_SET_PARSER_H_
