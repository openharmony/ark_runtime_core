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

#ifndef PANDA_VERIFICATION_UTIL_SET_OPERATIONS_H_
#define PANDA_VERIFICATION_UTIL_SET_OPERATIONS_H_

#include <cstddef>
#include "function_traits.h"

namespace panda::verifier {
using std::size_t;

template <size_t N, class... Args>
struct pack_element {
    using type = typename std::tuple_element<N, std::tuple<Args...>>::type;
};

template <size_t N, class... Args>
using pack_element_t = typename pack_element<N, Args...>::type;

template <typename... Args>
pack_element_t<0, Args...> SetIntersection(const Args &... args)
{
    using S = pack_element_t<0, Args...>;
    using elt_type = typename S::key_type;
    using iter_type = typename S::const_iterator;
    std::array<iter_type, sizeof...(Args)> iters {args.cbegin()...};
    std::array<iter_type, sizeof...(Args)> ends {args.cend()...};
    auto step = [&iters, &ends](bool &aligned, elt_type &val) {
        size_t min = 0;
        aligned = true;
        for (size_t idx = 0; idx < iters.size(); ++idx) {
            if (iters[idx] == ends[idx]) {
                return false;
            }
            aligned = aligned && (*iters[idx] == *iters[min]);
            if (*iters[idx] < *iters[min]) {
                min = idx;
            }
        }
        if (aligned) {
            val = *iters[min];
        }
        ++iters[min];
        return true;
    };
    bool store = false;
    elt_type val;
    S result;
    while (step(store, val)) {
        if (store) {
            result.insert(val);
        }
    }
    return result;
}

template <typename... Args>
pack_element_t<0, Args...> SetUnion(const Args &... args)
{
    using S = pack_element_t<0, Args...>;
    auto un = [](const S &lhs, const S &rhs) -> S {
        S result = lhs;
        result.insert(rhs.cbegin(), rhs.cend());
        return result;
    };
    return n_ary {un}(args...);
}

template <typename S>
S SetDifference(const S &lhs, const S &rhs)
{
    S intersection = SetIntersection(lhs, rhs);
    S result;
    for (const auto &elt : lhs) {
        if (rhs.count(elt) == 0) {
            result.insert(elt);
        }
    }
    return result;
}

template <typename Arg, typename... Args>
Arg SetDifference(const Arg &arg, const Args &... args)
{
    Arg sum = SetUnion(args...);
    Arg intersection = SetIntersection(arg, sum);
    Arg result;
    for (const auto &elt : arg) {
        if (intersection.count(elt) == 0) {
            result.insert(elt);
        }
    }
    return result;
}

template <typename S, typename C>
S ToSet(const C &c)
{
    S result;
    result.insert(c.cbegin(), c.cend());
    return result;
}
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_UTIL_SET_OPERATIONS_H_
