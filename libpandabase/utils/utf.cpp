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

#include "utf.h"

#include <cstddef>
#include <cstring>

#include <limits>
#include <tuple>
#include <utility>

namespace panda::utf {

constexpr size_t MAX_U16 = 0xffff;
constexpr size_t CONST_2 = 2;
constexpr size_t CONST_3 = 3;
constexpr size_t CONST_4 = 4;
constexpr size_t CONST_6 = 6;
constexpr size_t CONST_12 = 12;

struct MUtf8Char {
    size_t n;
    std::array<uint8_t, CONST_4> ch;
};

/*
 * MUtf-8
 *
 * U+0000 => C0 80
 *
 * N  Bits for     First        Last        Byte 1      Byte 2      Byte 3      Byte 4      Byte 5      Byte 6
 *    code point   code point   code point
 * 1  7            U+0000       U+007F      0xxxxxxx
 * 2  11           U+0080       U+07FF      110xxxxx    10xxxxxx
 * 3  16           U+0800       U+FFFF      1110xxxx    10xxxxxx    10xxxxxx
 * 6  21           U+10000      U+10FFFF    11101101    1010xxxx    10xxxxxx    11101101    1011xxxx    10xxxxxx
 * for U+10000 -- U+10FFFF encodes the following (value - 0x10000)
 */

/*
 * Convert mutf8 sequence to utf16 pair and return pair: [utf16 code point, mutf8 size].
 * In case of invalid sequence return first byte of it.
 */
std::pair<uint32_t, size_t> ConvertMUtf8ToUtf16Pair(const uint8_t *data, size_t max_bytes)
{
    Span<const uint8_t> sp(data, max_bytes);
    uint8_t d0 = sp[0];
    if ((d0 & MASK1) == 0) {
        return {d0, 1};
    }

    if (max_bytes < CONST_2) {
        return {d0, 1};
    }
    uint8_t d1 = sp[1];
    if ((d0 & MASK2) == 0) {
        return {((d0 & MASK_5BIT) << DATA_WIDTH) | (d1 & MASK_6BIT), 2};
    }

    if (max_bytes < CONST_3) {
        return {d0, 1};
    }
    uint8_t d2 = sp[CONST_2];
    if ((d0 & MASK3) == 0) {
        return {((d0 & MASK_4BIT) << (DATA_WIDTH * CONST_2)) | ((d1 & MASK_6BIT) << DATA_WIDTH) | (d2 & MASK_6BIT),
                CONST_3};
    }

    if (max_bytes < CONST_4) {
        return {d0, 1};
    }
    uint8_t d3 = sp[CONST_3];
    uint32_t code_point = ((d0 & MASK_4BIT) << (DATA_WIDTH * CONST_3)) | ((d1 & MASK_6BIT) << (DATA_WIDTH * CONST_2)) |
                          ((d2 & MASK_6BIT) << DATA_WIDTH) | (d3 & MASK_6BIT);

    uint32_t pair = 0;
    pair |= ((code_point >> (PAIR_ELEMENT_WIDTH - DATA_WIDTH)) + U16_LEAD) & MASK_16BIT;
    pair <<= PAIR_ELEMENT_WIDTH;
    pair |= (code_point & MASK_10BIT) + U16_TAIL;

    return {pair, CONST_4};
}

static constexpr uint32_t CombineTwoU16(uint16_t d0, uint16_t d1)
{
    uint32_t codePoint = d0 - HI_SURROGATE_MIN;
    codePoint <<= (PAIR_ELEMENT_WIDTH - DATA_WIDTH);
    codePoint |= d1 - LO_SURROGATE_MIN;
    codePoint += LO_SUPPLEMENTS_MIN;
    return codePoint;
}

constexpr MUtf8Char ConvertUtf16ToMUtf8(uint16_t d0, uint16_t d1)
{
    // When the first utf16 code is in 0xd800-0xdfff and the second utf16 code is 0,
    // it is a single code point, and it needs to be represented by three MUTF8 code.
    if (d1 == 0 && d0 >= HI_SURROGATE_MIN && d0 <= LO_SURROGATE_MAX) {
        auto ch0 = static_cast<uint8_t>(MUTF8_3B_FIRST | static_cast<uint8_t>(d0 >> CONST_12));
        auto ch1 = static_cast<uint8_t>(MUTF8_3B_SECOND | (static_cast<uint8_t>(d0 >> CONST_6) & MASK_6BIT));
        auto ch2 = static_cast<uint8_t>(MUTF8_3B_THIRD | (d0 & MASK_6BIT));
        return {CONST_3, {ch0, ch1, ch2}};
    }

    if (d0 == 0) {
        return {CONST_2, {MUTF8_2B_FIRST, MUTF8_2B_SECOND}};
    }
    if (d0 <= MUTF8_1B_MAX) {
        return {1, {static_cast<uint8_t>(d0)}};
    }
    if (d0 <= MUTF8_2B_MAX) {
        auto ch0 = static_cast<uint8_t>(MUTF8_2B_FIRST | static_cast<uint8_t>(d0 >> CONST_6));
        auto ch1 = static_cast<uint8_t>(MUTF8_2B_SECOND | (d0 & MASK_6BIT));
        return {CONST_2, {ch0, ch1}};
    }
    if (d0 < HI_SURROGATE_MIN || d0 > HI_SURROGATE_MAX) {
        auto ch0 = static_cast<uint8_t>(MUTF8_3B_FIRST | static_cast<uint8_t>(d0 >> CONST_12));
        auto ch1 = static_cast<uint8_t>(MUTF8_3B_SECOND | (static_cast<uint8_t>(d0 >> CONST_6) & MASK_6BIT));
        auto ch2 = static_cast<uint8_t>(MUTF8_3B_THIRD | (d0 & MASK_6BIT));
        return {CONST_3, {ch0, ch1, ch2}};
    }

    uint32_t codePoint = CombineTwoU16(d0, d1);

    auto ch0 = static_cast<uint8_t>((codePoint >> (DATA_WIDTH * CONST_3)) | MUTF8_4B_FIRST);
    auto ch1 = static_cast<uint8_t>(((codePoint >> (DATA_WIDTH * CONST_2)) & MASK_6BIT) | MASK1);
    auto ch2 = static_cast<uint8_t>(((codePoint >> DATA_WIDTH) & MASK_6BIT) | MASK1);
    auto ch3 = static_cast<uint8_t>((codePoint & MASK_6BIT) | MASK1);

    return {CONST_4, {ch0, ch1, ch2, ch3}};
}

bool IsMUtf8OnlySingleBytes(const uint8_t *mutf8_in)
{
    while (*mutf8_in != '\0') {    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (*mutf8_in >= MASK1) {  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            return false;
        }
        mutf8_in += 1;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }
    return true;
}

size_t ConvertRegionUtf16ToMUtf8(const uint16_t *utf16_in, uint8_t *mutf8_out, size_t utf16_len, size_t mutf8_len,
                                 size_t start)
{
    size_t mutf8_pos = 0;
    if (utf16_in == nullptr || mutf8_out == nullptr || mutf8_len == 0) {
        return 0;
    }
    size_t end = start + utf16_len;
    for (size_t i = start; i < end; ++i) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        uint16_t next16Code = (i + 1) != end && IsAvailableNextUtf16Code(utf16_in[i + 1]) ? utf16_in[i + 1] : 0;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        MUtf8Char ch = ConvertUtf16ToMUtf8(utf16_in[i], next16Code);
        if (mutf8_pos + ch.n > mutf8_len) {
            break;
        }
        for (size_t c = 0; c < ch.n; ++c) {
            mutf8_out[mutf8_pos++] = ch.ch[c];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }
        if (ch.n == CONST_4) {  // Two UTF-16 chars are used
            ++i;
        }
    }
    return mutf8_pos;
}

