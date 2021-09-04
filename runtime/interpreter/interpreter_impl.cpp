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

#include "runtime/interpreter/interpreter_impl.h"

#include "libpandabase/macros.h"
#include "runtime/interpreter/interpreter-inl.h"
#include "runtime/interpreter/runtime_interface.h"

namespace panda::interpreter {

void ExecuteImpl(ManagedThread *thread, const uint8_t *pc, Frame *frame, bool jump_to_eh)
{
    const uint8_t *inst_ = frame->GetMethod()->GetInstructions();
    frame->SetInstruction(inst_);
    if (UNLIKELY(Runtime::GetCurrent()->IsDebugMode())) {
        if (jump_to_eh) {
            ExecuteImpl_Inner<RuntimeInterface, true, true>(thread, pc, frame);
        } else {
            ExecuteImpl_Inner<RuntimeInterface, true, false>(thread, pc, frame);
        }
    } else {
        if (jump_to_eh) {
            ExecuteImpl_Inner<RuntimeInterface, false, true>(thread, pc, frame);
        } else {
            ExecuteImpl_Inner<RuntimeInterface, false, false>(thread, pc, frame);
        }
    }
}

// Methods for debugging

template <class RuntimeIfaceT, bool enable_instrumentation>
void InstructionHandlerBase<RuntimeIfaceT, enable_instrumentation>::DebugDump()
{
#ifndef NDEBUG
    auto frame = GetFrame();
    auto method = frame->GetMethod();
    PandaString pad = "     ";
    std::cerr << "Method " << method->GetFullName(true) << std::endl;
    std::cerr << pad << "nargs = " << method->GetNumArgs() << std::endl;
    std::cerr << pad << "nregs = " << method->GetNumVregs() << std::endl;
    std::cerr << pad << "total frame size = " << frame->GetSize() << std::endl;
    std::cerr << "Frame:" << std::endl;
    std::cerr << pad << "acc." << GetAcc().DumpVReg() << std::endl;
    for (size_t i = 0; i < frame->GetSize(); ++i) {
        std::cerr << pad << "v" << i << "." << frame->GetVReg(i).DumpVReg() << std::endl;
    }
    std::cerr << "Bytecode:" << std::endl;
    size_t offset = 0;
    BytecodeInstruction inst(method->GetInstructions());
    while (offset < method->GetCodeSize()) {
        if (inst.GetAddress() == GetInst().GetAddress()) {
            std::cerr << "  -> ";
        } else {
            std::cerr << "     ";
        }

        std::cerr << std::hex << std::setw(sizeof(uintptr_t)) << std::setfill('0')
                  << reinterpret_cast<uintptr_t>(inst.GetAddress()) << std::dec << ": " << inst << std::endl;
        offset += inst.GetSize();
        inst = inst.GetNext();
    }
#endif  // NDEBUG
}

void EnsureDebugMethodsInstantiation(void *handler)
{
    static_cast<InstructionHandlerBase<RuntimeInterface, false> *>(handler)->DebugDump();
    static_cast<InstructionHandlerBase<RuntimeInterface, true> *>(handler)->DebugDump();
}

}  // namespace panda::interpreter
