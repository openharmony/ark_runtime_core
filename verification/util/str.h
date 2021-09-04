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

#ifndef PANDA_VERIFICATION_UTIL_STR_H_
#define PANDA_VERIFICATION_UTIL_STR_H_

#include "lazy.h"
#include "include/mem/panda_containers.h"

#include <type_traits>

namespace panda::verifier {

template <typename StrT, typename Gen>
StrT Join(Gen gen, StrT delim = {", "})
{
    return FoldLeft(gen, StrT {""}, [need_delim = false, &delim](StrT accum, StrT str) mutable {
        if (need_delim) {
            accum += delim;
        }
        need_delim = true;
        return accum + str;
    });
}

template <typename Str, typename Int, typename = std::enable_if_t<std::is_integral_v<Int>>>
Str NumToStr(Int val, Int Base = 0x0A, int width = -1)
{
    Str result = "";
    bool neg = false;
    if (val < 0) {
        neg = true;
        val = -val;
    }
    do {
        char c = static_cast<char>(val % Base);
        if (c >= 0x0A) {
            c -= 0x0A;
            c += 'a';
        } else {
            c += '0';
        }
        result = Str {c} + result;
        val = val / Base;
    } while (val);
    if (width > 0) {
        while (result.length() < static_cast<size_t>(width - (neg ? 1 : 0))) {
            result = "0" + result;
        }
    }
    if (neg) {
        result = "-" + result;
    }
    return result;
}
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_UTIL_STR_H_