void ConvertMUtf8ToUtf16(const uint8_t *mutf8_in, size_t mutf8_len, uint16_t *utf16_out)
{
    size_t in_pos = 0;
    while (in_pos < mutf8_len) {
        auto [pair, nbytes] = ConvertMUtf8ToUtf16Pair(mutf8_in, mutf8_len - in_pos);
        auto [p_hi, p_lo] = SplitUtf16Pair(pair);

        if (p_hi != 0) {
            *utf16_out++ = p_hi;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }
        *utf16_out++ = p_lo;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

        mutf8_in += nbytes;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        in_pos += nbytes;
    }
}

size_t ConvertRegionMUtf8ToUtf16(const uint8_t *mutf8_in, uint16_t *utf16_out, size_t mutf8_len, size_t utf16_len,
                                 size_t start)
{
    size_t in_pos = 0;
    size_t out_pos = 0;
    while (in_pos < mutf8_len) {
        auto [pair, nbytes] = ConvertMUtf8ToUtf16Pair(mutf8_in, mutf8_len - in_pos);
        auto [p_hi, p_lo] = SplitUtf16Pair(pair);

        mutf8_in += nbytes;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        in_pos += nbytes;
        if (start > 0) {
            start -= nbytes;
            continue;
        }

        if (p_hi != 0) {
            if (out_pos++ >= utf16_len - 1) {  // check for place for two uint16
                --out_pos;
                break;
            }
            *utf16_out++ = p_hi;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }
        if (out_pos++ >= utf16_len) {
            --out_pos;
            break;
        }
        *utf16_out++ = p_lo;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }
    return out_pos;
}

