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

#ifndef PANDA_RUNTIME_INTERPRETER_INSTRUCTION_HANDLER_STATE_H_
#define PANDA_RUNTIME_INTERPRETER_INSTRUCTION_HANDLER_STATE_H_

#include "runtime/interpreter/state.h"
#include "runtime/jit/profiling_data.h"

namespace panda::interpreter {

class InstructionHandlerState {
public:
    ALWAYS_INLINE InstructionHandlerState(ManagedThread *thread, const uint8_t *pc, Frame *frame)
        : state_(thread, pc, frame)
    {
        instructions_ = GetFrame()->GetInstruction();
    }
    ~InstructionHandlerState() = default;
    DEFAULT_MOVE_SEMANTIC(InstructionHandlerState);
    DEFAULT_COPY_SEMANTIC(InstructionHandlerState);

    ALWAYS_INLINE void UpdateInstructionHandlerState(const uint8_t *pc, Frame *frame)
    {
        state_.UpdateState(pc, frame);
        instructions_ = GetFrame()->GetInstruction();
    }

    ALWAYS_INLINE ManagedThread *GetThread() const
    {
        return state_.GetThread();
    }

    ALWAYS_INLINE void SetThread(ManagedThread *thread)
    {
        state_.SetThread(thread);
    }

    ALWAYS_INLINE void SetInst(BytecodeInstruction inst)
    {
        state_.SetInst(inst);
    }

    ALWAYS_INLINE Frame *GetFrame() const
    {
        return state_.GetFrame();
    }

    ALWAYS_INLINE void SetFrame(Frame *frame)
    {
        state_.SetFrame(frame);
    }

    ALWAYS_INLINE const void *const *GetDispatchTable() const
    {
        return state_.GetDispatchTable();
    }

    ALWAYS_INLINE void SetDispatchTable(const void *const *dispatch_table)
    {
        return state_.SetDispatchTable(dispatch_table);
    }

    ALWAYS_INLINE void SaveState()
    {
        state_.SaveState();
    }

    ALWAYS_INLINE void RestoreState()
    {
        state_.RestoreState();
    }

    ALWAYS_INLINE uint16_t GetOpcodeExtension() const
    {
        return opcode_extension_;
    }

    ALWAYS_INLINE void SetOpcodeExtension(uint16_t opcode_extension)
    {
        opcode_extension_ = opcode_extension;
    }

    ALWAYS_INLINE uint8_t GetPrimaryOpcode() const
    {
        return static_cast<unsigned>(GetInst().GetOpcode()) & 0xff;
    }

    ALWAYS_INLINE uint8_t GetSecondaryOpcode() const
    {
        return (static_cast<unsigned>(GetInst().GetOpcode()) >> 8) & 0xff;
    }

    ALWAYS_INLINE bool IsPrimaryOpcodeValid() const
    {
        return GetInst().IsPrimaryOpcodeValid();
    }

    ALWAYS_INLINE BytecodeInstruction GetInst() const
    {
        return state_.GetInst();
    }

    ALWAYS_INLINE const AccVRegister &GetAcc() const
    {
        return state_.GetAcc();
    }

    ALWAYS_INLINE AccVRegister &GetAcc()
    {
        return state_.GetAcc();
    }

    ALWAYS_INLINE auto &GetFakeInstBuf()
    {
        return fake_inst_buf_;
    }

    ALWAYS_INLINE uint32_t GetBytecodeOffset() const
    {
        return GetInst().GetAddress() - instructions_;
    }

private:
    static constexpr size_t FAKE_INST_BUF_SIZE = 4;

    State state_;
    std::array<uint8_t, FAKE_INST_BUF_SIZE> fake_inst_buf_;
    uint16_t opcode_extension_ {0};
    const uint8_t *instructions_ {nullptr};
};

}  // namespace panda::interpreter

#endif  // PANDA_RUNTIME_INTERPRETER_INSTRUCTION_HANDLER_STATE_H_
