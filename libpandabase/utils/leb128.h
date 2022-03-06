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

#ifndef PANDA_LIBPANDABASE_UTILS_LEB128_H_
#define PANDA_LIBPANDABASE_UTILS_LEB128_H_

#include "bit_helpers.h"
#include "bit_utils.h"

#include <limits>
#include <tuple>

namespace panda::leb128 {

constexpr size_t PAYLOAD_WIDTH = 7;
constexpr size_t PAYLOAD_MASK = 0x7f;
constexpr size_t EXTENSION_BIT = 0x80;
constexpr size_t SIGN_BIT = 0x40;

template <class T>
inline std::tuple<T, size_t, bool> DecodeUnsigned(const uint8_t *data)
{
    static_assert(std::is_unsigned_v<T>, "T must be unsigned");

    T result = 0;

    constexpr size_t BITWIDTH = std::numeric_limits<T>::digits;
    constexpr size_t N = (BITWIDTH + PAYLOAD_WIDTH - 1) / PAYLOAD_WIDTH;

    for (size_t i = 0; i < N; i++) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        uint8_t byte = data[i] & PAYLOAD_MASK;
        size_t shift = i * PAYLOAD_WIDTH;
        result |= static_cast<T>(byte) << shift;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if ((data[i] & EXTENSION_BIT) == 0) {
            bool is_full = MinimumBitsToStore(byte) <= (BITWIDTH - shift);
            return {result, i + 1, is_full};
        }
    }

    return {result, N, false};
}

template <class T>
inline std::tuple<T, size_t, bool> DecodeSigned(const uint8_t *data)
{
    static_assert(std::is_signed_v<T>, "T must be signed");

    T result = 0;

    using unsigned_type = std::make_unsigned_t<T>;

    constexpr size_t BITWIDTH = std::numeric_limits<unsigned_type>::digits;
    constexpr size_t N = (BITWIDTH + PAYLOAD_WIDTH - 1) / PAYLOAD_WIDTH;

    for (size_t i = 0; i < N; i++) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        uint8_t byte = data[i];
        size_t shift = i * PAYLOAD_WIDTH;
        // NOLINTNEXTLINE(hicpp-signed-bitwise)
        result |= static_cast<unsigned_type>(byte & PAYLOAD_MASK) << shift;

        if ((byte & EXTENSION_BIT) == 0) {
            shift = BITWIDTH - shift;
            // NOLINTNEXTLINE(hicpp-signed-bitwise)
            int8_t signed_extended = static_cast<int8_t>(byte << 1) >> 1;
            // NOLINTNEXTLINE(hicpp-signed-bitwise)
            uint8_t masked = (signed_extended ^ (signed_extended >> PAYLOAD_WIDTH)) | 1;
            bool is_full = MinimumBitsToStore(masked) <= shift;
            if (shift > PAYLOAD_WIDTH) {
                shift -= PAYLOAD_WIDTH;
                // NOLINTNEXTLINE(hicpp-signed-bitwise)
                result = static_cast<T>(result << shift) >> shift;
            }
            return {result, i + 1, is_full};
        }
    }

    return {result, N, false};
}

template <class T>
inline size_t EncodeUnsigned(T data, uint8_t *out)
{
    static_assert(std::is_unsigned_v<T>, "T must be unsigned");

    size_t i = 0;
    uint8_t byte = data & PAYLOAD_MASK;
    data >>= PAYLOAD_WIDTH;

    while (data != 0) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        out[i++] = byte | EXTENSION_BIT;
        byte = data & PAYLOAD_MASK;
        data >>= PAYLOAD_WIDTH;
    }

    out[i++] = byte;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    return i;
}

template <class T>
inline size_t EncodeSigned(T data, uint8_t *out)
{
    static_assert(std::is_signed_v<T>, "T must be signed");

    size_t i = 0;
    bool more = true;

    while (more) {
        auto byte = static_cast<uint8_t>(static_cast<size_t>(data) & PAYLOAD_MASK);
        // NOLINTNEXTLINE(hicpp-signed-bitwise)
        data >>= PAYLOAD_WIDTH;
        more = !((data == 0 && (byte & SIGN_BIT) == 0) || (data == -1 && (byte & SIGN_BIT) != 0));
        if (more) {
            byte |= EXTENSION_BIT;
        }
        out[i++] = byte;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }

    return i;
}

template <class T>
inline size_t UnsignedEncodingSize(T data)
{
    return (MinimumBitsToStore(data | 1U) + PAYLOAD_WIDTH - 1) / PAYLOAD_WIDTH;
}

template <class T>
inline size_t SignedEncodingSize(T data)
{
    // NOLINTNEXTLINE(hicpp-signed-bitwise)
    data = data ^ (data >> (std::numeric_limits<T>::digits - 1));
    using unsigned_type = std::make_unsigned_t<T>;
    auto udata = static_cast<unsigned_type>(data);
    return MinimumBitsToStore(static_cast<unsigned_type>(udata | 1U)) / PAYLOAD_WIDTH + 1;
}

}  // namespace panda::leb128

#endif  // PANDA_LIBPANDABASE_UTILS_LEB128_H_
