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

#ifndef PANDA_VERIFICATION_DEBUG_PARSER_PARSER_H_
#define PANDA_VERIFICATION_DEBUG_PARSER_PARSER_H_

#include "charset.h"
#include "verification/util/callable.h"

namespace panda::parser {

template <typename...>
struct type_sum;

template <typename...>
struct type_prod;

struct op_next;
struct op_end;
struct op_optional;
struct op_action;
struct op_sequence;
struct op_or;
struct op_and;
struct op_lookup;
struct op_not;
struct op_times;
struct op_times_ref;

template <typename A, typename B, typename C>
struct if_type {
};

template <typename A, typename C>
struct if_type<A, A, C> {
    using type = C;
};

template <typename T>
T &ref_to();

template <typename T>
T val_of();

enum class action { START, PARSED, CANCEL };

template <typename Context, typename Type, typename Char, typename Iter>
struct base_parser : public verifier::callable<bool(Context &, Iter &, Iter)> {
    template <typename F>
    constexpr base_parser(const F &f) : verifier::callable<bool(Context &, Iter &, Iter)> {f}
    {
    }

    using Ctx = Context;
    using T = Type;

    base_parser() = default;

    template <typename NType>
    using next = base_parser<Context, type_sum<NType, T>, Char, Iter>;

    using p = base_parser<Context, type_sum<op_next, T>, Char, Iter>;

    static next<charset<Char>> of_charset(const charset<Char> &c)
    {
        static const auto l = [c](Context &, Iter &start, Iter end) {
            Iter s = start;
            while (s != end && c(*s)) {
                s++;
            }
            bool result = (s != start);
            if (result) {
                start = s;
            }
            return result;
        };
        return l;
    }

    static next<Char *> of_string(Char *str)
    {
        static const auto l = [=](Context &, Iter &start, Iter end) {
            Iter s = start;
            Iter c = str;
            while (s != end && *c != 0 && *c == *s) {
                ++c;
                ++s;
            }
            bool result = (*c == 0);
            if (result) {
                start = s;
            }
            return result;
        };
        return l;
    }

    static next<op_end> end()
    {
        static const auto l = [](Context &, Iter &start, Iter end) { return start == end; };
        return l;
    }

    next<op_optional> operator~() &&
    {
        static const auto l = [p = *this](Context &c, Iter &start, Iter end) {
            p(c, start, end);
            return true;
        };
        return l;
    }

    next<op_optional> operator~() const &
    {
        static const auto l = [this](Context &c, Iter &start, Iter end) {
            (*this)(c, start, end);
            return true;
        };
        return l;
    }

    template <typename F>
    auto operator|=(F f) const ->
        typename if_type<decltype(f(action::START, ref_to<Context>(), ref_to<Iter>(), val_of<Iter>(), val_of<Iter>())),
                         bool, next<type_prod<op_action, F>>>::type
    {
        static const auto l = [p = *this, f](Context &c, Iter &start, Iter end) {
            auto saved = start;
            if (!f(action::START, c, start, start, end)) {
                start = saved;
                return false;
            }
            bool result = p(c, start, end);
            if (!result) {
                f(action::CANCEL, c, saved, start, end);
                start = saved;
                return false;
            }
            if (!f(action::PARSED, c, saved, start, end)) {
                start = saved;
                return false;
            }
            return true;
        };
        return l;
    }

    template <typename F>
    auto operator|=(F f) const ->
        typename if_type<decltype(f(action::START, ref_to<Context>(), ref_to<Iter>(), val_of<Iter>())), bool,
                         next<type_prod<op_action, F>>>::type
    {
        static const auto l = [p = *this, f](Context &c, Iter &start, Iter end) {
            auto saved = start;
            if (!f(action::START, c, start, start)) {
                start = saved;
                return false;
            }
            bool result = p(c, start, end);
            if (!result) {
                f(action::CANCEL, c, saved, start);
                start = saved;
                return false;
            }
            if (!f(action::PARSED, c, saved, start)) {
                start = saved;
                return false;
            }
            return true;
        };
        return l;
    }

