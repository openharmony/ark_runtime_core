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

#include "value/abstract_typed_value.h"
#include "type/type_system.h"
#include "type/type_sort.h"
#include "type/type_image.h"

#include "util/tests/verifier_test.h"

#include <gtest/gtest.h>

#include <functional>

namespace panda::verifier::test {

TEST_F(VerifierTest, AbstractTypedValue)
{
    SortNames<PandaString> sort {"Bot", "Top"};
    TypeSystem type_system {sort["Bot"], sort["Top"]};
    Variables variables;

    auto Top = type_system.Top();

    auto i8 = type_system.Parametric(sort["i8"])();
    auto i16 = type_system.Parametric(sort["i16"])();
    auto i32 = type_system.Parametric(sort["i32"])();
    auto i64 = type_system.Parametric(sort["i64"])();

    i8 << i16 << i32 << i64;

    auto u8 = type_system.Parametric(sort["u8"])();
    auto u16 = type_system.Parametric(sort["u16"])();
    auto u32 = type_system.Parametric(sort["u32"])();
    auto u64 = type_system.Parametric(sort["u64"])();

    u8 << u16 << u32 << u64;

    auto nv = std::bind(&Variables::NewVar, variables);

    AbstractTypedValue av1 {i16, nv()};
    AbstractTypedValue av2 {i32, nv()};

    auto av3 = av1 & av2;

    auto t3 = av3.GetAbstractType().GetType();

    EXPECT_EQ(t3, i32);

    AbstractTypedValue av4 {u16, nv()};

    auto av5 = av1 & av4;

    auto t5 = av5.GetAbstractType().GetType();

    EXPECT_EQ(t5, Top);
}

}  // namespace panda::verifier::test
