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

#ifndef PANDA_VERIFICATION_UTIL_ACCESS_H_
#define PANDA_VERIFICATION_UTIL_ACCESS_H_

#include <type_traits>

namespace panda::verifier::access {

struct ReadOnly {
};
struct ReadWrite {
};

template <typename T>
inline constexpr bool if_readonly = std::is_same_v<T, ReadOnly>;

template <typename T>
inline constexpr bool if_readwrite = std::is_same_v<T, ReadWrite>;

template <typename T, typename R>
using enable_if_readonly = std::enable_if_t<std::is_same_v<T, ReadOnly>, R>;

template <typename T, typename R>
using enable_if_readwrite = std::enable_if_t<std::is_same_v<T, ReadWrite>, R>;

#define ACCESS_IS_READONLY_OR_READWRITE(A)                                \
    static_assert(std::is_same_v<A, panda::verifier::access::ReadOnly> || \
                  std::is_same_v<A, panda::verifier::access::ReadWrite>)

}  // namespace panda::verifier::access

#endif  // PANDA_VERIFICATION_UTIL_ACCESS_H_
