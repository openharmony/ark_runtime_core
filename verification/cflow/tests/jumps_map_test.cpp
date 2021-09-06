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

#include "cflow/jumps_map.h"
#include "cflow/instructions_map.h"

#include "util/tests/verifier_test.h"

#include <gtest/gtest.h>

namespace panda::verifier::test {

TEST_F(VerifierTest, JumpsMap)
{
    constexpr size_t N = 148;
    char code[N];
    JumpsMap map {&code[5], &code[147]};

    // 11110010
    EXPECT_FALSE(map.PutJump(&code[5], &code[4]));
    EXPECT_TRUE(map.PutJump(&code[5], &code[5]));
    EXPECT_TRUE(map.PutJump(&code[5], &code[128]));
    EXPECT_FALSE(map.PutJump(&code[3], &code[6]));
    EXPECT_TRUE(map.PutJump(&code[147], &code[145]));
    EXPECT_FALSE(map.PutJump(&code[148], &code[145]));
    EXPECT_FALSE(map.PutJump(&code[143], &code[148]));

    EXPECT_TRUE(map.PutJump(&code[13], &code[5]));
    EXPECT_TRUE(map.PutJump(&code[12], &code[5]));
    EXPECT_TRUE(map.PutJump(&code[11], &code[5]));

    uintptr_t sum = 0;
    map.EnumerateAllTargets<const char *>([&sum, &code](const char *tgt) {
        uintptr_t val = reinterpret_cast<uintptr_t>(tgt) - reinterpret_cast<uintptr_t>(code);
        sum += val;
        return true;
    });

    EXPECT_EQ(sum, 5 + 128 + 145);

    sum = 0;

    map.EnumerateAllJumpsToTarget<const char *>(&code[5], [&sum, &code](const char *tgt) {
        uintptr_t val = reinterpret_cast<uintptr_t>(tgt) - reinterpret_cast<uintptr_t>(code);
        sum += val;
        return true;
    });

    EXPECT_EQ(sum, 5 + 11 + 12 + 13);
}

TEST_F(VerifierTest, JumpsMapConflicts)
{
    char code[148];
    JumpsMap jmap {&code[5], &code[147]};
    InstructionsMap imap {&code[5], &code[147]};

    EXPECT_TRUE(imap.PutInstruction(&code[13], 5));  // 13 - ok, 14,15,16,17 - prohibited
    EXPECT_TRUE(jmap.PutJump(&code[25], &code[13]));

    EXPECT_FALSE(jmap.IsConflictingWith(imap));

    EXPECT_TRUE(jmap.PutJump(&code[45], &code[14]));
    EXPECT_TRUE(jmap.IsConflictingWith(imap));

    const char *tgt_pc = nullptr;
    const char *jmp_pc = nullptr;

    EXPECT_TRUE(jmap.GetFirstConflictingJump(imap, &jmp_pc, &tgt_pc));

    EXPECT_EQ(tgt_pc, &code[14]);
    EXPECT_EQ(jmp_pc, &code[45]);

    EXPECT_TRUE(jmap.PutJump(&code[35], &code[14]));
    EXPECT_TRUE(jmap.IsConflictingWith(imap));

    EXPECT_TRUE(jmap.GetFirstConflictingJump(imap, &jmp_pc, &tgt_pc));

    EXPECT_EQ(tgt_pc, &code[14]);
    EXPECT_EQ(jmp_pc, &code[45]);  // should be 45, since 35 added after
}

}  // namespace panda::verifier::test
