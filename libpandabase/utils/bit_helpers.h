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

#ifndef PANDA_LIBPANDABASE_UTILS_BIT_HELPERS_H_
#define PANDA_LIBPANDABASE_UTILS_BIT_HELPERS_H_

#include "macros.h"

#include <cstddef>
#include <cstdint>

#include <limits>
#include <type_traits>

namespace panda::helpers {

template <size_t width>
struct UnsignedTypeHelper {
    using type = std::conditional_t<
        width <= std::numeric_limits<uint8_t>::digits, uint8_t,
        std::conditional_t<
            width <= std::numeric_limits<uint16_t>::digits, uint16_t,
            std::conditional_t<width <= std::numeric_limits<uint32_t>::digits, uint32_t,
                               std::conditional_t<width <= std::numeric_limits<uint64_t>::digits, uint64_t, void>>>>;
};

template <size_t width>
using UnsignedTypeHelperT = typename UnsignedTypeHelper<width>::type;

template <size_t width, bool is_signed>
struct TypeHelper {
    using type =
        std::conditional_t<is_signed, std::make_signed_t<UnsignedTypeHelperT<width>>, UnsignedTypeHelperT<width>>;
};

template <size_t width, bool is_signed>
using TypeHelperT = typename TypeHelper<width, is_signed>::type;

}  // namespace panda::helpers

#endif  // PANDA_LIBPANDABASE_UTILS_BIT_HELPERS_H_
