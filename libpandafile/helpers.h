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

#ifndef PANDA_LIBPANDAFILE_HELPERS_H_
#define PANDA_LIBPANDAFILE_HELPERS_H_

#include "macros.h"
#include "utils/bit_helpers.h"
#include "utils/leb128.h"
#include "utils/span.h"

#include <cstdint>

#include <limits>
#include <optional>

namespace panda::panda_file::helpers {

template <size_t width>
inline auto Read(Span<const uint8_t> *sp)
{
    constexpr size_t BYTE_WIDTH = std::numeric_limits<uint8_t>::digits;
    constexpr size_t BITWIDTH = BYTE_WIDTH * width;
    using unsigned_type = panda::helpers::TypeHelperT<BITWIDTH, false>;

    unsigned_type result = 0;
    for (size_t i = 0; i < width; i++) {
        unsigned_type tmp = static_cast<unsigned_type>((*sp)[i]) << (i * BYTE_WIDTH);
        result |= tmp;
    }
    *sp = sp->SubSpan(width);
    return result;
}

template <size_t width>
inline auto Read(Span<const uint8_t> sp)
{
    return Read<width>(&sp);
}

inline uint32_t ReadULeb128(Span<const uint8_t> *sp)
{
    uint32_t result;
    size_t n;
    [[maybe_unused]] bool is_full;
    std::tie(result, n, is_full) = leb128::DecodeUnsigned<uint32_t>(sp->data());
    ASSERT(is_full);
    *sp = sp->SubSpan(n);
    return result;
}

inline int32_t ReadLeb128(Span<const uint8_t> *sp)
{
    uint32_t result;
    size_t n;
    [[maybe_unused]] bool is_full;
    std::tie(result, n, is_full) = leb128::DecodeSigned<int32_t>(sp->data());
    ASSERT(is_full);
    *sp = sp->SubSpan(n);
    return result;
}

template <size_t alignment>
inline const uint8_t *Align(const uint8_t *ptr)
{
    auto intptr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t aligned = (intptr - 1U + alignment) & -alignment;
    return reinterpret_cast<const uint8_t *>(aligned);
}

template <size_t alignment, class T>
inline T Align(T n)
{
    return (n - 1U + alignment) & -alignment;
}

template <class T, class E>
inline std::optional<T> GetOptionalTaggedValue(Span<const uint8_t> sp, E tag, Span<const uint8_t> *next)
{
    if (sp[0] == static_cast<uint8_t>(tag)) {
        sp = sp.SubSpan(1);
        T value = static_cast<T>(Read<sizeof(T)>(&sp));
        *next = sp;
        return value;
    }
    *next = sp;

    // NB! This is a workaround for https://gcc.gnu.org/bugzilla/show_bug.cgi?id=80635
    // which fails Release builds for GCC 8 and 9.
    std::optional<T> novalue = {};
    return novalue;
}

template <class T, class E, class Callback>
inline void EnumerateTaggedValues(Span<const uint8_t> sp, E tag, Callback cb, Span<const uint8_t> *next)
{
    while (sp[0] == static_cast<uint8_t>(tag)) {
        sp = sp.SubSpan(1);
        T value(Read<sizeof(T)>(&sp));
        cb(value);
    }

    if (next == nullptr) {
        return;
    }

    *next = sp;
}

}  // namespace panda::panda_file::helpers

#endif  // PANDA_LIBPANDAFILE_HELPERS_H_
