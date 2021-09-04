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

#include "runtime/bridge/bridge.h"

#include "libpandafile/bytecode_instruction-inl.h"
#include "runtime/entrypoints/entrypoints.h"
#include "runtime/interpreter/interpreter.h"
#include "bytecode_instruction.h"
#include "bytecode_instruction-inl.h"

namespace panda {

// Actually it is wrong signature but it is the only way to make linker don't remove this function
extern "C" void CompiledCodeToInterpreterBridge(Method *);
extern "C" void CompiledCodeToInterpreterBridgeDyn(Method *);
extern "C" void AbstractMethodStub();

const void *GetCompiledCodeToInterpreterBridge(const Method *method)
{
    ASSERT(method != nullptr);
    const void *bridge = nullptr;

    if (method->GetClass() == nullptr) {
        bridge = reinterpret_cast<const void *>(CompiledCodeToInterpreterBridgeDyn);
    } else {
        if (method->GetClass()->GetSourceLang() == panda_file::SourceLang::ECMASCRIPT) {
            bridge = reinterpret_cast<const void *>(CompiledCodeToInterpreterBridgeDyn);
        } else {
            bridge = reinterpret_cast<const void *>(CompiledCodeToInterpreterBridge);
        }
    }
    return bridge;
}

/**
 * This function is supposed to be called from the deoptimization code. It aims to call interpreter for given frame
 * from specific pc. Note, that it releases input interpreter's frame at the exit.
 */
extern "C" int64_t InvokeInterpreter(ManagedThread *thread, const uint8_t *pc, Frame *frame, Frame *last_frame)
{
    bool prev_frame_kind = thread->IsCurrentFrameCompiled();
    thread->SetCurrentFrame(frame);
    thread->SetCurrentFrameIsCompiled(false);
    LOG(DEBUG, INTEROP) << "InvokeInterpreter for method: " << frame->GetMethod()->GetFullName();

    interpreter::Execute(thread, pc, frame, thread->HasPendingException());

    auto acc = frame->GetAcc();
    auto res = acc.HasObject() ? static_cast<int64_t>(bit_cast<uintptr_t>(acc.GetReference())) : acc.GetLong();

    auto prev_frame = frame->GetPrevFrame();
    thread->SetCurrentFrame(prev_frame);
    FreeFrame(frame);

    // We need to execute (find catch block) in all inlined methods. For this we use the number of inlined method.
    // Else we can execute previous interpreter frames and we will FreeFrames in incorrect order.
    while (prev_frame != nullptr && last_frame != frame) {
        ASSERT(!StackWalker::IsBoundaryFrame<FrameKind::INTERPRETER>(prev_frame));
        frame = prev_frame;
        LOG(DEBUG, INTEROP) << "InvokeInterpreter for method: " << frame->GetMethod()->GetFullName();
        prev_frame = frame->GetPrevFrame();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        pc = frame->GetMethod()->GetInstructions() + frame->GetBytecodeOffset();
        if (!thread->HasPendingException()) {
            auto bc_inst = BytecodeInstruction(pc);
            auto opcode = bc_inst.GetOpcode();
            // Compiler splits InitObj to NewObject + CallStatic
            // if we have a deoptimization occurred in the CallStatic, we must not copy acc from CallStatic,
            // because acc contains result of the NewObject
            if (opcode != BytecodeInstruction::Opcode::INITOBJ_SHORT_V4_V4_ID16 &&
                opcode != BytecodeInstruction::Opcode::INITOBJ_V4_V4_V4_V4_ID16 &&
                opcode != BytecodeInstruction::Opcode::INITOBJ_RANGE_V8_ID16) {
                frame->GetAcc() = acc;
            }
            pc = bc_inst.GetNext().GetAddress();
        } else {
            frame->GetAcc() = acc;
        }
        interpreter::Execute(thread, pc, frame, thread->HasPendingException());

        acc = frame->GetAcc();
        res = acc.HasObject() ? static_cast<int64_t>(bit_cast<uintptr_t>(acc.GetReference())) : acc.GetLong();

        thread->SetCurrentFrame(prev_frame);
        FreeFrame(frame);
    }
    thread->SetCurrentFrameIsCompiled(prev_frame_kind);

    return res;
}

}  // namespace panda
