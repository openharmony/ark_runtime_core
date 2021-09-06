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

#include "utils/bit_helpers.h"

#include <cstdint>
#include <type_traits>

#include <gtest/gtest.h>

namespace panda::helpers::test {

TEST(BitHelpers, UnsignedTypeHelper)
{
    EXPECT_TRUE((std::is_same_v<UnsignedTypeHelperT<0>, uint8_t>));
    EXPECT_TRUE((std::is_same_v<UnsignedTypeHelperT<1>, uint8_t>));
    EXPECT_TRUE((std::is_same_v<UnsignedTypeHelperT<8>, uint8_t>));

    EXPECT_TRUE((std::is_same_v<UnsignedTypeHelperT<9>, uint16_t>));
    EXPECT_TRUE((std::is_same_v<UnsignedTypeHelperT<16>, uint16_t>));

    EXPECT_TRUE((std::is_same_v<UnsignedTypeHelperT<17>, uint32_t>));
    EXPECT_TRUE((std::is_same_v<UnsignedTypeHelperT<32>, uint32_t>));

    EXPECT_TRUE((std::is_same_v<UnsignedTypeHelperT<33>, uint64_t>));
    EXPECT_TRUE((std::is_same_v<UnsignedTypeHelperT<64>, uint64_t>));

    EXPECT_TRUE((std::is_same_v<UnsignedTypeHelperT<65>, void>));
}

TEST(BitHelpers, TypeHelper)
{
    EXPECT_TRUE((std::is_same_v<TypeHelperT<0, false>, uint8_t>));
    EXPECT_TRUE((std::is_same_v<TypeHelperT<1, false>, uint8_t>));
    EXPECT_TRUE((std::is_same_v<TypeHelperT<8, false>, uint8_t>));

    EXPECT_TRUE((std::is_same_v<TypeHelperT<0, true>, int8_t>));
    EXPECT_TRUE((std::is_same_v<TypeHelperT<1, true>, int8_t>));
    EXPECT_TRUE((std::is_same_v<TypeHelperT<8, true>, int8_t>));

    EXPECT_TRUE((std::is_same_v<TypeHelperT<9, false>, uint16_t>));
    EXPECT_TRUE((std::is_same_v<TypeHelperT<16, false>, uint16_t>));

    EXPECT_TRUE((std::is_same_v<TypeHelperT<9, true>, int16_t>));
    EXPECT_TRUE((std::is_same_v<TypeHelperT<16, true>, int16_t>));

    EXPECT_TRUE((std::is_same_v<TypeHelperT<17, false>, uint32_t>));
    EXPECT_TRUE((std::is_same_v<TypeHelperT<32, false>, uint32_t>));

    EXPECT_TRUE((std::is_same_v<TypeHelperT<17, true>, int32_t>));
    EXPECT_TRUE((std::is_same_v<TypeHelperT<32, true>, int32_t>));

    EXPECT_TRUE((std::is_same_v<TypeHelperT<33, false>, uint64_t>));
    EXPECT_TRUE((std::is_same_v<TypeHelperT<64, false>, uint64_t>));

    EXPECT_TRUE((std::is_same_v<TypeHelperT<33, true>, int64_t>));
    EXPECT_TRUE((std::is_same_v<TypeHelperT<64, true>, int64_t>));
}

}  // namespace panda::helpers::test
