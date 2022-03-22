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
#include "value/variables.h"

#include "util/tests/verifier_test.h"

#include <gtest/gtest.h>

namespace panda::verifier::test {

TEST_F(VerifierTest, Variables)
{
    Variables vars;

    auto v1 = vars.NewVar();
    auto v2 = vars.NewVar();

    EXPECT_TRUE(v1 != v2);

    auto v4 = vars.NewVar();

    {
        auto v3 = vars.NewVar();
        EXPECT_TRUE(v3 != v2 && v3 != v1 && v3 != v4);
        EXPECT_EQ(vars.AmountOfUsedVars(), 4);
        v4 = v3;
        EXPECT_EQ(vars.AmountOfUsedVars(), 3);
    }

    auto v5 = vars.NewVar();
    EXPECT_EQ(vars.AmountOfUsedVars(), 4);
    EXPECT_FALSE(v4 == v5);

    size_t count = 0;

    ForEach(vars.AllVariables(), [&count, &v1, &v2, &v4, &v5](auto v) {
        ++count;
        EXPECT_TRUE(v == v1 || v == v2 || v == v4 || v == v5);
    });

    EXPECT_EQ(count, 4);
}

}  // namespace panda::verifier::test