int CompareMUtf8ToMUtf8(const uint8_t *mutf8_1, const uint8_t *mutf8_2)
{
    uint32_t c1;
    uint32_t c2;
    uint32_t n1;
    uint32_t n2;

    do {
        c1 = *mutf8_1;
        c2 = *mutf8_2;

        if (c1 == 0 && c2 == 0) {
            return 0;
        }

        if (c1 == 0 && c2 != 0) {
            return -1;
        }

        if (c1 != 0 && c2 == 0) {
            return 1;
        }

        std::tie(c1, n1) = ConvertMUtf8ToUtf16Pair(mutf8_1);
        std::tie(c2, n2) = ConvertMUtf8ToUtf16Pair(mutf8_2);

        mutf8_1 += n1;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        mutf8_2 += n2;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    } while (c1 == c2);

    auto [c1p1, c1p2] = SplitUtf16Pair(c1);
    auto [c2p1, c2p2] = SplitUtf16Pair(c2);

    int result = c1p1 - c2p1;
    if (result != 0) {
        return result;
    }

    return c1p2 - c2p2;
}

// Compare plain utf8, which allows 0 inside a string
int CompareUtf8ToUtf8(const uint8_t *utf8_1, size_t utf8_1_length, const uint8_t *utf8_2, size_t utf8_2_length)
{
    uint32_t c1;
    uint32_t c2;
    uint32_t n1;
    uint32_t n2;

    uint32_t utf8_1_index = 0;
    uint32_t utf8_2_index = 0;

    do {
        if (utf8_1_index == utf8_1_length && utf8_2_index == utf8_2_length) {
            return 0;
        }

        if (utf8_1_index == utf8_1_length && utf8_2_index < utf8_2_length) {
            return -1;
        }

        if (utf8_1_index < utf8_1_length && utf8_2_index == utf8_2_length) {
            return 1;
        }

        c1 = *utf8_1;
        c2 = *utf8_2;

        std::tie(c1, n1) = ConvertMUtf8ToUtf16Pair(utf8_1);
        std::tie(c2, n2) = ConvertMUtf8ToUtf16Pair(utf8_2);

        utf8_1 += n1;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        utf8_2 += n2;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        utf8_1_index += n1;
        utf8_2_index += n2;
    } while (c1 == c2);

    auto [c1p1, c1p2] = SplitUtf16Pair(c1);
    auto [c2p1, c2p2] = SplitUtf16Pair(c2);

    int result = c1p1 - c2p1;
    if (result != 0) {
        return result;
    }

    return c1p2 - c2p2;
}

size_t Mutf8Size(const uint8_t *mutf8)
{
    return strlen(Mutf8AsCString(mutf8));
}

size_t MUtf8ToUtf16Size(const uint8_t *mutf8)
{
    size_t res = 0;
    while (*mutf8 != '\0') {  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        auto [pair, nbytes] = ConvertMUtf8ToUtf16Pair(mutf8);
        res += pair > MAX_U16 ? CONST_2 : 1;
        mutf8 += nbytes;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }
    return res;
}

size_t MUtf8ToUtf16Size(const uint8_t *mutf8, size_t mutf8_len)
{
    size_t pos = 0;
    size_t res = 0;
    while (pos != mutf8_len) {
        auto [pair, nbytes] = ConvertMUtf8ToUtf16Pair(mutf8, mutf8_len - pos);
        if (nbytes == 0) {
            nbytes = 1;
        }
        res += pair > MAX_U16 ? CONST_2 : 1;
        mutf8 += nbytes;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        pos += nbytes;
    }
    return res;
}

size_t Utf16ToMUtf8Size(const uint16_t *mutf16, uint32_t length)
{
    size_t res = 1;  // zero byte
    // When the utf16 data length is only 1 and the code is in 0xd800-0xdfff,
    // it is a single code point, and it needs to be represented by three MUTF8 code.
    if (length == 1 && mutf16[0] >= HI_SURROGATE_MIN &&  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        mutf16[0] <= LO_SURROGATE_MAX) {                 // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        res += CONST_3;
        return res;
    }

    for (uint32_t i = 0; i < length; ++i) {
        // NOLINTNEXTLINE(bugprone-branch-clone)
        if (mutf16[i] == 0) {                    // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            res += CONST_2;                      // special case for U+0000 => C0 80
        } else if (mutf16[i] <= MUTF8_1B_MAX) {  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            res += 1;
        } else if (mutf16[i] <= MUTF8_2B_MAX) {  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            res += CONST_2;
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        } else if (mutf16[i] < HI_SURROGATE_MIN || mutf16[i] > HI_SURROGATE_MAX) {
            res += CONST_3;
        } else {
            res += CONST_4;
            ++i;
        }
    }
    return res;
}

bool IsEqual(Span<const uint8_t> utf8_1, Span<const uint8_t> utf8_2)
{
    if (utf8_1.size() != utf8_2.size()) {
        return false;
    }

    return memcmp(utf8_1.data(), utf8_2.data(), utf8_1.size()) == 0;
}

bool IsEqual(const uint8_t *mutf8_1, const uint8_t *mutf8_2)
{
    return strcmp(Mutf8AsCString(mutf8_1), Mutf8AsCString(mutf8_2)) == 0;
}

}  // namespace panda::utf
