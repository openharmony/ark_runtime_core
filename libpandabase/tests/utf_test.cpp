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

#include "utils/utf.h"

#include <cstdint>

#include <vector>

#include <gtest/gtest.h>

namespace panda::utf::test {

static uint16_t U16_lead(uint32_t codepoint)
{
    return ((codepoint >> 10U) + 0xd7c0) & 0xffff;
}

static uint16_t U16_tail(uint32_t codepoint)
{
    return (codepoint & 0x3ff) | 0xdc00;
}

TEST(Utf, ConvertMUtf8ToUtf16)
{
    // 2-byte mutf-8 U+0000
    {
        const std::vector<uint8_t> in {0xc0, 0x80, 0x00};
        const std::vector<uint16_t> res {0x0};
        std::vector<uint16_t> out(res.size());
        ConvertMUtf8ToUtf16(in.data(), utf::Mutf8Size(in.data()), out.data());
        EXPECT_EQ(out, res);
    }

    // 1-byte mutf-8: 0xxxxxxx
    {
        const std::vector<uint8_t> in {0x7f, 0x00};
        const std::vector<uint16_t> res {0x7f};
        std::vector<uint16_t> out(res.size());
        ConvertMUtf8ToUtf16(in.data(), utf::Mutf8Size(in.data()), out.data());
        EXPECT_EQ(out, res);
    }

    // 2-byte mutf-8: 110xxxxx 10xxxxxx
    {
        const std::vector<uint8_t> in {0xc2, 0xa7, 0x33, 0x00};
        const std::vector<uint16_t> res {0xa7, 0x33};
        std::vector<uint16_t> out(res.size());
        ConvertMUtf8ToUtf16(in.data(), utf::Mutf8Size(in.data()), out.data());
        EXPECT_EQ(out, res);
    }

    // 3-byte mutf-8: 1110xxxx 10xxxxxx 10xxxxxx
    {
        const std::vector<uint8_t> in {0xef, 0xbf, 0x83, 0x33, 0x00};
        const std::vector<uint16_t> res {0xffc3, 0x33};
        std::vector<uint16_t> out(res.size());
        ConvertMUtf8ToUtf16(in.data(), utf::Mutf8Size(in.data()), out.data());
        EXPECT_EQ(out, res);
    }

    // double 3-byte mutf-8: 11101101 1010xxxx 10xxxxxx 11101101 1011xxxx 10xxxxxx
    {
        const std::vector<uint8_t> in {0xed, 0xa0, 0x81, 0xed, 0xb0, 0xb7, 0x00};
        const std::vector<uint16_t> res {0xd801, 0xdc37};
        std::vector<uint16_t> out(res.size());
        ConvertMUtf8ToUtf16(in.data(), utf::Mutf8Size(in.data()), out.data());
        EXPECT_EQ(out, res);
    }

    {
        const std::vector<uint8_t> in {0x5b, 0x61, 0x62, 0x63, 0xed, 0xa3, 0x92, 0x5d, 0x00};
        const std::vector<uint16_t> res {0x5b, 0x61, 0x62, 0x63, 0xd8d2, 0x5d};
        std::vector<uint16_t> out(res.size());
        ConvertMUtf8ToUtf16(in.data(), utf::Mutf8Size(in.data()), out.data());
        EXPECT_EQ(out, res);
    }

    {
        const std::vector<uint8_t> in {0xF0, 0x9F, 0x91, 0xB3, 0x00};
        const std::vector<uint16_t> res {0xD83D, 0xDC73};
        std::vector<uint16_t> out(res.size());
        ConvertMUtf8ToUtf16(in.data(), utf::Mutf8Size(in.data()), out.data());
        EXPECT_EQ(out, res);
    }
}

TEST(Utf, Utf16ToMUtf8Size)
{
    // 2-byte mutf-8 U+0000
    {
        const std::vector<uint16_t> in {0x0};
        size_t res = Utf16ToMUtf8Size(in.data(), in.size());
        EXPECT_EQ(res, 3);
    }

    // 1-byte mutf-8: 0xxxxxxx
    {
        const std::vector<uint16_t> in {0x7f};
        size_t res = Utf16ToMUtf8Size(in.data(), in.size());
        EXPECT_EQ(res, 2);
    }

    // 2-byte mutf-8: 110xxxxx 10xxxxxx
    {
        const std::vector<uint16_t> in {0xa7, 0x33};
        size_t res = Utf16ToMUtf8Size(in.data(), in.size());
        EXPECT_EQ(res, 4);
    }

    // 3-byte mutf-8: 1110xxxx 10xxxxxx 10xxxxxx
    {
        const std::vector<uint16_t> in {0xffc3, 0x33};
        size_t res = Utf16ToMUtf8Size(in.data(), in.size());
        EXPECT_EQ(res, 5);
    }

    // double 3-byte mutf-8: 11101101 1010xxxx 10xxxxxx 11101101 1011xxxx 10xxxxxx
    {
        const std::vector<uint16_t> in {0xd801, 0xdc37};
        size_t res = Utf16ToMUtf8Size(in.data(), in.size());
        EXPECT_EQ(res, 5);
    }
}

TEST(Utf, ConvertRegionUtf16ToMUtf8)
{
    // 2-byte mutf-8 U+0000
    {
        const std::vector<uint16_t> in {0x0};
        const std::vector<uint8_t> res {0xc0, 0x80, 0x00};
        std::vector<uint8_t> out(res.size());
        size_t sz = ConvertRegionUtf16ToMUtf8(in.data(), out.data(), in.size(), out.size() - 1, 0);
        EXPECT_EQ(sz, 2);
        out[out.size() - 1] = '\0';
        EXPECT_EQ(out, res);
    }

    // 1-byte mutf-8: 0xxxxxxx
    {
        const std::vector<uint16_t> in {0x7f};
        const std::vector<uint8_t> res {0x7f, 0x00};
        std::vector<uint8_t> out(res.size());
        size_t sz = ConvertRegionUtf16ToMUtf8(in.data(), out.data(), in.size(), out.size() - 1, 0);
        EXPECT_EQ(sz, 1);
        out[out.size() - 1] = '\0';
        EXPECT_EQ(out, res);
    }

    // 2-byte mutf-8: 110xxxxx 10xxxxxx
    {
        const std::vector<uint16_t> in {0xa7, 0x33};
        const std::vector<uint8_t> res {0xc2, 0xa7, 0x33, 0x00};
        std::vector<uint8_t> out(res.size());
        size_t sz = ConvertRegionUtf16ToMUtf8(in.data(), out.data(), in.size(), out.size() - 1, 0);
        EXPECT_EQ(sz, 3);
        out[out.size() - 1] = '\0';
        EXPECT_EQ(out, res);
    }

    // 3-byte mutf-8: 1110xxxx 10xxxxxx 10xxxxxx
    {
        const std::vector<uint16_t> in {0xffc3, 0x33};
        const std::vector<uint8_t> res {0xef, 0xbf, 0x83, 0x33, 0x00};
        std::vector<uint8_t> out(res.size());
        size_t sz = ConvertRegionUtf16ToMUtf8(in.data(), out.data(), in.size(), out.size() - 1, 0);
        EXPECT_EQ(sz, 4);
        out[out.size() - 1] = '\0';
        EXPECT_EQ(out, res);
    }

    // 3-byte mutf-8: 1110xxxx 10xxxxxx 10xxxxxx
    // utf-16 data in 0xd800-0xdfff
    {
        const std::vector<uint16_t> in {0xd834, 0x33};
        const std::vector<uint8_t> res {0xed, 0xa0, 0xb4, 0x33, 0x00};
        std::vector<uint8_t> out(res.size());
        size_t sz = ConvertRegionUtf16ToMUtf8(in.data(), out.data(), in.size(), out.size() - 1, 0);
        EXPECT_EQ(sz, 4);
        out[out.size() - 1] = '\0';
        EXPECT_EQ(out, res);
    }

    // 3-byte mutf-8: 1110xxxx 10xxxxxx 10xxxxxx
    // utf-16 data in 0xd800-0xdfff
    {
        const std::vector<uint16_t> in {0xdf06, 0x33};
        const std::vector<uint8_t> res {0xed, 0xbc, 0x86, 0x33, 0x00};
        std::vector<uint8_t> out(res.size());
        size_t sz = ConvertRegionUtf16ToMUtf8(in.data(), out.data(), in.size(), out.size() - 1, 0);
        EXPECT_EQ(sz, 4);
        out[out.size() - 1] = '\0';
        EXPECT_EQ(out, res);
    }

    // double 3-byte mutf-8: 11101101 1010xxxx 10xxxxxx 11101101 1011xxxx 10xxxxxx
    {
        const std::vector<uint16_t> in {0xd801, 0xdc37};
        const std::vector<uint8_t> res {0xf0, 0x90, 0x90, 0xb7, 0x00};
        std::vector<uint8_t> out(res.size());
        size_t sz = ConvertRegionUtf16ToMUtf8(in.data(), out.data(), in.size(), out.size() - 1, 0);
        EXPECT_EQ(sz, 4);
        out[out.size() - 1] = '\0';
        EXPECT_EQ(out, res);
    }
}

TEST(Utf, CompareMUtf8ToMUtf8)
{
    // 1-byte utf-8: 0xxxxxxx
    {
        const std::vector<uint8_t> v1 {0x00};
        const std::vector<uint8_t> v2 {0x7f, 0x00};
        EXPECT_TRUE(CompareMUtf8ToMUtf8(v1.data(), v2.data()) < 0);
    }

    {
        const std::vector<uint8_t> v1 {0x02, 0x00};
        const std::vector<uint8_t> v2 {0x00};
        EXPECT_TRUE(CompareMUtf8ToMUtf8(v1.data(), v2.data()) > 0);
    }

    {
        const std::vector<uint8_t> v1 {0x7f, 0x00};
        const std::vector<uint8_t> v2 {0x7f, 0x00};
        EXPECT_TRUE(CompareMUtf8ToMUtf8(v1.data(), v2.data()) == 0);
    }

    {
        const std::vector<uint8_t> v1 {0x01, 0x7f, 0x00};
        const std::vector<uint8_t> v2 {0x01, 0x70, 0x00};
        EXPECT_TRUE(CompareMUtf8ToMUtf8(v1.data(), v2.data()) > 0);
    }

    {
        const std::vector<uint8_t> v1 {0x01, 0x71, 0x00};
        const std::vector<uint8_t> v2 {0x01, 0x73, 0x00};
        EXPECT_TRUE(CompareMUtf8ToMUtf8(v1.data(), v2.data()) < 0);
    }

    // 2-byte utf-8: 110xxxxx 10xxxxxx
    {
        const std::vector<uint8_t> v1 {0xdf, 0xbf, 0x03, 0x00};
        const std::vector<uint8_t> v2 {0xdf, 0xbf, 0x03, 0x00};
        EXPECT_TRUE(CompareMUtf8ToMUtf8(v1.data(), v2.data()) == 0);
    }

    {
        const std::vector<uint8_t> v1 {0xdf, 0xb1, 0x03, 0x00};
        const std::vector<uint8_t> v2 {0xd1, 0xb2, 0x03, 0x00};
        EXPECT_TRUE(CompareMUtf8ToMUtf8(v1.data(), v2.data()) > 0);
    }

    {
        const std::vector<uint8_t> v1 {0xd1, 0xbf, 0x03, 0x00};
        const std::vector<uint8_t> v2 {0xdf, 0xb0, 0x03, 0x00};
        EXPECT_TRUE(CompareMUtf8ToMUtf8(v1.data(), v2.data()) < 0);
    }

    // 3-byte utf-8: 1110xxxx 10xxxxxx 10xxxxxx
    {
        const std::vector<uint8_t> v1 {0xef, 0xbf, 0x03, 0x04, 0x00};
        const std::vector<uint8_t> v2 {0xef, 0xbf, 0x03, 0x04, 0x00};
        EXPECT_TRUE(CompareMUtf8ToMUtf8(v1.data(), v2.data()) == 0);
    }

    {
        const std::vector<uint8_t> v1 {0xef, 0xb2, 0x03, 0x04, 0x00};
        const std::vector<uint8_t> v2 {0xe0, 0xbf, 0x03, 0x04, 0x00};
        EXPECT_TRUE(CompareMUtf8ToMUtf8(v1.data(), v2.data()) > 0);
    }

    {
        const std::vector<uint8_t> v1 {0xef, 0xb0, 0x03, 0x04, 0x00};
        const std::vector<uint8_t> v2 {0xef, 0xbf, 0x05, 0x04, 0x00};
        EXPECT_TRUE(CompareMUtf8ToMUtf8(v1.data(), v2.data()) < 0);
    }

    // 4-byte utf-8: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    {
        const std::vector<uint8_t> v1 {0xf7, 0xbf, 0xbf, 0x04, 0x05, 0x00};
        const std::vector<uint8_t> v2 {0xf7, 0xbf, 0xbf, 0x04, 0x05, 0x00};
        EXPECT_TRUE(CompareMUtf8ToMUtf8(v1.data(), v2.data()) == 0);
    }

