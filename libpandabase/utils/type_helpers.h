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

#ifndef PANDA_LIBPANDABASE_UTILS_TYPE_HELPERS_H_
#define PANDA_LIBPANDABASE_UTILS_TYPE_HELPERS_H_

namespace panda::helpers {

template <class T>
constexpr auto ToSigned(T v)
{
    using signed_type = std::make_signed_t<T>;
    return static_cast<signed_type>(v);
}

template <class T>
constexpr auto ToUnsigned(T v)
{
    using unsigned_type = std::make_unsigned_t<T>;
    return static_cast<unsigned_type>(v);
}

template <typename T, std::enable_if_t<std::is_enum_v<T>> * = nullptr>
constexpr auto ToUnderlying(T value)
{
    return static_cast<std::underlying_type_t<T>>(value);
}

constexpr size_t UnsignedDifference(size_t x, size_t y)
{
    return x > y ? x - y : 0;
}

constexpr uint64_t UnsignedDifferenceUint64(uint64_t x, uint64_t y)
{
    return x > y ? x - y : 0;
}

}  // namespace panda::helpers

#ifdef __SIZEOF_INT128__
__extension__ using int128 = __int128;
#else
#include <cstdint>
using int128 = struct int128_type {
    constexpr int128_type() = default;
    constexpr explicit int128_type(std::int64_t v) : lo(v) {};
    std::int64_t hi {0};
    std::int64_t lo {0};
    bool operator==(std::int64_t v) const
    {
        return (hi == 0) && (lo == v);
    }
};
static_assert(sizeof(int128) == sizeof(std::int64_t) * 2U);
#endif

#endif  // PANDA_LIBPANDABASE_UTILS_TYPE_HELPERS_H_
