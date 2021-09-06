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

#include "util/tests/environment.h"

#include "util/tests/verifier_test.h"

#include "type/type_system.h"
#include "type/type_sort.h"
#include "type/type_image.h"
#include "type/type_systems.h"

#include "include/runtime.h"

#include "runtime/include/mem/panda_string.h"

#include <iostream>

#include <gtest/gtest.h>

namespace panda::verifier::test {

using SortNames = SortNames<PandaString>;

TEST_F(VerifierTest, TypeSystemIncrementalClosure)
{
    auto &&type_system = TypeSystems::Get(TypeSystemKind::PANDA);
    auto sort = [](const auto &name) { return TypeSystems::GetSort(TypeSystemKind::PANDA, name); };

    type_system.SetIncrementalRelationClosureMode(true);
    type_system.SetDeferIncrementalRelationClosure(false);

    auto Bot = type_system.Bot();
    auto Top = type_system.Top();

    auto i8 = type_system.Parametric(sort("i8"))();
    auto i16 = type_system.Parametric(sort("i16"))();
    auto i32 = type_system.Parametric(sort("i32"))();
    auto i64 = type_system.Parametric(sort("i64"))();

    auto u8 = type_system.Parametric(sort("u8"))();
    auto u16 = type_system.Parametric(sort("u16"))();
    auto u32 = type_system.Parametric(sort("u32"))();
    auto u64 = type_system.Parametric(sort("u64"))();

    auto method = type_system.Parametric(sort("method"));

    auto top_method_of3args = method(-Bot >> -Bot >> +Top);
    auto bot_method_of3args = method(-Top >> -Top >> +Bot);

    auto method1 = method(-i8 >> -i8 >> +i64);
    auto method2 = method(-i32 >> -i16 >> +i32);

    // method2 <: method1
    auto method3 = method(-i16 >> -method2 >> +method1);
    auto method4 = method(-i64 >> -method1 >> +method2);
    // method4 <: method3

    EXPECT_TRUE(Bot <= i8);
    EXPECT_TRUE(Bot <= u64);

    EXPECT_TRUE(i8 <= Top);
    EXPECT_TRUE(u64 <= Top);

    i8 << (i16 | i32) << i64;
    (u8 | u16) << (u32 | u64);

    EXPECT_TRUE(i8 <= i64);
    EXPECT_TRUE(i16 <= i64);
    EXPECT_TRUE(i32 <= i64);
    EXPECT_FALSE(i16 <= i32);

    EXPECT_TRUE(u8 <= u64);
    EXPECT_TRUE(u16 <= u64);
    EXPECT_FALSE(u8 <= u16);
    EXPECT_FALSE(u32 <= u64);

    EXPECT_TRUE(method2 <= method1);
    EXPECT_FALSE(method1 <= method2);

    EXPECT_TRUE(method4 <= method3);
    EXPECT_FALSE(method3 <= method4);

    EXPECT_TRUE(bot_method_of3args <= method1);
    EXPECT_TRUE(bot_method_of3args <= method4);

    EXPECT_TRUE(method1 <= top_method_of3args);
    EXPECT_TRUE(method4 <= top_method_of3args);
}

TEST_F(VerifierTest, TypeSystemClosureAtTheEnd)
{
    auto &&type_system = TypeSystems::Get(TypeSystemKind::PANDA);
    auto sort = [](const auto &name) { return TypeSystems::GetSort(TypeSystemKind::PANDA, name); };

    type_system.SetIncrementalRelationClosureMode(false);
    type_system.SetDeferIncrementalRelationClosure(false);

    auto Bot = type_system.Bot();
    auto Top = type_system.Top();

    auto i8 = type_system.Parametric(sort("i8"))();
    auto i16 = type_system.Parametric(sort("i16"))();
    auto i32 = type_system.Parametric(sort("i32"))();
    auto i64 = type_system.Parametric(sort("i64"))();

    auto u8 = type_system.Parametric(sort("u8"))();
    auto u16 = type_system.Parametric(sort("u16"))();
    auto u32 = type_system.Parametric(sort("u32"))();
    auto u64 = type_system.Parametric(sort("u64"))();

    auto method = type_system.Parametric(sort("method"));

    auto top_method_of3args = method(-Bot >> -Bot >> +Top);
    auto bot_method_of3args = method(-Top >> -Top >> +Bot);

    auto method1 = method(-i8 >> -i8 >> +i64);
    auto method2 = method(-i32 >> -i16 >> +i32);

    // method2 <: method1
    auto method3 = method(-i16 >> -method2 >> +method1);
    auto method4 = method(-i64 >> -method1 >> +method2);
    // method4 <: method3

    i8 << (i16 | i32) << i64;
    (u8 | u16) << (u32 | u64);

    // bofore closure all methods are unrelated
    EXPECT_FALSE(method2 <= method1);
    EXPECT_FALSE(method1 <= method2);

    EXPECT_FALSE(method4 <= method3);
    EXPECT_FALSE(method3 <= method4);

    EXPECT_FALSE(bot_method_of3args <= method1);
    EXPECT_FALSE(bot_method_of3args <= method4);

    EXPECT_FALSE(method1 <= top_method_of3args);
    EXPECT_FALSE(method4 <= top_method_of3args);

    type_system.CloseSubtypingRelation();

    // after closure all realations are correct
    EXPECT_TRUE(method2 <= method1);

    EXPECT_TRUE(method4 <= method3);
    EXPECT_TRUE(bot_method_of3args <= method1);
    EXPECT_TRUE(method4 <= top_method_of3args);
}

TEST_F(VerifierTest, TypeSystemLeastUpperBound)
{
    auto &&type_system = TypeSystems::Get(TypeSystemKind::PANDA);
    auto sort = [](const auto &name) { return TypeSystems::GetSort(TypeSystemKind::PANDA, name); };

    /*
        G<--
        ^   \
        |    \
        |     \
        |      E<-   .F
        |      ^  \ /  ^
        D      |   X   |
        ^      |  / \  |
        |      | /   \ |
        |      |/     \|
        A      B       C

        NB!!!
        Here is contradiction to conjecture in relation.h about LUB EqClass.
        So here is many object in LUB class but they are not from the same
        class of equivalence.

        In current Panda type system design with Top and Bot, this issue is not
        significant, because in case of such situation (as with E and F),
        LUB will be Top.

        But in general case assumptions that all elements in LUB are from the same
        class of equivalence is wrong. And corresponding functions in relation.h
        should always return full LUB set. And they should be renamed accordingly,
        to do not mislead other developers.
    */

    auto Top = type_system.Top();

    auto A = type_system.Parametric(sort("A"))();
    auto B = type_system.Parametric(sort("B"))();
    auto C = type_system.Parametric(sort("C"))();
    auto D = type_system.Parametric(sort("D"))();
    auto E = type_system.Parametric(sort("E"))();
    auto F = type_system.Parametric(sort("F"))();
    auto G = type_system.Parametric(sort("G"))();

    A << D << G;
    B << E << G;
    B << F;
    C << E;
    C << F;

    auto R = A & B;
    EXPECT_EQ(R, (TypeSet {G, Top}));

    R = E & F;
    EXPECT_EQ(R, TypeSet {Top});

    R = C & D;
    EXPECT_EQ(R, (TypeSet {G, Top}));

    R = A & B & C;
    EXPECT_EQ(R, (TypeSet {G, Top}));

    R = A & B & C & F;
    EXPECT_EQ(R, TypeSet {Top});

    EXPECT_TRUE(R.TheOnlyType().IsTop());
}

}  // namespace panda::verifier::test