    template <typename F>
    auto operator|=(F f) const -> typename if_type<decltype(f(action::START, ref_to<Context>(), ref_to<Iter>())), bool,
                                                   next<type_prod<op_action, F>>>::type
    {
        static const auto l = [p = *this, f](Context &c, Iter &start, Iter end) {
            auto saved = start;
            if (!f(action::START, c, start)) {
                start = saved;
                return false;
            }
            bool result = p(c, start, end);
            if (!result) {
                f(action::CANCEL, c, saved);
                start = saved;
                return false;
            }
            if (!f(action::PARSED, c, saved)) {
                start = saved;
                return false;
            }
            return true;
        };
        return l;
    }

    template <typename F>
    auto operator|=(F f) const ->
        typename if_type<decltype(f(action::START, ref_to<Context>())), bool, next<type_prod<op_action, F>>>::type
    {
        static const auto l = [p = *this, f](Context &c, Iter &start, Iter end) {
            auto saved = start;
            if (!f(action::START, c)) {
                start = saved;
                return false;
            }
            bool result = p(c, start, end);
            if (!result) {
                f(action::CANCEL, c);
                start = saved;
                return false;
            }
            if (!f(action::PARSED, c)) {
                start = saved;
                return false;
            }
            return true;
        };
        return l;
    }

    template <typename P>
    next<type_prod<op_sequence, typename P::T>> operator>>(P param_p) const
    {
        static const auto l = [left = *this, right = param_p](Context &c, Iter &start, Iter end) {
            auto saved = start;
            if (left(c, start, end) && right(c, start, end)) {
                return true;
            }
            start = saved;
            return false;
        };
        return l;
    }

    template <typename P>
    next<type_prod<op_or, typename P::T>> operator|(P param_p) const
    {
        static const auto l = [left = *this, right = param_p](Context &c, Iter &start, Iter end) {
            auto saved = start;
            if (left(c, start, end)) {
                return true;
            }
            start = saved;
            if (right(c, start, end)) {
                return true;
            }
            start = saved;
            return false;
        };
        return l;
    }

    template <typename P>
    next<type_prod<op_and, typename P::T>> operator&(P param_p) const
    {
        static const auto l = [left = *this, right = param_p](Context &c, Iter &start, Iter end) {
            auto saved = start;
            if (left(c, start, end)) {
                start = saved;
                return right(c, start, end);
            }
            start = saved;
            return false;
        };
        return l;
    }

    template <typename P>
    next<type_prod<op_lookup, typename P::T>> operator<<(P param_p) const
    {
        static const auto l = [left = *this, right = param_p](Context &c, Iter &start, Iter end) {
            auto saved1 = start;
            if (left(c, start, end)) {
                auto saved2 = start;
                if (right(c, start, end)) {
                    start = saved2;
                    return true;
                }
            }
            start = saved1;
            return false;
        };
        return l;
    }

    next<op_not> operator!() &&
    {
        static const auto l = [p = *this](Context &c, Iter &start, Iter end) {
            auto saved = start;
            if (p(c, start, end)) {
                start = saved;
                return false;
            }
            start = saved;
            return true;
        };
        return l;
    }

    next<op_not> operator!() const &
    {
        static const auto l = [this](Context &c, Iter &start, Iter end) {
            auto saved = start;
            if ((*this)(c, start, end)) {
                start = saved;
                return false;
            }
            start = saved;
            return true;
        };
        return l;
    }

    next<op_times> operator*() &&
    {
        static const auto l = [p = *this](Context &c, Iter &start, Iter end) {
            while (true) {
                auto saved = start;
                if (!p(c, start, end)) {
                    start = saved;
                    break;
                }
            }
            return true;
        };
        return l;
    }

    next<op_times_ref> operator*() const &
    {
        static const auto l = [this](Context &c, Iter &start, Iter end) {
            while (true) {
                auto saved = start;
                if (!(*this)(c, start, end)) {
                    start = saved;
                    break;
                }
            }
            return true;
        };
        return l;
    }
};

struct initial;

template <typename Context, typename Char, typename Iter>
using parser = base_parser<Context, initial, Char, Iter>;

}  // namespace panda::parser

#endif  // PANDA_VERIFICATION_DEBUG_PARSER_PARSER_H_
