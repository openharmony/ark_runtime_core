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

#ifndef PANDA_LIBPANDABASE_UTILS_STRING_HELPERS_H_
#define PANDA_LIBPANDABASE_UTILS_STRING_HELPERS_H_

#include <securec.h>

#include <cstdarg>

#include <algorithm>
#include <array>
#include <string>
#include <cerrno>
#include <cstdlib>
#include <limits>
#include <type_traits>

namespace panda::helpers::string {

inline std::string Vformat(const char *fmt, va_list args)
{
    static constexpr size_t SIZE = 1024;

    std::string result;
    result.resize(SIZE);

    bool is_truncated = true;
    while (is_truncated) {
        va_list copy_args;
        va_copy(copy_args, args);
        int r = vsnprintf_truncated_s(result.data(), result.size() + 1, fmt, copy_args);
        va_end(copy_args);

        if (r < 0) {
            return "";
        }

        is_truncated = static_cast<size_t>(r) == result.size();
        result.resize(result.size() * 2U);
    }

    result.erase(std::find(result.begin(), result.end(), '\0'), result.end());

    return result;
}

// NOLINTNEXTLINE(cert-dcl50-cpp)
inline std::string Format(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);  // NOLINT(cppcoreguidelines-pro-type-vararg)

    std::string result = Vformat(fmt, args);

    va_end(args);
    return result;
}

template <typename T>
bool ParseInt(const char *str, T *num, T min = std::numeric_limits<T>::min(), T max = std::numeric_limits<T>::max())
{
    static_assert(std::is_signed<T>::value, "it can't parse unsigned types");
    while (isspace(*str)) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        str++;
    }

    constexpr size_t BASE16 = 16;
    constexpr size_t BASE10 = 10;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    int base = (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) ? BASE16 : BASE10;
    errno = 0;
    char *end = nullptr;
    // NOLINTNEXTLINE(google-runtime-int)
    long long int result = strtoll(str, &end, base);
    if (str == end || *end != '\0') {
        errno = EINVAL;
        return false;
    }
    if (result < min || result > max) {
        errno = ERANGE;
        return false;
    }
    if (num != nullptr) {
        *num = static_cast<T>(result);
    }
    return true;
}

template <typename T>
bool ParseInt(const std::string &str, T *num, T min = std::numeric_limits<T>::min(),
              T max = std::numeric_limits<T>::max())
{
    return ParseInt(str.c_str(), num, min, max);
}

}  // namespace panda::helpers::string

#endif  // PANDA_LIBPANDABASE_UTILS_STRING_HELPERS_H_
