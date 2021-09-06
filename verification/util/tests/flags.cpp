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

#include "util/flags.h"

#include <gtest/gtest.h>

namespace panda::verifier::test {

TEST(VerifierTest_FlagsForEnum, simple)
{
    enum class Enum { E1, E2, E3 };
    using F = panda::verifier::FlagsForEnum<size_t, Enum, Enum::E1, Enum::E2, Enum::E3>;
    F flags;

    flags[Enum::E2] = true;
    EXPECT_TRUE(flags[Enum::E2]);
    EXPECT_FALSE(flags[Enum::E1]);
    EXPECT_FALSE(flags[Enum::E3]);
    flags[Enum::E2] = false;
    EXPECT_FALSE(flags[Enum::E1]);
    EXPECT_FALSE(flags[Enum::E2]);
    EXPECT_FALSE(flags[Enum::E3]);

    flags[Enum::E2] = true;
    flags[Enum::E1] = true;
    EXPECT_TRUE(flags[Enum::E1]);
    EXPECT_TRUE(flags[Enum::E2]);
    EXPECT_FALSE(flags[Enum::E3]);
    flags[Enum::E1] = false;
    EXPECT_FALSE(flags[Enum::E1]);
    EXPECT_TRUE(flags[Enum::E2]);
    EXPECT_FALSE(flags[Enum::E3]);
}

}  // namespace panda::verifier::test
