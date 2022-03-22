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

#ifndef PANDA_VERIFICATION_UTIL_MISC_H_
#define PANDA_VERIFICATION_UTIL_MISC_H_

#include <functional>
#include <cstddef>
#include <tuple>

namespace std {

template <typename T1, typename T2>
struct hash<std::pair<T1, T2>> {
    size_t operator()(const std::pair<T1, T2> &pair) const
    {
        return std::hash<T1> {}(pair.first) ^ std::hash<T2> {}(pair.second);
    }
};

template <class T, class Tuple>
struct tuple_type_index;

template <class T, class... Types>
struct tuple_type_index<T, tuple<T, Types...>> {
    static constexpr size_t value = 0;
};

template <class T, class U, class... Types>
struct tuple_type_index<T, tuple<U, Types...>> {
    static constexpr size_t value = 1 + tuple_type_index<T, tuple<Types...>>::value;
};

}  // namespace std

#endif  // PANDA_VERIFICATION_UTIL_MISC_H_
