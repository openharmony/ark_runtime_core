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

#include "absint/reg_context.h"
#include "absint/exec_context.h"

#include "value/abstract_typed_value.h"
#include "type/type_system.h"
#include "type/type_sort.h"
#include "type/type_image.h"

#include "util/lazy.h"

#include "util/tests/verifier_test.h"

#include <gtest/gtest.h>

#include <functional>

namespace panda::verifier::test {

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_SIZE)
TEST_F(VerifierTest, AbsIntExecContext)
{
    SortNames<PandaString> sort {"Bot", "Top"};
    TypeSystem type_system {sort["Bot"], sort["Top"]};
    Variables variables;

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
    AbstractTypedValue av3 {u16, nv()};

    uint8_t instructions[128];

    ExecContext exec_ctx {&instructions[0], &instructions[127]};

    std::array<const uint8_t *, 6> cp = {&instructions[8],  &instructions[17], &instructions[23],
                                         &instructions[49], &instructions[73], &instructions[103]};

    exec_ctx.SetCheckPoints(ConstLazyFetch(cp));

    // 1 1 1 1 1 1
    // C I C I C I
    //   E     E

    RegContext ctx1, ctx2, ctx3;

    ctx1[-1] = av1;
    ctx1[0] = av2;

    ctx2[0] = av1;  // compat with 1

    ctx3[-1] = av3;  // incompat with 1

    exec_ctx.CurrentRegContext() = ctx1;

    exec_ctx.StoreCurrentRegContextForAddr(cp[0]);
    exec_ctx.StoreCurrentRegContextForAddr(cp[1]);
    exec_ctx.StoreCurrentRegContextForAddr(cp[2]);
    exec_ctx.StoreCurrentRegContextForAddr(cp[3]);
    exec_ctx.StoreCurrentRegContextForAddr(cp[4]);
    exec_ctx.StoreCurrentRegContextForAddr(cp[5]);

    exec_ctx.AddEntryPoint(cp[1], EntryPointType::METHOD_BODY);
    exec_ctx.AddEntryPoint(cp[4], EntryPointType::METHOD_BODY);

    const uint8_t *ep = &instructions[0];
    EntryPointType ept;

    exec_ctx.CurrentRegContext() = ctx2;
    while (ep != cp[0]) {
        exec_ctx.StoreCurrentRegContextForAddr(ep++);
    }
    exec_ctx.StoreCurrentRegContextForAddr(ep++);

    exec_ctx.CurrentRegContext() = ctx3;
    while (ep != cp[1]) {
        exec_ctx.StoreCurrentRegContextForAddr(ep++);
    }
    exec_ctx.StoreCurrentRegContextForAddr(ep);

    exec_ctx.CurrentRegContext() = ctx2;
    while (ep != cp[2]) {
        exec_ctx.StoreCurrentRegContextForAddr(ep++);
    }
    exec_ctx.StoreCurrentRegContextForAddr(ep++);

    exec_ctx.CurrentRegContext() = ctx3;
    while (ep != cp[3]) {
        exec_ctx.StoreCurrentRegContextForAddr(ep++);
    }
    exec_ctx.StoreCurrentRegContextForAddr(ep++);

    exec_ctx.CurrentRegContext() = ctx2;
    while (ep != cp[4]) {
        exec_ctx.StoreCurrentRegContextForAddr(ep++);
    }
    exec_ctx.StoreCurrentRegContextForAddr(ep++);

    exec_ctx.CurrentRegContext() = ctx3;
    while (ep != cp[5]) {
        exec_ctx.StoreCurrentRegContextForAddr(ep++);
    }
    exec_ctx.StoreCurrentRegContextForAddr(ep++);

    auto status = exec_ctx.GetEntryPointForChecking(&ep, &ept);

    status = exec_ctx.GetEntryPointForChecking(&ep, &ept);

    status = exec_ctx.GetEntryPointForChecking(&ep, &ept);

    EXPECT_EQ(status, ExecContext::Status::ALL_DONE);
}

}  // namespace panda::verifier::test
