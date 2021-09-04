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

#include "bytecode_emitter.h"
#include <gtest/gtest.h>

#include <array>
#include <functional>
#include <limits>
#include <tuple>
#include <vector>

namespace panda {
using Opcode = BytecodeInstruction::Opcode;

using Tuple16 = std::tuple<uint8_t, uint8_t>;
using Tuple32 = std::tuple<uint8_t, uint8_t, uint8_t, uint8_t>;
using Tuple64 = std::tuple<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t>;

static std::vector<uint8_t> &operator<<(std::vector<uint8_t> &out, uint8_t val)
{
    out.push_back(val);
    return out;
}

static std::vector<uint8_t> &operator<<(std::vector<uint8_t> &out, Opcode op);

static std::vector<uint8_t> &operator<<(std::vector<uint8_t> &out, Tuple16 val)
{
    return out << std::get<0>(val) << std::get<1>(val);
}

static std::vector<uint8_t> &operator<<(std::vector<uint8_t> &out, Tuple32 val)
{
    return out << std::get<0>(val) << std::get<1>(val) << std::get<2U>(val) << std::get<3U>(val);
}

static std::vector<uint8_t> &operator<<(std::vector<uint8_t> &out, Tuple64 val)
{
    return out << std::get<0>(val) << std::get<1U>(val) << std::get<2U>(val) << std::get<3U>(val) << std::get<4U>(val)
               << std::get<5U>(val) << std::get<6U>(val) << std::get<7U>(val);
}

static Tuple16 Split16(uint16_t val)
{
    return Tuple16 {val & 0xFF, val >> 8U};
}

static Tuple32 Split32(uint32_t val)
{
    return Tuple32 {val & 0xFF, (val >> 8U) & 0xFF, (val >> 16U) & 0xFF, (val >> 24U) & 0xFF};
}

static Tuple64 Split64(uint64_t val)
{
    return Tuple64 {val & 0xFF,          (val >> 8U) & 0xFF,  (val >> 16U) & 0xFF, (val >> 24U) & 0xFF,
                    (val >> 32U) & 0xFF, (val >> 40U) & 0xFF, (val >> 48U) & 0xFF, (val >> 56U) & 0xFF};
}

TEST(BytecodeEmitter, JmpBwd_IMM8)
{
    BytecodeEmitter emitter;
    Label label = emitter.CreateLabel();
    emitter.Bind(label);
    int num_ret = -std::numeric_limits<int8_t>::min();
    for (int i = 0; i < num_ret; ++i) {
        emitter.ReturnVoid();
    }
    emitter.Jmp(label);
    std::vector<uint8_t> out;
    ASSERT_EQ(BytecodeEmitter::ErrorCode::SUCCESS, emitter.Build(&out));
    std::vector<uint8_t> expected;
    for (int i = 0; i < num_ret; ++i) {
        expected << Opcode::RETURN_VOID;
    }
    expected << Opcode::JMP_IMM8 << -num_ret;
    ASSERT_EQ(expected, out);
}

TEST(BytecodeEmitter, JmpFwd_IMM8)
{
    BytecodeEmitter emitter;
    Label label = emitter.CreateLabel();
    emitter.Jmp(label);
    // -5 because 2 bytes takes jmp itself and
    // emitter estimate length of jmp is 3 greater than what it actually is
    int num_ret = std::numeric_limits<int8_t>::max() - 5;
    for (int i = 0; i < num_ret; ++i) {
        emitter.ReturnVoid();
    }
    emitter.Bind(label);
    emitter.ReturnVoid();
    std::vector<uint8_t> out;
    ASSERT_EQ(BytecodeEmitter::ErrorCode::SUCCESS, emitter.Build(&out));
    std::vector<uint8_t> expected;
    expected << Opcode::JMP_IMM8 << num_ret + 2;
    for (int i = 0; i < num_ret + 1; ++i) {
        expected << Opcode::RETURN_VOID;
    }
    ASSERT_EQ(expected, out);
}

TEST(BytecodeEmitter, JmpBwd_IMM16)
{
    {
        BytecodeEmitter emitter;
        Label label = emitter.CreateLabel();
        emitter.Bind(label);
        int num_ret = -std::numeric_limits<int8_t>::min() + 1;
        for (int i = 0; i < num_ret; ++i) {
            emitter.ReturnVoid();
        }
        emitter.Jmp(label);
        std::vector<uint8_t> out;
        ASSERT_EQ(BytecodeEmitter::ErrorCode::SUCCESS, emitter.Build(&out));
        std::vector<uint8_t> expected;
        for (int i = 0; i < num_ret; ++i) {
            expected << Opcode::RETURN_VOID;
        }
        expected << Opcode::JMP_IMM16 << Split16(-num_ret);
        ASSERT_EQ(expected, out);
    }
    {
        BytecodeEmitter emitter;
        Label label = emitter.CreateLabel();
        emitter.Bind(label);
        int num_ret = -std::numeric_limits<int16_t>::min();
        for (int i = 0; i < num_ret; ++i) {
            emitter.ReturnVoid();
        }
        emitter.Jmp(label);
        std::vector<uint8_t> out;
        ASSERT_EQ(BytecodeEmitter::ErrorCode::SUCCESS, emitter.Build(&out));
        std::vector<uint8_t> expected;
        for (int i = 0; i < num_ret; ++i) {
            expected << Opcode::RETURN_VOID;
        }
        expected << Opcode::JMP_IMM16 << Split16(-num_ret);
        ASSERT_EQ(expected, out);
    }
}

TEST(BytecodeEmitter, JmpFwd_IMM16)
{
    {
        BytecodeEmitter emitter;
        Label label = emitter.CreateLabel();
        emitter.Jmp(label);
        // -4 because 2 bytes takes jmp itself and
        // emitter estimate length of jmp by 3 greater the it is actually
        // and plus one byte to make 8bit overflow
        int num_ret = std::numeric_limits<int8_t>::max() - 4;
        for (int i = 0; i < num_ret; ++i) {
            emitter.ReturnVoid();
        }
        emitter.Bind(label);
        emitter.ReturnVoid();
        std::vector<uint8_t> out;
        ASSERT_EQ(BytecodeEmitter::ErrorCode::SUCCESS, emitter.Build(&out));
        std::vector<uint8_t> expected;
        expected << Opcode::JMP_IMM16 << Split16(num_ret + 3);
        for (int i = 0; i < num_ret + 1; ++i) {
            expected << Opcode::RETURN_VOID;
        }
        ASSERT_EQ(expected, out);
    }
    {
        BytecodeEmitter emitter;
        Label label = emitter.CreateLabel();
        emitter.Jmp(label);
        // -5 because 2 bytes takes jmp itself and
        // emitter estimate length of jmp by 3 greater the it is actually
        int num_ret = std::numeric_limits<int16_t>::max() - 5;
        for (int i = 0; i < num_ret; ++i) {
            emitter.ReturnVoid();
        }
        emitter.Bind(label);
        emitter.ReturnVoid();
        std::vector<uint8_t> out;
        ASSERT_EQ(BytecodeEmitter::ErrorCode::SUCCESS, emitter.Build(&out));
        std::vector<uint8_t> expected;
        expected << Opcode::JMP_IMM16 << Split16(num_ret + 3);
        for (int i = 0; i < num_ret + 1; ++i) {
            expected << Opcode::RETURN_VOID;
        }
        ASSERT_EQ(expected, out);
    }
}

static void EmitJmp(Opcode op, int32_t imm, std::vector<uint8_t> *out)
{
    *out << op;
    switch (op) {
        case Opcode::JMP_IMM8:
            *out << static_cast<int8_t>(imm);
            break;
        case Opcode::JMP_IMM16:
            *out << Split16(imm);
            break;
        default:
            *out << Split32(imm);
            break;
    }
}

static Opcode GetOpcode(size_t inst_size)
{
    switch (inst_size) {
        case 2:  // 2 -- opcode + imm8
            return Opcode::JMP_IMM8;
        case 3:  // 3 -- opcode + imm16
            return Opcode::JMP_IMM16;
        default:
            return Opcode::JMP_IMM32;
    }
}

/*
 * Emit bytecode for the following program:
 *
 * label1:
 * jmp label2
 * ...          <- n1 return.void instructions
 * jmp label1
 * ...          <- n2 return.void instructions
 * label2:
 * return.void
 */
static std::vector<uint8_t> EmitJmpFwdBwd(size_t n1, size_t n2)
{
    std::array<std::tuple<size_t, int32_t, int32_t>, 3U> jmps {
        std::tuple {2, std::numeric_limits<int8_t>::min(), std::numeric_limits<int8_t>::max()},
        std::tuple {3, std::numeric_limits<int16_t>::min(), std::numeric_limits<int16_t>::max()},
        std::tuple {5, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()}};

    std::vector<uint8_t> out;

    size_t jmp_size1, jmp_size2;
    int32_t imm_max1, imm_min2;

    for (const auto &t1 : jmps) {
        std::tie(jmp_size1, std::ignore, imm_max1) = t1;

        for (const auto &t2 : jmps) {
            std::tie(jmp_size2, imm_min2, std::ignore) = t2;

            int32_t imm1 = jmp_size1 + n1 + jmp_size2 + n2;
            int32_t imm2 = jmp_size1 + n1;

            if (imm1 <= imm_max1 && -imm2 >= imm_min2) {
                EmitJmp(GetOpcode(jmp_size1), imm1, &out);

                for (size_t i = 0; i < n1; i++) {
                    out << Opcode::RETURN_VOID;
                }

                EmitJmp(GetOpcode(jmp_size2), -imm2, &out);

                for (size_t i = 0; i < n2; i++) {
                    out << Opcode::RETURN_VOID;
                }

                out << Opcode::RETURN_VOID;

                return out;
            }
        }
    }

    return out;
}

/*
 * Test following control flow:
 *
 * label1:
 * jmp label2
 * ...          <- n1 return.void instructions
 * jmp label1
 * ...          <- n2 return.void instructions
 * label2:
 * return.void
 */
void TestJmpFwdBwd(size_t n1, size_t n2)
{
    BytecodeEmitter emitter;
    Label label1 = emitter.CreateLabel();
    Label label2 = emitter.CreateLabel();

    emitter.Bind(label1);
    emitter.Jmp(label2);
    for (size_t i = 0; i < n1; ++i) {
        emitter.ReturnVoid();
    }
    emitter.Jmp(label1);
    for (size_t i = 0; i < n2; ++i) {
        emitter.ReturnVoid();
    }
    emitter.Bind(label2);
    emitter.ReturnVoid();

    std::vector<uint8_t> out;
    ASSERT_EQ(BytecodeEmitter::ErrorCode::SUCCESS, emitter.Build(&out)) << "n1 = " << n1 << " n2 = " << n2;

    ASSERT_EQ(EmitJmpFwdBwd(n1, n2), out) << "n1 = " << n1 << " n2 = " << n2;
}

TEST(BytecodeEmitter, JmpFwdBwd)
{
    // fwd jmp imm16
    // bwd jmp imm8
    TestJmpFwdBwd(0, std::numeric_limits<int8_t>::max());

    // fwd jmp imm16
    // bwd jmp imm16
    TestJmpFwdBwd(std::numeric_limits<int8_t>::max(), 0);

    // fwd jmp imm32
    // bwd jmp imm8
    TestJmpFwdBwd(0, std::numeric_limits<int16_t>::max());

    // fwd jmp imm32
    // bwd jmp imm16
    TestJmpFwdBwd(std::numeric_limits<int8_t>::max(), std::numeric_limits<int16_t>::max());

    // fwd jmp imm32
    // bwd jmp imm32
    TestJmpFwdBwd(std::numeric_limits<int16_t>::max(), 0);
}

TEST(BytecodeEmitter, JmpBwd_IMM32)
{
    BytecodeEmitter emitter;
    Label label = emitter.CreateLabel();
    emitter.Bind(label);
    int num_ret = -std::numeric_limits<int16_t>::min() + 1;
    for (int i = 0; i < num_ret; ++i) {
        emitter.ReturnVoid();
    }
    emitter.Jmp(label);
    std::vector<uint8_t> out;
    ASSERT_EQ(BytecodeEmitter::ErrorCode::SUCCESS, emitter.Build(&out));
    std::vector<uint8_t> expected;
    for (int i = 0; i < num_ret; ++i) {
        expected << Opcode::RETURN_VOID;
    }
    expected << Opcode::JMP_IMM32 << Split32(-num_ret);
    ASSERT_EQ(expected, out);
}

TEST(BytecodeEmitter, JmpFwd_IMM32)
{
    BytecodeEmitter emitter;
    Label label = emitter.CreateLabel();
    emitter.Jmp(label);
    // -4 because 2 bytes takes jmp itself and
    // emitter estimate length of jmp by 3 greater the it is actually
    // and plus one byte to make 16bit overflow
    int num_ret = std::numeric_limits<int16_t>::max() - 4;
    for (int i = 0; i < num_ret; ++i) {
        emitter.ReturnVoid();
    }
    emitter.Bind(label);
    emitter.ReturnVoid();
    std::vector<uint8_t> out;
    ASSERT_EQ(BytecodeEmitter::ErrorCode::SUCCESS, emitter.Build(&out));
    std::vector<uint8_t> expected;
    expected << Opcode::JMP_IMM32 << Split32(num_ret + 5);
    for (int i = 0; i < num_ret + 1; ++i) {
        expected << Opcode::RETURN_VOID;
    }
    ASSERT_EQ(expected, out);
}

void JcmpBwd_V8_IMM8(Opcode opcode, std::function<void(BytecodeEmitter *, uint8_t, const Label &label)> emit_jcmp)
{
    BytecodeEmitter emitter;
    Label label = emitter.CreateLabel();
    emitter.Bind(label);
    int num_ret = 15;
    for (int i = 0; i < num_ret; ++i) {
        emitter.ReturnVoid();
    }
    emit_jcmp(&emitter, 15U, label);
    std::vector<uint8_t> out;
    ASSERT_EQ(BytecodeEmitter::ErrorCode::SUCCESS, emitter.Build(&out));
    std::vector<uint8_t> expected;
    for (int i = 0; i < num_ret; ++i) {
        expected << Opcode::RETURN_VOID;
    }

    expected << opcode << 15u << static_cast<uint8_t>(-num_ret);
    ASSERT_EQ(expected, out);
}

void JcmpFwd_V8_IMM8(Opcode opcode, std::function<void(BytecodeEmitter *, uint8_t, const Label &label)> emit_jcmp)
{
    BytecodeEmitter emitter;
    Label label = emitter.CreateLabel();
    emit_jcmp(&emitter, 15U, label);
    int num_ret = 12;
    for (int i = 0; i < num_ret; ++i) {
        emitter.ReturnVoid();
    }
    emitter.Bind(label);
    emitter.ReturnVoid();
    std::vector<uint8_t> out;
    ASSERT_EQ(BytecodeEmitter::ErrorCode::SUCCESS, emitter.Build(&out));
    std::vector<uint8_t> expected;
    // 2 bytes takes jmp itself and plus one byte to make.
    expected << opcode << 15u << static_cast<uint8_t>(num_ret + 2 + 1);

    for (int i = 0; i < num_ret + 1; ++i) {
        expected << Opcode::RETURN_VOID;
    }

    ASSERT_EQ(expected, out);
}

void JcmpBwd_V8_IMM16(Opcode opcode, std::function<void(BytecodeEmitter *, uint8_t, const Label &label)> emit_jcmp)
{
    {
        // Test min imm value
        BytecodeEmitter emitter;
        Label label = emitter.CreateLabel();
        emitter.Bind(label);
        int num_ret = -std::numeric_limits<int8_t>::min();
        ++num_ret;
        for (int i = 0; i < num_ret; ++i) {
            emitter.ReturnVoid();
        }
        emit_jcmp(&emitter, 0, label);
        std::vector<uint8_t> out;
        ASSERT_EQ(BytecodeEmitter::ErrorCode::SUCCESS, emitter.Build(&out));
        std::vector<uint8_t> expected;
        for (int i = 0; i < num_ret; ++i) {
            expected << Opcode::RETURN_VOID;
        }
        expected << opcode << 0u << Split16(-num_ret);
        ASSERT_EQ(expected, out);
    }
    {
        // Test max imm value
        BytecodeEmitter emitter;
        Label label = emitter.CreateLabel();
        emitter.Bind(label);
        int num_ret = -std::numeric_limits<int16_t>::min();
        for (int i = 0; i < num_ret; ++i) {
            emitter.ReturnVoid();
        }
        emit_jcmp(&emitter, 0, label);
        std::vector<uint8_t> out;
        ASSERT_EQ(BytecodeEmitter::ErrorCode::SUCCESS, emitter.Build(&out));
        std::vector<uint8_t> expected;
        for (int i = 0; i < num_ret; ++i) {
            expected << Opcode::RETURN_VOID;
        }
        expected << opcode << 0u << Split16(-num_ret);
        ASSERT_EQ(expected, out);
    }
}

void JcmpFwd_V8_IMM16(Opcode opcode, std::function<void(BytecodeEmitter *, uint8_t, const Label &label)> emit_jcmp)
{
    {
        // Test min imm
        BytecodeEmitter emitter;
        Label label = emitter.CreateLabel();
        emit_jcmp(&emitter, 0, label);
        // -3 because 4 bytes takes jmp itself
        // plus one to make 8bit overflow
        int num_ret = std::numeric_limits<int8_t>::max() - 3U;
        for (int i = 0; i < num_ret; ++i) {
            emitter.ReturnVoid();
        }
        emitter.Bind(label);
        emitter.ReturnVoid();
        std::vector<uint8_t> out;
        ASSERT_EQ(BytecodeEmitter::ErrorCode::SUCCESS, emitter.Build(&out));
        std::vector<uint8_t> expected;
        expected << opcode << 0u << Split16(num_ret + 4U);
        for (int i = 0; i < num_ret + 1; ++i) {
            expected << Opcode::RETURN_VOID;
        }
        ASSERT_EQ(expected, out);
    }
    {
        // Test max imm
        BytecodeEmitter emitter;
        Label label = emitter.CreateLabel();
        emit_jcmp(&emitter, 0, label);
        // -4 because 4 bytes takes jmp itself
        int num_ret = std::numeric_limits<int16_t>::max() - 4;
        for (int i = 0; i < num_ret; ++i) {
            emitter.ReturnVoid();
        }
        emitter.Bind(label);
        emitter.ReturnVoid();
        std::vector<uint8_t> out;
        ASSERT_EQ(BytecodeEmitter::ErrorCode::SUCCESS, emitter.Build(&out));
        std::vector<uint8_t> expected;
        expected << opcode << 0u << Split16(num_ret + 4U);
        for (int i = 0; i < num_ret + 1; ++i) {
            expected << Opcode::RETURN_VOID;
        }
        ASSERT_EQ(expected, out);
    }
}

TEST(BytecodeEmitter, Jne_V8_IMM8)
{
    BytecodeEmitter emitter;
    Label label = emitter.CreateLabel();
    emitter.Jne(0, label);
    emitter.Bind(label);
    emitter.ReturnVoid();
    std::vector<uint8_t> out;
    ASSERT_EQ(BytecodeEmitter::ErrorCode::SUCCESS, emitter.Build(&out));
    std::vector<uint8_t> expected;
    expected << Opcode::JNE_V8_IMM8 << 0 << 3 << Opcode::RETURN_VOID;
    ASSERT_EQ(expected, out);
}

TEST(BytecodeEmitter, Jne_V8_IMM16)
{
    BytecodeEmitter emitter;
    Label label = emitter.CreateLabel();
    emitter.Jcmp(Opcode::JNE_V8_IMM16, Opcode::JNE_V8_IMM16, 16, label);
    emitter.Bind(label);
    emitter.ReturnVoid();
    std::vector<uint8_t> out;
    ASSERT_EQ(BytecodeEmitter::ErrorCode::SUCCESS, emitter.Build(&out));
    std::vector<uint8_t> expected;
    expected << Opcode::JNE_V8_IMM16 << 16 << Split16(4) << Opcode::RETURN_VOID;
    ASSERT_EQ(expected, out);
}

void Jcmpz_IMM8(Opcode opcode, std::function<void(BytecodeEmitter *, const Label &label)> emit_jcmp)
{
    BytecodeEmitter emitter;
    Label label = emitter.CreateLabel();
    emit_jcmp(&emitter, label);
    emitter.Bind(label);
    emitter.ReturnVoid();
    std::vector<uint8_t> out;
    ASSERT_EQ(BytecodeEmitter::ErrorCode::SUCCESS, emitter.Build(&out));
    std::vector<uint8_t> expected;
    expected << opcode << 2U << Opcode::RETURN_VOID;
    ASSERT_EQ(expected, out);
}

void Jcmpz_IMM16(Opcode opcode, std::function<void(BytecodeEmitter *, const Label &label)> emit_jcmp)
{
    BytecodeEmitter emitter;
    Label label = emitter.CreateLabel();
    emit_jcmp(&emitter, label);
    for (size_t i = 0; i < std::numeric_limits<uint8_t>::max() - 2U; ++i) {
        emitter.ReturnVoid();
    }
    emitter.Bind(label);
    emitter.ReturnVoid();
    std::vector<uint8_t> out;
    ASSERT_EQ(BytecodeEmitter::ErrorCode::SUCCESS, emitter.Build(&out));
    std::vector<uint8_t> expected;
    expected << opcode << Split16(std::numeric_limits<uint8_t>::max() + 1);
    for (int i = 0; i < std::numeric_limits<uint8_t>::max() - 1; ++i) {
        expected << Opcode::RETURN_VOID;
    }
    ASSERT_EQ(expected, out);
}

TEST(BytecodeEmitter, JmpFwdCrossRef)
{
    // Situation:
    //         +---------+
    //    +----|----+    |
    //    |    |    |    |
    //    |    |    v    v
    // ---*----*----*----*----
    //             lbl1 lbl2
    BytecodeEmitter emitter;
    Label lbl1 = emitter.CreateLabel();
    Label lbl2 = emitter.CreateLabel();
    emitter.Jeq(0, lbl1);
    emitter.Jeq(0, lbl2);
    emitter.ReturnVoid();
    emitter.Bind(lbl1);
    emitter.ReturnVoid();
    for (int i = 0; i < 6; ++i) {
        emitter.ReturnVoid();
    }
    emitter.Bind(lbl2);
    emitter.ReturnVoid();
    std::vector<uint8_t> out;
    ASSERT_EQ(BytecodeEmitter::ErrorCode::SUCCESS, emitter.Build(&out));

    std::vector<uint8_t> expected;
    expected << Opcode::JEQ_V8_IMM8 << 0u << (9 - 2) << Opcode::JEQ_V8_IMM8 << 0u << (12 - 1);
    for (int i = 0; i < 9; ++i) {
        expected << Opcode::RETURN_VOID;
    }
    ASSERT_EQ(expected, out);
}

TEST(BytecodeEmitter, JmpBwdCrossRef)
{
    // Situation:
    //         +---------+
    //         |         |
    //    +---------+    |
    //    |    |    |    |
    //    v    |    |    v
    // ---*----*----*----*----
    //   lbl1           lbl2
    BytecodeEmitter emitter;
    Label lbl1 = emitter.CreateLabel();
    Label lbl2 = emitter.CreateLabel();
    emitter.Bind(lbl1);
    emitter.ReturnVoid();
    emitter.Jeq(0, lbl2);
    for (int i = 0; i < 5; ++i) {
        emitter.ReturnVoid();
    }
    emitter.Jeq(0, lbl1);
    emitter.Bind(lbl2);
    emitter.ReturnVoid();

    std::vector<uint8_t> out;
    ASSERT_EQ(BytecodeEmitter::ErrorCode::SUCCESS, emitter.Build(&out));

    std::vector<uint8_t> expected;
    expected << Opcode::RETURN_VOID << Opcode::JEQ_V8_IMM8 << 0u << (13 - 2);
    for (int i = 0; i < 5; ++i) {
        expected << Opcode::RETURN_VOID;
    }
    expected << Opcode::JEQ_V8_IMM8 << 0u << static_cast<uint8_t>(-9) << Opcode::RETURN_VOID;
    ASSERT_EQ(expected, out);
}

TEST(BytecodeEmitter, Jmp3FwdCrossRefs)
{
    // Situation:
    //     +--------+
    //    +|--------+
    //    ||+-------+---+
    //    |||       v   v
    // ---***-------*---*
    //            lbl1 lbl2
    BytecodeEmitter emitter;
    Label lbl1 = emitter.CreateLabel();
    Label lbl2 = emitter.CreateLabel();

    emitter.Jmp(lbl1);
    emitter.Jmp(lbl1);
    emitter.Jmp(lbl2);

    constexpr int32_t INT8T_MAX = std::numeric_limits<int8_t>::max();

    size_t n = INT8T_MAX - 4;
    for (size_t i = 0; i < n; i++) {
        emitter.ReturnVoid();
    }

    emitter.Bind(lbl1);
    emitter.ReturnVoid();
    emitter.ReturnVoid();
    emitter.Bind(lbl2);

    std::vector<uint8_t> out;
    ASSERT_EQ(BytecodeEmitter::ErrorCode::SUCCESS, emitter.Build(&out));

    std::vector<uint8_t> expected;
    expected << Opcode::JMP_IMM16 << Split16(INT8T_MAX + 5);
    expected << Opcode::JMP_IMM16 << Split16(INT8T_MAX + 2);
    expected << Opcode::JMP_IMM16 << Split16(INT8T_MAX + 1);
    for (size_t i = 0; i < n + 2; i++) {
        expected << Opcode::RETURN_VOID;
    }
    ASSERT_EQ(expected, out);
}

TEST(BytecodeEmitter, UnboundLabel)
{
    BytecodeEmitter emitter;
    Label label = emitter.CreateLabel();
    emitter.Bind(label);
    std::vector<uint8_t> out;
    ASSERT_EQ(BytecodeEmitter::ErrorCode::SUCCESS, emitter.Build(&out));
}

TEST(BytecodeEmitter, JumpToUnboundLabel)
{
    BytecodeEmitter emitter;
    Label label = emitter.CreateLabel();
    emitter.Jmp(label);
    std::vector<uint8_t> out;
    ASSERT_EQ(BytecodeEmitter::ErrorCode::UNBOUND_LABELS, emitter.Build(&out));
}

TEST(BytecodeEmitter, JumpToUnboundLabel2)
{
    BytecodeEmitter emitter;
    Label label1 = emitter.CreateLabel();
    Label label2 = emitter.CreateLabel();
    emitter.Jmp(label1);
    emitter.Bind(label2);
    emitter.Mov(0, 1);
    std::vector<uint8_t> out;
    ASSERT_EQ(BytecodeEmitter::ErrorCode::UNBOUND_LABELS, emitter.Build(&out));
}

TEST(BytecodeEmitter, TwoJumpsToOneLabel)
{
    BytecodeEmitter emitter;
    Label label = emitter.CreateLabel();
    emitter.Bind(label);
    emitter.Mov(0, 1);
    emitter.Jmp(label);
    emitter.Jmp(label);
    std::vector<uint8_t> out;
    ASSERT_EQ(BytecodeEmitter::ErrorCode::SUCCESS, emitter.Build(&out));
}

void TestNoneFormat(Opcode opcode, std::function<void(BytecodeEmitter *)> emit)
{
    BytecodeEmitter emitter;
    emit(&emitter);
    std::vector<uint8_t> out;
    ASSERT_EQ(BytecodeEmitter::ErrorCode::SUCCESS, emitter.Build(&out));
    std::vector<uint8_t> expected;
    expected << opcode;
    ASSERT_EQ(expected, out);
}

#include <bytecode_emitter_tests_gen.h>

}  // namespace panda