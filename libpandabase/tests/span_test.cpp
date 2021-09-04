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

#include "utils/span.h"

#include <array>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

namespace panda::test::span {

template <class T>
std::string ToString(Span<T> s)
{
    std::ostringstream oss;
    for (const auto &e : s) {
        oss << e << " ";
    }
    return oss.str();
}

template <class T>
Span<T> Double(Span<T> s)
{
    for (auto &e : s) {
        e *= 2U;
    }
    return s;
}

TEST(Span, Conversions)
{
    int c[] {1, 2, 3};
    std::vector v {4, 5, 6};
    const std::vector const_v {-4, -5, -6};
    std::array a {7, 8, 9};
    size_t sz = 3;
    auto p = std::make_unique<int[]>(sz);
    p[0] = 10;
    p[1] = 11;
    p[2] = 12;
    std::string s = " !\"";

    EXPECT_EQ(ToString(Double(Span(c))), "2 4 6 ");
    EXPECT_EQ(ToString(Double(Span(v))), "8 10 12 ");
    EXPECT_EQ(ToString(Span(const_v)), "-4 -5 -6 ");
    EXPECT_EQ(ToString(Double(Span(a))), "14 16 18 ");
    EXPECT_EQ(ToString(Double(Span(p.get(), sz))), "20 22 24 ");
    EXPECT_EQ(ToString(Double(Span(p.get(), p.get() + 2))), "40 44 ");
    EXPECT_EQ(ToString(Double(Span(s))), "@ B D ");
}

TEST(Span, SubSpan)
{
    int c[] {1, 2, 3, 4, 5};
    auto s = Span(c).SubSpan(1, 3);
    auto f = s.First(2);
    auto l = s.Last(2);

    EXPECT_EQ(ToString(s), "2 3 4 ");
    EXPECT_EQ(ToString(f), "2 3 ");
    EXPECT_EQ(ToString(l), "3 4 ");
}

TEST(Span, SubSpanT)
{
    {
        uint8_t buf[] = {1, 1, 1, 1, 1, 0, 0, 0, 2, 0, 0, 0, 0x78, 0x56, 0x34, 0x12, 0xfe, 0xff, 0xff, 0xff};
        struct Foo {
            uint32_t a;
            int32_t b;
        };
        auto sp = Span(buf);
#ifndef NDEBUG
        ASSERT_DEATH(sp.SubSpan<Foo>(4, 3), ".*");
        ASSERT_DEATH(sp.SubSpan<Foo>(3, 2), ".*");
#endif
        auto sub_sp = sp.SubSpan<Foo>(4, 2);
        ASSERT_EQ(sub_sp.size(), 2U);
        ASSERT_EQ(sub_sp[0].a, 1U);
        ASSERT_EQ(sub_sp[0].b, 2);
        ASSERT_EQ(sub_sp[1].a, 0x12345678U);
        ASSERT_EQ(sub_sp[1].b, -2);
    }
    {
        uint32_t buf[] = {0x01020304, 0x05060708, 0x090a0b0c};
        auto sp = Span(buf);
#ifndef NDEBUG
        ASSERT_DEATH(sp.SubSpan<uint16_t>(4, 1), ".*");
#endif
        auto sub_sp = sp.SubSpan<uint16_t>(1, 4);
        ASSERT_EQ(sub_sp.size(), 4U);
        ASSERT_EQ(sub_sp[0], 0x0708);
        ASSERT_EQ(sub_sp[1], 0x0506);
        ASSERT_EQ(sub_sp[2], 0x0b0c);
        ASSERT_EQ(sub_sp[3], 0x090a);
    }
}

TEST(Span, AsBytes)
{
    const int c1[] {1, 2, 3};
    int c2[] {4, 5, 6};
    auto cs = Span(c1);
    auto s = Span(c2);
    EXPECT_EQ(cs.SizeBytes(), 12U);
    EXPECT_EQ(AsBytes(cs)[sizeof(int)], static_cast<std::byte>(2));
    AsWritableBytes(s)[4] = static_cast<std::byte>(1);
    EXPECT_EQ(s[1], 1);
}

}  // namespace panda::test::span
