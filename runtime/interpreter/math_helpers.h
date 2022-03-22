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

#ifndef PANDA_RUNTIME_INTERPRETER_MATH_HELPERS_H_
#define PANDA_RUNTIME_INTERPRETER_MATH_HELPERS_H_

#include <cmath>
#include <functional>
#include <limits>
#include <type_traits>

#include "libpandabase/macros.h"

namespace panda::interpreter::math_helpers {

template <typename T>
struct bit_shl : public std::binary_function<T, T, T> {  // NOLINT(readability-identifier-naming)
    constexpr T operator()(const T &x, const T &y) const
    {
        using unsigned_type = std::make_unsigned_t<T>;
        size_t mask = std::numeric_limits<unsigned_type>::digits - 1;
        size_t shift = static_cast<unsigned_type>(y) & mask;
        return static_cast<T>(static_cast<unsigned_type>(x) << shift);
    }
};

template <typename T>
struct bit_shr : public std::binary_function<T, T, T> {  // NOLINT(readability-identifier-naming)
    constexpr T operator()(const T &x, const T &y) const
    {
        using unsigned_type = std::make_unsigned_t<T>;
        size_t mask = std::numeric_limits<unsigned_type>::digits - 1;
        size_t shift = static_cast<unsigned_type>(y) & mask;
        return static_cast<T>(static_cast<unsigned_type>(x) >> shift);
    }
};

template <typename T>
struct bit_ashr : public std::binary_function<T, T, T> {  // NOLINT(readability-identifier-naming)
    constexpr T operator()(const T &x, const T &y) const
    {
        using unsigned_type = std::make_unsigned_t<T>;
        size_t mask = std::numeric_limits<unsigned_type>::digits - 1;
        size_t shift = static_cast<unsigned_type>(y) & mask;
        return x >> shift;  // NOLINT(hicpp-signed-bitwise)
    }
};

template <typename T>
struct fmodulus : public std::binary_function<T, T, T> {  // NOLINT(readability-identifier-naming)
    constexpr T operator()(const T &x, const T &y) const
    {
        static_assert(std::is_floating_point_v<T>, "T should be floating point type");
        return std::fmod(x, y);
    }
};

template <typename T>
struct cmp : public std::binary_function<T, T, int32_t> {  // NOLINT(readability-identifier-naming)
    constexpr int32_t operator()(const T &x, const T &y) const
    {
        static_assert(std::is_integral_v<T>, "T should be integral type");

        if (x > y) {
            return 1;
        }

        if (x == y) {
            return 0;
        }

        return -1;
    }
};

template <typename T>
struct fcmpl : public std::binary_function<T, T, int32_t> {  // NOLINT(readability-identifier-naming)
    constexpr int32_t operator()(const T &x, const T &y) const
    {
        static_assert(std::is_floating_point_v<T>, "T should be floating point type");

        if (std::isnan(x) || std::isnan(y)) {
            return -1;  // this is the difference between fcmpl and fcmpg for NAN
        }

        if (x < y) {
            return -1;
        }

        if (x > y) {
            return 1;
        }

        return 0;
    }
};

template <typename T>
struct fcmpg : public std::binary_function<T, T, int32_t> {  // NOLINT(readability-identifier-naming)
    constexpr int32_t operator()(const T &x, const T &y) const
    {
        static_assert(std::is_floating_point_v<T>, "T should be floating point type");

        if (std::isnan(x) || std::isnan(y)) {
            return 1;  // this is the difference between fcmpl and fcmpg for NAN
        }

        if (x < y) {
            return -1;
        }

        if (x > y) {
            return 1;
        }

        return 0;
    }
};

template <typename T>
struct idivides : public std::binary_function<T, T, T> {  // NOLINT(readability-identifier-naming)
    constexpr T operator()(const T &x, const T &y) const
    {
        static_assert(std::is_integral_v<T>, "T should be integral type");

        // Disable checks due to clang-tidy bug https://bugs.llvm.org/show_bug.cgi?id=32203
        // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
        if constexpr (std::is_signed_v<T>) {
            constexpr T MIN = std::numeric_limits<T>::min();

            if (UNLIKELY(x == MIN && y == -1)) {
                return MIN;
            }
        }

        return x / y;
    }
};

template <typename T>
struct imodulus : public std::binary_function<T, T, T> {  // NOLINT(readability-identifier-naming)
    constexpr T operator()(const T &x, const T &y) const
    {
        static_assert(std::is_integral_v<T>, "T should be integral type");

        // Disable checks due to clang-tidy bug https://bugs.llvm.org/show_bug.cgi?id=32203
        // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
        if constexpr (std::is_signed_v<T>) {
            constexpr T MIN = std::numeric_limits<T>::min();

            if (UNLIKELY(x == MIN && y == -1)) {
                return 0;
            }
        }

        return x % y;
    }
};

template <template <typename OpT> class Op, typename T>
ALWAYS_INLINE static inline T SafeMath(T a, T b)
{
    using unsigned_T = std::make_unsigned_t<T>;
    static_assert(std::is_signed<T>::value, "Expected T to be signed");
    auto val1 = static_cast<unsigned_T>(a);
    auto val2 = static_cast<unsigned_T>(b);
    return static_cast<T>(Op<unsigned_T>()(val1, val2));
}

template <typename T>
struct Plus {
    ALWAYS_INLINE inline T operator()(T a, T b)
    {
        return SafeMath<std::plus>(a, b);
    }
};

template <typename T>
struct Minus {
    ALWAYS_INLINE inline T operator()(T a, T b)
    {
        return SafeMath<std::minus>(a, b);
    }
};

template <typename T>
struct Multiplies {
    ALWAYS_INLINE inline T operator()(T a, T b)
    {
        return SafeMath<std::multiplies>(a, b);
    }
};

template <typename T>
struct inc : public std::unary_function<T, T> {  // NOLINT(readability-identifier-naming)
    constexpr T operator()(const T &x) const
    {
        return SafeMath<std::plus>(x, 1);
    }
};

template <typename T>
struct dec : public std::unary_function<T, T> {  // NOLINT(readability-identifier-naming)
    constexpr T operator()(const T &x) const
    {
        return SafeMath<std::minus>(x, 1);
    }
};

}  // namespace panda::interpreter::math_helpers

#endif  // PANDA_RUNTIME_INTERPRETER_MATH_HELPERS_H_
