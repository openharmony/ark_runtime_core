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

#include <gtest/gtest.h>

#include <type_traits>

#include <cstddef>
#include <cstdint>

#include "bytecode_instruction-inl.h"

namespace panda::test {

TEST(BytecodeInstruction, Parse)
{
    // V4_IMM4
    {
        const uint8_t bytecode[] = {0x00, 0xa1, 0xff};
        BytecodeInstruction inst(bytecode);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstruction::Format::V4_IMM4, 0>()), 1);
        EXPECT_EQ((inst.GetImm<BytecodeInstruction::Format::V4_IMM4, 0>()), -6);
    }

    {
        const uint8_t bytecode[] = {0x00, 0x2f, 0xff};
        BytecodeInstruction inst(bytecode);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstruction::Format::V4_IMM4, 0>()), 0xf);
        EXPECT_EQ((inst.GetImm<BytecodeInstruction::Format::V4_IMM4, 0>()), 0x2);
    }

    // IMM8
    {
        const uint8_t bytecode[] = {0x00, 0xf2, 0xff};
        BytecodeInstruction inst(bytecode);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetImm<BytecodeInstruction::Format::IMM8, 0>()), static_cast<int8_t>(0xf2));
    }

    {
        const uint8_t bytecode[] = {0x00, 0x21, 0xff};
        BytecodeInstruction inst(bytecode);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetImm<BytecodeInstruction::Format::IMM8, 0>()), 0x21);
    }

    // V8_IMM8
    {
        const uint8_t bytecode[] = {0x00, 0x12, 0xf2, 0xff};
        BytecodeInstruction inst(bytecode);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstruction::Format::V8_IMM8, 0>()), 0x12);
        EXPECT_EQ((inst.GetImm<BytecodeInstruction::Format::V8_IMM8, 0>()), static_cast<int8_t>(0xf2));
    }

    {
        const uint8_t bytecode[] = {0x00, 0xf2, 0x12, 0xff};
        BytecodeInstruction inst(bytecode);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstruction::Format::V8_IMM8, 0>()), 0xf2);
        EXPECT_EQ((inst.GetImm<BytecodeInstruction::Format::V8_IMM8, 0>()), 0x12);
    }

    // IMM16
    {
        const uint8_t bytecode[] = {0x00, 0xf2, 0x12, 0xff};
        BytecodeInstruction inst(bytecode);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetImm<BytecodeInstruction::Format::IMM16, 0>()), 0x12f2);
    }

    {
        const uint8_t bytecode[] = {0x00, 0x12, 0xf2, 0xff};
        BytecodeInstruction inst(bytecode);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetImm<BytecodeInstruction::Format::IMM16, 0>()), static_cast<int16_t>(0xf212));
    }

    // V8_IMM16
    {
        const uint8_t bytecode[] = {0x00, 0x10, 0xf2, 0x12, 0xff};
        BytecodeInstruction inst(bytecode);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstruction::Format::V8_IMM16, 0>()), 0x10);
        EXPECT_EQ((inst.GetImm<BytecodeInstruction::Format::V8_IMM16, 0>()), 0x12f2);
    }

    {
        const uint8_t bytecode[] = {0x00, 0xff, 0x12, 0xf2, 0xff};
        BytecodeInstruction inst(bytecode);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstruction::Format::V8_IMM16, 0>()), 0xff);
        EXPECT_EQ((inst.GetImm<BytecodeInstruction::Format::V8_IMM16, 0>()), static_cast<int16_t>(0xf212));
    }

    // IMM32
    {
        const uint8_t bytecode[] = {0x00, 0x34, 0xf2, 0x12, 0x10, 0xff};
        BytecodeInstruction inst(bytecode);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetImm<BytecodeInstruction::Format::IMM32, 0>()), 0x1012f234);
    }

    {
        const uint8_t bytecode[] = {0x00, 0x34, 0x12, 0xf2, 0xf1, 0xff};
        BytecodeInstruction inst(bytecode);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetImm<BytecodeInstruction::Format::IMM32, 0>()), static_cast<int32_t>(0xf1f21234));
    }

    // V8_IMM32
    {
        const uint8_t bytecode[] = {0x00, 0x04, 0x34, 0xf2, 0x12, 0x10, 0xff};
        BytecodeInstruction inst(bytecode);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstruction::Format::V8_IMM32, 0>()), 0x04);
        EXPECT_EQ((inst.GetImm<BytecodeInstruction::Format::V8_IMM32, 0>()), 0x1012f234);
    }

    {
        const uint8_t bytecode[] = {0x00, 0xaa, 0x34, 0x12, 0xf2, 0xf1, 0xff};
        BytecodeInstruction inst(bytecode);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstruction::Format::V8_IMM32, 0>()), 0xaa);
        EXPECT_EQ((inst.GetImm<BytecodeInstruction::Format::V8_IMM32, 0>()), static_cast<int32_t>(0xf1f21234));
    }

    // IMM64
    {
        const uint8_t bytecode[] = {0x00, 0x9a, 0x78, 0x56, 0x34, 0xf2, 0x12, 0x10, 0x4, 0xff};
        BytecodeInstruction inst(bytecode);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetImm<BytecodeInstruction::Format::IMM64, 0>()), 0x041012f23456789a);
    }

    {
        const uint8_t bytecode[] = {0x00, 0x9a, 0x78, 0x56, 0x34, 0xf2, 0x12, 0x10, 0xab, 0xff};
        BytecodeInstruction inst(bytecode);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetImm<BytecodeInstruction::Format::IMM64, 0>()), static_cast<int64_t>(0xab1012f23456789a));
    }

    // V8_IMM64
    {
        const uint8_t bytecode[] = {0x00, 0x11, 0x9a, 0x78, 0x56, 0x34, 0xf2, 0x12, 0x10, 0x4, 0xff};
        BytecodeInstruction inst(bytecode);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstruction::Format::V8_IMM64, 0>()), 0x11);
        EXPECT_EQ((inst.GetImm<BytecodeInstruction::Format::V8_IMM64, 0>()), 0x041012f23456789a);
    }

    {
        const uint8_t bytecode[] = {0x00, 0xab, 0x9a, 0x78, 0x56, 0x34, 0xf2, 0x12, 0x10, 0xab, 0xff};
        BytecodeInstruction inst(bytecode);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstruction::Format::V8_IMM64, 0>()), 0xab);
        EXPECT_EQ((inst.GetImm<BytecodeInstruction::Format::V8_IMM64, 0>()), static_cast<int64_t>(0xab1012f23456789a));
    }

    // V4_V4
    {
        const uint8_t bytecode[] = {0x00, 0xba, 0xff};
        BytecodeInstruction inst(bytecode);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstruction::Format::V4_V4, 0>()), 0xa);
        EXPECT_EQ((inst.GetVReg<BytecodeInstruction::Format::V4_V4, 1>()), 0xb);
    }

    // V8
    {
        const uint8_t bytecode[] = {0x00, 0xab, 0xff};
        BytecodeInstruction inst(bytecode);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstruction::Format::V8, 0>()), 0xab);
    }

    // V8_V8
    {
        const uint8_t bytecode[] = {0x00, 0xab, 0xcd, 0xff};
        BytecodeInstruction inst(bytecode);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstruction::Format::V8_V8, 0>()), 0xab);
        EXPECT_EQ((inst.GetVReg<BytecodeInstruction::Format::V8_V8, 1>()), 0xcd);
    }

    // V16_V16
    {
        const uint8_t bytecode[] = {0x00, 0xcd, 0xab, 0xf1, 0xee, 0xff};
        BytecodeInstruction inst(bytecode);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstruction::Format::V16_V16, 0>()), 0xabcd);
        EXPECT_EQ((inst.GetVReg<BytecodeInstruction::Format::V16_V16, 1>()), 0xeef1);
    }

    // ID32
    {
        const uint8_t bytecode[] = {0x00, 0xf1, 0xee, 0xcd, 0xab, 0xff};
        BytecodeInstruction inst(bytecode);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetId<BytecodeInstruction::Format::ID32, 0>()), BytecodeId(0xabcdeef1));
    }

    // V4_V4_ID16
    {
        const uint8_t bytecode[] = {0x00, 0x21, 0xf1, 0xee, 0xcd, 0xab, 0xff};
        BytecodeInstruction inst(bytecode);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstruction::Format::V4_V4_ID16, 0>()), 0x1);
        EXPECT_EQ((inst.GetVReg<BytecodeInstruction::Format::V4_V4_ID16, 1>()), 0x2);
        EXPECT_EQ((inst.GetId<BytecodeInstruction::Format::V4_V4_ID16, 0>()), BytecodeId(0xeef1));
    }

    // V8_ID16
    {
        const uint8_t bytecode[] = {0x00, 0x12, 0xf1, 0xee, 0xcd, 0xab, 0xff};
        BytecodeInstruction inst(bytecode);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstruction::Format::V8_ID16, 0>()), 0x12);
        EXPECT_EQ((inst.GetId<BytecodeInstruction::Format::V8_ID16, 0>()), BytecodeId(0xeef1));
    }

    // V4_V4_V4_V4_ID16
    {
        const uint8_t bytecode[] = {0x00, 0x21, 0x43, 0xf1, 0xee, 0xcd, 0xab, 0xff};
        BytecodeInstruction inst(bytecode);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstruction::Format::V4_V4_V4_V4_ID16, 0x0>()), 0x1);
        EXPECT_EQ((inst.GetVReg<BytecodeInstruction::Format::V4_V4_V4_V4_ID16, 0x1>()), 0x2);
        EXPECT_EQ((inst.GetVReg<BytecodeInstruction::Format::V4_V4_V4_V4_ID16, 0x2>()), 0x3);
        EXPECT_EQ((inst.GetVReg<BytecodeInstruction::Format::V4_V4_V4_V4_ID16, 0x3>()), 0x4);
        EXPECT_EQ((inst.GetId<BytecodeInstruction::Format::V4_V4_V4_V4_ID16, 0>()), BytecodeId(0xeef1));
    }
}

