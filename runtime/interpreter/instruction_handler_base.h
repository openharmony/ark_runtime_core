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

#ifndef PANDA_RUNTIME_INTERPRETER_INSTRUCTION_HANDLER_BASE_H_
#define PANDA_RUNTIME_INTERPRETER_INSTRUCTION_HANDLER_BASE_H_

#include <isa_constants_gen.h>
#include "runtime/interpreter/instruction_handler_state.h"

namespace panda::interpreter {

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_INST()                                                                           \
    LOG(DEBUG, INTERPRETER) << std::hex << std::setw(sizeof(uintptr_t)) << std::setfill('0') \
                            << reinterpret_cast<uintptr_t>(this->GetInst().GetAddress()) << std::dec << ": "

template <class RuntimeIfaceT, bool enable_instrumentation>
class InstructionHandlerBase {
public:
    ALWAYS_INLINE InstructionHandlerBase(InstructionHandlerState *state) : state_(state) {}
    ~InstructionHandlerBase() = default;
    DEFAULT_MOVE_SEMANTIC(InstructionHandlerBase);
    DEFAULT_COPY_SEMANTIC(InstructionHandlerBase);

    ALWAYS_INLINE uint16_t GetExceptionOpcode() const
    {
        // Need to call GetInst().GetOpcode() in this case too, otherwise compiler can generate non optimal code
        return (static_cast<unsigned>(GetInst().GetOpcode()) & 0xff) + state_->GetOpcodeExtension();
    }

    ALWAYS_INLINE uint8_t GetPrimaryOpcode() const
    {
        return static_cast<unsigned>(GetInst().GetOpcode()) & 0xff;
    }

    ALWAYS_INLINE uint8_t GetSecondaryOpcode() const
    {
        return (static_cast<unsigned>(GetInst().GetOpcode()) >> 8) & 0xff;
    }

    void DumpVRegs()
    {
#ifndef NDEBUG
        static constexpr uint64_t STANDARD_DEBUG_INDENT = 5;
        LOG(DEBUG, INTERPRETER) << PandaString(STANDARD_DEBUG_INDENT, ' ') << "acc." << GetAcc().DumpVReg();
        for (size_t i = 0; i < GetFrame()->GetSize(); ++i) {
            LOG(DEBUG, INTERPRETER) << PandaString(STANDARD_DEBUG_INDENT, ' ') << "v" << i << "."
                                    << GetFrame()->GetVReg(i).DumpVReg();
        }
#endif
    }

    ALWAYS_INLINE uint32_t UpdateBytecodeOffset()
    {
        auto pc = GetBytecodeOffset();
        GetFrame()->SetBytecodeOffset(pc);
        return pc;
    }

    void InstrumentInstruction()
    {
        if (!enable_instrumentation) {
            return;
        }

        // Should set ACC to Frame, so that ACC will be marked when GC
        GetFrame()->SetAcc(GetAcc());

        auto pc = UpdateBytecodeOffset();
        RuntimeIfaceT::GetNotificationManager()->BytecodePcChangedEvent(GetThread(), GetFrame()->GetMethod(), pc);

        // BytecodePcChangedEvent hook can call the GC, so we need to update the ACC
        GetAcc() = GetFrame()->GetAcc();
    }

    void InstrumentForceReturn()
    {
        Frame::VRegister result;  // empty result, because forced exit
        GetAcc() = result;
        GetFrame()->GetAcc() = result;
    }

    ALWAYS_INLINE const AccVRegister &GetAcc() const
    {
        return state_->GetAcc();
    }

    ALWAYS_INLINE AccVRegister &GetAcc()
    {
        return state_->GetAcc();
    }

    ALWAYS_INLINE BytecodeInstruction GetInst() const
    {
        return state_->GetInst();
    }

    void DebugDump();

    ALWAYS_INLINE Frame *GetFrame() const
    {
        return state_->GetFrame();
    }

    ALWAYS_INLINE void SetFrame(Frame *frame)
    {
        state_->SetFrame(frame);
    }

protected:
    template <BytecodeInstruction::Format format, bool can_throw>
    ALWAYS_INLINE void MoveToNextInst()
    {
        SetInst(GetInst().template GetNext<format>());

        if (can_throw) {
            SetOpcodeExtension(0);
        }
    }

    template <bool can_throw>
    ALWAYS_INLINE void JumpToInst(int32_t offset)
    {
        SetInst(GetInst().JumpTo(offset));

        if (can_throw) {
            SetOpcodeExtension(0);
        }
    }

    template <bool can_throw>
    ALWAYS_INLINE void JumpTo(const uint8_t *pc)
    {
        SetInst(BytecodeInstruction(pc));

        if (can_throw) {
            SetOpcodeExtension(0);
        }
    }

    ALWAYS_INLINE void MoveToExceptionHandler()
    {
        SetOpcodeExtension(NUM_OPS + NUM_PREFIXES - 1);
        SetOpcodeExtension(GetOpcodeExtension() - GetPrimaryOpcode());
    }

    ALWAYS_INLINE ManagedThread *GetThread() const
    {
        return state_->GetThread();
    }

    ALWAYS_INLINE void SetThread(ManagedThread *thread)
    {
        state_->SetThread(thread);
    }

    ALWAYS_INLINE void SetInst(BytecodeInstruction inst)
    {
        state_->SetInst(inst);
    }

    ALWAYS_INLINE const void *const *GetDispatchTable() const
    {
        return state_->GetDispatchTable();
    }

    ALWAYS_INLINE void SetDispatchTable(const void *const *dispatch_table)
    {
        return state_->SetDispatchTable(dispatch_table);
    }

    ALWAYS_INLINE void SaveState()
    {
        state_->SaveState();
    }

    ALWAYS_INLINE void RestoreState()
    {
        state_->RestoreState();
    }

    ALWAYS_INLINE uint16_t GetOpcodeExtension() const
    {
        return state_->GetOpcodeExtension();
    }

    ALWAYS_INLINE void SetOpcodeExtension(uint16_t opcode_extension)
    {
        state_->SetOpcodeExtension(opcode_extension);
    }

    ALWAYS_INLINE auto &GetFakeInstBuf()
    {
        return state_->GetFakeInstBuf();
    }

    ALWAYS_INLINE void UpdateHotness(Method *method)
    {
        method->IncrementHotnessCounter(0, nullptr);
    }

    ALWAYS_INLINE uint32_t GetBytecodeOffset() const
    {
        return state_->GetBytecodeOffset();
    }

    ALWAYS_INLINE InstructionHandlerState *GetInstructionHandlerState()
    {
        return state_;
    }

private:
    InstructionHandlerState *state_;
};

}  // namespace panda::interpreter

#endif  // PANDA_RUNTIME_INTERPRETER_INSTRUCTION_HANDLER_BASE_H_
