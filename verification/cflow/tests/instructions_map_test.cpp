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

#include "cflow/instructions_map.h"

#include "util/range.h"

#include "util/tests/verifier_test.h"

#include <gtest/gtest.h>

namespace panda::verifier::test {

TEST_F(VerifierTest, InstructionsMap)
{
    char code[148];
    InstructionsMap map {&code[5], &code[147]};

    // 11110010
    EXPECT_FALSE(map.PutInstruction(&code[4], 3));
    EXPECT_TRUE(map.PutInstruction(&code[5], 2));
    EXPECT_TRUE(map.CanJumpTo(&code[5]));
    EXPECT_FALSE(map.CanJumpTo(&code[6]));
    EXPECT_TRUE(map.CanJumpTo(&code[7]));

    map.MarkCodeBlock(&code[10], 104);
    EXPECT_TRUE(map.CanJumpTo(&code[9]));
    EXPECT_TRUE(map.CanJumpTo(&code[114]));
    for (auto addr : Range(10, 113)) {
        EXPECT_FALSE(map.CanJumpTo(&code[addr]));
    }

    EXPECT_TRUE(map.PutInstruction(&code[133], 1));
    EXPECT_TRUE(map.CanJumpTo(&code[133]));
}

}  // namespace panda::verifier::test
