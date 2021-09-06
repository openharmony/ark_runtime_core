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

#ifndef PANDA_LIBPANDABASE_UTILS_UTF_H_
#define PANDA_LIBPANDABASE_UTILS_UTF_H_

#include <cstdint>
#include <cstddef>

#include "utils/hash.h"
#include "utils/span.h"

namespace panda::utf {

/*
 * N  Bits for     First        Last        Byte 1      Byte 2      Byte 3      Byte 4
 *    code point   code point   code point
 * 1  7            U+0000       U+007F      0xxxxxxx
 * 2  11           U+0080       U+07FF      110xxxxx    10xxxxxx
 * 3  16           U+0800       U+FFFF      1110xxxx    10xxxxxx    10xxxxxx
 * 4  21           U+10000      U+10FFFF    11110xxx    10xxxxxx    10xxxxxx    10xxxxxx
 */
constexpr size_t MASK1 = 0x80;
constexpr size_t MASK2 = 0x20;
constexpr size_t MASK3 = 0x10;

constexpr size_t MASK_4BIT = 0x0f;
constexpr size_t MASK_5BIT = 0x1f;
constexpr size_t MASK_6BIT = 0x3f;
constexpr size_t MASK_10BIT = 0x03ff;
constexpr size_t MASK_16BIT = 0xffff;

constexpr size_t DATA_WIDTH = 6;
constexpr size_t PAIR_ELEMENT_WIDTH = 16;

constexpr size_t HI_SURROGATE_MIN = 0xd800;
constexpr size_t HI_SURROGATE_MAX = 0xdbff;
constexpr size_t LO_SURROGATE_MIN = 0xdc00;
constexpr size_t LO_SURROGATE_MAX = 0xdfff;

constexpr size_t LO_SUPPLEMENTS_MIN = 0x10000;

constexpr size_t U16_LEAD = 0xd7c0;
constexpr size_t U16_TAIL = 0xdc00;

constexpr uint8_t MUTF8_1B_MAX = 0x7f;

constexpr uint16_t MUTF8_2B_MAX = 0x7ff;
constexpr uint8_t MUTF8_2B_FIRST = 0xc0;
constexpr uint8_t MUTF8_2B_SECOND = 0x80;

constexpr uint8_t MUTF8_3B_FIRST = 0xe0;
constexpr uint8_t MUTF8_3B_SECOND = 0x80;
constexpr uint8_t MUTF8_3B_THIRD = 0x80;

constexpr uint8_t MUTF8_4B_FIRST = 0xf0;

std::pair<uint32_t, size_t> ConvertMUtf8ToUtf16Pair(const uint8_t *data, size_t max_bytes = 4);

bool IsMUtf8OnlySingleBytes(const uint8_t *mutf8_in);

void ConvertMUtf8ToUtf16(const uint8_t *mutf8_in, size_t mutf8_len, uint16_t *utf16_out);

size_t ConvertRegionMUtf8ToUtf16(const uint8_t *mutf8_in, uint16_t *utf16_out, size_t mutf8_len, size_t utf16_len,
                                 size_t start);

size_t ConvertRegionUtf16ToMUtf8(const uint16_t *utf16_in, uint8_t *mutf8_out, size_t utf16_len, size_t mutf8_len,
                                 size_t start);

int CompareMUtf8ToMUtf8(const uint8_t *mutf8_1, const uint8_t *mutf8_2);

int CompareUtf8ToUtf8(const uint8_t *utf8_1, size_t utf8_1_length, const uint8_t *utf8_2, size_t utf8_2_length);

bool IsEqual(Span<const uint8_t> utf8_1, Span<const uint8_t> utf8_2);

bool IsEqual(const uint8_t *mutf8_1, const uint8_t *mutf8_2);

size_t MUtf8ToUtf16Size(const uint8_t *mutf8);

size_t MUtf8ToUtf16Size(const uint8_t *mutf8, size_t mutf8_len);

size_t Utf16ToMUtf8Size(const uint16_t *mutf16, uint32_t length);

size_t Mutf8Size(const uint8_t *mutf8);

inline const uint8_t *CStringAsMutf8(const char *str)
{
    return reinterpret_cast<const uint8_t *>(str);
}

inline const char *Mutf8AsCString(const uint8_t *mutf8)
{
    return reinterpret_cast<const char *>(mutf8);
}

inline constexpr bool IsAvailableNextUtf16Code(uint16_t val)
{
    return val >= HI_SURROGATE_MIN && val <= LO_SURROGATE_MAX;
}

struct Mutf8Hash {
    uint32_t operator()(const uint8_t *data) const
    {
        return GetHash32String(data);
    }
};

struct Mutf8Equal {
    bool operator()(const uint8_t *mutf8_1, const uint8_t *mutf8_2) const
    {
        return IsEqual(mutf8_1, mutf8_2);
    }
};

struct Mutf8Less {
    bool operator()(const uint8_t *mutf8_1, const uint8_t *mutf8_2) const
    {
        return CompareMUtf8ToMUtf8(mutf8_1, mutf8_2) < 0;
    }
};

static inline std::pair<uint16_t, uint16_t> SplitUtf16Pair(uint32_t pair)
{
    constexpr size_t P1_MASK = 0xffff;
    constexpr size_t P2_SHIFT = 16;
    return {pair >> P2_SHIFT, pair & P1_MASK};
}

}  // namespace panda::utf

#endif  // PANDA_LIBPANDABASE_UTILS_UTF_H_
