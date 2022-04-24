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

#ifndef PANDA_RUNTIME_INTERPRETER_INTERPRETER_INL_H_
#define PANDA_RUNTIME_INTERPRETER_INTERPRETER_INL_H_

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <limits>
#include <memory>
#include <type_traits>
#include <unordered_map>

#include "bytecode_instruction.h"
#include "libpandabase/events/events.h"
#include "libpandabase/macros.h"
#include "libpandabase/utils/logger.h"
#include "libpandabase/utils/type_helpers.h"
#include "libpandafile/bytecode_instruction-inl.h"
#include "libpandafile/file_items.h"
#include "libpandafile/shorty_iterator.h"
#include "runtime/bridge/bridge.h"
#include "runtime/include/class.h"
#include "runtime/include/coretypes/array-inl.h"
#include "runtime/include/coretypes/dyn_objects.h"
#include "runtime/include/coretypes/string.h"
#include "runtime/include/exceptions.h"
#include "runtime/include/locks.h"
#include "runtime/include/method-inl.h"
#include "runtime/include/object_header-inl.h"
#include "runtime/include/runtime.h"
#include "runtime/include/runtime_notification.h"
#include "runtime/include/thread.h"
#include "runtime/include/value-inl.h"
#include "runtime/interpreter/acc_vregister.h"
#include "runtime/interpreter/arch/macros.h"
#include "runtime/interpreter/dispatch_table.h"
#include "runtime/interpreter/frame.h"
#include "runtime/interpreter/instruction_handler_base.h"
#include "runtime/interpreter/math_helpers.h"
#include "runtime/interpreter/runtime_interface.h"
#include "runtime/interpreter/vregister_iterator.h"
#include "runtime/jit/profiling_data.h"
#include "runtime/mem/vm_handle.h"
#include "runtime/handle_base-inl.h"

// ALWAYS_INLINE is mandatory attribute for handlers. There are cases which will be failed without it.

namespace panda::interpreter {

template <class RuntimeIfaceT, bool enable_instrumentation, bool jump_to_eh = false>
void ExecuteImpl(ManagedThread *thread, const uint8_t *pc, Frame *frame);

template <class RuntimeIfaceT, bool enable_instrumentation, bool jump_to_eh = false>
void ExecuteImpl_Inner(ManagedThread *thread, const uint8_t *pc, Frame *frame);

template <BytecodeInstruction::Format format>
class DimIterator final : VRegisterIterator<format> {
public:
    explicit DimIterator(BytecodeInstruction insn, Frame *frame) : VRegisterIterator<format>(std::move(insn), frame) {}