    {
        const std::vector<uint8_t> v1 {0xf7, 0xbf, 0xbf, 0x0a, 0x05, 0x00};
        const std::vector<uint8_t> v2 {0xf7, 0xbf, 0xbf, 0x04, 0x05, 0x00};
        EXPECT_TRUE(CompareMUtf8ToMUtf8(v1.data(), v2.data()) > 0);
    }

    {
        const std::vector<uint8_t> v1 {0xf7, 0xbf, 0xbf, 0x04, 0x05, 0x00};
        const std::vector<uint8_t> v2 {0xf8, 0xbf, 0xbf, 0x04, 0x05, 0x00};
        EXPECT_TRUE(CompareMUtf8ToMUtf8(v1.data(), v2.data()) < 0);
    }
}

TEST(Utf, CompareUtf8ToUtf8)
{
    // 1-byte utf-8: 0xxxxxxx
    {
        const std::vector<uint8_t> v1 {0x00};
        const std::vector<uint8_t> v2 {0x7f, 0x00};
        EXPECT_TRUE(CompareUtf8ToUtf8(v1.data(), v1.size(), v2.data(), v2.size()) < 0);
    }

    {
        const std::vector<uint8_t> v1 {0x02, 0x00};
        const std::vector<uint8_t> v2 {0x00};
        EXPECT_TRUE(CompareUtf8ToUtf8(v1.data(), v1.size(), v2.data(), v2.size()) > 0);
    }

    {
        const std::vector<uint8_t> v1 {0x7f, 0x00};
        const std::vector<uint8_t> v2 {0x7f, 0x00};
        EXPECT_TRUE(CompareUtf8ToUtf8(v1.data(), v1.size(), v2.data(), v2.size()) == 0);
    }

    {
        const std::vector<uint8_t> v1 {0x01, 0x7f, 0x00};
        const std::vector<uint8_t> v2 {0x01, 0x70, 0x00};
        EXPECT_TRUE(CompareUtf8ToUtf8(v1.data(), v1.size(), v2.data(), v2.size()) > 0);
    }

    {
        const std::vector<uint8_t> v1 {0x01, 0x71, 0x00};
        const std::vector<uint8_t> v2 {0x01, 0x73, 0x00};
        EXPECT_TRUE(CompareUtf8ToUtf8(v1.data(), v1.size(), v2.data(), v2.size()) < 0);
    }

    // 2-byte utf-8: 110xxxxx 10xxxxxx
    {
        const std::vector<uint8_t> v1 {0xdf, 0xbf, 0x03, 0x00};
        const std::vector<uint8_t> v2 {0xdf, 0xbf, 0x03, 0x00};
        EXPECT_TRUE(CompareUtf8ToUtf8(v1.data(), v1.size(), v2.data(), v2.size()) == 0);
    }

    {
        const std::vector<uint8_t> v1 {0xdf, 0xb1, 0x03, 0x00};
        const std::vector<uint8_t> v2 {0xd1, 0xb2, 0x03, 0x00};
        EXPECT_TRUE(CompareUtf8ToUtf8(v1.data(), v1.size(), v2.data(), v2.size()) > 0);
    }

    {
        const std::vector<uint8_t> v1 {0xd1, 0xbf, 0x03, 0x00};
        const std::vector<uint8_t> v2 {0xdf, 0xb0, 0x03, 0x00};
        EXPECT_TRUE(CompareUtf8ToUtf8(v1.data(), v1.size(), v2.data(), v2.size()) < 0);
    }

    // 3-byte utf-8: 1110xxxx 10xxxxxx 10xxxxxx
    {
        const std::vector<uint8_t> v1 {0xef, 0xbf, 0x03, 0x04, 0x00};
        const std::vector<uint8_t> v2 {0xef, 0xbf, 0x03, 0x04, 0x00};
        EXPECT_TRUE(CompareUtf8ToUtf8(v1.data(), v1.size(), v2.data(), v2.size()) == 0);
    }

    {
        const std::vector<uint8_t> v1 {0xef, 0xb2, 0x03, 0x04, 0x00};
        const std::vector<uint8_t> v2 {0xe0, 0xbf, 0x03, 0x04, 0x00};
        EXPECT_TRUE(CompareUtf8ToUtf8(v1.data(), v1.size(), v2.data(), v2.size()) > 0);
    }

    {
        const std::vector<uint8_t> v1 {0xef, 0xb0, 0x03, 0x04, 0x00};
        const std::vector<uint8_t> v2 {0xef, 0xbf, 0x05, 0x04, 0x00};
        EXPECT_TRUE(CompareUtf8ToUtf8(v1.data(), v1.size(), v2.data(), v2.size()) < 0);
    }

    // 4-byte utf-8: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    {
        const std::vector<uint8_t> v1 {0xf7, 0xbf, 0xbf, 0x04, 0x05, 0x00};
        const std::vector<uint8_t> v2 {0xf7, 0xbf, 0xbf, 0x04, 0x05, 0x00};
        EXPECT_TRUE(CompareUtf8ToUtf8(v1.data(), v1.size(), v2.data(), v2.size()) == 0);
    }

    {
        const std::vector<uint8_t> v1 {0xf7, 0xbf, 0xbf, 0x0a, 0x05, 0x00};
        const std::vector<uint8_t> v2 {0xf7, 0xbf, 0xbf, 0x04, 0x05, 0x00};
        EXPECT_TRUE(CompareUtf8ToUtf8(v1.data(), v1.size(), v2.data(), v2.size()) > 0);
    }

    {
        const std::vector<uint8_t> v1 {0xf7, 0xbf, 0xbf, 0x04, 0x05, 0x00};
        const std::vector<uint8_t> v2 {0xf8, 0xbf, 0xbf, 0x04, 0x05, 0x00};
        EXPECT_TRUE(CompareUtf8ToUtf8(v1.data(), v1.size(), v2.data(), v2.size()) < 0);
    }
}

}  // namespace panda::utf::test
