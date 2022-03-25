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

#include "utils/string_helpers.h"

#include <cerrno>
#include <cstdarg>

#include <sstream>

#include <gtest/gtest.h>

namespace panda::helpers::string::test {

TEST(StringHelpers, Format)
{
    EXPECT_EQ(Format("abc"), "abc");
    EXPECT_EQ(Format("%s %d %c", "a", 10, 0x31), "a 10 1");

    std::stringstream ss;
    for (size_t i = 0; i < 10000; i++) {
        ss << " ";
    }

    ss << "abc";

    EXPECT_EQ(Format("%10003s", "abc"), ss.str());
}

TEST(StringHelpers, ParseInt)
{
    errno = 0;
    int i = 0;
    // check format
    ASSERT_FALSE(ParseInt("x", &i));
    ASSERT_EQ(EINVAL, errno);
    errno = 0;
    ASSERT_FALSE(ParseInt("123x", &i));
    ASSERT_EQ(EINVAL, errno);

    ASSERT_TRUE(ParseInt("123", &i));
    ASSERT_EQ(123, i);
    ASSERT_EQ(0, errno);
    i = 0;
    EXPECT_TRUE(ParseInt("  123", &i));
    EXPECT_EQ(123, i);
    ASSERT_TRUE(ParseInt("-123", &i));
    ASSERT_EQ(-123, i);
    i = 0;
    EXPECT_TRUE(ParseInt("  -123", &i));
    EXPECT_EQ(-123, i);

    short s = 0;
    ASSERT_TRUE(ParseInt("1234", &s));
    ASSERT_EQ(1234, s);

    // check range
    ASSERT_TRUE(ParseInt("12", &i, 0, 15));
    ASSERT_EQ(12, i);
    errno = 0;
    ASSERT_FALSE(ParseInt("-12", &i, 0, 15));
    ASSERT_EQ(ERANGE, errno);
    errno = 0;
    ASSERT_FALSE(ParseInt("16", &i, 0, 15));
    ASSERT_EQ(ERANGE, errno);

    errno = 0;
    ASSERT_FALSE(ParseInt<int>("x", nullptr));
    ASSERT_EQ(EINVAL, errno);
    errno = 0;
    ASSERT_FALSE(ParseInt<int>("123x", nullptr));
    ASSERT_EQ(EINVAL, errno);
    ASSERT_TRUE(ParseInt<int>("1234", nullptr));

    i = 0;
    ASSERT_TRUE(ParseInt("0123", &i));
    ASSERT_EQ(123, i);

    i = 0;
    ASSERT_TRUE(ParseInt("0x123", &i));
    ASSERT_EQ(0x123, i);

    i = 0;
    EXPECT_TRUE(ParseInt("  0x123", &i));
    EXPECT_EQ(0x123, i);

    i = 0;
    ASSERT_TRUE(ParseInt(std::string("123"), &i));
    ASSERT_EQ(123, i);

    i = 123;
    ASSERT_FALSE(ParseInt("456x", &i));
    ASSERT_EQ(123, i);
}

}  // namespace panda::helpers::string::test