TEST(BytecodeInstruction, JumpTo)
{
    const uint8_t bytecode[] = {0x00, 0x11, 0x22, 0x33};
    BytecodeInstruction inst(bytecode);
    BytecodeInstruction next = inst.JumpTo(2);
    EXPECT_EQ(static_cast<uint8_t>(next.GetOpcode()), bytecode[2]);
}

TEST(BytecodeInstructionSafe, Parse)
{
    // Positive tests
    // V4_IMM4
    {
        const uint8_t bytecode[] = {0x00, 0xa1, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V4_IMM4, 0>()), 1);
        EXPECT_EQ((inst.GetImm<BytecodeInstructionSafe::Format::V4_IMM4, 0>()), -6);
    }

    {
        const uint8_t bytecode[] = {0x00, 0x2f, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V4_IMM4, 0>()), 0xf);
        EXPECT_EQ((inst.GetImm<BytecodeInstructionSafe::Format::V4_IMM4, 0>()), 0x2);
    }

    // IMM8
    {
        const uint8_t bytecode[] = {0x00, 0xf2, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetImm<BytecodeInstructionSafe::Format::IMM8, 0>()), static_cast<int8_t>(0xf2));
    }

    {
        const uint8_t bytecode[] = {0x00, 0x21, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetImm<BytecodeInstructionSafe::Format::IMM8, 0>()), 0x21);
    }

    // V8_IMM8
    {
        const uint8_t bytecode[] = {0x00, 0x12, 0xf2, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V8_IMM8, 0>()), 0x12);
        EXPECT_EQ((inst.GetImm<BytecodeInstructionSafe::Format::V8_IMM8, 0>()), static_cast<int8_t>(0xf2));
    }

    {
        const uint8_t bytecode[] = {0x00, 0xf2, 0x12, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V8_IMM8, 0>()), 0xf2);
        EXPECT_EQ((inst.GetImm<BytecodeInstructionSafe::Format::V8_IMM8, 0>()), 0x12);
    }

    // IMM16
    {
        const uint8_t bytecode[] = {0x00, 0xf2, 0x12, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetImm<BytecodeInstructionSafe::Format::IMM16, 0>()), 0x12f2);
    }

    {
        const uint8_t bytecode[] = {0x00, 0x12, 0xf2, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetImm<BytecodeInstructionSafe::Format::IMM16, 0>()), static_cast<int16_t>(0xf212));
    }

    // V8_IMM16
    {
        const uint8_t bytecode[] = {0x00, 0x10, 0xf2, 0x12, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V8_IMM16, 0>()), 0x10);
        EXPECT_EQ((inst.GetImm<BytecodeInstructionSafe::Format::V8_IMM16, 0>()), 0x12f2);
    }

    {
        const uint8_t bytecode[] = {0x00, 0xff, 0x12, 0xf2, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V8_IMM16, 0>()), 0xff);
        EXPECT_EQ((inst.GetImm<BytecodeInstructionSafe::Format::V8_IMM16, 0>()), static_cast<int16_t>(0xf212));
    }

    // IMM32
    {
        const uint8_t bytecode[] = {0x00, 0x34, 0xf2, 0x12, 0x10, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetImm<BytecodeInstructionSafe::Format::IMM32, 0>()), 0x1012f234);
    }

    {
        const uint8_t bytecode[] = {0x00, 0x34, 0x12, 0xf2, 0xf1, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetImm<BytecodeInstructionSafe::Format::IMM32, 0>()), static_cast<int32_t>(0xf1f21234));
    }

    // V8_IMM32
    {
        const uint8_t bytecode[] = {0x00, 0x04, 0x34, 0xf2, 0x12, 0x10, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V8_IMM32, 0>()), 0x04);
        EXPECT_EQ((inst.GetImm<BytecodeInstructionSafe::Format::V8_IMM32, 0>()), 0x1012f234);
    }

    {
        const uint8_t bytecode[] = {0x00, 0xaa, 0x34, 0x12, 0xf2, 0xf1, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V8_IMM32, 0>()), 0xaa);
        EXPECT_EQ((inst.GetImm<BytecodeInstructionSafe::Format::V8_IMM32, 0>()), static_cast<int32_t>(0xf1f21234));
    }

    // IMM64
    {
        const uint8_t bytecode[] = {0x00, 0x9a, 0x78, 0x56, 0x34, 0xf2, 0x12, 0x10, 0x4, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetImm<BytecodeInstructionSafe::Format::IMM64, 0>()), 0x041012f23456789a);
    }

    {
        const uint8_t bytecode[] = {0x00, 0x9a, 0x78, 0x56, 0x34, 0xf2, 0x12, 0x10, 0xab, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetImm<BytecodeInstructionSafe::Format::IMM64, 0>()), static_cast<int64_t>(0xab1012f23456789a));
    }

    // V8_IMM64
    {
        const uint8_t bytecode[] = {0x00, 0x11, 0x9a, 0x78, 0x56, 0x34, 0xf2, 0x12, 0x10, 0x4, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V8_IMM64, 0>()), 0x11);
        EXPECT_EQ((inst.GetImm<BytecodeInstructionSafe::Format::V8_IMM64, 0>()), 0x041012f23456789a);
    }

    {
        const uint8_t bytecode[] = {0x00, 0xab, 0x9a, 0x78, 0x56, 0x34, 0xf2, 0x12, 0x10, 0xab, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V8_IMM64, 0>()), 0xab);
        EXPECT_EQ((inst.GetImm<BytecodeInstructionSafe::Format::V8_IMM64, 0>()),
                  static_cast<int64_t>(0xab1012f23456789a));
    }

    // V4_V4
    {
        const uint8_t bytecode[] = {0x00, 0xba, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V4_V4, 0>()), 0xa);
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V4_V4, 1>()), 0xb);
    }

    // V8
    {
        const uint8_t bytecode[] = {0x00, 0xab, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V8, 0>()), 0xab);
    }

    // V8_V8
    {
        const uint8_t bytecode[] = {0x00, 0xab, 0xcd, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V8_V8, 0>()), 0xab);
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V8_V8, 1>()), 0xcd);
    }

    // V16_V16
    {
        const uint8_t bytecode[] = {0x00, 0xcd, 0xab, 0xf1, 0xee, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V16_V16, 0>()), 0xabcd);
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V16_V16, 1>()), 0xeef1);
    }

    // ID32
    {
        const uint8_t bytecode[] = {0x00, 0xf1, 0xee, 0xcd, 0xab, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetId<BytecodeInstructionSafe::Format::ID32, 0>()), BytecodeId(0xabcdeef1));
    }

    // V4_V4_ID16
    {
        const uint8_t bytecode[] = {0x00, 0x21, 0xf1, 0xee, 0xcd, 0xab, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V4_V4_ID16, 0>()), 0x1);
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V4_V4_ID16, 1>()), 0x2);
        EXPECT_EQ((inst.GetId<BytecodeInstructionSafe::Format::V4_V4_ID16, 0>()), BytecodeId(0xeef1));
    }

    // V8_ID16
    {
        const uint8_t bytecode[] = {0x00, 0x12, 0xf1, 0xee, 0xcd, 0xab, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V8_ID16, 0>()), 0x12);
        EXPECT_EQ((inst.GetId<BytecodeInstructionSafe::Format::V8_ID16, 0>()), BytecodeId(0xeef1));
    }

    // V4_V4_V4_V4_ID16
    {
        const uint8_t bytecode[] = {0x00, 0x21, 0x43, 0xf1, 0xee, 0xcd, 0xab, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V4_V4_V4_V4_ID16, 0x0>()), 0x1);
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V4_V4_V4_V4_ID16, 0x1>()), 0x2);
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V4_V4_V4_V4_ID16, 0x2>()), 0x3);
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V4_V4_V4_V4_ID16, 0x3>()), 0x4);
        EXPECT_EQ((inst.GetId<BytecodeInstructionSafe::Format::V4_V4_V4_V4_ID16, 0>()), BytecodeId(0xeef1));
    }

    // Negative tests

    // V4_IMM4
    {
        const uint8_t bytecode[] = {0x00};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V4_IMM4, 0>()), 0);
        EXPECT_FALSE(inst.IsValid());
    }

    {
        const uint8_t bytecode[] = {0x00};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetImm<BytecodeInstructionSafe::Format::V4_IMM4, 0>()), 0);
        EXPECT_FALSE(inst.IsValid());
    }

    // IMM8
    {
        const uint8_t bytecode[] = {0x00};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetImm<BytecodeInstructionSafe::Format::IMM8, 0>()), static_cast<int8_t>(0));
        EXPECT_FALSE(inst.IsValid());
    }

    {
        const uint8_t bytecode[] = {0x00};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetImm<BytecodeInstructionSafe::Format::IMM8, 0>()), 0);
        EXPECT_FALSE(inst.IsValid());
    }

    // V8_IMM8
    {
        const uint8_t bytecode[] = {0x00, 0x12};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V8_IMM8, 0>()), 0x12);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetImm<BytecodeInstructionSafe::Format::V8_IMM8, 0>()), static_cast<int8_t>(0));
        EXPECT_FALSE(inst.IsValid());
    }

    // IMM16
    {
        const uint8_t bytecode[] = {0x00, 0xf2, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 2]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetImm<BytecodeInstructionSafe::Format::IMM16, 0>()), 0xf2);
        EXPECT_FALSE(inst.IsValid());
    }

    // V8_IMM16
    {
        const uint8_t bytecode[] = {0x00, 0x10, 0xf2, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 2]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V8_IMM16, 0>()), 0x10);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetImm<BytecodeInstructionSafe::Format::V8_IMM16, 0>()), 0xf2);
        EXPECT_FALSE(inst.IsValid());
    }

    // IMM32
    {
        const uint8_t bytecode[] = {0x00, 0x34, 0xf2, 0x12, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 2]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetImm<BytecodeInstructionSafe::Format::IMM32, 0>()), 0x12f234);
        EXPECT_FALSE(inst.IsValid());
    }

    // V8_IMM32
    {
        const uint8_t bytecode[] = {0x00, 0x04, 0x34, 0xf2, 0x12, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 2]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V8_IMM32, 0>()), 0x04);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetImm<BytecodeInstructionSafe::Format::V8_IMM32, 0>()), 0x12f234);
        EXPECT_FALSE(inst.IsValid());
    }

    // IMM64
    {
        const uint8_t bytecode[] = {0x00, 0x9a, 0x78, 0x56, 0x34, 0xf2, 0x12, 0x10, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 3]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetImm<BytecodeInstructionSafe::Format::IMM64, 0>()), 0x12f23456789a);
        EXPECT_FALSE(inst.IsValid());
    }

    // V8_IMM64
    {
        const uint8_t bytecode[] = {0x00, 0x11, 0x9a, 0x78, 0x56, 0x34, 0xf2, 0x12, 0x10, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 3]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V8_IMM64, 0>()), 0x11);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetImm<BytecodeInstructionSafe::Format::V8_IMM64, 0>()), 0x12f23456789a);
        EXPECT_FALSE(inst.IsValid());
    }

    // V4_V4
    {
        const uint8_t bytecode[] = {0x00};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V4_V4, 0>()), 0);
        EXPECT_FALSE(inst.IsValid());
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V4_V4, 1>()), 0);
        EXPECT_FALSE(inst.IsValid());
    }

    // V8
    {
        const uint8_t bytecode[] = {0x00};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V8, 0>()), 0);
        EXPECT_FALSE(inst.IsValid());
    }

    // V8_V8
    {
        const uint8_t bytecode[] = {0x00, 0xab};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V8_V8, 0>()), 0xab);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V8_V8, 1>()), 0);
        EXPECT_FALSE(inst.IsValid());
    }

    // V16_V16
    {
        const uint8_t bytecode[] = {0x00, 0xcd, 0xab, 0xf1, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 2]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V16_V16, 0>()), 0xabcd);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V16_V16, 1>()), 0xf1);
        EXPECT_FALSE(inst.IsValid());
    }

    // ID32
    {
        const uint8_t bytecode[] = {0x00, 0xf1, 0xee, 0xcd, 0xff};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 2]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetId<BytecodeInstructionSafe::Format::ID32, 0>()), BytecodeId(0xcdeef1));
        EXPECT_FALSE(inst.IsValid());
    }

    // V4_V4_ID16
    {
        const uint8_t bytecode[] = {0x00, 0x21, 0xf1, 0xee};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 2]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V4_V4_ID16, 0>()), 0x1);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V4_V4_ID16, 1>()), 0x2);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetId<BytecodeInstructionSafe::Format::V4_V4_ID16, 0>()), BytecodeId(0xf1));
        EXPECT_FALSE(inst.IsValid());
    }

    // V8_ID16
    {
        const uint8_t bytecode[] = {0x00, 0x12, 0xf1, 0xee};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 3]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V8_ID16, 0>()), 0x12);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetId<BytecodeInstructionSafe::Format::V8_ID16, 0>()), BytecodeId(0x00));
        EXPECT_FALSE(inst.IsValid());
    }

    // V4_V4_V4_V4_ID16
    {
        const uint8_t bytecode[] = {0x00, 0x21, 0x43, 0xf1, 0xee};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 3]);
        EXPECT_EQ(static_cast<uint8_t>(inst.GetOpcode()), 0x00);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V4_V4_V4_V4_ID16, 0x0>()), 0x1);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V4_V4_V4_V4_ID16, 0x1>()), 0x2);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V4_V4_V4_V4_ID16, 0x2>()), 0x3);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetVReg<BytecodeInstructionSafe::Format::V4_V4_V4_V4_ID16, 0x3>()), 0x4);
        EXPECT_TRUE(inst.IsValid());
        EXPECT_EQ((inst.GetId<BytecodeInstructionSafe::Format::V4_V4_V4_V4_ID16, 0>()), BytecodeId(0x0));
        EXPECT_FALSE(inst.IsValid());
    }
}

TEST(BytecodeInstructionSafe, JumpTo)
{
    // Positive
    {
        const uint8_t bytecode[] = {0x00, 0x11, 0x22, 0x33};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        BytecodeInstructionSafe next = inst.JumpTo(2);
        EXPECT_EQ(static_cast<uint8_t>(next.GetOpcode()), bytecode[2]);
    }
    // Negative
    {
        const uint8_t bytecode[] = {0x00, 0x11, 0x22, 0x33};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        BytecodeInstructionSafe next = inst.JumpTo(4);
        EXPECT_FALSE(inst.IsValid());
        EXPECT_FALSE(next.IsValid());
    }
    {
        const uint8_t bytecode[] = {0x00, 0x11, 0x22, 0x33};
        BytecodeInstructionSafe inst(bytecode, &bytecode[0], &bytecode[sizeof(bytecode) - 1]);
        BytecodeInstructionSafe next = inst.JumpTo(-1);
        EXPECT_FALSE(inst.IsValid());
        EXPECT_FALSE(next.IsValid());
    }
}

}  // namespace panda::test
