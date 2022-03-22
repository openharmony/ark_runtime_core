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

#include "util/equiv_classes.h"

#include "util/tests/verifier_test.h"

#include <gtest/gtest.h>

namespace {

struct Obj {
    int data = 0;
    bool operator==(const Obj &rhs) const
    {
        return data == rhs.data;
    }
    bool operator<(const Obj &rhs) const
    {
        return data < rhs.data;
    }
};

}  // namespace

namespace std {

template <>
struct hash<Obj> {
    constexpr size_t operator()(const Obj &obj) const
    {
        return static_cast<size_t>(obj.data);
    }
};

}  // namespace std

namespace panda::verifier::test {

TEST_F(VerifierTest, ClassesOfEquivalence)
{
    EqClass<Obj> eqc;

    Obj o1 {1};
    Obj o2 {2};
    Obj o3 {3};
    Obj o4 {4};
    Obj o5 {5};
    Obj o6 {6};
    Obj o7 {7};
    Obj o8 {8};

    eqc.Equate({o1, o2, o3});
    eqc.Equate({o4, o5, o6});
    eqc.Equate({o7, o8});

    EXPECT_EQ(eqc.ClassSizeOf(o1), 3);
    EXPECT_EQ(eqc.ClassSizeOf(o5), 3);
    EXPECT_EQ(eqc.ClassSizeOf(o8), 2);

    EXPECT_TRUE(eqc.IsAllEqual({o1, o3}));
    EXPECT_TRUE(eqc.IsAllEqual({o2, o1}));
    EXPECT_TRUE(eqc.IsAllEqual({o2, o3}));

    EXPECT_FALSE(eqc.IsAllEqual({o1, o4}));
    EXPECT_FALSE(eqc.IsAllEqual({o5, o8}));

    EXPECT_TRUE(eqc.IsAllEqual({o4, o6}));

    EXPECT_TRUE(eqc.IsAllEqual({o7, o8}));

    EXPECT_FALSE(eqc.IsAllEqual({o5, o7}));

    eqc.Equate({o3, o8});

    EXPECT_EQ(eqc.ClassSizeOf(o2), 5);
    EXPECT_EQ(eqc.ClassSizeOf(o7), 5);

    EXPECT_TRUE(eqc.IsAllEqual({o1, o7}));

    eqc.Equate({o2, o4});

    EXPECT_EQ(eqc.ClassSizeOf(o8), 8);

    EXPECT_TRUE(eqc.IsAllEqual({o3, o5, o8}));
}

}  // namespace panda::verifier::test
