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

#include "utils/expected.h"

#include <gtest/gtest.h>

namespace panda::test::expected {

enum class ErrorCode { First, Second };

static Expected<int, ErrorCode> helper(int v)
{
    switch (v) {
        case 0:
            return Unexpected {ErrorCode::First};
        case 1:
            return {42};
        default:
            return Unexpected {ErrorCode::Second};
    }
}

struct Default {
    int v;
};

TEST(Expected, Unexpected)
{
    int e = 1;
    auto u = Unexpected(e);
    EXPECT_EQ(Unexpected<int>(1).Value(), 1);
    EXPECT_EQ(u.Value(), 1);
    EXPECT_EQ(static_cast<const Unexpected<int> &>(u).Value(), 1);
}

TEST(Expected, Ctor)
{
    int v = 1;
    auto e = Expected<int, ErrorCode>(v);
    EXPECT_TRUE(e);
    EXPECT_EQ(e.Value(), 1);
    EXPECT_EQ(*e, 1);

    auto e0 = Expected<int, ErrorCode>();
    EXPECT_EQ(*e0, 0);

    auto e1 = Expected<int, ErrorCode>(2);
    EXPECT_EQ(e1.Value(), 2);

    auto e2 = Expected<int, ErrorCode>(Unexpected(ErrorCode::First));
    auto u = Unexpected(ErrorCode::Second);
    auto e3 = Expected<int, ErrorCode>(u);
    EXPECT_FALSE(e2);
    EXPECT_EQ(e2.Error(), ErrorCode::First);
    EXPECT_EQ(e3.Error(), ErrorCode::Second);
}

TEST(Expected, Access)
{
    const auto e1 = Expected<int, ErrorCode>(Unexpected(ErrorCode::First));
    EXPECT_EQ(e1.Error(), ErrorCode::First);
    EXPECT_EQ((Expected<int, ErrorCode>(Unexpected(ErrorCode::Second)).Error()), ErrorCode::Second);
    const auto e2 = Expected<int, ErrorCode>(1);
    EXPECT_EQ(e2.Value(), 1);
    EXPECT_EQ(*e2, 1);
    EXPECT_EQ((*Expected<int, ErrorCode>(2)), 2);
    EXPECT_EQ((Expected<int, ErrorCode>(3).Value()), 3);
}

TEST(Expected, Assignment)
{
    auto d = Default {1};
    Expected<Default, ErrorCode> t = d;
    t.Value() = Default {2};
    EXPECT_TRUE(t);
    EXPECT_EQ((*t).v, 2);
    t = Unexpected(ErrorCode::First);
    EXPECT_FALSE(t);
    EXPECT_EQ(t.Error(), ErrorCode::First);
}

TEST(Expected, Basic)
{
    auto res1 = helper(0);
    auto res2 = helper(1);
    auto res3 = helper(2);
    EXPECT_FALSE(res1);
    EXPECT_TRUE(res2);
    EXPECT_FALSE(res3);
    EXPECT_EQ(res1.Error(), ErrorCode::First);
    EXPECT_EQ(*res2, 42);
    EXPECT_EQ(res3.Error(), ErrorCode::Second);
}

TEST(Expected, ValueOr)
{
    auto res1 = helper(0).ValueOr(1);
    auto res2 = helper(res1).ValueOr(res1);
    auto e = Expected<int, ErrorCode>(1);
    EXPECT_EQ(res1, 1);
    EXPECT_EQ(res2, 42);
    EXPECT_EQ(e.ValueOr(0), 1);
}
}  // namespace panda::test::expected
