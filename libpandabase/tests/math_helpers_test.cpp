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

#include "utils/math_helpers.h"

#include <cmath>
#include <gtest/gtest.h>

namespace panda::helpers::math::test {

TEST(MathHelpers, GetIntLog2)
{
    for (int i = 1; i < 32; i++) {
        uint64_t val = 1U << i;
        EXPECT_EQ(GetIntLog2(val), i);
        EXPECT_EQ(GetIntLog2(val), log2(static_cast<double>(val)));
    }

    for (int i = 1; i < 32; i++) {
        uint64_t val = (1U << i) + (i == 31 ? -1 : 1);
#ifndef NDEBUG
        EXPECT_DEATH_IF_SUPPORTED(GetIntLog2(val), "");
#endif
    }
}

TEST(MathHelpers, IsPowerOfTwo)
{
    EXPECT_TRUE(IsPowerOfTwo(1));
    EXPECT_TRUE(IsPowerOfTwo(2));
    EXPECT_TRUE(IsPowerOfTwo(4));
    EXPECT_TRUE(IsPowerOfTwo(64));
    EXPECT_TRUE(IsPowerOfTwo(1024));
    EXPECT_TRUE(IsPowerOfTwo(2048));
    EXPECT_FALSE(IsPowerOfTwo(3));
    EXPECT_FALSE(IsPowerOfTwo(63));
    EXPECT_FALSE(IsPowerOfTwo(65));
    EXPECT_FALSE(IsPowerOfTwo(100));
}

TEST(MathHelpers, GetPowerOfTwoValue32)
{
    for (int i = 0; i <= 1; i++) {
        EXPECT_EQ(GetPowerOfTwoValue32(i), 1);
    }
    for (int i = 2; i <= 2; i++) {
        EXPECT_EQ(GetPowerOfTwoValue32(i), 2);
    }
    for (int i = 9; i <= 16; i++) {
        EXPECT_EQ(GetPowerOfTwoValue32(i), 16);
    }
    for (int i = 33; i <= 64; i++) {
        EXPECT_EQ(GetPowerOfTwoValue32(i), 64);
    }
    for (int i = 1025; i <= 2048; i++) {
        EXPECT_EQ(GetPowerOfTwoValue32(i), 2048);
    }
}

}  // namespace panda::helpers::math::test
