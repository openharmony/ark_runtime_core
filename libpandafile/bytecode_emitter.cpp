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
#include <bytecode_instruction-inl.h>
#include <macros.h>
#include <utils/bit_utils.h>
#include <utils/span.h>

namespace panda {

using Opcode = BytecodeInstruction::Opcode;
using Format = BytecodeInstruction::Format;
using BitImmSize = BytecodeEmitter::BitImmSize;

static BitImmSize GetBitLengthUnsigned(uint32_t val)
{
    static constexpr size_t BIT_4 = 4;
    static constexpr size_t BIT_8 = 8;

    auto bitlen = MinimumBitsToStore(val);
    if (bitlen <= BIT_4) {
        return BitImmSize::BITSIZE_4;
    }
    if (bitlen <= BIT_8) {
        return BitImmSize::BITSIZE_8;
    }
    return BitImmSize::BITSIZE_16;
}

static BitImmSize GetBitLengthSigned(int32_t val)
{
    static constexpr int32_t INT4T_MIN = -8;
    static constexpr int32_t INT4T_MAX = 7;
    static constexpr int32_t INT8T_MIN = std::numeric_limits<int8_t>::min();
    static constexpr int32_t INT8T_MAX = std::numeric_limits<int8_t>::max();
    static constexpr int32_t INT16T_MIN = std::numeric_limits<int16_t>::min();
    static constexpr int32_t INT16T_MAX = std::numeric_limits<int16_t>::max();
    if (INT4T_MIN <= val && val <= INT4T_MAX) {
        return BitImmSize::BITSIZE_4;
    }
    if (INT8T_MIN <= val && val <= INT8T_MAX) {
        return BitImmSize::BITSIZE_8;
    }
    if (INT16T_MIN <= val && val <= INT16T_MAX) {
        return BitImmSize::BITSIZE_16;
    }
    return BitImmSize::BITSIZE_32;
}

inline bool IsJcondImm8(Opcode opcode)
{
    switch (opcode) {
        case Opcode::JEQZ_IMM8:
        case Opcode::JNEZ_IMM8:
        case Opcode::JLTZ_IMM8:
        case Opcode::JGTZ_IMM8:
        case Opcode::JLEZ_IMM8:
        case Opcode::JGEZ_IMM8:
        case Opcode::JEQZ_OBJ_IMM8:
        case Opcode::JNEZ_OBJ_IMM8:
            return true;
        default:
            return false;
    }
}

inline bool IsJcondImm16(Opcode opcode)
{
    switch (opcode) {
        case Opcode::JEQZ_IMM16:
        case Opcode::JNEZ_IMM16:
        case Opcode::JLTZ_IMM16:
        case Opcode::JGTZ_IMM16:
        case Opcode::JLEZ_IMM16:
        case Opcode::JGEZ_IMM16:
        case Opcode::JEQZ_OBJ_IMM16:
        case Opcode::JNEZ_OBJ_IMM16:
            return true;
        default:
            return false;
    }
}

static bool IsJcondV8Imm8(Opcode opcode)
{
    switch (opcode) {
        case Opcode::JEQ_OBJ_V8_IMM8:
        case Opcode::JNE_OBJ_V8_IMM8:
        case Opcode::JEQ_V8_IMM8:
        case Opcode::JNE_V8_IMM8:
        case Opcode::JLT_V8_IMM8:
        case Opcode::JGT_V8_IMM8:
        case Opcode::JLE_V8_IMM8:
        case Opcode::JGE_V8_IMM8:
            return true;
        default:
            return false;
    }
}

inline bool IsJcondV8Imm16(Opcode opcode)
{
    switch (opcode) {
        case Opcode::JEQ_V8_IMM16:
        case Opcode::JNE_V8_IMM16:
        case Opcode::JLT_V8_IMM16:
        case Opcode::JGT_V8_IMM16:
        case Opcode::JLE_V8_IMM16:
        case Opcode::JGE_V8_IMM16:
        case Opcode::JEQ_OBJ_V8_IMM16:
        case Opcode::JNE_OBJ_V8_IMM16:
            return true;
        default:
            return false;
    }
}

static void EmitImpl([[maybe_unused]] Span<uint8_t> buf, [[maybe_unused]] Span<const uint8_t> offsets) {}

template <typename Type, typename... Types>
static void EmitImpl(Span<uint8_t> buf, Span<const uint8_t> offsets, Type arg, Types... args)
{
    static constexpr uint8_t BYTEMASK = 0xFF;
    static constexpr uint8_t BITMASK_4 = 0xF;
    static constexpr size_t BIT_4 = 4;
    static constexpr size_t BIT_8 = 8;
    static constexpr size_t BIT_16 = 16;
    static constexpr size_t BIT_32 = 32;
    static constexpr size_t BIT_64 = 64;

    uint8_t offset = offsets[0];
    size_t bitlen = offsets[1] - offsets[0];
    size_t byte_offset = offset / BIT_8;
    size_t bit_offset = offset % BIT_8;
    switch (bitlen) {
        case BIT_4: {
            auto val = static_cast<uint8_t>(arg);
            buf[byte_offset] |= static_cast<uint8_t>(static_cast<uint8_t>(val & BITMASK_4) << bit_offset);
            break;
        }
        case BIT_8: {
            auto val = static_cast<uint8_t>(arg);
            buf[byte_offset] = val;
            break;
        }
        case BIT_16: {
            auto val = static_cast<uint16_t>(arg);
            buf[byte_offset] = val & BYTEMASK;
            buf[byte_offset + 1] = val >> BIT_8;
            break;
        }
        case BIT_32: {
            auto val = static_cast<uint32_t>(arg);
            for (size_t i = 0; i < sizeof(uint32_t); i++) {
                buf[byte_offset + i] = (val >> (i * BIT_8)) & BYTEMASK;
            }
            break;
        }
        case BIT_64: {
            auto val = static_cast<uint64_t>(arg);
            for (size_t i = 0; i < sizeof(uint64_t); i++) {
                buf[byte_offset + i] = (val >> (i * BIT_8)) & BYTEMASK;
            }
            break;
        }
        default: {
            UNREACHABLE();
            break;
        }
    }
    EmitImpl(buf, offsets.SubSpan(1), args...);
}

template <Format format, typename It, typename... Types>
static size_t Emit(It out, Types... args);

void BytecodeEmitter::Bind(const Label &label)
{
    *label.pc_ = pc_;
    targets_.insert(label);
}

void BytecodeEmitter::Jmp(const Label &label)
{
    branches_.insert(std::make_pair(pc_, label));
    pc_ += Emit<Format::IMM8>(std::back_inserter(bytecode_), Opcode::JMP_IMM8, 0);
}

BytecodeEmitter::ErrorCode BytecodeEmitter::Build(std::vector<uint8_t> *output)
{
    ErrorCode res = CheckLabels();
    if (res != ErrorCode::SUCCESS) {
        return res;
    }
    res = ReserveSpaceForOffsets();
    if (res != ErrorCode::SUCCESS) {
        return res;
    }
    res = UpdateBranches();
    if (res != ErrorCode::SUCCESS) {
        return res;
    }
    *output = bytecode_;
    return ErrorCode::SUCCESS;
}

void BytecodeEmitter::Jcmp(Opcode opcode_short, Opcode opcode_long, uint8_t reg, const Label &label)
{
    branches_.insert(std::make_pair(pc_, label));
    if (GetBitLengthUnsigned(reg) <= BitImmSize::BITSIZE_8 &&
        GetBitImmSizeByOpcode(opcode_short) == BitImmSize::BITSIZE_8) {
        pc_ += Emit<Format::V8_IMM8>(std::back_inserter(bytecode_), opcode_short, reg, 0);
    } else {
        pc_ += Emit<Format::V8_IMM16>(std::back_inserter(bytecode_), opcode_long, reg, 0);
    }
}

void BytecodeEmitter::Jcmpz(BytecodeInstruction::Opcode opcode, const Label &label)
{
    branches_.insert(std::make_pair(pc_, label));
    pc_ += Emit<Format::IMM8>(std::back_inserter(bytecode_), opcode, 0);
}

/*
 * Note well! All conditional jumps with displacements not fitting into imm16
 * are transformed into two instructions:
 * jcc far   # cc is any condiitonal code
 *      =>
 * jCC next  # CC is inverted cc
 * jmp far
 * next:     # This label is inserted just after previous instruction.
 */
BytecodeEmitter::ErrorCode BytecodeEmitter::ReserveSpaceForOffsets()
{
    uint32_t bias = 0;
    std::map<uint32_t, Label> new_branches;
    auto it = branches_.begin();
    while (it != branches_.end()) {
        auto insn_pc = static_cast<uint32_t>(it->first + bias);
        auto label = it->second;

        auto opcode = static_cast<Opcode>(bytecode_[insn_pc]);
        const auto ENCODED_IMM_SIZE = GetBitImmSizeByOpcode(opcode);
        const auto REAL_IMM_SIZE = GetBitLengthSigned(EstimateMaxDistance(insn_pc, label.GetPc(), bias));

        auto new_target = insn_pc;
        size_t extra_bytes = 0;

        if (REAL_IMM_SIZE > ENCODED_IMM_SIZE) {
            auto res = DoReserveSpaceForOffset(opcode, insn_pc, REAL_IMM_SIZE, &extra_bytes, &new_target);
            if (res != ErrorCode::SUCCESS) {
                return res;
            }
        }

        new_branches.insert(std::make_pair(new_target, label));
        if (extra_bytes > 0) {
            bias += extra_bytes;
            UpdateLabelTargets(insn_pc, extra_bytes);
        }
        it = branches_.erase(it);
    }
    branches_ = std::move(new_branches);
    return ErrorCode::SUCCESS;
}

static uint8_t getRegJcond(const std::vector<uint8_t> &bytecode, uint32_t insn_pc, BitImmSize encoded_imm_size)
{
    switch (encoded_imm_size) {
        case BitImmSize::BITSIZE_4:
        case BitImmSize::BITSIZE_8:
            return BytecodeInstruction(bytecode.data()).JumpTo(insn_pc).GetVReg<Format::V8_IMM8, 0>();
        default:
            return BytecodeInstruction(bytecode.data()).JumpTo(insn_pc).GetVReg<Format::V8_IMM16, 0>();
    }
}

BytecodeEmitter::ErrorCode BytecodeEmitter::DoReserveSpaceForOffset(BytecodeInstruction::Opcode opcode,
                                                                    uint32_t insn_pc, BitImmSize expected_imm_size,
                                                                    size_t *extra_bytes_ptr, uint32_t *target_ptr)
{
    const auto INSN_SIZE = GetSizeByOpcode(opcode);
    const auto ENCODED_IMM_SIZE = GetBitImmSizeByOpcode(opcode);
    if (opcode == Opcode::JMP_IMM8) {
        if (expected_imm_size == BitImmSize::BITSIZE_16) {
            *extra_bytes_ptr = GetSizeByOpcode(Opcode::JMP_IMM16) - INSN_SIZE;
            bytecode_[insn_pc] = static_cast<uint8_t>(Opcode::JMP_IMM16);
            bytecode_.insert(bytecode_.begin() + insn_pc + INSN_SIZE, *extra_bytes_ptr, 0);
        } else if (expected_imm_size == BitImmSize::BITSIZE_32) {
            *extra_bytes_ptr = GetSizeByOpcode(Opcode::JMP_IMM32) - INSN_SIZE;
            bytecode_[insn_pc] = static_cast<uint8_t>(Opcode::JMP_IMM32);
            bytecode_.insert(bytecode_.begin() + insn_pc + INSN_SIZE, *extra_bytes_ptr, 0);
        }
    } else if ((IsJcondImm8(opcode) || IsJcondImm16(opcode))) {
        const auto EXTENDED_INSN_SIZE = GetSizeByOpcode(GetLongestConditionalJump(opcode));
        const auto NEEDS_FAR_JUMP = expected_imm_size == BitImmSize::BITSIZE_32;

        *extra_bytes_ptr = EXTENDED_INSN_SIZE - INSN_SIZE + (NEEDS_FAR_JUMP ? GetSizeByOpcode(Opcode::JMP_IMM32) : 0);
        ASSERT(*extra_bytes_ptr > 0);
        bytecode_.insert(bytecode_.begin() + insn_pc + INSN_SIZE, *extra_bytes_ptr, 0);

        if (NEEDS_FAR_JUMP) {
            Emit<Format::IMM16>(bytecode_.begin() + insn_pc, GetLongestConditionalJump(RevertConditionCode(opcode)),
                                EXTENDED_INSN_SIZE + GetSizeByOpcode(Opcode::JMP_IMM32));
            Emit<Format::IMM32>(bytecode_.begin() + insn_pc + EXTENDED_INSN_SIZE, Opcode::JMP_IMM32, 0);
            *target_ptr = insn_pc + EXTENDED_INSN_SIZE;
        } else {
            Emit<Format::IMM16>(bytecode_.begin() + insn_pc, GetLongestConditionalJump(opcode), 0);
        }
    } else if (IsJcondV8Imm16(opcode) || IsJcondV8Imm8(opcode)) {
        uint8_t reg = getRegJcond(bytecode_, insn_pc, ENCODED_IMM_SIZE);

        const auto EXTENDED_INSN_SIZE = GetSizeByOpcode(GetLongestConditionalJump(opcode));
        const auto NEEDS_FAR_JUMP = expected_imm_size == BitImmSize::BITSIZE_32;

        *extra_bytes_ptr = EXTENDED_INSN_SIZE - INSN_SIZE + (NEEDS_FAR_JUMP ? GetSizeByOpcode(Opcode::JMP_IMM32) : 0);
        ASSERT(*extra_bytes_ptr > 0);
        bytecode_.insert(bytecode_.begin() + insn_pc + INSN_SIZE, *extra_bytes_ptr, 0);

        if (NEEDS_FAR_JUMP) {
            Emit<Format::V8_IMM16>(bytecode_.begin() + insn_pc, GetLongestConditionalJump(RevertConditionCode(opcode)),
                                   reg, EXTENDED_INSN_SIZE + GetSizeByOpcode(Opcode::JMP_IMM32));
            Emit<Format::IMM32>(bytecode_.begin() + insn_pc + EXTENDED_INSN_SIZE, Opcode::JMP_IMM32, 0);
            *target_ptr = insn_pc + EXTENDED_INSN_SIZE;
        } else {
            Emit<Format::V8_IMM16>(bytecode_.begin() + insn_pc, GetLongestConditionalJump(opcode), reg, 0);
        }
    } else {
        return ErrorCode::INTERNAL_ERROR;
    }
    return ErrorCode::SUCCESS;
}

BytecodeEmitter::ErrorCode BytecodeEmitter::UpdateBranches()
{
    uint8_t *bytecode = bytecode_.data();
    for (std::pair<const uint32_t, Label> &branch : branches_) {
        uint32_t insn_pc = branch.first;
        Label label = branch.second;
        auto offset = static_cast<int32_t>(label.GetPc()) - static_cast<int32_t>(insn_pc);
        auto opcode = static_cast<Opcode>(bytecode_[insn_pc]);
        if (opcode == Opcode::JMP_IMM8) {
            Emit<Format::IMM8>(bytecode_.begin() + insn_pc, Opcode::JMP_IMM8, offset);
        } else if (opcode == Opcode::JMP_IMM16) {
            Emit<Format::IMM16>(bytecode_.begin() + insn_pc, Opcode::JMP_IMM16, offset);
        } else if (opcode == Opcode::JMP_IMM32) {
            Emit<Format::IMM32>(bytecode_.begin() + insn_pc, Opcode::JMP_IMM32, offset);
        } else if (IsJcondImm8(opcode)) {
            Emit<Format::IMM8>(bytecode_.begin() + insn_pc, opcode, offset);
        } else if (IsJcondImm16(opcode)) {
            Emit<Format::IMM16>(bytecode_.begin() + insn_pc, opcode, offset);
        } else if (IsJcondV8Imm8(opcode)) {
            uint8_t reg = BytecodeInstruction(bytecode).JumpTo(insn_pc).GetVReg<Format::V8_IMM8, 0>();
            Emit<Format::V8_IMM8>(bytecode_.begin() + insn_pc, opcode, reg, offset);
        } else if (IsJcondV8Imm16(opcode)) {
            uint8_t reg = BytecodeInstruction(bytecode).JumpTo(insn_pc).GetVReg<Format::V8_IMM16, 0>();
            Emit<Format::V8_IMM16>(bytecode_.begin() + insn_pc, opcode, reg, offset);
        } else {
            return ErrorCode::INTERNAL_ERROR;
        }
    }
    return ErrorCode::SUCCESS;
}

void BytecodeEmitter::UpdateLabelTargets(uint32_t pc, size_t bias)
{
    pc_list_.push_front(pc);
    Label fake(pc_list_.begin());
    std::list<Label> updated_labels;
    auto it = targets_.upper_bound(fake);
    while (it != targets_.end()) {
        Label label = *it;
        it = targets_.erase(it);
        *label.pc_ += bias;
        updated_labels.push_back(label);
    }
    targets_.insert(updated_labels.begin(), updated_labels.end());
    pc_list_.pop_front();
}

static int32_t EstimateInsnSizeMaxIncrease(Opcode opcode)
{
    static constexpr int32_t JMP_IMM8_OFFSET_INCREASE = 3;
    static constexpr int32_t JCOND_IMM8_OFFSET_INCREASE = 1;
    static constexpr int32_t JCOND_V8_IMM8_OFFSET_INCREASE = 2;

    switch (opcode) {
        case Opcode::JMP_IMM8:
            return JMP_IMM8_OFFSET_INCREASE;
        case Opcode::JEQZ_IMM8:
        case Opcode::JNEZ_IMM8:
        case Opcode::JLTZ_IMM8:
        case Opcode::JGTZ_IMM8:
        case Opcode::JLEZ_IMM8:
        case Opcode::JGEZ_IMM8:
        case Opcode::JEQZ_OBJ_IMM8:
        case Opcode::JNEZ_OBJ_IMM8:
        case Opcode::JEQ_OBJ_V8_IMM8:
        case Opcode::JNE_OBJ_V8_IMM8:
        case Opcode::JEQ_V8_IMM8:
        case Opcode::JNE_V8_IMM8:
        case Opcode::JLT_V8_IMM8:
        case Opcode::JGT_V8_IMM8:
        case Opcode::JLE_V8_IMM8:
        case Opcode::JGE_V8_IMM8:
            return JCOND_IMM8_OFFSET_INCREASE;
        case Opcode::JEQ_V8_IMM16:
        case Opcode::JNE_V8_IMM16:
        case Opcode::JLT_V8_IMM16:
        case Opcode::JGT_V8_IMM16:
        case Opcode::JLE_V8_IMM16:
        case Opcode::JGE_V8_IMM16:
        case Opcode::JEQ_OBJ_V8_IMM16:
        case Opcode::JNE_OBJ_V8_IMM16:
            return JCOND_V8_IMM8_OFFSET_INCREASE;
        default:
            return 0;
    }
}

int32_t BytecodeEmitter::EstimateMaxDistance(uint32_t insn_pc, uint32_t target_pc, uint32_t bias) const
{
    int32_t distance = 0;
    uint32_t end_pc = 0;
    std::map<uint32_t, Label>::const_iterator it;
    if (target_pc > insn_pc) {
        it = branches_.lower_bound(insn_pc - bias);
        distance = static_cast<int32_t>(target_pc - insn_pc);
        end_pc = target_pc - bias;
    } else if (target_pc < insn_pc) {
        it = branches_.lower_bound(target_pc - bias);
        distance = static_cast<int32_t>(target_pc - insn_pc);
        end_pc = insn_pc - bias;
    } else {
        return 0;
    }

    while (it != branches_.end() && it->first < end_pc) {
        auto opcode = static_cast<Opcode>(bytecode_[it->first + bias]);
        distance += EstimateInsnSizeMaxIncrease(opcode);
        ++it;
    }
    return distance;
}

BytecodeEmitter::ErrorCode BytecodeEmitter::CheckLabels() const
{
    for (const std::pair<const uint32_t, Label> &branch : branches_) {
        const Label &label = branch.second;
        if (targets_.find(label) == targets_.end()) {
            return ErrorCode::UNBOUND_LABELS;
        }
    }
    return ErrorCode::SUCCESS;
}

#include <bytecode_emitter_gen.h>

}  // namespace panda