    ALWAYS_INLINE inline int32_t Get(size_t param_idx) const
    {
        // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE)
        return this->template GetAs<int32_t>(param_idx);
    }
};

template <class RuntimeIfaceT, bool enable_instrumentation>
class InstructionHandler : public InstructionHandlerBase<RuntimeIfaceT, enable_instrumentation> {
public:
    ALWAYS_INLINE inline InstructionHandler(InstructionHandlerState *state)
        : InstructionHandlerBase<RuntimeIfaceT, enable_instrumentation>(state)
    {
    }

#include "unimplemented_handlers-inl.h"

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleNop()
    {
        LOG_INST() << "nop";
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleFldaiDyn()
    {
        auto imm = bit_cast<double>(this->GetInst().template GetImm<format>());
        LOG_INST() << "fldai.dyn " << imm;
        this->GetAcc().SetValue(coretypes::TaggedValue(imm).GetRawData());
        auto ctx = this->GetThread()->GetLanguageContext();
        auto tag = ctx.GetTypeTag(TypeTag::DOUBLE);
        this->GetAcc().SetTag(tag);
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleLdaiDyn()
    {
        int32_t imm = this->GetInst().template GetImm<format>();
        LOG_INST() << "ldai.dyn " << std::hex << imm;
        this->GetAcc().SetValue(coretypes::TaggedValue(imm).GetRawData());
        auto ctx = this->GetThread()->GetLanguageContext();
        auto tag = ctx.GetTypeTag(TypeTag::INT);
        this->GetAcc().SetTag(tag);
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleMov()
    {
        uint16_t vd = this->GetInst().template GetVReg<format, 0>();
        uint16_t vs = this->GetInst().template GetVReg<format, 1>();
        LOG_INST() << "mov v" << vd << ", v" << vs;
        this->GetFrame()->GetVReg(vd).MoveFrom(this->GetFrame()->GetVReg(vs));
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleMovWide()
    {
        uint16_t vd = this->GetInst().template GetVReg<format, 0>();
        uint16_t vs = this->GetInst().template GetVReg<format, 1>();
        LOG_INST() << "mov.64 v" << vd << ", v" << vs;
        this->GetFrame()->GetVReg(vd).MoveFrom(this->GetFrame()->GetVReg(vs));
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleMovObj()
    {
        uint16_t vd = this->GetInst().template GetVReg<format, 0>();
        uint16_t vs = this->GetInst().template GetVReg<format, 1>();
        LOG_INST() << "mov.obj v" << vd << ", v" << vs;
        this->GetFrame()->GetVReg(vd).MoveFromObj(this->GetFrame()->GetVReg(vs));
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleMovDyn()
    {
        uint16_t vd = this->GetInst().template GetVReg<format, 0>();
        uint16_t vs = this->GetInst().template GetVReg<format, 1>();
        LOG_INST() << "mov.dyn v" << vd << ", v" << vs;
        this->GetFrame()->GetVReg(vd).Move(this->GetFrame()->GetVReg(vs));
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleMovi()
    {
        int32_t imm = this->GetInst().template GetImm<format>();
        uint16_t vd = this->GetInst().template GetVReg<format>();
        LOG_INST() << "movi v" << vd << ", " << std::hex << imm;
        this->GetFrame()->GetVReg(vd).SetPrimitive(imm);
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleMoviWide()
    {
        int64_t imm = this->GetInst().template GetImm<format>();
        uint16_t vd = this->GetInst().template GetVReg<format>();
        LOG_INST() << "movi.64 v" << vd << ", " << std::hex << imm;
        this->GetFrame()->GetVReg(vd).SetPrimitive(imm);
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleFmovi()
    {
        auto imm = bit_cast<float>(this->GetInst().template GetImm<format>());
        uint16_t vd = this->GetInst().template GetVReg<format>();
        LOG_INST() << "fmovi v" << vd << ", " << imm;
        this->GetFrame()->GetVReg(vd).SetPrimitive(imm);
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleFmoviWide()
    {
        auto imm = bit_cast<double>(this->GetInst().template GetImm<format>());
        uint16_t vd = this->GetInst().template GetVReg<format>();
        LOG_INST() << "fmovi.64 v" << vd << ", " << imm;
        this->GetFrame()->GetVReg(vd).SetPrimitive(imm);
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleMovNull()
    {
        uint16_t vd = this->GetInst().template GetVReg<format>();
        LOG_INST() << "mov.null v" << vd;
        this->GetFrame()->GetVReg(vd).SetReference(nullptr);
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleLda()
    {
        uint16_t vs = this->GetInst().template GetVReg<format>();
        LOG_INST() << "lda v" << vs;
        this->GetAcc().SetPrimitive(this->GetFrame()->GetVReg(vs).Get());
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleLdaWide()
    {
        uint16_t vs = this->GetInst().template GetVReg<format>();
        LOG_INST() << "lda.64 v" << vs;
        this->GetAcc().SetPrimitive(this->GetFrame()->GetVReg(vs).GetLong());
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleLdaObj()
    {
        uint16_t vs = this->GetInst().template GetVReg<format>();
        LOG_INST() << "lda.obj v" << vs;
        this->GetAcc().SetReference(this->GetFrame()->GetVReg(vs).GetReference());
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleLdai()
    {
        int32_t imm = this->GetInst().template GetImm<format>();
        LOG_INST() << "ldai " << std::hex << imm;
        this->GetAcc().SetPrimitive(imm);
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleLdaiWide()
    {
        int64_t imm = this->GetInst().template GetImm<format>();
        LOG_INST() << "ldai.64 " << std::hex << imm;
        this->GetAcc().SetPrimitive(imm);
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleFldai()
    {
        auto imm = bit_cast<float>(this->GetInst().template GetImm<format>());
        LOG_INST() << "fldai " << imm;
        this->GetAcc().SetPrimitive(imm);
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleFldaiWide()
    {
        auto imm = bit_cast<double>(this->GetInst().template GetImm<format>());
        LOG_INST() << "fldai.64 " << imm;
        this->GetAcc().SetPrimitive(imm);
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleLdaStr()
    {
        auto string_id = this->GetInst().template GetId<format>();
        LOG_INST() << "lda.str " << std::hex << string_id;
        coretypes::String *str = ResolveString(string_id);
        this->GetAcc().SetReference(str);
        auto ctx = this->GetThread()->GetLanguageContext();
        auto tag = ctx.GetTypeTag(TypeTag::STRING);
        this->GetAcc().SetTag(tag);
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleLdaConst()
    {
        auto litarr_id = this->GetInst().template GetId<format>();
        uint16_t vd = this->GetInst().template GetVReg<format>();

        LOG_INST() << "lda.const v" << vd << ", " << std::hex << litarr_id;
        auto array = ResolveLiteralArray(litarr_id);
        if (UNLIKELY(array == nullptr)) {
            this->MoveToExceptionHandler();
        } else {
            this->GetFrame()->GetVReg(vd).SetReference(array);
            this->template MoveToNextInst<format, false>();
        }
    }

    template <class T>
    ALWAYS_INLINE bool CheckLoadConstOp(coretypes::Array *array, T elem)
    {
        if constexpr (std::is_same_v<T, ObjectHeader *>) {
            if (elem != nullptr) {
                auto *array_class = array->ClassAddr<Class>();
                auto *element_class = array_class->GetComponentType();
                if (UNLIKELY(!elem->IsInstanceOf(element_class))) {
                    RuntimeIfaceT::ThrowArrayStoreException(array_class, elem->template ClassAddr<Class>());
                    return false;
                }
            }
        }
        return true;
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleLdaType()
    {
        auto type_id = this->GetInst().template GetId<format>();
        LOG_INST() << "lda.type " << std::hex << type_id;
        Class *type = ResolveType(type_id);
        if (LIKELY(type != nullptr)) {
            this->GetAcc().SetReference(type->GetManagedObject());
            this->template MoveToNextInst<format, false>();
        } else {
            this->MoveToExceptionHandler();
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleLdaNull()
    {
        LOG_INST() << "lda.null";
        this->GetAcc().SetReference(nullptr);
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleSta()
    {
        uint16_t vd = this->GetInst().template GetVReg<format>();
        LOG_INST() << "sta v" << vd;
        this->GetFrame()->GetVReg(vd).SetPrimitive(this->GetAcc().Get());
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleStaWide()
    {
        uint16_t vd = this->GetInst().template GetVReg<format>();
        LOG_INST() << "sta.64 v" << vd;
        this->GetFrame()->GetVReg(vd).SetPrimitive(this->GetAcc().GetLong());
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleStaObj()
    {
        uint16_t vd = this->GetInst().template GetVReg<format>();
        LOG_INST() << "sta.obj v" << vd;
        this->GetFrame()->GetVReg(vd).SetReference(this->GetAcc().GetReference());
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleStaDyn()
    {
        uint16_t vd = this->GetInst().template GetVReg<format>();
        LOG_INST() << "sta.dyn v" << vd;
        this->GetFrame()->GetVReg(vd).Move(this->GetAcc());
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleJmp()
    {
        int32_t imm = this->GetInst().template GetImm<format>();
        LOG_INST() << "jmp " << std::hex << imm;
        if (!InstrumentBranches(imm)) {
            this->template JumpToInst<false>(imm);
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleCmpWide()
    {
        LOG_INST() << "cmp_64 ->";
        HandleBinaryOp2<format, int64_t, math_helpers::cmp>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleUcmp()
    {
        LOG_INST() << "ucmp ->";
        HandleBinaryOp2<format, uint32_t, math_helpers::cmp>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleUcmpWide()
    {
        LOG_INST() << "ucmp_64 ->";
        HandleBinaryOp2<format, uint64_t, math_helpers::cmp>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleFcmpl()
    {
        LOG_INST() << "fcmpl ->";
        HandleBinaryOp2<format, float, math_helpers::fcmpl>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleFcmplWide()
    {
        LOG_INST() << "fcmpl.64 ->";
        HandleBinaryOp2<format, double, math_helpers::fcmpl>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleFcmpg()
    {
        LOG_INST() << "fcmpg ->";
        HandleBinaryOp2<format, float, math_helpers::fcmpg>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleFcmpgWide()
    {
        LOG_INST() << "fcmpg.64 ->";
        HandleBinaryOp2<format, double, math_helpers::fcmpg>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleJeqz()
    {
        LOG_INST() << "jeqz ->";
        HandleCondJmpz<format, std::equal_to>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleJnez()
    {
        LOG_INST() << "jnez ->";
        HandleCondJmpz<format, std::not_equal_to>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleJltz()
    {
        LOG_INST() << "jltz ->";
        HandleCondJmpz<format, std::less>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleJgtz()
    {
        LOG_INST() << "jgtz ->";
        HandleCondJmpz<format, std::greater>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleJlez()
    {
        LOG_INST() << "jlez ->";
        HandleCondJmpz<format, std::less_equal>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleJgez()
    {
        LOG_INST() << "jgez ->";
        HandleCondJmpz<format, std::greater_equal>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleJeq()
    {
        LOG_INST() << "jeq ->";
        HandleCondJmp<format, std::equal_to>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleJne()
    {
        LOG_INST() << "jne ->";
        HandleCondJmp<format, std::not_equal_to>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleJlt()
    {
        LOG_INST() << "jlt ->";
        HandleCondJmp<format, std::less>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleJgt()
    {
        LOG_INST() << "jgt ->";
        HandleCondJmp<format, std::greater>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleJle()
    {
        LOG_INST() << "jle ->";
        HandleCondJmp<format, std::less_equal>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleJge()
    {
        LOG_INST() << "jge ->";
        HandleCondJmp<format, std::greater_equal>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleJeqzObj()
    {
        LOG_INST() << "jeqz.obj ->";
        HandleCondJmpzObj<format, std::equal_to>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleJnezObj()
    {
        LOG_INST() << "jnez.obj ->";
        HandleCondJmpzObj<format, std::not_equal_to>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleJeqObj()
    {
        LOG_INST() << "jeq.obj ->";
        HandleCondJmpObj<format, std::equal_to>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleJneObj()
    {
        LOG_INST() << "jne.obj ->";
        HandleCondJmpObj<format, std::not_equal_to>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleAdd2()
    {
        LOG_INST() << "add2 ->";
        HandleBinaryOp2<format, int32_t, math_helpers::Plus>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleAdd2Wide()
    {
        LOG_INST() << "add2.64 ->";
        HandleBinaryOp2<format, int64_t, math_helpers::Plus>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleFadd2()
    {
        LOG_INST() << "fadd2 ->";
        HandleBinaryOp2<format, float, std::plus>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleFadd2Wide()
    {
        LOG_INST() << "fadd2.64 ->";
        HandleBinaryOp2<format, double, std::plus>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleSub2()
    {
        LOG_INST() << "sub2 ->";
        HandleBinaryOp2<format, int32_t, math_helpers::Minus>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleSub2Wide()
    {
        LOG_INST() << "sub2.64 ->";
        HandleBinaryOp2<format, int64_t, math_helpers::Minus>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleFsub2()
    {
        LOG_INST() << "fsub2 ->";
        HandleBinaryOp2<format, float, std::minus>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleFsub2Wide()
    {
        LOG_INST() << "fsub2.64 ->";
        HandleBinaryOp2<format, double, std::minus>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleMul2()
    {
        LOG_INST() << "mul2 ->";
        HandleBinaryOp2<format, int32_t, math_helpers::Multiplies>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleMul2Wide()
    {
        LOG_INST() << "mul2.64 ->";
        HandleBinaryOp2<format, int64_t, math_helpers::Multiplies>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleFmul2()
    {
        LOG_INST() << "fmul2 ->";
        HandleBinaryOp2<format, float, std::multiplies>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleFmul2Wide()
    {
        LOG_INST() << "fmul2.64 ->";
        HandleBinaryOp2<format, double, std::multiplies>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleFdiv2()
    {
        LOG_INST() << "fdiv2 ->";
        HandleBinaryOp2<format, float, std::divides>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleFdiv2Wide()
    {
        LOG_INST() << "fdiv2.64 ->";
        HandleBinaryOp2<format, double, std::divides>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleFmod2()
    {
        LOG_INST() << "fmod2 ->";
        HandleBinaryOp2<format, float, math_helpers::fmodulus>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleFmod2Wide()
    {
        LOG_INST() << "fmod2.64 ->";
        HandleBinaryOp2<format, double, math_helpers::fmodulus>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleAnd2()
    {
        LOG_INST() << "and2 ->";
        HandleBinaryOp2<format, int32_t, std::bit_and>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleAnd2Wide()
    {
        LOG_INST() << "and2.64 ->";
        HandleBinaryOp2<format, int64_t, std::bit_and>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleOr2()
    {
        LOG_INST() << "or2 ->";
        HandleBinaryOp2<format, int32_t, std::bit_or>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleOr2Wide()
    {
        LOG_INST() << "or2.64 ->";
        HandleBinaryOp2<format, int64_t, std::bit_or>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleXor2()
    {
        LOG_INST() << "xor2 ->";
        HandleBinaryOp2<format, int32_t, std::bit_xor>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleXor2Wide()
    {
        LOG_INST() << "xor2.64 ->";
        HandleBinaryOp2<format, int64_t, std::bit_xor>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleShl2()
    {
        LOG_INST() << "shl2 ->";
        HandleBinaryOp2<format, int32_t, math_helpers::bit_shl>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleShl2Wide()
    {
        LOG_INST() << "shl2.64 ->";
        HandleBinaryOp2<format, int64_t, math_helpers::bit_shl>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleShr2()
    {
        LOG_INST() << "shr2 ->";
        HandleBinaryOp2<format, int32_t, math_helpers::bit_shr>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleShr2Wide()
    {
        LOG_INST() << "shr2.64 ->";
        HandleBinaryOp2<format, int64_t, math_helpers::bit_shr>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleAshr2()
    {
        LOG_INST() << "ashr2 ->";
        HandleBinaryOp2<format, int32_t, math_helpers::bit_ashr>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleAshr2Wide()
    {
        LOG_INST() << "ashr2.64 ->";
        HandleBinaryOp2<format, int64_t, math_helpers::bit_ashr>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleDiv2()
    {
        LOG_INST() << "div2 ->";
        HandleBinaryOp2<format, int32_t, math_helpers::idivides, true>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleDiv2Wide()
    {
        LOG_INST() << "div2.64 ->";
        HandleBinaryOp2<format, int64_t, math_helpers::idivides, true>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleMod2()
    {
        LOG_INST() << "mod2 ->";
        HandleBinaryOp2<format, int32_t, math_helpers::imodulus, true>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleMod2Wide()
    {
        LOG_INST() << "mod2.64 ->";
        HandleBinaryOp2<format, int64_t, math_helpers::imodulus, true>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleDivu2()
    {
        LOG_INST() << "divu2 ->";
        HandleBinaryOp2<format, uint32_t, math_helpers::idivides, true>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleDivu2Wide()
    {
        LOG_INST() << "divu2.64 ->";
        HandleBinaryOp2<format, uint64_t, math_helpers::idivides, true>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleModu2()
    {
        LOG_INST() << "modu2 ->";
        HandleBinaryOp2<format, uint32_t, math_helpers::imodulus, true>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleModu2Wide()
    {
        LOG_INST() << "modu2.64 ->";
        HandleBinaryOp2<format, uint64_t, math_helpers::imodulus, true>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleAdd()
    {
        LOG_INST() << "add ->";
        HandleBinaryOp<format, int32_t, math_helpers::Plus>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleSub()
    {
        LOG_INST() << "sub ->";
        HandleBinaryOp<format, int32_t, math_helpers::Minus>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleMul()
    {
        LOG_INST() << "mul ->";
        HandleBinaryOp<format, int32_t, math_helpers::Multiplies>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleAnd()
    {
        LOG_INST() << "and ->";
        HandleBinaryOp<format, int32_t, std::bit_and>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleOr()
    {
        LOG_INST() << "or ->";
        HandleBinaryOp<format, int32_t, std::bit_or>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleXor()
    {
        LOG_INST() << "xor ->";
        HandleBinaryOp<format, int32_t, std::bit_xor>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleShl()
    {
        LOG_INST() << "shl ->";
        HandleBinaryOp<format, int32_t, math_helpers::bit_shl>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleShr()
    {
        LOG_INST() << "shr ->";
        HandleBinaryOp<format, int32_t, math_helpers::bit_shr>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleAshr()
    {
        LOG_INST() << "ashr ->";
        HandleBinaryOp<format, int32_t, math_helpers::bit_ashr>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleDiv()
    {
        LOG_INST() << "div ->";
        HandleBinaryOp<format, int32_t, math_helpers::idivides, true>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleMod()
    {
        LOG_INST() << "mod ->";
        HandleBinaryOp<format, int32_t, math_helpers::imodulus, true>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleAddi()
    {
        LOG_INST() << "addi ->";
        HandleBinaryOp2Imm<format, int32_t, math_helpers::Plus>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleSubi()
    {
        LOG_INST() << "subi ->";
        HandleBinaryOp2Imm<format, int32_t, math_helpers::Minus>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleMuli()
    {
        LOG_INST() << "muli ->";
        HandleBinaryOp2Imm<format, int32_t, math_helpers::Multiplies>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleAndi()
    {
        LOG_INST() << "andi ->";
        HandleBinaryOp2Imm<format, int32_t, std::bit_and>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleOri()
    {
        LOG_INST() << "ori ->";
        HandleBinaryOp2Imm<format, int32_t, std::bit_or>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleXori()
    {
        LOG_INST() << "xori ->";
        HandleBinaryOp2Imm<format, int32_t, std::bit_xor>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleShli()
    {
        LOG_INST() << "shli ->";
        HandleBinaryOp2Imm<format, int32_t, math_helpers::bit_shl>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleShri()
    {
        LOG_INST() << "shri ->";
        HandleBinaryOp2Imm<format, int32_t, math_helpers::bit_shr>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleAshri()
    {
        LOG_INST() << "ashri ->";
        HandleBinaryOp2Imm<format, int32_t, math_helpers::bit_ashr>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleDivi()
    {
        LOG_INST() << "divi ->";
        HandleBinaryOp2Imm<format, int32_t, math_helpers::idivides, true>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleModi()
    {
        LOG_INST() << "modi ->";
        HandleBinaryOp2Imm<format, int32_t, math_helpers::imodulus, true>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleNeg()
    {
        LOG_INST() << "neg";
        HandleUnaryOp<format, int32_t, std::negate>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleNegWide()
    {
        LOG_INST() << "neg.64";
        HandleUnaryOp<format, int64_t, std::negate>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleFneg()
    {
        LOG_INST() << "fneg";
        HandleUnaryOp<format, float, std::negate>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleFnegWide()
    {
        LOG_INST() << "fneg.64";
        HandleUnaryOp<format, double, std::negate>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleNot()
    {
        LOG_INST() << "not";
        HandleUnaryOp<format, int32_t, std::bit_not>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleNotWide()
    {
        LOG_INST() << "not.64";
        HandleUnaryOp<format, int64_t, std::bit_not>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleInci()
    {
        int32_t imm = this->GetInst().template GetImm<format>();
        uint16_t vx = this->GetInst().template GetVReg<format>();
        LOG_INST() << "inci v" << vx << ", " << std::hex << imm;
        auto &reg = this->GetFrame()->GetVReg(vx);
        int32_t value = reg.template GetAs<int32_t>();
        reg.Set(value + imm);
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleU32toi64()
    {
        LOG_INST() << "u32toi64";
        HandleConversion<format, uint32_t, int64_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleU32toi16()
    {
        LOG_INST() << "u32toi16";
        HandleConversion<format, uint32_t, int16_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleU32tou16()
    {
        LOG_INST() << "u32tou16";
        HandleConversion<format, uint32_t, uint16_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleU32toi8()
    {
        LOG_INST() << "u32toi8";
        HandleConversion<format, uint32_t, int8_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleU32tou8()
    {
        LOG_INST() << "u32tou8";
        HandleConversion<format, uint32_t, uint8_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleU32tou1()
    {
        LOG_INST() << "u32tou1";
        HandleConversion<format, uint32_t, bool>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleI32toi64()
    {
        LOG_INST() << "i32toi64";
        HandleConversion<format, int32_t, int64_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleI32tou16()
    {
        LOG_INST() << "i32tou16";
        HandleConversion<format, int32_t, uint16_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleI32toi16()
    {
        LOG_INST() << "i32toi16";
        HandleConversion<format, int32_t, int16_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleI32toi8()
    {
        LOG_INST() << "i32toi8";
        HandleConversion<format, int32_t, int8_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleI32tou8()
    {
        LOG_INST() << "i32tou8";
        HandleConversion<format, int32_t, uint8_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleI32tou1()
    {
        LOG_INST() << "i32tou1";
        HandleConversion<format, int32_t, bool>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleI32tof32()
    {
        LOG_INST() << "i32tof32";
        HandleConversion<format, int32_t, float>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleI32tof64()
    {
        LOG_INST() << "i32tof64";
        HandleConversion<format, int32_t, double>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleU32tof32()
    {
        LOG_INST() << "u32tof32";
        HandleConversion<format, uint32_t, float>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleU32tof64()
    {
        LOG_INST() << "u32tof64";
        HandleConversion<format, uint32_t, double>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleI64toi32()
    {
        LOG_INST() << "i64toi32";
        HandleConversion<format, int64_t, int32_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleI64tou1()
    {
        LOG_INST() << "i64tou1";
        HandleConversion<format, int64_t, bool>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleI64tof32()
    {
        LOG_INST() << "i64tof32";
        HandleConversion<format, int64_t, float>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleI64tof64()
    {
        LOG_INST() << "i64tof64";
        HandleConversion<format, int64_t, double>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleU64toi32()
    {
        LOG_INST() << "u64toi32";
        HandleConversion<format, uint64_t, int32_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleU64tou32()
    {
        LOG_INST() << "u64tou32";
        HandleConversion<format, uint64_t, uint32_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleU64tou1()
    {
        LOG_INST() << "u64tou1";
        HandleConversion<format, uint64_t, bool>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleU64tof32()
    {
        LOG_INST() << "u64tof32";
        HandleConversion<format, uint64_t, float>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleU64tof64()
    {
        LOG_INST() << "u64tof64";
        HandleConversion<format, uint64_t, double>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleF32tof64()
    {
        LOG_INST() << "f32tof64";
        HandleConversion<format, float, double>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleF32toi32()
    {
        LOG_INST() << "f32toi32";
        HandleFloatToIntConversion<format, float, int32_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleF32toi64()
    {
        LOG_INST() << "f32toi64";
        HandleFloatToIntConversion<format, float, int64_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleF32tou32()
    {
        LOG_INST() << "f32tou32";
        HandleFloatToIntConversion<format, float, uint32_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleF32tou64()
    {
        LOG_INST() << "f32tou64";
        HandleFloatToIntConversion<format, float, uint64_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleF64tof32()
    {
        LOG_INST() << "f64tof32";
        HandleConversion<format, double, float>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleF64toi64()
    {
        LOG_INST() << "f64toi64";
        HandleFloatToIntConversion<format, double, int64_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleF64toi32()
    {
        LOG_INST() << "f64toi32";
        HandleFloatToIntConversion<format, double, int32_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleF64tou64()
    {
        LOG_INST() << "f64tou64";
        HandleFloatToIntConversion<format, double, uint64_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleF64tou32()
    {
        LOG_INST() << "f64tou32";
        HandleFloatToIntConversion<format, double, uint32_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleLdarr8()
    {
        LOG_INST() << "ldarr.8";
        HandleArrayPrimitiveLoad<format, int8_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleLdarr16()
    {
        LOG_INST() << "ldarr.16";
        HandleArrayPrimitiveLoad<format, int16_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleLdarr()
    {
        LOG_INST() << "ldarr";
        HandleArrayPrimitiveLoad<format, int32_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleLdarrWide()
    {
        LOG_INST() << "ldarr.64";
        HandleArrayPrimitiveLoad<format, int64_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleLdarru8()
    {
        LOG_INST() << "ldarru.8";
        HandleArrayPrimitiveLoad<format, uint8_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleLdarru16()
    {
        LOG_INST() << "ldarru.16";
        HandleArrayPrimitiveLoad<format, uint16_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleFldarr32()
    {
        LOG_INST() << "fldarr.32";
        HandleArrayPrimitiveLoad<format, float>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleFldarrWide()
    {
        LOG_INST() << "fldarr.64";
        HandleArrayPrimitiveLoad<format, double>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleLdarrObj()
    {
        uint16_t vs = this->GetInst().template GetVReg<format>();

        LOG_INST() << "ldarr.obj v" << vs;

        auto *array = static_cast<coretypes::Array *>(this->GetFrame()->GetVReg(vs).GetReference());
        int32_t idx = this->GetAcc().Get();

        if (LIKELY(CheckLoadArrayOp(array, idx))) {
            this->GetAcc().SetReference(
                array->Get<ObjectHeader *, RuntimeIfaceT::NEED_READ_BARRIER>(this->GetThread(), idx));
            this->template MoveToNextInst<format, true>();
        } else {
            this->MoveToExceptionHandler();
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleLdaDyn()
    {
        uint16_t vs = this->GetInst().template GetVReg<format>();
        LOG_INST() << "lda.dyn v" << vs;
        this->GetAcc().Move(this->GetFrame()->GetVReg(vs));
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleStarr8()
    {
        LOG_INST() << "starr.8";
        HandleArrayStore<format, uint8_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleStarr16()
    {
        LOG_INST() << "starr.16";
        HandleArrayStore<format, uint16_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleStarr()
    {
        LOG_INST() << "starr";
        HandleArrayStore<format, uint32_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleStarrWide()
    {
        LOG_INST() << "starr.64";
        HandleArrayStore<format, uint64_t>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleFstarr32()
    {
        LOG_INST() << "fstarr.32";
        HandleArrayStore<format, float>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleFstarrWide()
    {
        LOG_INST() << "fstarr.64";
        HandleArrayStore<format, double>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleStarrObj()
    {
        LOG_INST() << "starr.obj";
        HandleArrayStore<format, ObjectHeader *>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleLenarr()
    {
        uint16_t vs = this->GetInst().template GetVReg<format>();

        LOG_INST() << "lenarr v" << vs;

        ObjectHeader *obj = this->GetFrame()->GetVReg(vs).GetReference();
        if (UNLIKELY(obj == nullptr)) {
            RuntimeIfaceT::ThrowNullPointerException();
            this->MoveToExceptionHandler();
        } else {
            auto *array = static_cast<coretypes::Array *>(obj);
            this->GetAcc().SetPrimitive(static_cast<int32_t>(array->GetLength()));
            this->template MoveToNextInst<format, true>();
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleNewarr()
    {
        uint16_t vd = this->GetInst().template GetVReg<format, 0>();
        uint16_t vs = this->GetInst().template GetVReg<format, 1>();
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "newarr v" << vd << ", v" << vs << ", " << std::hex << id;

        int32_t size = this->GetFrame()->GetVReg(vs).Get();

        if (UNLIKELY(size < 0)) {
            RuntimeIfaceT::ThrowNegativeArraySizeException(size);
            this->MoveToExceptionHandler();
        } else {
            Class *klass = ResolveType<true>(id);
            if (LIKELY(klass != nullptr)) {
                this->GetFrame()->GetAcc() = this->GetAcc();
                coretypes::Array *array = RuntimeIfaceT::CreateArray(klass, size);
                this->GetAcc() = this->GetFrame()->GetAcc();
                this->GetFrame()->GetVReg(vd).SetReference(array);
                if (UNLIKELY(array == nullptr)) {
                    this->MoveToExceptionHandler();
                } else {
                    this->template MoveToNextInst<format, true>();
                }
            } else {
                this->MoveToExceptionHandler();
            }
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleNewobj()
    {
        uint16_t vd = this->GetInst().template GetVReg<format, 0>();
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "newobj v" << vd << std::hex << id;

        Class *klass = ResolveType<true>(id);
        if (LIKELY(klass != nullptr)) {
            this->GetFrame()->GetAcc() = this->GetAcc();
            ObjectHeader *obj = RuntimeIfaceT::CreateObject(klass);
            this->GetAcc() = this->GetFrame()->GetAcc();
            if (LIKELY(obj != nullptr)) {
                this->GetFrame()->GetVReg(vd).SetReference(obj);
                this->template MoveToNextInst<format, false>();
            } else {
                this->MoveToExceptionHandler();
            }
        } else {
            this->MoveToExceptionHandler();
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleInitobj()
    {
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "initobj " << this->GetInst().template GetVReg<format, 0>() << ", v"
                   << this->GetInst().template GetVReg<format, 1>() << ", v"
                   << this->GetInst().template GetVReg<format, 2>() << ", v"
                   << this->GetInst().template GetVReg<format, 3>() << ", " << std::hex << id;

        InitializeObject<format>(id);
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleInitobjShort()
    {
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "initobj.short v" << this->GetInst().template GetVReg<format, 0>() << ", v"
                   << this->GetInst().template GetVReg<format, 1>() << ", " << std::hex << id;

        InitializeObject<format>(id);
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleInitobjRange()
    {
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "initobj.range v" << this->GetInst().template GetVReg<format, 0>() << ", " << std::hex << id;

        InitializeObject<format>(id);
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleLdobj()
    {
        uint16_t vs = this->GetInst().template GetVReg<format>();
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "ldobj v" << vs << ", " << std::hex << id;

        ObjectHeader *obj = this->GetFrame()->GetVReg(vs).GetReference();
        if (UNLIKELY(obj == nullptr)) {
            RuntimeIfaceT::ThrowNullPointerException();
            this->MoveToExceptionHandler();
        } else {
            Field *field = ResolveField(id);
            if (LIKELY(field != nullptr)) {
                ASSERT(!field->IsStatic());
                LoadPrimitiveField(obj, field);
                this->template MoveToNextInst<format, true>();
            } else {
                this->MoveToExceptionHandler();
            }
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleLdobjWide()
    {
        uint16_t vs = this->GetInst().template GetVReg<format>();
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "ldobj.64 v" << vs << ", " << std::hex << id;

        ObjectHeader *obj = this->GetFrame()->GetVReg(vs).GetReference();
        if (UNLIKELY(obj == nullptr)) {
            RuntimeIfaceT::ThrowNullPointerException();
            this->MoveToExceptionHandler();
        } else {
            Field *field = ResolveField(id);
            if (LIKELY(field != nullptr)) {
                ASSERT(!field->IsStatic());
                LoadPrimitiveField(obj, field);
                this->template MoveToNextInst<format, true>();
            } else {
                this->MoveToExceptionHandler();
            }
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleLdobjObj()
    {
        uint16_t vs = this->GetInst().template GetVReg<format>();
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "ldobj.obj v" << vs << ", " << std::hex << id;

        ObjectHeader *obj = this->GetFrame()->GetVReg(vs).GetReference();
        if (UNLIKELY(obj == nullptr)) {
            RuntimeIfaceT::ThrowNullPointerException();
            this->MoveToExceptionHandler();
        } else {
            Field *field = ResolveField(id);
            if (LIKELY(field != nullptr)) {
                ASSERT(!field->IsStatic());
                ASSERT(field->GetType().IsReference());
                this->GetAcc().SetReference(
                    obj->GetFieldObject<RuntimeIfaceT::NEED_READ_BARRIER>(this->GetThread(), *field));
                this->template MoveToNextInst<format, true>();
            } else {
                this->MoveToExceptionHandler();
            }
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleLdobjV()
    {
        uint16_t vd = this->GetInst().template GetVReg<format, 0>();
        uint16_t vs = this->GetInst().template GetVReg<format, 1>();
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "ldobj.v v" << vd << ", v" << vs << ", " << std::hex << id;

        ObjectHeader *obj = this->GetFrame()->GetVReg(vs).GetReference();
        if (UNLIKELY(obj == nullptr)) {
            RuntimeIfaceT::ThrowNullPointerException();
            this->MoveToExceptionHandler();
        } else {
            Field *field = ResolveField(id);
            if (LIKELY(field != nullptr)) {
                ASSERT(!field->IsStatic());
                LoadPrimitiveFieldReg(this->GetFrame()->GetVReg(vd), obj, field);
                this->template MoveToNextInst<format, true>();
            } else {
                this->MoveToExceptionHandler();
            }
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleLdobjVWide()
    {
        uint16_t vd = this->GetInst().template GetVReg<format, 0>();
        uint16_t vs = this->GetInst().template GetVReg<format, 1>();
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "ldobj.v.64 v" << vd << ", v" << vs << ", " << std::hex << id;

        ObjectHeader *obj = this->GetFrame()->GetVReg(vs).GetReference();
        if (UNLIKELY(obj == nullptr)) {
            RuntimeIfaceT::ThrowNullPointerException();
            this->MoveToExceptionHandler();
        } else {
            Field *field = ResolveField(id);
            if (LIKELY(field != nullptr)) {
                ASSERT(!field->IsStatic());
                LoadPrimitiveFieldReg(this->GetFrame()->GetVReg(vd), obj, field);
                this->template MoveToNextInst<format, true>();
            } else {
                this->MoveToExceptionHandler();
            }
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleLdobjVObj()
    {
        uint16_t vd = this->GetInst().template GetVReg<format, 0>();
        uint16_t vs = this->GetInst().template GetVReg<format, 1>();
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "ldobj.v.obj v" << vd << ", v" << vs << ", " << std::hex << id;

        ObjectHeader *obj = this->GetFrame()->GetVReg(vs).GetReference();
        if (UNLIKELY(obj == nullptr)) {
            RuntimeIfaceT::ThrowNullPointerException();
            this->MoveToExceptionHandler();
        } else {
            Field *field = ResolveField(id);
            if (LIKELY(field != nullptr)) {
                ASSERT(!field->IsStatic());
                ASSERT(field->GetType().IsReference());
                this->GetFrame()->GetVReg(vd).SetReference(
                    obj->GetFieldObject<RuntimeIfaceT::NEED_READ_BARRIER>(this->GetThread(), *field));
                this->template MoveToNextInst<format, true>();
            } else {
                this->MoveToExceptionHandler();
            }
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleStobj()
    {
        uint16_t vs = this->GetInst().template GetVReg<format>();
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "stobj v" << vs << ", " << std::hex << id;

        ObjectHeader *obj = this->GetFrame()->GetVReg(vs).GetReference();
        if (UNLIKELY(obj == nullptr)) {
            RuntimeIfaceT::ThrowNullPointerException();
            this->MoveToExceptionHandler();
        } else {
            Field *field = ResolveField(id);
            if (LIKELY(field != nullptr)) {
                ASSERT(!field->IsStatic());
                StorePrimitiveField(obj, field);
                this->template MoveToNextInst<format, true>();
            } else {
                this->MoveToExceptionHandler();
            }
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleStobjWide()
    {
        uint16_t vs = this->GetInst().template GetVReg<format>();
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "stobj.64 v" << vs << ", " << std::hex << id;

        ObjectHeader *obj = this->GetFrame()->GetVReg(vs).GetReference();
        if (UNLIKELY(obj == nullptr)) {
            RuntimeIfaceT::ThrowNullPointerException();
            this->MoveToExceptionHandler();
        } else {
            Field *field = ResolveField(id);
            if (LIKELY(field != nullptr)) {
                ASSERT(!field->IsStatic());
                StorePrimitiveField(obj, field);
                this->template MoveToNextInst<format, true>();
            } else {
                this->MoveToExceptionHandler();
            }
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleStobjObj()
    {
        uint16_t vs = this->GetInst().template GetVReg<format>();
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "stobj.obj v" << vs << ", " << std::hex << id;

        ObjectHeader *obj = this->GetFrame()->GetVReg(vs).GetReference();
        if (UNLIKELY(obj == nullptr)) {
            RuntimeIfaceT::ThrowNullPointerException();
            this->MoveToExceptionHandler();
        } else {
            Field *field = ResolveField(id);
            if (LIKELY(field != nullptr)) {
                ASSERT(!field->IsStatic());
                ASSERT(field->GetType().IsReference());
                obj->SetFieldObject<RuntimeIfaceT::NEED_WRITE_BARRIER>(this->GetThread(), *field,
                                                                       this->GetAcc().GetReference());
                this->template MoveToNextInst<format, true>();
            } else {
                this->MoveToExceptionHandler();
            }
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleStobjV()
    {
        uint16_t vd = this->GetInst().template GetVReg<format, 0>();
        uint16_t vs = this->GetInst().template GetVReg<format, 1>();
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "stobj.v v" << vd << ", v" << vs << ", " << std::hex << id;

        ObjectHeader *obj = this->GetFrame()->GetVReg(vs).GetReference();
        if (UNLIKELY(obj == nullptr)) {
            RuntimeIfaceT::ThrowNullPointerException();
            this->MoveToExceptionHandler();
        } else {
            Field *field = ResolveField(id);
            if (LIKELY(field != nullptr)) {
                ASSERT(!field->IsStatic());
                StorePrimitiveFieldReg(this->GetFrame()->GetVReg(vd), obj, field);
                this->template MoveToNextInst<format, true>();
            } else {
                this->MoveToExceptionHandler();
            }
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleStobjVWide()
    {
        uint16_t vd = this->GetInst().template GetVReg<format, 0>();
        uint16_t vs = this->GetInst().template GetVReg<format, 1>();
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "stobj.v.64 v" << vd << ", v" << vs << ", " << std::hex << id;

        ObjectHeader *obj = this->GetFrame()->GetVReg(vs).GetReference();
        if (UNLIKELY(obj == nullptr)) {
            RuntimeIfaceT::ThrowNullPointerException();
            this->MoveToExceptionHandler();
        } else {
            Field *field = ResolveField(id);
            if (LIKELY(field != nullptr)) {
                ASSERT(!field->IsStatic());
                StorePrimitiveFieldReg(this->GetFrame()->GetVReg(vd), obj, field);
                this->template MoveToNextInst<format, true>();
            } else {
                this->MoveToExceptionHandler();
            }
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleStobjVObj()
    {
        uint16_t vd = this->GetInst().template GetVReg<format, 0>();
        uint16_t vs = this->GetInst().template GetVReg<format, 1>();
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "stobj.v.obj v" << vd << ", v" << vs << ", " << std::hex << id;

        ObjectHeader *obj = this->GetFrame()->GetVReg(vs).GetReference();
        if (UNLIKELY(obj == nullptr)) {
            RuntimeIfaceT::ThrowNullPointerException();
            this->MoveToExceptionHandler();
        } else {
            Field *field = ResolveField(id);
            if (LIKELY(field != nullptr)) {
                ASSERT(!field->IsStatic());
                ASSERT(field->GetType().IsReference());
                obj->SetFieldObject<RuntimeIfaceT::NEED_WRITE_BARRIER>(this->GetThread(), *field,
                                                                       this->GetFrame()->GetVReg(vd).GetReference());
                this->template MoveToNextInst<format, true>();
            } else {
                this->MoveToExceptionHandler();
            }
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleLdstatic()
    {
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "ldstatic " << std::hex << id;

        Field *field = ResolveField<true>(id);
        if (LIKELY(field != nullptr)) {
            ASSERT(field->IsStatic());
            LoadPrimitiveField(GetClass(field), field);
            this->template MoveToNextInst<format, false>();
        } else {
            this->MoveToExceptionHandler();
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleLdstaticWide()
    {
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "ldstatic.64 " << std::hex << id;

        Field *field = ResolveField<true>(id);
        if (LIKELY(field != nullptr)) {
            ASSERT(field->IsStatic());
            LoadPrimitiveField(GetClass(field), field);
            this->template MoveToNextInst<format, false>();
        } else {
            this->MoveToExceptionHandler();
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleLdstaticObj()
    {
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "ldstatic.obj " << std::hex << id;

        Field *field = ResolveField<true>(id);
        if (LIKELY(field != nullptr)) {
            ASSERT(field->IsStatic());
            Class *klass = GetClass(field);
            ASSERT(field->GetType().IsReference());
            this->GetAcc().SetReference(
                klass->GetFieldObject<RuntimeIfaceT::NEED_READ_BARRIER>(this->GetThread(), *field));
            this->template MoveToNextInst<format, false>();
        } else {
            this->MoveToExceptionHandler();
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleStstatic()
    {
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "ststatic " << std::hex << id;

        Field *field = ResolveField<true>(id);
        if (LIKELY(field != nullptr)) {
            ASSERT(field->IsStatic());
            Class *klass = GetClass(field);
            StorePrimitiveField(klass, field);
            this->template MoveToNextInst<format, false>();
        } else {
            this->MoveToExceptionHandler();
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleStstaticWide()
    {
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "ststatic.64 " << std::hex << id;

        Field *field = ResolveField<true>(id);
        if (LIKELY(field != nullptr)) {
            ASSERT(field->IsStatic());
            Class *klass = GetClass(field);
            StorePrimitiveField(klass, field);
            this->template MoveToNextInst<format, false>();
        } else {
            this->MoveToExceptionHandler();
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleStstaticObj()
    {
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "ststatic.obj " << std::hex << id;

        Field *field = ResolveField<true>(id);
        if (LIKELY(field != nullptr)) {
            ASSERT(field->IsStatic());
            Class *klass = GetClass(field);
            ASSERT(field->GetType().IsReference());
            klass->SetFieldObject<RuntimeIfaceT::NEED_WRITE_BARRIER>(this->GetThread(), *field,
                                                                     this->GetAcc().GetReference());
            this->template MoveToNextInst<format, false>();
        } else {
            this->MoveToExceptionHandler();
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleReturn()
    {
        LOG_INST() << "return";
        this->GetFrame()->GetAcc().SetPrimitive(this->GetAcc().Get());
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleReturnWide()
    {
        LOG_INST() << "return.64";
        this->GetFrame()->GetAcc().SetPrimitive(this->GetAcc().GetLong());
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleReturnObj()
    {
        LOG_INST() << "return.obj";
        this->GetFrame()->GetAcc().SetReference(this->GetAcc().GetReference());
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleReturnVoid()
    {
        LOG_INST() << "return.void";
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleReturnDyn()
    {
        LOG_INST() << "return.dyn";
        this->GetFrame()->SetAcc((this->GetAcc()));
    }

    ALWAYS_INLINE void HandleReturnStackless()
    {
        Frame *frame = this->GetFrame();
        Frame *prev = frame->GetPrevFrame();

        ASSERT(frame->IsStackless());

        Method *method = frame->GetMethod();
        ManagedThread *thread = this->GetThread();

#if EVENT_METHOD_EXIT_ENABLED
        EVENT_METHOD_EXIT(frame->GetMethod()->GetFullName(), events::MethodExitKind::INTERP,
                          thread->RecordMethodExit());
#endif

        Runtime::GetCurrent()->GetNotificationManager()->MethodExitEvent(thread, method);

        this->GetInstructionHandlerState()->UpdateInstructionHandlerState(
            prev->GetInstruction() + prev->GetBytecodeOffset(), prev);

        RuntimeIfaceT::SetCurrentFrame(thread, prev);

        if (UNLIKELY(this->GetThread()->HasPendingException())) {
            this->MoveToExceptionHandler();
        } else {
            this->GetAcc() = frame->GetAcc();
            this->SetInst(prev->GetNextInstruction());
        }

        if (frame->IsInitobj()) {
            this->GetAcc() = prev->GetAcc();
        }

        RuntimeIfaceT::FreeFrame(frame);

        LOG(DEBUG, INTERPRETER) << "Exit: Runtime Call.";
    }

    ALWAYS_INLINE void HandleInstrumentForceReturn()
    {
        HandleReturnStackless();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleCheckcast()
    {
        auto type_id = this->GetInst().template GetId<format>();

        LOG_INST() << "checkcast " << std::hex << type_id;

        Class *type = ResolveType(type_id);
        if (LIKELY(type != nullptr)) {
            ObjectHeader *obj = this->GetAcc().GetReference();

            if (UNLIKELY(obj != nullptr && !type->IsAssignableFrom(obj->ClassAddr<Class>()))) {
                RuntimeIfaceT::ThrowClassCastException(type, obj->ClassAddr<Class>());
                this->MoveToExceptionHandler();
            } else {
                this->template MoveToNextInst<format, true>();
            }
        } else {
            this->MoveToExceptionHandler();
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleIsinstance()
    {
        auto type_id = this->GetInst().template GetId<format>();

        LOG_INST() << "isinstance " << std::hex << type_id;

        Class *type = ResolveType(type_id);
        if (LIKELY(type != nullptr)) {
            ObjectHeader *obj = this->GetAcc().GetReference();

            if (obj != nullptr && type->IsAssignableFrom(obj->ClassAddr<Class>())) {
                this->GetAcc().SetPrimitive(1);
            } else {
                this->GetAcc().SetPrimitive(0);
            }
            this->template MoveToNextInst<format, false>();
        } else {
            this->MoveToExceptionHandler();
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleCallShort()
    {
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "call.short v" << this->GetInst().template GetVReg<format, 0>() << ", v"
                   << this->GetInst().template GetVReg<format, 1>() << ", " << std::hex << id;

        auto *method = ResolveMethod(id);
        if (LIKELY(method != nullptr)) {
            if (!method->IsStatic() && this->GetCallerObject<format, false>() == nullptr) {
                return;
            }
            HandleCall<format>(method);
        } else {
            this->MoveToExceptionHandler();
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleCallAccShort()
    {
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "call.acc.short v" << this->GetInst().template GetVReg<format, 0>() << ", "
                   << this->GetInst().template GetImm<format, 0>() << ", " << std::hex << id;

        auto *method = ResolveMethod(id);
        if (LIKELY(method != nullptr)) {
            if (!method->IsStatic() && this->GetCallerObject<format, true>() == nullptr) {
                return;
            }
            HandleCall<format, false, false, true>(method);
        } else {
            this->MoveToExceptionHandler();
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleCall()
    {
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "call v" << this->GetInst().template GetVReg<format, 0>() << ", v"
                   << this->GetInst().template GetVReg<format, 1>() << ", v"
                   << this->GetInst().template GetVReg<format, 2>() << ", v"
                   << this->GetInst().template GetVReg<format, 3>() << ", " << std::hex << id;

        auto *method = ResolveMethod(id);
        if (LIKELY(method != nullptr)) {
            if (!method->IsStatic() && this->GetCallerObject<format, false>() == nullptr) {
                return;
            }
            HandleCall<format>(method);
        } else {
            this->MoveToExceptionHandler();
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleCallAcc()
    {
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "call.acc v" << this->GetInst().template GetVReg<format, 0>() << ", v"
                   << this->GetInst().template GetVReg<format, 1>() << ", v"
                   << this->GetInst().template GetVReg<format, 2>() << ", "
                   << this->GetInst().template GetImm<format, 0>() << ", " << std::hex << id;

        auto *method = ResolveMethod(id);
        if (LIKELY(method != nullptr)) {
            if (!method->IsStatic() && this->GetCallerObject<format, true>() == nullptr) {
                return;
            }
            HandleCall<format, false, false, true>(method);
        } else {
            this->MoveToExceptionHandler();
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleCallRange()
    {
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "call.range v" << this->GetInst().template GetVReg<format, 0>() << ", " << std::hex << id;

        auto *method = ResolveMethod(id);
        if (LIKELY(method != nullptr)) {
            if (!method->IsStatic() && this->GetCallerObject<format, false>() == nullptr) {
                return;
            }
            HandleCall<format, false, true>(method);
        } else {
            this->MoveToExceptionHandler();
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleCallVirtShort()
    {
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "call.virt.short v" << this->GetInst().template GetVReg<format, 0>() << ", v"
                   << this->GetInst().template GetVReg<format, 1>() << ", " << std::hex << id;

        auto *method = ResolveMethod(id);
        if (LIKELY(method != nullptr)) {
            HandleVirtualCall<format>(method);
        } else {
            this->MoveToExceptionHandler();
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleCallVirtAccShort()
    {
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "call.virt.acc.short v" << this->GetInst().template GetVReg<format, 0>() << ", "
                   << this->GetInst().template GetImm<format, 0>() << ", " << std::hex << id;

        auto *method = ResolveMethod(id);
        if (LIKELY(method != nullptr)) {
            HandleVirtualCall<format, false, true>(method);
        } else {
            this->MoveToExceptionHandler();
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleCallVirt()
    {
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "call.virt v" << this->GetInst().template GetVReg<format, 0>() << ", v"
                   << this->GetInst().template GetVReg<format, 1>() << ", v"
                   << this->GetInst().template GetVReg<format, 2>() << ", v"
                   << this->GetInst().template GetVReg<format, 3>() << ", " << std::hex << id;

        auto *method = ResolveMethod(id);
        if (LIKELY(method != nullptr)) {
            HandleVirtualCall<format>(method);
        } else {
            this->MoveToExceptionHandler();
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleCallVirtAcc()
    {
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "call.virt.acc v" << this->GetInst().template GetVReg<format, 0>() << ", v"
                   << this->GetInst().template GetVReg<format, 1>() << ", v"
                   << this->GetInst().template GetVReg<format, 2>() << ", "
                   << this->GetInst().template GetImm<format, 0>() << ", " << std::hex << id;

        auto *method = ResolveMethod(id);
        if (LIKELY(method != nullptr)) {
            HandleVirtualCall<format, false, true>(method);
        } else {
            this->MoveToExceptionHandler();
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleCallVirtRange()
    {
        auto id = this->GetInst().template GetId<format>();

        LOG_INST() << "call.virt.range v" << this->GetInst().template GetVReg<format, 0>() << ", " << std::hex << id;

        auto *method = ResolveMethod(id);
        if (LIKELY(method != nullptr)) {
            HandleVirtualCall<format, true>(method);
        } else {
            this->MoveToExceptionHandler();
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleCalliDynRange()
    {
        auto actual_num_args = static_cast<uint16_t>(this->GetInst().template GetImm<format, 0>());
        auto first_arg_reg_idx = static_cast<uint16_t>(this->GetInst().template GetVReg<format, 0>());

        LOG_INST() << "calli.dyn.range " << actual_num_args << ", v" << first_arg_reg_idx;

        Frame::VRegister &vreg = this->GetFrame()->GetVReg(first_arg_reg_idx);

        if (!vreg.HasObject()) {
            RuntimeInterface::ThrowTypedErrorDyn("is not object");
            this->MoveToExceptionHandler();
            return;
        }
        auto obj = reinterpret_cast<ObjectHeader *>(vreg.GetValue());
        auto ctx = this->GetThread()->GetLanguageContext();
        if (!ctx.IsCallableObject(obj)) {
            RuntimeInterface::ThrowTypedErrorDyn("is not callable");
            this->MoveToExceptionHandler();
            return;
        }

        HandleCall<format, true, true>(ctx.GetCallTarget(obj));
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void HandleThrow()
    {
        uint16_t vs = this->GetInst().template GetVReg<format>();

        LOG_INST() << "throw v" << vs;

        ObjectHeader *exception = this->GetFrame()->GetVReg(vs).GetReference();
        if (UNLIKELY(exception == nullptr)) {
            RuntimeIfaceT::ThrowNullPointerException();
        } else {
            this->GetThread()->SetException(exception);
        }

        this->MoveToExceptionHandler();
    }

    ALWAYS_INLINE uint32_t FindCatchBlockStackless()
    {
        Frame *frame = this->GetFrame();
        while (frame != nullptr) {
            this->InstrumentInstruction();
            ManagedThread *thread = this->GetThread();
            Frame *prev = frame->GetPrevFrame();
            Method *method = frame->GetMethod();

            ASSERT(thread->HasPendingException());

            auto curr_insn = reinterpret_cast<uintptr_t>(this->GetInst().GetAddress());
            auto first_insn = reinterpret_cast<uintptr_t>(method->GetInstructions());
            uint32_t pc_offset = this->FindCatchBlock(thread->GetException(), curr_insn - first_insn);

            if (pc_offset != panda_file::INVALID_OFFSET) {
                return pc_offset;
            }

            if (!frame->IsStackless() || prev == nullptr ||
                StackWalker::IsBoundaryFrame<FrameKind::INTERPRETER>(prev)) {
                return pc_offset;
            }

            // pc_offset == panda_file::INVALID_OFFSET
            EVENT_METHOD_EXIT(frame->GetMethod()->GetFullName(), events::MethodExitKind::INTERP,
                              thread->RecordMethodExit());

            Runtime::GetCurrent()->GetNotificationManager()->MethodExitEvent(thread, method);

            this->GetInstructionHandlerState()->UpdateInstructionHandlerState(
                prev->GetInstruction() + prev->GetBytecodeOffset(), prev);

            RuntimeIfaceT::SetCurrentFrame(thread, prev);

            ASSERT(thread->HasPendingException());

            if (frame->IsInitobj()) {
                this->GetAcc() = prev->GetAcc();
            }

            RuntimeIfaceT::FreeFrame(frame);

            LOG(DEBUG, INTERPRETER) << "Exit: Runtime Call.";

            frame = prev;
        }
        return panda_file::INVALID_OFFSET;
    }

    ALWAYS_INLINE static bool IsCompilerEnableJit()
    {
        return !enable_instrumentation && RuntimeIfaceT::IsCompilerEnableJit();
    }

    ALWAYS_INLINE bool UpdateHotnessOSR(Method *method, int offset)
    {
        ASSERT(ArchTraits<RUNTIME_ARCH>::SUPPORT_OSR);
        if (this->GetFrame()->IsDeoptimized()) {
            method->IncrementHotnessCounter(0, nullptr);
            return false;
        }
        return method->IncrementHotnessCounter(this->GetBytecodeOffset() + offset, &this->GetAcc(), true);
    }

    uint32_t FindCatchBlock(ObjectHeader *exception, uint32_t pc) const
    {
        auto *method = this->GetFrame()->GetMethod();
        return RuntimeIfaceT::FindCatchBlock(*method, exception, pc);
    }

    template <class T>
    ALWAYS_INLINE Class *GetClass(const T *entity)
    {
        auto *klass = entity->GetClass();

        // Whenever we obtain a class by a field, method, etc.,
        // we expect it to be either fully initialized or in process
        // of initialization (e.g. during executing a cctor).
        ASSERT(klass != nullptr);
        ASSERT(klass->IsInitializing() || klass->IsInitialized());

        return klass;
    }

    template <class F, class T, class R>
    ALWAYS_INLINE void LoadPrimitiveFieldReg(R &vreg, T *obj, Field *field)
    {
        auto value = static_cast<int64_t>(obj->template GetFieldPrimitive<F>(*field));
        vreg.SetPrimitive(value);
    }

    template <class T, class R>
    ALWAYS_INLINE void LoadPrimitiveFieldReg(R &vreg, T *obj, Field *field)
    {
        switch (field->GetType().GetId()) {
            case panda_file::Type::TypeId::U1:
            case panda_file::Type::TypeId::U8:
                LoadPrimitiveFieldReg<uint8_t>(vreg, obj, field);
                break;
            case panda_file::Type::TypeId::I8:
                LoadPrimitiveFieldReg<int8_t>(vreg, obj, field);
                break;
            case panda_file::Type::TypeId::I16:
                LoadPrimitiveFieldReg<int16_t>(vreg, obj, field);
                break;
            case panda_file::Type::TypeId::U16:
                LoadPrimitiveFieldReg<uint16_t>(vreg, obj, field);
                break;
            case panda_file::Type::TypeId::I32:
                LoadPrimitiveFieldReg<int32_t>(vreg, obj, field);
                break;
            case panda_file::Type::TypeId::U32:
                LoadPrimitiveFieldReg<uint32_t>(vreg, obj, field);
                break;
            case panda_file::Type::TypeId::I64:
                LoadPrimitiveFieldReg<int64_t>(vreg, obj, field);
                break;
            case panda_file::Type::TypeId::U64:
                LoadPrimitiveFieldReg<uint64_t>(vreg, obj, field);
                break;
            case panda_file::Type::TypeId::F32:
                vreg.SetPrimitive(obj->template GetFieldPrimitive<float>(*field));
                break;
            case panda_file::Type::TypeId::F64:
                vreg.SetPrimitive(obj->template GetFieldPrimitive<double>(*field));
                break;
            default:
                UNREACHABLE();
                break;
        }
    }

    template <class F, class T>
    ALWAYS_INLINE void LoadPrimitiveField(T *obj, Field *field)
    {
        auto value = static_cast<int64_t>(obj->template GetFieldPrimitive<F>(*field));
        this->GetAcc().SetPrimitive(value);
    }

    template <class T>
    ALWAYS_INLINE void LoadPrimitiveField(T *obj, Field *field)
    {
        switch (field->GetType().GetId()) {
            case panda_file::Type::TypeId::U1:
            case panda_file::Type::TypeId::U8:
                LoadPrimitiveField<uint8_t>(obj, field);
                break;
            case panda_file::Type::TypeId::I8:
                LoadPrimitiveField<int8_t>(obj, field);
                break;
            case panda_file::Type::TypeId::I16:
                LoadPrimitiveField<int16_t>(obj, field);
                break;
            case panda_file::Type::TypeId::U16:
                LoadPrimitiveField<uint16_t>(obj, field);
                break;
            case panda_file::Type::TypeId::I32:
                LoadPrimitiveField<int32_t>(obj, field);
                break;
            case panda_file::Type::TypeId::U32:
                LoadPrimitiveField<uint32_t>(obj, field);
                break;
            case panda_file::Type::TypeId::I64:
                LoadPrimitiveField<int64_t>(obj, field);
                break;
            case panda_file::Type::TypeId::U64:
                LoadPrimitiveField<uint64_t>(obj, field);
                break;
            case panda_file::Type::TypeId::F32:
                this->GetAcc().SetPrimitive(obj->template GetFieldPrimitive<float>(*field));
                break;
            case panda_file::Type::TypeId::F64:
                this->GetAcc().SetPrimitive(obj->template GetFieldPrimitive<double>(*field));
                break;
            default:
                UNREACHABLE();
                break;
        }
    }

    template <class T, class R>
    ALWAYS_INLINE void StorePrimitiveFieldReg(R &vreg, T *obj, Field *field)
    {
        switch (field->GetType().GetId()) {
            case panda_file::Type::TypeId::U1:
            case panda_file::Type::TypeId::U8: {
                obj->SetFieldPrimitive(*field, vreg.template GetAs<uint8_t>());
                break;
            }
            case panda_file::Type::TypeId::I8: {
                obj->SetFieldPrimitive(*field, vreg.template GetAs<int8_t>());
                break;
            }
            case panda_file::Type::TypeId::I16: {
                obj->SetFieldPrimitive(*field, vreg.template GetAs<int16_t>());
                break;
            }
            case panda_file::Type::TypeId::U16: {
                obj->SetFieldPrimitive(*field, vreg.template GetAs<uint16_t>());
                break;
            }
            case panda_file::Type::TypeId::I32: {
                obj->SetFieldPrimitive(*field, vreg.template GetAs<int32_t>());
                break;
            }
            case panda_file::Type::TypeId::U32: {
                obj->SetFieldPrimitive(*field, vreg.template GetAs<uint32_t>());
                break;
            }
            case panda_file::Type::TypeId::I64: {
                obj->SetFieldPrimitive(*field, vreg.template GetAs<int64_t>());
                break;
            }
            case panda_file::Type::TypeId::U64: {
                obj->SetFieldPrimitive(*field, vreg.template GetAs<uint64_t>());
                break;
            }
            case panda_file::Type::TypeId::F32: {
                obj->SetFieldPrimitive(*field, vreg.template GetAs<float>());
                break;
            }
            case panda_file::Type::TypeId::F64: {
                obj->SetFieldPrimitive(*field, vreg.template GetAs<double>());
                break;
            }
            default: {
                UNREACHABLE();
                break;
            }
        }
    }

    template <class T>
    ALWAYS_INLINE void StorePrimitiveField(T *obj, Field *field)
    {
        switch (field->GetType().GetId()) {
            case panda_file::Type::TypeId::U1:
            case panda_file::Type::TypeId::U8: {
                obj->SetFieldPrimitive(*field, this->GetAcc().template GetAs<uint8_t>());
                break;
            }
            case panda_file::Type::TypeId::I8: {
                obj->SetFieldPrimitive(*field, this->GetAcc().template GetAs<int8_t>());
                break;
            }
            case panda_file::Type::TypeId::I16: {
                obj->SetFieldPrimitive(*field, this->GetAcc().template GetAs<int16_t>());
                break;
            }
            case panda_file::Type::TypeId::U16: {
                obj->SetFieldPrimitive(*field, this->GetAcc().template GetAs<uint16_t>());
                break;
            }
            case panda_file::Type::TypeId::I32: {
                obj->SetFieldPrimitive(*field, this->GetAcc().template GetAs<int32_t>());
                break;
            }
            case panda_file::Type::TypeId::U32: {
                obj->SetFieldPrimitive(*field, this->GetAcc().template GetAs<uint32_t>());
                break;
            }
            case panda_file::Type::TypeId::I64: {
                obj->SetFieldPrimitive(*field, this->GetAcc().template GetAs<int64_t>());
                break;
            }
            case panda_file::Type::TypeId::U64: {
                obj->SetFieldPrimitive(*field, this->GetAcc().template GetAs<uint64_t>());
                break;
            }
            case panda_file::Type::TypeId::F32: {
                obj->SetFieldPrimitive(*field, this->GetAcc().template GetAs<float>());
                break;
            }
            case panda_file::Type::TypeId::F64: {
                obj->SetFieldPrimitive(*field, this->GetAcc().template GetAs<double>());
                break;
            }
            default: {
                UNREACHABLE();
                break;
            }
        }
    }

    template <BytecodeInstruction::Format format, class T>
    ALWAYS_INLINE void HandleArrayPrimitiveLoad()
    {
        static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>,
                      "T should be either integral or floating point type");
        uint16_t vs = this->GetInst().template GetVReg<format>();

        LOG_INST() << "\t"
                   << "load v" << vs;

        auto *array = static_cast<coretypes::Array *>(this->GetFrame()->GetVReg(vs).GetReference());
        int32_t idx = this->GetAcc().Get();

        if (LIKELY(CheckLoadArrayOp(array, idx))) {
            this->GetAcc().Set(array->Get<T>(idx));
            this->template MoveToNextInst<format, true>();
        } else {
            this->MoveToExceptionHandler();
        }
    }

    template <BytecodeInstruction::Format format, class T>
    ALWAYS_INLINE void HandleArrayStore()
    {
        uint16_t vs1 = this->GetInst().template GetVReg<format, 0>();
        uint16_t vs2 = this->GetInst().template GetVReg<format, 1>();

        LOG_INST() << "\t"
                   << "store v" << vs1 << ", v" << vs2;

        auto *array = static_cast<coretypes::Array *>(this->GetFrame()->GetVReg(vs1).GetReference());
        int32_t idx = this->GetFrame()->GetVReg(vs2).Get();

        auto elem = this->GetAcc().template GetAs<T>();
        if (LIKELY(CheckStoreArrayOp(array, idx, elem))) {
            array->Set<T, RuntimeIfaceT::NEED_WRITE_BARRIER>(this->GetThread(), idx, elem);
            this->template MoveToNextInst<format, true>();
        } else {
            this->MoveToExceptionHandler();
        }
    }

    template <class T>
    ALWAYS_INLINE bool CheckStoreArrayOp(coretypes::Array *array, int32_t idx, [[maybe_unused]] T elem)
    {
        if (UNLIKELY(array == nullptr)) {
            RuntimeIfaceT::ThrowNullPointerException();
            return false;
        }

        if (UNLIKELY(idx < 0 || helpers::ToUnsigned(idx) >= array->GetLength())) {
            RuntimeIfaceT::ThrowArrayIndexOutOfBoundsException(idx, array->GetLength());
            return false;
        }

        if constexpr (std::is_same_v<T, ObjectHeader *>) {
            if (elem != nullptr) {
                auto *array_class = array->ClassAddr<Class>();
                auto *element_class = array_class->GetComponentType();
                if (UNLIKELY(!elem->IsInstanceOf(element_class))) {
                    RuntimeIfaceT::ThrowArrayStoreException(array_class, elem->template ClassAddr<Class>());
                    return false;
                }
            }
        }

        return true;
    }

    ALWAYS_INLINE bool CheckLoadArrayOp(coretypes::Array *array, int32_t idx)
    {
        if (UNLIKELY(array == nullptr)) {
            RuntimeIfaceT::ThrowNullPointerException();
            return false;
        }

        if (UNLIKELY(idx < 0 || helpers::ToUnsigned(idx) >= array->GetLength())) {
            RuntimeIfaceT::ThrowArrayIndexOutOfBoundsException(idx, array->GetLength());
            return false;
        }

        return true;
    }

    ALWAYS_INLINE bool InstrumentBranches(int32_t offset)
    {
        // Offset may be 0 in case of infinite empty loops (see issue #5301)
        if (offset <= 0) {
            if (this->GetThread()->TestAllFlags()) {
                this->GetFrame()->SetAcc(this->GetAcc());
                RuntimeIfaceT::Safepoint();
                this->GetAcc() = this->GetFrame()->GetAcc();
                if (UNLIKELY(this->GetThread()->HasPendingException())) {
                    this->MoveToExceptionHandler();
                    return true;
                }
            }
            if constexpr (ArchTraits<RUNTIME_ARCH>::SUPPORT_OSR) {
                if (UpdateHotnessOSR(this->GetFrame()->GetMethod(), offset)) {
                    static_assert(static_cast<unsigned>(BytecodeInstruction::Opcode::RETURN_VOID) <=
                                  std::numeric_limits<uint8_t>::max());
                    this->GetFakeInstBuf()[0] = static_cast<uint8_t>(BytecodeInstruction::Opcode::RETURN_VOID);
                    this->SetInst(BytecodeInstruction(this->GetFakeInstBuf().data()));
                    return true;
                }
            } else {
                this->UpdateHotness(this->GetFrame()->GetMethod());
            }
        }
        return false;
    }

    ALWAYS_INLINE coretypes::String *ResolveString(BytecodeId id)
    {
        return RuntimeIfaceT::ResolveString(this->GetThread()->GetVM(), *this->GetFrame()->GetMethod(), id);
    }

    ALWAYS_INLINE coretypes::Array *ResolveLiteralArray(BytecodeId id)
    {
        return RuntimeIfaceT::ResolveLiteralArray(this->GetThread()->GetVM(), *this->GetFrame()->GetMethod(), id);
    }

    ALWAYS_INLINE Method *ResolveMethod(BytecodeId id)
    {
        this->UpdateBytecodeOffset();

        auto cache = this->GetThread()->GetInterpreterCache();
        // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE)
        auto *res = cache->template Get<Method>(this->GetInst().GetAddress(), this->GetFrame()->GetMethod());
        if (res != nullptr) {
            return res;
        }

        this->GetFrame()->SetAcc(this->GetAcc());
        auto *method = RuntimeIfaceT::ResolveMethod(this->GetThread(), *this->GetFrame()->GetMethod(), id);
        this->GetAcc() = this->GetFrame()->GetAcc();
        if (UNLIKELY(method == nullptr)) {
            ASSERT(this->GetThread()->HasPendingException());
            return nullptr;
        }

        cache->Set(this->GetInst().GetAddress(), method, this->GetFrame()->GetMethod());
        return method;
    }

    template <bool need_init = false>
    ALWAYS_INLINE Field *ResolveField(BytecodeId id)
    {
        auto cache = this->GetThread()->GetInterpreterCache();
        // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE)
        auto *res = cache->template Get<Field>(this->GetInst().GetAddress(), this->GetFrame()->GetMethod());
        if (res != nullptr) {
            return res;
        }

        if (need_init) {
            // Update bytecode offset in the current frame as RuntimeIfaceT::ResolveField can trigger class initializer
            this->UpdateBytecodeOffset();
        }

        this->GetFrame()->SetAcc(this->GetAcc());
        auto *field = RuntimeIfaceT::ResolveField(this->GetThread(), *this->GetFrame()->GetMethod(), id);
        this->GetAcc() = this->GetFrame()->GetAcc();
        if (UNLIKELY(field == nullptr)) {
            ASSERT(this->GetThread()->HasPendingException());
            return nullptr;
        }

        cache->Set(this->GetInst().GetAddress(), field, this->GetFrame()->GetMethod());
        return field;
    }

    template <bool need_init = false>
    ALWAYS_INLINE Class *ResolveType(BytecodeId id)
    {
        auto cache = this->GetThread()->GetInterpreterCache();
        // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE)
        auto *res = cache->template Get<Class>(this->GetInst().GetAddress(), this->GetFrame()->GetMethod());
        if (res != nullptr) {
            ASSERT(!need_init || res->IsInitializing() || res->IsInitialized());
            return res;
        }

        this->GetFrame()->SetAcc(this->GetAcc());
        auto *klass =
            RuntimeIfaceT::template ResolveClass<need_init>(this->GetThread(), *this->GetFrame()->GetMethod(), id);
        this->GetAcc() = this->GetFrame()->GetAcc();
        if (UNLIKELY(klass == nullptr)) {
            ASSERT(this->GetThread()->HasPendingException());
            return nullptr;
        }

        ASSERT(!need_init || klass->IsInitializing() || klass->IsInitialized());

        cache->Set(this->GetInst().GetAddress(), klass, this->GetFrame()->GetMethod());
        return klass;
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE inline void CopyCallAccShortArguments(Frame &frame, uint32_t num_vregs)
    {
        static_assert(format == BytecodeInstruction::Format::V4_IMM4_ID16, "Invalid call acc short format");
        auto acc_position = static_cast<size_t>(this->GetInst().template GetImm<format, 0>());
        switch (acc_position) {
            case 0U:
                frame.GetVReg(num_vregs) = this->GetAcc();
                frame.GetVReg(num_vregs + 1U) =
                    this->GetFrame()->GetVReg(this->GetInst().template GetVReg<format, 0>());
                break;
            case 1U:
                frame.GetVReg(num_vregs) = this->GetFrame()->GetVReg(this->GetInst().template GetVReg<format, 0>());
                frame.GetVReg(num_vregs + 1U) = this->GetAcc();
                break;
            default:
                UNREACHABLE();
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE inline void CopyCallAccArguments(Frame &frame, uint32_t num_vregs)
    {
        static_assert(format == BytecodeInstruction::Format::V4_V4_V4_IMM4_ID16, "Invalid call acc format");
        auto acc_position = static_cast<size_t>(this->GetInst().template GetImm<format, 0>());
        switch (acc_position) {
            case 0U:
                frame.GetVReg(num_vregs) = this->GetAcc();
                frame.GetVReg(num_vregs + 1U) =
                    this->GetFrame()->GetVReg(this->GetInst().template GetVReg<format, 0>());
                frame.GetVReg(num_vregs + 2U) =
                    this->GetFrame()->GetVReg(this->GetInst().template GetVReg<format, 1>());
                frame.GetVReg(num_vregs + 3U) =
                    this->GetFrame()->GetVReg(this->GetInst().template GetVReg<format, 2>());
                break;
            case 1U:
                frame.GetVReg(num_vregs) = this->GetFrame()->GetVReg(this->GetInst().template GetVReg<format, 0>());
                frame.GetVReg(num_vregs + 1U) = this->GetAcc();
                frame.GetVReg(num_vregs + 2U) =
                    this->GetFrame()->GetVReg(this->GetInst().template GetVReg<format, 1>());
                frame.GetVReg(num_vregs + 3U) =
                    this->GetFrame()->GetVReg(this->GetInst().template GetVReg<format, 2>());
                break;
            case 2U:
                frame.GetVReg(num_vregs) = this->GetFrame()->GetVReg(this->GetInst().template GetVReg<format, 0>());
                frame.GetVReg(num_vregs + 1U) =
                    this->GetFrame()->GetVReg(this->GetInst().template GetVReg<format, 1>());
                frame.GetVReg(num_vregs + 2U) = this->GetAcc();
                frame.GetVReg(num_vregs + 3U) =
                    this->GetFrame()->GetVReg(this->GetInst().template GetVReg<format, 2>());
                break;
            case 3U:
                frame.GetVReg(num_vregs) = this->GetFrame()->GetVReg(this->GetInst().template GetVReg<format, 0>());
                frame.GetVReg(num_vregs + 1U) =
                    this->GetFrame()->GetVReg(this->GetInst().template GetVReg<format, 1>());
                frame.GetVReg(num_vregs + 2U) =
                    this->GetFrame()->GetVReg(this->GetInst().template GetVReg<format, 2>());
                frame.GetVReg(num_vregs + 3U) = this->GetAcc();
                break;
            default:
                UNREACHABLE();
        }
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE inline void CopyCallAccArguments(Frame &frame, uint32_t num_vregs, uint32_t num_actual_args)
    {
        size_t acc_position = this->GetInst().template GetImm<format, 0>();
        for (size_t i = 0; i < num_actual_args; i++) {
            if (i < acc_position) {
                uint16_t vs = this->GetInst().GetVReg(i);
                frame.GetVReg(num_vregs + i) = this->GetFrame()->GetVReg(vs);
            } else if (i == acc_position) {
                frame.GetVReg(num_vregs + i) = this->GetAcc();
            } else {
                uint16_t vs = this->GetInst().GetVReg(i - 1);
                frame.GetVReg(num_vregs + i) = this->GetFrame()->GetVReg(vs);
            }
        }
    }

    template <BytecodeInstruction::Format format, bool initobj>
    ALWAYS_INLINE inline void CopyCallShortArguments(Frame &frame, uint32_t num_vregs)
    {
        static_assert(format == BytecodeInstruction::Format::V4_V4_ID16, "Invalid call short format");

        constexpr size_t shitf = initobj ? 1U : 0;
        if constexpr (initobj) {
            frame.GetVReg(num_vregs) = this->GetAcc();
        }

        frame.GetVReg(num_vregs + shitf) = this->GetFrame()->GetVReg(this->GetInst().template GetVReg<format, 0>());
        frame.GetVReg(num_vregs + shitf + 1U) =
            this->GetFrame()->GetVReg(this->GetInst().template GetVReg<format, 1U>());
    }

    template <BytecodeInstruction::Format format, bool initobj>
    ALWAYS_INLINE inline void CopyCallArguments(Frame &frame, uint32_t num_vregs)
    {
        static_assert(format == BytecodeInstruction::Format::V4_V4_V4_V4_ID16, "Invalid call format");

        constexpr size_t shift = initobj ? 1U : 0;
        if constexpr (initobj) {
            frame.GetVReg(num_vregs) = this->GetAcc();
        }

        frame.GetVReg(num_vregs + shift) = this->GetFrame()->GetVReg(this->GetInst().template GetVReg<format, 0>());
        frame.GetVReg(num_vregs + shift + 1U) =
            this->GetFrame()->GetVReg(this->GetInst().template GetVReg<format, 1U>());
        frame.GetVReg(num_vregs + shift + 2U) =
            this->GetFrame()->GetVReg(this->GetInst().template GetVReg<format, 2U>());
        frame.GetVReg(num_vregs + shift + 3U) =
            this->GetFrame()->GetVReg(this->GetInst().template GetVReg<format, 3U>());
    }

    template <bool initobj>
    ALWAYS_INLINE inline void CopyCallArguments(Frame &frame, uint32_t num_vregs, uint32_t num_actual_args)
    {
        constexpr size_t shift = initobj ? 1U : 0;
        if constexpr (initobj) {
            frame.GetVReg(num_vregs) = this->GetAcc();
        }

        for (size_t i = 0; i < num_actual_args - shift; i++) {
            uint16_t vs = this->GetInst().GetVReg(i);
            frame.GetVReg(num_vregs + shift + i) = this->GetFrame()->GetVReg(vs);
        }
    }

    template <BytecodeInstruction::Format format, bool initobj>
    ALWAYS_INLINE inline void CopyRangeArguments(Frame &frame, uint32_t num_vregs, uint32_t num_actual_args)
    {
        constexpr size_t shift = initobj ? 1U : 0;
        if constexpr (initobj) {
            frame.GetVReg(num_vregs) = this->GetAcc();
        }

        uint16_t start_reg = this->GetInst().template GetVReg<format, 0>();
        for (size_t i = 0; i < num_actual_args - shift; i++) {
            frame.GetVReg(num_vregs + shift + i) = this->GetFrame()->GetVReg(start_reg + i);
        }
    }

    template <BytecodeInstruction::Format format, bool is_dynamic, bool is_range, bool accept_acc, bool initobj>
    ALWAYS_INLINE inline void CopyArguments(Frame &frame, uint32_t num_vregs, [[maybe_unused]] uint32_t num_actual_args,
                                            uint32_t num_args)
    {
        if (num_args == 0) {
            return;
        }
        if constexpr (is_range) {
            CopyRangeArguments<format, initobj>(frame, num_vregs, num_actual_args);
        } else if constexpr (accept_acc) {
            if constexpr (format == BytecodeInstruction::Format::V4_IMM4_ID16) {
                CopyCallAccShortArguments<format>(frame, num_vregs);
            } else if constexpr (format == BytecodeInstruction::Format::V4_V4_V4_IMM4_ID16) {
                CopyCallAccArguments<format>(frame, num_vregs);
            } else {
                CopyCallAccArguments<format>(frame, num_vregs, num_actual_args);
            }
        } else {
            if constexpr (format == BytecodeInstruction::Format::V4_V4_ID16) {
                CopyCallShortArguments<format, initobj>(frame, num_vregs);
            } else if constexpr (format == BytecodeInstruction::Format::V4_V4_V4_V4_ID16) {
                CopyCallArguments<format, initobj>(frame, num_vregs);
            } else {
                CopyCallArguments<initobj>(frame, num_vregs, num_actual_args);
            }
        }
        if constexpr (is_dynamic) {
            LanguageContext ctx = this->GetThread()->GetLanguageContext();
            DecodedTaggedValue initial_value = ctx.GetInitialDecodedValue();
            for (size_t i = num_actual_args; i < num_args; ++i) {
                frame.GetVReg(num_vregs + i).SetValue(initial_value.value);
                frame.GetVReg(num_vregs + i).SetTag(initial_value.tag);
            }
        }
    }

    template <BytecodeInstruction::Format format, bool is_dynamic, bool is_range, bool accept_acc, bool initobj>
    ALWAYS_INLINE inline bool CreateAndSetFrame(Method *method, Frame **frame, uint32_t num_vregs)
    {
        uint32_t num_declared_args = method->GetNumArgs();
        uint32_t num_actual_args;
        uint32_t frame_size;
        uint32_t nregs;
        if constexpr (is_dynamic) {
            // +1 means function object itself
            num_actual_args = static_cast<uint32_t>(this->GetInst().template GetImm<format, 0>() + 1);
            frame_size = num_vregs + std::max(num_declared_args, num_actual_args);
            nregs = frame_size;
        } else {
            num_actual_args = num_declared_args;
            if (format == BytecodeInstruction::Format::V4_V4_ID16 ||
                format == BytecodeInstruction::Format::V4_IMM4_ID16) {
                frame_size = num_vregs + (initobj ? 3U : 2U);
            } else if (format == BytecodeInstruction::Format::V4_V4_V4_V4_ID16 ||
                       format == BytecodeInstruction::Format::V4_V4_V4_IMM4_ID16) {
                frame_size = num_vregs + (initobj ? 5U : 4U);
            } else {
                frame_size = num_vregs + num_declared_args;
            }
            nregs = num_vregs + num_declared_args;
        }
        *frame = RuntimeIfaceT::CreateFrameWithActualArgs(frame_size, nregs, num_actual_args, method, this->GetFrame());
        if (UNLIKELY(*frame == nullptr)) {
            RuntimeIfaceT::ThrowOutOfMemoryError("CreateFrame failed: " + method->GetFullName());
            this->MoveToExceptionHandler();
            return false;
        }
        (*frame)->SetAcc(this->GetAcc());

        CopyArguments<format, is_dynamic, is_range, accept_acc, initobj>(**frame, num_vregs, num_actual_args,
                                                                         num_declared_args);

        RuntimeIfaceT::SetCurrentFrame(this->GetThread(), *frame);

        return true;
    }

    template <BytecodeInstruction::Format format, bool is_dynamic, bool is_range, bool accept_acc, bool initobj>
    ALWAYS_INLINE inline void CallInterpreter(Method *method)
    {
        if (!method->Verify()) {
            RuntimeIfaceT::ThrowVerificationException(method->GetFullName());
            this->MoveToExceptionHandler();
            return;
        }

        Frame *frame = nullptr;

        panda_file::CodeDataAccessor cda(*method->GetPandaFile(), method->GetCodeId());
        auto num_vregs = cda.GetNumVregs();
        auto *instructions = cda.GetInstructions();

        CreateAndSetFrame<format, is_dynamic, is_range, accept_acc, initobj>(method, &frame, num_vregs);

        Runtime::GetCurrent()->GetNotificationManager()->MethodEntryEvent(this->GetThread(), method);

        volatile auto prev = this->GetFrame();
        volatile auto inst = this->GetInst();
        volatile auto dtable = this->GetDispatchTable();

        frame->SetInstruction(instructions);
        // currently we only support nodebug -> debug transfer.
        if (UNLIKELY(Runtime::GetCurrent()->IsDebugMode())) {
            ExecuteImpl_Inner<RuntimeIfaceT, true, false>(this->GetThread(), instructions, frame);
        } else {
            ExecuteImpl_Inner<RuntimeIfaceT, false>(this->GetThread(), instructions, frame);
        }

        Runtime::GetCurrent()->GetNotificationManager()->MethodExitEvent(this->GetThread(), method);

        BytecodeInstruction bi(inst.GetAddress());
        Frame *f = prev;

        this->SetFrame(f);
        this->SetInst(bi);
        this->SetDispatchTable(dtable);

        RuntimeIfaceT::SetCurrentFrame(this->GetThread(), this->GetFrame());

        if (UNLIKELY(this->GetThread()->HasPendingException())) {
            this->MoveToExceptionHandler();
        } else {
            this->GetAcc() = frame->GetAcc();
            this->template MoveToNextInst<format, true>();
        }

        if constexpr (initobj) {
            this->GetAcc() = prev->GetAcc();
        }

        RuntimeIfaceT::FreeFrame(frame);
    }

    template <BytecodeInstruction::Format format, bool is_dynamic, bool is_range, bool accept_acc, bool initobj>
    ALWAYS_INLINE inline void CallInterpreterStackless(Method *method)
    {
        if (!method->Verify()) {
            RuntimeIfaceT::ThrowVerificationException(method->GetFullName());
            this->MoveToExceptionHandler();
            return;
        }

        Frame *frame = nullptr;

        panda_file::CodeDataAccessor cda(*method->GetPandaFile(), method->GetCodeId());
        auto num_vregs = cda.GetNumVregs();
        auto *instructions = cda.GetInstructions();

        CreateAndSetFrame<format, is_dynamic, is_range, accept_acc, initobj>(method, &frame, num_vregs);

        Runtime::GetCurrent()->GetNotificationManager()->MethodEntryEvent(this->GetThread(), method);

        frame->SetStackless();
        if constexpr (initobj) {
            frame->SetInitobj();
        }
        frame->SetInstruction(instructions);
        this->template MoveToNextInst<format, false>();
        this->GetFrame()->SetNextInstruction(this->GetInst());
        this->GetInstructionHandlerState()->UpdateInstructionHandlerState(instructions, frame);
        EVENT_METHOD_ENTER(frame->GetMethod()->GetFullName(), events::MethodEnterKind::INTERP,
                           this->GetThread()->RecordMethodEnter());
    }

    template <bool is_dynamic = false>
    ALWAYS_INLINE void HandleCallPrologue(Method *method)
    {
        ASSERT(method != nullptr);
        if constexpr (is_dynamic) {
            LOG(DEBUG, INTERPRETER) << "Entry: Runtime Call.";
        } else {
            LOG(DEBUG, INTERPRETER) << "Entry: " << method->GetFullName();
        }
        if (this->GetThread()->TestAllFlags()) {
            this->GetFrame()->SetAcc(this->GetAcc());
            RuntimeIfaceT::Safepoint();
            this->GetAcc() = this->GetFrame()->GetAcc();
            if (UNLIKELY(this->GetThread()->HasPendingException())) {
                this->MoveToExceptionHandler();
                return;
            }
        }
        if (!method->HasCompiledCode()) {
            this->UpdateHotness(method);
        }
    }

    template <BytecodeInstruction::Format format, bool is_dynamic = false, bool is_range = false,
              bool accept_acc = false, bool initobj = false>
    ALWAYS_INLINE void HandleCall(Method *method)
    {
        HandleCallPrologue<is_dynamic>(method);

        if (!method->HasCompiledCode()) {
            if (Runtime::GetCurrent()->IsDebugMode() == enable_instrumentation) {
                CallInterpreterStackless<format, is_dynamic, is_range, accept_acc, initobj>(method);
                return;
            } else {
                CallInterpreter<format, is_dynamic, is_range, accept_acc, initobj>(method);
            }
        } else {
            this->GetFrame()->SetAcc(this->GetAcc());
            if constexpr (is_dynamic) {
                InterpreterToCompiledCodeBridgeDyn(this->GetInst().GetAddress(), this->GetFrame(), method,
                                                   this->GetThread());
            } else {
                InterpreterToCompiledCodeBridge(this->GetInst().GetAddress(), this->GetFrame(), method,
                                                this->GetThread());
            }
            this->GetThread()->SetCurrentFrameIsCompiled(false);
            this->GetThread()->SetCurrentFrame(this->GetFrame());

            if (UNLIKELY(this->GetThread()->HasPendingException())) {
                this->MoveToExceptionHandler();
            } else {
                this->GetAcc() = this->GetFrame()->GetAcc();
                this->template MoveToNextInst<format, true>();
            }
        }
        if constexpr (is_dynamic) {
            LOG(DEBUG, INTERPRETER) << "Exit: Runtime Call.";
        } else {
            LOG(DEBUG, INTERPRETER) << "Exit: " << method->GetFullName();
        }
    }

    template <BytecodeInstruction::Format format, bool is_range = false, bool accept_acc = false>
    ALWAYS_INLINE void HandleVirtualCall(Method *method)
    {
        ASSERT(method != nullptr);
        ASSERT(!method->IsStatic());
        ASSERT(!method->IsConstructor());

        ObjectHeader *obj = this->GetCallerObject<format, accept_acc>();
        if (UNLIKELY(obj == nullptr)) {
            return;
        }
        auto *cls = obj->ClassAddr<Class>();
        ASSERT(cls != nullptr);
        auto *resolved = cls->ResolveVirtualMethod(method);
        ASSERT(resolved != nullptr);

        if (UNLIKELY(resolved->IsAbstract())) {
            RuntimeIfaceT::ThrowAbstractMethodError(resolved);
            this->MoveToExceptionHandler();
            return;
        }

        ProfilingData *prof_data = this->GetFrame()->GetMethod()->GetProfilingData();
        if (prof_data != nullptr) {
            prof_data->UpdateInlineCaches(this->GetBytecodeOffset(), obj->ClassAddr<Class>());
        }

        HandleCall<format, false, is_range, accept_acc>(resolved);
    }

    template <BytecodeInstruction::Format format, template <typename OpT> class Op>
    ALWAYS_INLINE void HandleCondJmpz()
    {
        auto imm = this->GetInst().template GetImm<format>();
        int32_t v1 = this->GetAcc().Get();

        LOG_INST() << "\t"
                   << "cond jmpz " << std::hex << imm;

        std::size_t false_value = 0;

        if (Op<int32_t>()(v1, false_value)) {
            if (!InstrumentBranches(imm)) {
                this->template JumpToInst<false>(imm);
            }
        } else {
            this->template MoveToNextInst<format, false>();
        }
    }

    template <BytecodeInstruction::Format format, template <typename OpT> class Op>
    ALWAYS_INLINE void HandleCondJmp()
    {
        auto imm = this->GetInst().template GetImm<format>();
        uint16_t vs = this->GetInst().template GetVReg<format>();

        LOG_INST() << "\t"
                   << "cond jmp v" << vs << ", " << std::hex << imm;

        int32_t v1 = this->GetAcc().Get();
        int32_t v2 = this->GetFrame()->GetVReg(vs).Get();

        if (Op<int32_t>()(v1, v2)) {
            if (!InstrumentBranches(imm)) {
                this->template JumpToInst<false>(imm);
            }
        } else {
            this->template MoveToNextInst<format, false>();
        }
    }

    template <BytecodeInstruction::Format format, template <typename OpT> class Op>
    ALWAYS_INLINE void HandleCondJmpzObj()
    {
        auto imm = this->GetInst().template GetImm<format>();
        ObjectHeader *v1 = this->GetAcc().GetReference();

        LOG_INST() << "\t"
                   << "cond jmpz.obj " << std::hex << imm;

        if (Op<ObjectHeader *>()(v1, nullptr)) {
            if (!InstrumentBranches(imm)) {
                this->template JumpToInst<false>(imm);
            }
        } else {
            this->template MoveToNextInst<format, false>();
        }
    }

    template <BytecodeInstruction::Format format, template <typename OpT> class Op>
    ALWAYS_INLINE void HandleCondJmpObj()
    {
        auto imm = this->GetInst().template GetImm<format>();
        uint16_t vs = this->GetInst().template GetVReg<format>();

        LOG_INST() << "\t"
                   << "cond jmp.obj v" << vs << ", " << std::hex << imm;

        ObjectHeader *v1 = this->GetAcc().GetReference();
        ObjectHeader *v2 = this->GetFrame()->GetVReg(vs).GetReference();

        if (Op<ObjectHeader *>()(v1, v2)) {
            if (!InstrumentBranches(imm)) {
                this->template JumpToInst<false>(imm);
            }
        } else {
            this->template MoveToNextInst<format, false>();
        }
    }

    template <BytecodeInstruction::Format format, typename OpT, template <typename> class Op, bool is_div = false>
    ALWAYS_INLINE void HandleBinaryOp2Imm()
    {
        OpT v1 = this->GetAcc().template GetAs<OpT>();
        OpT v2 = this->GetInst().template GetImm<format>();
        LOG_INST() << "\t"
                   << "binop2imm " << std::hex << v2;

        if (is_div && UNLIKELY(v2 == 0)) {
            RuntimeIfaceT::ThrowArithmeticException();
            this->MoveToExceptionHandler();
        } else {
            this->GetAcc().Set(Op<OpT>()(v1, v2));
            this->template MoveToNextInst<format, is_div>();
        }
    }

    template <BytecodeInstruction::Format format, typename OpT, template <typename> class Op, bool is_div = false>
    ALWAYS_INLINE void HandleBinaryOp2()
    {
        OpT v1 = this->GetAcc().template GetAs<OpT>();
        uint16_t vs1 = this->GetInst().template GetVReg<format>();
        LOG_INST() << "\t"
                   << "binop2 v" << vs1;
        OpT v2 = this->GetFrame()->GetVReg(vs1).template GetAs<OpT>();

        if (is_div && UNLIKELY(v2 == 0)) {
            RuntimeIfaceT::ThrowArithmeticException();
            this->MoveToExceptionHandler();
        } else {
            this->GetAcc().Set(Op<OpT>()(v1, v2));
            this->template MoveToNextInst<format, is_div>();
        }
    }

    template <BytecodeInstruction::Format format, typename OpT, template <typename> class Op, bool is_div = false>
    ALWAYS_INLINE void HandleBinaryOp()
    {
        uint16_t vs1 = this->GetInst().template GetVReg<format, 0>();
        uint16_t vs2 = this->GetInst().template GetVReg<format, 1>();
        LOG_INST() << "\t"
                   << "binop2 v" << vs1 << ", v" << vs2;
        OpT v1 = this->GetFrame()->GetVReg(vs1).template GetAs<OpT>();
        OpT v2 = this->GetFrame()->GetVReg(vs2).template GetAs<OpT>();

        if (is_div && UNLIKELY(v2 == 0)) {
            RuntimeIfaceT::ThrowArithmeticException();
            this->MoveToExceptionHandler();
        } else {
            this->GetAcc().SetPrimitive(Op<OpT>()(v1, v2));
            this->template MoveToNextInst<format, is_div>();
        }
    }

    template <BytecodeInstruction::Format format, typename OpT, template <typename> class Op>
    ALWAYS_INLINE void HandleUnaryOp()
    {
        OpT v = this->GetAcc().template GetAs<OpT>();
        this->GetAcc().Set(Op<OpT>()(v));
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format, typename From, typename To>
    ALWAYS_INLINE void HandleConversion()
    {
        this->GetAcc().Set(static_cast<To>(this->GetAcc().template GetAs<From>()));
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format, typename From, typename To>
    ALWAYS_INLINE void HandleFloatToIntConversion()
    {
        From value = this->GetAcc().template GetAs<From>();
        To res;

        constexpr To MIN_INT = std::numeric_limits<To>::min();
        constexpr To MAX_INT = std::numeric_limits<To>::max();
        const auto FLOAT_MIN_INT = static_cast<From>(MIN_INT);
        const auto FLOAT_MAX_INT = static_cast<From>(MAX_INT);

        if (value > FLOAT_MIN_INT) {
            if (value < FLOAT_MAX_INT) {
                res = static_cast<To>(value);
            } else {
                res = MAX_INT;
            }
        } else if (std::isnan(value)) {
            res = 0;
        } else {
            res = MIN_INT;
        }

        this->GetAcc().Set(static_cast<To>(res));
        this->template MoveToNextInst<format, false>();
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void InitializeObject(Class *klass, Method *method)
    {
        if (UNLIKELY(method == nullptr)) {
            this->MoveToExceptionHandler();
            return;
        }

        if (UNLIKELY(method->IsAbstract())) {
            RuntimeIfaceT::ThrowAbstractMethodError(method);
            this->MoveToExceptionHandler();
            return;
        }

        auto *obj = RuntimeIfaceT::CreateObject(klass);
        if (UNLIKELY(obj == nullptr)) {
            this->MoveToExceptionHandler();
            return;
        }

        this->GetAcc().SetReference(obj);
        this->GetFrame()->GetAcc() = this->GetAcc();

        constexpr bool is_range = format == BytecodeInstruction::Format::V8_ID16;
        HandleCall<format, false, is_range, false, true>(method);
    }

    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE void InitializeObject(BytecodeId &method_id)
    {
        Class *klass;
        auto cache = this->GetThread()->GetInterpreterCache();
        // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE)
        auto *method = cache->template Get<Method>(this->GetInst().GetAddress(), this->GetFrame()->GetMethod());
        if (method != nullptr) {
            klass = method->GetClass();
        } else {
            klass = RuntimeIfaceT::GetMethodClass(this->GetFrame()->GetMethod(), method_id);
            this->GetAcc().SetPrimitive(0);
            if (UNLIKELY(klass == nullptr)) {
                this->MoveToExceptionHandler();
                return;
            }
        }

        if (UNLIKELY(klass->IsArrayClass())) {
            ASSERT(utf::IsEqual(RuntimeIfaceT::GetMethodName(this->GetFrame()->GetMethod(), method_id),
                                utf::CStringAsMutf8("<init>")));

            DimIterator<format> dim_iter {this->GetInst(), this->GetFrame()};
            auto nargs = RuntimeIfaceT::GetMethodArgumentsCount(this->GetFrame()->GetMethod(), method_id);
            auto obj = coretypes::Array::CreateMultiDimensionalArray<DimIterator<format>>(this->GetThread(), klass,
                                                                                          nargs, dim_iter);
            if (LIKELY(obj != nullptr)) {
                this->GetAcc().SetReference(obj);
                this->template MoveToNextInst<format, false>();
            } else {
                this->MoveToExceptionHandler();
            }
        } else {
            if (UNLIKELY(method == nullptr)) {
                method = ResolveMethod(method_id);
            }
            this->UpdateBytecodeOffset();
            InitializeObject<format>(klass, method);
        }
    }

private:
    template <BytecodeInstruction::Format format>
    ALWAYS_INLINE ObjectHeader *GetObjHelper()
    {
        uint16_t obj_vreg = this->GetInst().template GetVReg<format, 0>();
        return this->GetFrame()->GetVReg(obj_vreg).GetReference();
    }

    template <BytecodeInstruction::Format format, bool accept_acc = false>
    ALWAYS_INLINE ObjectHeader *GetCallerObject()
    {
        ObjectHeader *obj = nullptr;
        if constexpr (accept_acc) {
            if (this->GetInst().template GetImm<format, 0>() == 0) {
                obj = this->GetAcc().GetReference();
            } else {
                obj = GetObjHelper<format>();
            }
        } else {
            obj = GetObjHelper<format>();
        }

        if (UNLIKELY(obj == nullptr)) {
            RuntimeIfaceT::ThrowNullPointerException();
            this->MoveToExceptionHandler();
        }
        return obj;
    }
};

#include <interpreter-inl_gen.h>

extern "C" void ExecuteImplStub(ManagedThread *thread, const uint8_t *pc, Frame *frame, void *impl);

template <class RuntimeIfaceT, bool enable_instrumentation, bool jump_to_eh>
void ExecuteImpl_Inner(ManagedThread *thread, const uint8_t *pc, Frame *frame)
{
    void *impl = reinterpret_cast<void *>(&ExecuteImpl<RuntimeIfaceT, enable_instrumentation, jump_to_eh>);
    ExecuteImplStub(thread, pc, frame, impl);
}

}  // namespace panda::interpreter

#endif  // PANDA_RUNTIME_INTERPRETER_INTERPRETER_INL_H_
