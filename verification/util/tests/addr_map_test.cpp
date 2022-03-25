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

#include "util/addr_map.h"
#include "util/range.h"

#include "util/tests/verifier_test.h"

#include <gtest/gtest.h>

namespace panda::verifier::test {

TEST_F(VerifierTest, AddrMap)
{
    constexpr size_t N = 123;
    char mem[N];
    AddrMap amap1 {&mem[0], &mem[122]};
    AddrMap amap2 {&mem[0], &mem[122]};
    amap1.Mark(&mem[50], &mem[60]);
    EXPECT_TRUE(amap1.HasMark(&mem[50]));
    EXPECT_TRUE(amap1.HasMark(&mem[60]));
    EXPECT_FALSE(amap1.HasMark(&mem[49]));
    EXPECT_FALSE(amap1.HasMark(&mem[61]));
    amap2.Mark(&mem[70], &mem[90]);
    EXPECT_FALSE(amap1.HasCommonMarks(amap2));
    amap2.Mark(&mem[60]);
    char *ptr;
    EXPECT_TRUE(amap1.GetFirstCommonMark(amap2, &ptr));
    EXPECT_EQ(ptr, &mem[60]);

    PandaVector<const char *> ptrs;
    amap1.Clear();
    amap1.Mark(&mem[48]);
    amap1.Mark(&mem[61]);
    amap1.Mark(&mem[50]);
    amap1.Mark(&mem[60]);
    amap1.EnumerateMarksInScope<const char *>(&mem[49], &mem[60], [&ptrs](const char *addr) {
        ptrs.push_back(addr);
        return true;
    });

    EXPECT_EQ(ptrs.size(), 2);
    EXPECT_EQ(ptrs[0], reinterpret_cast<const char *>(&mem[50]));
    EXPECT_EQ(ptrs[1], reinterpret_cast<const char *>(&mem[60]));
}

}  // namespace panda::verifier::test
