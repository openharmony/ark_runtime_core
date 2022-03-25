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

#include "utils/bit_utils.h"

#include <cstdint>

#include <gtest/gtest.h>

namespace panda::test {

TEST(BitUtils, ReverseBytesUint32)
{
    ASSERT_EQ(ReverseBytes(0x33221100U), 0x00112233U);
    ASSERT_EQ(ReverseBytes(0U), 0);
    ASSERT_EQ(ReverseBytes(0xffffffffU), 0xffffffffU);
}

TEST(BitUtils, ReverseBytesUint64)
{
    ASSERT_EQ(ReverseBytes(UINT64_C(0x7766554433221100)), UINT64_C(0x0011223344556677));
    ASSERT_EQ(ReverseBytes(UINT64_C(0)), 0);
    ASSERT_EQ(ReverseBytes(UINT64_C(0xffffffffffffffff)), UINT64_C(0xffffffffffffffff));
}

TEST(BitUtils, ReverseBitsUint32)
{
    ASSERT_EQ(ReverseBits(0x33221100U), 0x008844ccU);
    ASSERT_EQ(ReverseBits(0U), 0);
    ASSERT_EQ(ReverseBits(0xffffffffU), 0xffffffffU);
}

TEST(BitUtils, ReverseBitsUint64)
{
    ASSERT_EQ(ReverseBits(UINT64_C(0x7766554433221100)), UINT64_C(0x008844cc22aa66ee));
    ASSERT_EQ(ReverseBits(UINT64_C(0)), 0);
    ASSERT_EQ(ReverseBits(UINT64_C(0xffffffffffffffff)), UINT64_C(0xffffffffffffffff));
}

TEST(BitUtils, RoundUp)
{
    ASSERT_EQ(RoundUp(0, 4), 0);
    ASSERT_EQ(RoundUp(8, 4), 8);
    ASSERT_EQ(RoundUp(7, 4), 8);
    ASSERT_EQ(RoundUp(5, 8), 8);
}

TEST(BitUtils, RoundDown)
{
    ASSERT_EQ(RoundDown(0, 4), 0);
    ASSERT_EQ(RoundDown(3, 4), 0);
    ASSERT_EQ(RoundDown(8, 4), 8);
}

}  // namespace panda::test
