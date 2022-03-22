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

#ifndef PANDA_VERIFICATION_ABSINT_ABS_INT_INL_H_
#define PANDA_VERIFICATION_ABSINT_ABS_INT_INL_H_

#include "type/type_system.h"
#include "panda_types.h"
#include "verification_context.h"
#include "verification_status.h"

#include "verification/debug/breakpoint/breakpoint.h"
#include "verification/debug/allowlist/allowlist.h"

#include "verification/job_queue/cache.h"

#include "verifier_messages.h"
#include "abs_int_inl_compat_checks.h"

#include "bytecode_instruction-inl.h"
#include "file_items.h"
#include "macros.h"
#include "include/method.h"
#include "include/runtime.h"
#include "runtime/include/class.h"
#include "runtime/interpreter/runtime_interface.h"
#include "include/mem/panda_containers.h"

#include "util/lazy.h"
#include "util/str.h"

#include "utils/logger.h"

#include <cstdint>
#include <cmath>

#include <array>
#include <functional>
#include <iomanip>
#include <limits>
#include <memory>
#include <type_traits>
#include <unordered_map>

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_INST()                                             \
    do {                                                       \
        if (!GetInst().IsValid()) {                            \
            SHOW_MSG(InvalidInstruction)                       \
            LOG_VERIFIER_INVALID_INSTRUCTION();                \
            END_SHOW_MSG();                                    \
            SET_STATUS_FOR_MSG(InvalidInstruction);            \
            return false;                                      \
        }                                                      \
        if (CurrentJob.Options().ShowContext()) {              \
            DumpRegs(ExecCtx().CurrentRegContext());           \
        }                                                      \
        SHOW_MSG(DebugAbsIntLogInstruction)                    \
        LOG_VERIFIER_DEBUG_ABS_INT_LOG_INSTRUCTION(GetInst()); \
        END_SHOW_MSG();                                        \
    } while (0)

#ifndef NDEBUG
#define DBGBRK()                                                                                      \
    if (debug_) {                                                                                     \
        DBG_MANAGED_BRK(panda::verifier::debug::Component::VERIFIER, CurrentJob.JobCachedMethod().id, \
                        inst_.GetOffset());                                                           \
    }
#else
#define DBGBRK()
#endif

#define MSG(Message) CurrentJob.Options().Msg(VerifierMessage::Message)

#define SHOW_MSG(Message) \
    MSG(Message).IfNotHidden([&]{
#define END_SHOW_MSG() \
    })

#define SET_STATUS_FOR_MSG(Message)                                         \
    do {                                                                    \
        MSG(Message).IfWarning([&] {                                        \
            if (status_ != VerificationStatus::ERROR) {                     \
                status_ = VerificationStatus::WARNING;                      \
            }                                                               \
        });                                                                 \
        MSG(Message).IfError([&] { status_ = VerificationStatus::ERROR; }); \
    } while (0)

/*
Current definition of context incompatibility is NOT CORRECT one!
There are situations when verifier will rule out fully correct programs.
For instance:
....
movi v0,0
ldai 0
jmp label1
....
lda.str ""
sta v0
ldai 0
jmp label1
.....
label1:
  return

Here we have context incompatibility on label1, but it does not harm, because conflicting reg is
not used anymore.

Solutions:
1(current). Conflicts are reported as warnings, conflicting regs are removed for resulting context.
  So, on attempt of usage of conflicting reg, absint will fail with message of undefined reg.
  May be mark them as conflicting? (done)
2. On each label/checkpoint compute set of registers that will be used in next computations and process
   conflicting contexts modulo used registers set. It is complex solution, but very precise.
*/

namespace panda::verifier {
template <typename Handler>
PandaVector<Type> FilterItems(const PandaVector<Type> &items, Handler &&handler)
{
    PandaVector<Type> result;
    for (const auto &item : items) {
        if (handler(item)) {
            result.push_back(item);
        }
    }
    return result;
}

template <typename Handler>
bool IsItemPresent(const PandaVector<Type> &items, Handler &&handler)
{
    for (const auto &item : items) {
        if (handler(item)) {
            return true;
        }
    }
    return false;
}

class AbsIntInstructionHandler {
    using CachedClass = CacheOfRuntimeThings::CachedClass;
    using CachedMethod = CacheOfRuntimeThings::CachedMethod;
    using CachedField = CacheOfRuntimeThings::CachedField;

public:
    static constexpr int ACC = -1;
    static constexpr int INVALID_REG = -2;
    using TypeId = panda_file::Type::TypeId;

    const Type &U1;
    const Type &I8;
    const Type &U8;
    const Type &I16;
    const Type &U16;
    const Type &I32;
    const Type &U32;
    const Type &I64;
    const Type &U64;
    const Type &F32;
    const Type &F64;

    const Job &CurrentJob;

    AbsIntInstructionHandler(VerificationContext *v_ctx, const uint8_t *pc, EntryPointType code_type)
        : U1 {v_ctx->Types().U1()},
          I8 {v_ctx->Types().I8()},
          U8 {v_ctx->Types().U8()},
          I16 {v_ctx->Types().I16()},
          U16 {v_ctx->Types().U16()},
          I32 {v_ctx->Types().I32()},
          U32 {v_ctx->Types().U32()},
          I64 {v_ctx->Types().I64()},
          U64 {v_ctx->Types().U64()},
          F32 {v_ctx->Types().F32()},
          F64 {v_ctx->Types().F64()},
          CurrentJob {v_ctx->GetJob()},
          inst_(pc, v_ctx->CflowInfo().InstMap().AddrStart<const uint8_t *>(),
                v_ctx->CflowInfo().InstMap().AddrEnd<const uint8_t *>()),
          context_ {*v_ctx},
          status_ {VerificationStatus::OK},
          code_type_ {code_type}
    {
        ASSERT(v_ctx != nullptr);
#ifndef NDEBUG
        const auto &verif_opts = Runtime::GetCurrent()->GetVerificationOptions();
        if (verif_opts.Mode.DebugEnable) {
            debug_ =
                DBG_MANAGED_BRK_PRESENT(panda::verifier::debug::Component::VERIFIER, CurrentJob.JobCachedMethod().id);
            if (debug_) {
                LOG(DEBUG, VERIFIER) << "Debug mode is on";
            }
        }
#endif
    }

    ~AbsIntInstructionHandler() = default;

    VerificationStatus GetStatus() const
    {
        return status_;
    }

    uint8_t GetPrimaryOpcode() const
    {
        return inst_.GetPrimaryOpcode();
    }

    uint8_t GetSecondaryOpcode() const
    {
        return inst_.GetSecondaryOpcode();
    }

    bool IsPrimaryOpcodeValid() const
    {
        return inst_.IsPrimaryOpcodeValid();
    }

    bool IsRegDefined(int reg);

    const PandaString &ImageOf(const Type &type);

    PandaString ImageOf(const AbstractType &abstract_type);

    PandaString ImageOf(const TypeSet &types);

    template <typename Container>
    PandaString ImagesOf(const Container &types)
    {
        PandaString result {"[ "};
        bool comma = false;
        for (const auto &type : types) {
            if (comma) {
                result += ", ";
            }
            result += ImageOf(type);
            comma = true;
        }
        result += " ]";
        return result;
    }

    PandaVector<Type> SubtypesOf(const PandaVector<Type> &types);
    PandaVector<Type> SubtypesOf(std::initializer_list<Type> types);

    PandaVector<Type> SupertypesOf(const PandaVector<Type> &types);
    PandaVector<Type> SupertypesOf(std::initializer_list<Type> types);

    template <typename Container>
    bool CheckTypes(const Type &type, const Container &tgt_types)
    {
        for (const auto &t : tgt_types) {
            if (type <= t) {
                return true;
            }
        }
        return false;
    }

    template <typename Container>
    bool CheckTypes(const AbstractType &type, const Container &tgt_types)
    {
        for (const auto &t : tgt_types) {
            if (type.ExistsType([&](auto type1) { return type1 <= t; })) {
                return true;
            }
        }
        return false;
    }

    template <typename Container>
    bool CheckRegTypes(int reg, const Container &tgt_types)
    {
        if (!IsRegDefined(reg)) {
            return false;
        }
        auto &&type = GetRegType(reg);

        if (CheckTypes(type, tgt_types) == true) {
            return true;
        }

        SHOW_MSG(BadRegisterType)
        LOG_VERIFIER_BAD_REGISTER_TYPE(RegisterName(reg, true), ImageOf(type), ImagesOf(tgt_types),
                                       ImagesOf(SubtypesOf(tgt_types)));
        END_SHOW_MSG();
        return false;
    }

    bool CheckRegTypesTakingIntoAccountTypecasts(int reg, Type type)
    {
        if (!IsRegDefined(reg)) {
            return false;
        }
        bool result = false;
        auto handler = [&result, &type](const AbstractTypedValue &atv) {
            const auto &at = atv.GetAbstractType();
            if (at.IsType()) {
                if (at.GetType() <= type) {
                    result = true;
                }
            } else if (at.IsTypeSet()) {
                at.GetTypeSet().ForAll([&](const Type &type_in_at) {
                    if (type_in_at <= type) {
                        result = true;
                    }
                    return !result;
                });
            }
            return !result;
        };
        ExecCtx().ForAllTypesOfRegAccordingToTypecasts(reg, ExecCtx().CurrentRegContext(), handler);
        return result;
    }

    bool CheckRegTypes(int reg, std::initializer_list<Type> types);

    bool CheckTypes(const Type &type, std::initializer_list<Type> types);

    bool CheckTypes(const AbstractType &type, std::initializer_list<Type> types)
    {
        return CheckTypes<std::initializer_list<Type>>(type, types);
    }

    const AbstractTypedValue &GetReg(int reg_idx);

    const AbstractType &GetRegType(int reg_idx);

    void SetReg(int reg_idx, const AbstractTypedValue &val);
    void SetReg(int reg_idx, const AbstractType &type);

    void SetRegAndOthersOfSameOrigin(int reg_idx, const AbstractTypedValue &val);
    void SetRegAndOthersOfSameOrigin(int reg_idx, const AbstractType &type);

    const AbstractTypedValue &GetAcc();

    const AbstractType &GetAccType();

    void SetAcc(const AbstractTypedValue &val);
    void SetAcc(const AbstractType &type);

    void SetAccAndOthersOfSameOrigin(const AbstractTypedValue &val);
    void SetAccAndOthersOfSameOrigin(const AbstractType &type);

    template <typename T>
    AbstractTypedValue MkVal(T t)
    {
        return AbstractTypedValue {Types().TypeOf(t), context_.NewVar()};
    }

    AbstractTypedValue MkVal(const AbstractType &t);

    PandaTypes &Types();

    const Type &ReturnType();

    ExecContext &ExecCtx();

    void DumpRegs(const RegContext &ctx);

    bool CheckCtxCompatibility(const RegContext &src, const RegContext &dst);

    void Sync();

    void AssignRegToReg(int dst, int src)
    {
        auto atv = GetReg(src);
        if (!atv.GetOrigin().IsValid()) {
            // generate new origin and set all values to be originated at it
            AbstractTypedValue new_atv {atv, inst_};
            SetReg(src, new_atv);
            SetReg(dst, new_atv);
        } else {
            SetReg(dst, atv);
        }
    }

    void AssignAccToReg(int src)
    {
        AssignRegToReg(ACC, src);
    }

    void AssignRegToAcc(int dst)
    {
        AssignRegToReg(dst, ACC);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleNop()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleMov()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vd = inst_.GetVReg<format, 0x00>();
        uint16_t vs = inst_.GetVReg<format, 0x01>();
        Sync();
        if (!CheckRegTypes(vs, {Types().Bits32Type()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        AssignRegToReg(vd, vs);
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleMovWide()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vd = inst_.GetVReg<format, 0x00>();
        uint16_t vs = inst_.GetVReg<format, 0x01>();
        Sync();
        if (!CheckRegTypes(vs, {Types().Bits64Type()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        AssignRegToReg(vd, vs);
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleMovObj()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vd = inst_.GetVReg<format, 0x00>();
        uint16_t vs = inst_.GetVReg<format, 0x01>();
        Sync();
        if (!CheckRegTypes(vs, {Types().RefType()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        AssignRegToReg(vd, vs);
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleMovDyn()
    {
        status_ = VerificationStatus::ERROR;
        return false;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleMovi()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vd = inst_.GetVReg<format>();
        Sync();
        SetReg(vd, I32);
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleMoviDyn()
    {
        status_ = VerificationStatus::ERROR;
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleMoviWide()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vd = inst_.GetVReg<format>();
        Sync();
        SetReg(vd, I64);
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleFmovi()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vd = inst_.GetVReg<format>();
        Sync();
        SetReg(vd, F32);
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleFmoviWide()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vd = inst_.GetVReg<format>();
        Sync();
        SetReg(vd, F64);
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleMovNull()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vd = inst_.GetVReg<format>();
        Sync();
        SetReg(vd, Types().NullRefType());
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLdglobalDyn()
    {
        MoveToNextInst<format>();
        status_ = VerificationStatus::ERROR;
        return false;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLda()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs = inst_.GetVReg<format>();
        Sync();
        if (!CheckRegTypes(vs, {Types().Bits32Type()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        AssignAccToReg(vs);
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLdaDyn()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        MoveToNextInst<format>();
        status_ = VerificationStatus::ERROR;
        return false;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLdaiDyn()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        MoveToNextInst<format>();
        status_ = VerificationStatus::ERROR;
        return false;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLdaWide()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs = inst_.GetVReg<format>();
        Sync();
        if (!CheckRegTypes(vs, {Types().Bits64Type()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        AssignAccToReg(vs);
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLdaObj()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs = inst_.GetVReg<format>();
        Sync();
        if (!CheckRegTypes(vs, {Types().RefType()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        AssignAccToReg(vs);
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLdobjDyn()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        MoveToNextInst<format>();
        status_ = VerificationStatus::ERROR;
        return false;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLdai()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        SetAcc(I32);
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLdaiWide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        SetAcc(I64);
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleFldai()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        SetAcc(F32);
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleFldaiWide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        SetAcc(F64);
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleFldaiDyn()
    {
        LOG_INST();
        DBGBRK();
        status_ = VerificationStatus::ERROR;
        return false;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLdaStr()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        const auto *cached_class = GetCachedClass();
        if (cached_class == nullptr) {
            SET_STATUS_FOR_MSG(CannotResolveClassId);
            return false;
        }
        auto type = Types().TypeOf(*cached_class);
        SetAcc(type);
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLdaConst()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vd = inst_.GetVReg<format, 0>();
        Sync();
        const CachedClass *cached_class = GetCachedClass();
        if (cached_class == nullptr) {
            status_ = VerificationStatus::ERROR;
            return false;
        }
        auto type = Types().TypeOf(*cached_class);
        SetReg(vd, type);
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLdaType()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        auto *cached_class = GetCachedClass();
        if (cached_class == nullptr) {
            SET_STATUS_FOR_MSG(CannotResolveClassId);
            return false;
        }
        auto type = Types().TypeOf(*cached_class);
        if (!type.IsValid()) {
            LOG(DEBUG, VERIFIER) << "LDA_TYPE type of class is not valid.";
            return false;
        }
        auto lang = CurrentJob.JobCachedMethod().GetClass().source_lang;
        if (lang == panda_file::SourceLang::PANDA_ASSEMBLY) {
            SetAcc(Types().PandaClass());
        } else {
            SHOW_MSG(LdaTypeBadLanguage)
            LOG_VERIFIER_LDA_TYPE_BAD_LANGUAGE();
            END_SHOW_MSG();
            return false;
        }
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLdaNull()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        SetAcc(Types().NullRefType());
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleSta()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vd = inst_.GetVReg<format>();
        Sync();
        if (!CheckRegTypes(ACC, {Types().Bits32Type()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        AssignRegToAcc(vd);
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleStaDyn()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        MoveToNextInst<format>();
        status_ = VerificationStatus::ERROR;
        return false;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleStaWide()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vd = inst_.GetVReg<format>();
        Sync();
        if (!CheckRegTypes(ACC, {Types().Bits64Type()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        AssignRegToAcc(vd);
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleStaObj()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vd = inst_.GetVReg<format>();
        Sync();
        if (!CheckRegTypes(ACC, {Types().RefType()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        AssignRegToAcc(vd);
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleJmp()
    {
        LOG_INST();
        DBGBRK();
        int32_t imm = inst_.GetImm<format>();
        Sync();
        ProcessBranching(imm);
        return false;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleCmpDyn()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        status_ = VerificationStatus::ERROR;
        return false;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleCmpWide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(I64, I64, I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleUcmp()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(Types().Integral32Type(), Types().Integral32Type(), I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleUcmpWide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(Types().Integral64Type(), Types().Integral64Type(), I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleFcmpl()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(F32, F32, I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleFcmplWide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(F64, F64, I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleFcmpg()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(F32, F32, I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleFcmpgWide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(F64, F64, I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleJeqz()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleCondJmpz<format, std::equal_to>();
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleJeqzObj()
    {
        LOG_INST();
        DBGBRK();
        auto imm = inst_.GetImm<format>();

        Sync();

        if (!CheckRegTypes(ACC, {Types().RefType()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }

        if (!ProcessBranching(imm)) {
            return false;
        }

        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleJnez()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleCondJmpz<format, std::not_equal_to>();
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleJnezObj()
    {
        LOG_INST();
        DBGBRK();
        auto imm = inst_.GetImm<format>();
        Sync();

        if (!CheckRegTypes(ACC, {Types().RefType()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }

        if (!ProcessBranching(imm)) {
            return false;
        }

        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleJltz()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleCondJmpz<format, std::less>();
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleJgtz()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleCondJmpz<format, std::greater>();
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleJlez()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleCondJmpz<format, std::less_equal>();
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleJgez()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleCondJmpz<format, std::greater_equal>();
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleJeq()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleCondJmp<format, std::equal_to>();
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleJeqObj()
    {
        LOG_INST();
        DBGBRK();
        auto imm = inst_.GetImm<format>();
        uint16_t vs = inst_.GetVReg<format>();

        Sync();

        if (!CheckRegTypes(ACC, {Types().RefType()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }

        if (!CheckRegTypes(vs, {Types().RefType()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }

        if (!ProcessBranching(imm)) {
            return false;
        }

        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleJne()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleCondJmp<format, std::not_equal_to>();
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleJneObj()
    {
        LOG_INST();
        DBGBRK();
        auto imm = inst_.GetImm<format>();
        uint16_t vs = inst_.GetVReg<format>();

        Sync();

        if (!CheckRegTypes(ACC, {Types().RefType()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }

        if (!CheckRegTypes(vs, {Types().RefType()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }

        if (!ProcessBranching(imm)) {
            return false;
        }

        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleJlt()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleCondJmp<format, std::less>();
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleJgt()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleCondJmp<format, std::greater>();
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleJle()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleCondJmp<format, std::less_equal>();
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleJge()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleCondJmp<format, std::greater_equal>();
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleAdd2()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(Types().Integral32Type(), Types().Integral32Type(), I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleAdd2Dyn()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        status_ = VerificationStatus::ERROR;
        return false;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleAdd2Wide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(Types().Integral64Type(), Types().Integral64Type(), I64);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleFadd2()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(F32, F32, F32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleFadd2Wide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(F64, F64, F64);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleSub2()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(Types().Integral32Type(), Types().Integral32Type(), I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleSub2Dyn()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        status_ = VerificationStatus::ERROR;
        return false;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleSub2Wide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(Types().Integral64Type(), Types().Integral64Type(), I64);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleFsub2()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(F32, F32, F32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleFsub2Wide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(F64, F64, F64);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleMul2()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(Types().Integral32Type(), Types().Integral32Type(), I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleMul2Dyn()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        status_ = VerificationStatus::ERROR;
        return false;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleMul2Wide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(Types().Integral64Type(), Types().Integral64Type(), I64);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleFmul2()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(F32, F32, F32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleFmul2Wide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(F64, F64, F64);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleFdiv2()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        // take into consideration possible exception generation
        // context is of good precision here
        return CheckBinaryOp2<format>(F32, F32, F32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleFdiv2Wide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        //                context is of good precision here
        return CheckBinaryOp2<format>(F64, F64, F64);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleFmod2()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(F32, F32, F32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleFmod2Wide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(F64, F64, F64);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleAnd2()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(Types().Bits32Type(), Types().Bits32Type());
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleAnd2Wide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(Types().Bits64Type(), Types().Bits64Type());
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleOr2()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(Types().Bits32Type(), Types().Bits32Type());
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleOr2Wide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(Types().Bits64Type(), Types().Bits64Type());
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleXor2()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(Types().Bits32Type(), Types().Bits32Type());
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleXor2Wide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(Types().Bits64Type(), Types().Bits64Type());
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleShl2()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(Types().Bits32Type(), Types().Bits32Type());
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleShl2Wide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(Types().Bits64Type(), Types().Bits64Type());
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleShr2()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(Types().Bits32Type(), Types().Bits32Type());
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleShr2Wide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(Types().Bits64Type(), Types().Bits64Type());
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleAshr2()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(Types().Bits32Type(), Types().Bits32Type());
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleAshr2Wide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(Types().Bits64Type(), Types().Bits64Type());
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleDiv2()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        //                context is of good precision here
        return CheckBinaryOp2<format>(Types().Integral32Type(), Types().Integral32Type(), I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleDiv2Dyn()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        status_ = VerificationStatus::ERROR;
        return false;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleDiv2Wide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(Types().Integral64Type(), Types().Integral64Type(), I64);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleMod2()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(Types().Integral32Type(), Types().Integral32Type(), I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleMod2Dyn()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        status_ = VerificationStatus::ERROR;
        return false;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleMod2Wide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(Types().Integral64Type(), Types().Integral64Type(), I64);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleDivu2()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(Types().Integral32Type(), Types().Integral32Type(), U32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleDivu2Wide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(Types().Integral64Type(), Types().Integral64Type(), U64);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleModu2()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(Types().Integral32Type(), Types().Integral32Type(), U32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleModu2Wide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2<format>(Types().Integral64Type(), Types().Integral64Type(), U64);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleAdd()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp<format>(Types().Integral32Type(), Types().Integral32Type(), I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleSub()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp<format>(Types().Integral32Type(), Types().Integral32Type(), I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleMul()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp<format>(Types().Integral32Type(), Types().Integral32Type(), I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleAnd()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp<format>(Types().Bits32Type(), Types().Bits32Type(), I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleOr()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp<format>(Types().Bits32Type(), Types().Bits32Type(), I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleXor()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp<format>(Types().Bits32Type(), Types().Bits32Type(), I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleShl()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp<format>(Types().Bits32Type(), Types().Bits32Type(), I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleShr()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp<format>(Types().Bits32Type(), Types().Bits32Type(), I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleAshr()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp<format>(Types().Bits32Type(), Types().Bits32Type(), I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleDiv()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp<format>(Types().Integral32Type(), Types().Integral32Type(), I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleMod()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp<format>(Types().Integral32Type(), Types().Integral32Type(), I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleAddi()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2Imm<format>(Types().Integral32Type(), I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleSubi()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2Imm<format>(Types().Integral32Type(), I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleMuli()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2Imm<format>(Types().Integral32Type(), I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleAndi()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2Imm<format>(Types().Bits32Type());
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleOri()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2Imm<format>(Types().Bits32Type());
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleXori()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2Imm<format>(Types().Bits32Type());
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleShli()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2Imm<format>(Types().Bits32Type());
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleShri()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2Imm<format>(Types().Bits32Type());
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleAshri()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2Imm<format>(Types().Bits32Type());
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleDivi()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2Imm<format>(Types().Integral32Type(), I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleModi()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckBinaryOp2Imm<format>(Types().Integral32Type(), I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleNeg()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckUnaryOp<format>(Types().Integral32Type(), I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleNegWide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckUnaryOp<format>(Types().Integral64Type(), I64);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleFneg()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckUnaryOp<format>(F32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleFnegWide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckUnaryOp<format>(F64);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleNot()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckUnaryOp<format>(Types().Integral32Type());
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleNotWide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return CheckUnaryOp<format>(Types().Integral64Type());
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleInci()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vx = inst_.GetVReg<format>();
        Sync();
        if (!CheckRegTypes(vx, {Types().Integral32Type()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleI32toi64()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(Types().Integral32Type(), I64);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleI32toi16()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(Types().Integral32Type(), I16);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleI32tou16()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(Types().Integral32Type(), U16);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleI32toi8()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(Types().Integral32Type(), I8);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleI32tou8()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(Types().Integral32Type(), U8);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleI32tou1()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(Types().Integral32Type(), U1);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleI32tof32()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(Types().Integral32Type(), F32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleI32tof64()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(Types().Integral32Type(), F64);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleU32toi64()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(Types().Integral32Type(), I64);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleU32toi16()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(Types().Integral32Type(), I16);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleU32tou16()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(Types().Integral32Type(), U16);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleU32toi8()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(Types().Integral32Type(), I8);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleU32tou8()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(Types().Integral32Type(), U8);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleU32tou1()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(Types().Integral32Type(), U1);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleU32tof32()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(Types().Integral32Type(), F32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleU32tof64()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(Types().Integral32Type(), F64);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleI64toi32()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(Types().Integral64Type(), I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleI64tou1()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(Types().Integral64Type(), U1);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleI64tof32()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(Types().Integral64Type(), F32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleI64tof64()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(Types().Integral64Type(), F64);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleU64toi32()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(Types().Integral64Type(), I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleU64tou32()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(Types().Integral64Type(), U32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleU64tou1()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(Types().Integral64Type(), U1);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleU64tof32()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(Types().Integral64Type(), F32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleU64tof64()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(Types().Integral64Type(), F64);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleF32tof64()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(F32, F64);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleF32toi32()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(F32, I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleF32toi64()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(F32, I64);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleF32tou32()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(F32, U32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleF32tou64()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(F32, U64);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleF64tof32()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(F64, F32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleF64toi64()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(F64, I64);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleF64toi32()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(F64, I32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleF64tou64()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(F64, U64);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleF64tou32()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return HandleConversion<format>(F64, U32);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLdarr8()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs = inst_.GetVReg<format>();
        Sync();
        return CheckArrayLoad<format>(vs, {U1, I8});
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLdarr16()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs = inst_.GetVReg<format>();
        Sync();
        return CheckArrayLoad<format>(vs, {I16});
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLdarr()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs = inst_.GetVReg<format>();
        Sync();
        return CheckArrayLoad<format>(vs, {I32, U32});
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLdarrWide()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs = inst_.GetVReg<format>();
        Sync();
        return CheckArrayLoad<format>(vs, {I64, U64});
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLdarru8()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs = inst_.GetVReg<format>();
        Sync();
        return CheckArrayLoad<format>(vs, {U1, U8});
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLdarru16()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs = inst_.GetVReg<format>();
        Sync();
        return CheckArrayLoad<format>(vs, {U16});
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleFldarr32()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs = inst_.GetVReg<format>();
        Sync();
        return CheckArrayLoad<format>(vs, {F32});
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleFldarrWide()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs = inst_.GetVReg<format>();
        Sync();
        return CheckArrayLoad<format>(vs, {F64});
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLdarrObj()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs = inst_.GetVReg<format>();
        Sync();
        if (!CheckRegTypes(ACC, {Types().Integral32Type()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        if (!CheckRegTypes(vs, {Types().ArrayType()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        auto &&reg_type = GetRegType(vs);
        if (reg_type.ForAllTypes([this](auto reg_type1) { return reg_type1 == Types().NullRefType(); })) {
            // treat it as always throw NPE
            SHOW_MSG(AlwaysNpe)
            LOG_VERIFIER_ALWAYS_NPE(vs);
            END_SHOW_MSG();
            SetAcc(Types().Top());
            SET_STATUS_FOR_MSG(AlwaysNpe);
            return false;
        }
        auto ref_type = Types().RefType();
        auto &&arr_elt_type = GetArrayEltType(reg_type);
        TypeSet subtypes_of_ref_type_in_arr_elt_type(Types().GetKind());
        arr_elt_type.ForAllTypes([&](Type arr_elt_type1) {
            if (arr_elt_type1 <= ref_type) {
                subtypes_of_ref_type_in_arr_elt_type.Insert(arr_elt_type1);
            }
            return true;
        });
        switch (subtypes_of_ref_type_in_arr_elt_type.Size()) {
            case 0:
                SHOW_MSG(BadArrayElementType)
                LOG_VERIFIER_BAD_ARRAY_ELEMENT_TYPE(ImageOf(arr_elt_type), ImageOf(Types().RefType()),
                                                    ImagesOf(SubtypesOf({Types().RefType()})));
                END_SHOW_MSG();
                SET_STATUS_FOR_MSG(BadArrayElementType);
                status_ = VerificationStatus::ERROR;
                return false;
            case 1:
                SetAcc(subtypes_of_ref_type_in_arr_elt_type.TheOnlyType());
                break;
            default:
                SetAcc(std::move(subtypes_of_ref_type_in_arr_elt_type));
        }
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleStarr8()
    {
        LOG_INST();
        DBGBRK();
        uint16_t v1 = inst_.GetVReg<format, 0x00>();
        uint16_t v2 = inst_.GetVReg<format, 0x01>();
        Sync();
        return CheckArrayStoreExact<format>(v1, v2, Types().Integral32Type(), {U1, I8, U8});
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleStarr16()
    {
        LOG_INST();
        DBGBRK();
        uint16_t v1 = inst_.GetVReg<format, 0x00>();
        uint16_t v2 = inst_.GetVReg<format, 0x01>();
        Sync();
        return CheckArrayStoreExact<format>(v1, v2, Types().Integral32Type(), {I16, U16});
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleStarr()
    {
        LOG_INST();
        DBGBRK();
        uint16_t v1 = inst_.GetVReg<format, 0x00>();
        uint16_t v2 = inst_.GetVReg<format, 0x01>();
        Sync();
        return CheckArrayStoreExact<format>(v1, v2, Types().Integral32Type(), {I32, U32});
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleStarrWide()
    {
        LOG_INST();
        DBGBRK();
        uint16_t v1 = inst_.GetVReg<format, 0x00>();
        uint16_t v2 = inst_.GetVReg<format, 0x01>();
        Sync();
        return CheckArrayStoreExact<format>(v1, v2, Types().Integral64Type(), {I64, U64});
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleFstarr32()
    {
        LOG_INST();
        DBGBRK();
        uint16_t v1 = inst_.GetVReg<format, 0x00>();
        uint16_t v2 = inst_.GetVReg<format, 0x01>();
        Sync();
        return CheckArrayStoreExact<format>(v1, v2, Types().Float32Type(), {F32});
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleFstarrWide()
    {
        LOG_INST();
        DBGBRK();
        uint16_t v1 = inst_.GetVReg<format, 0x00>();
        uint16_t v2 = inst_.GetVReg<format, 0x01>();
        Sync();
        return CheckArrayStoreExact<format>(v1, v2, Types().Float64Type(), {F64});
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleStarrObj()
    {
        LOG_INST();
        DBGBRK();
        uint16_t v1 = inst_.GetVReg<format, 0x00>();
        uint16_t v2 = inst_.GetVReg<format, 0x01>();
        Sync();
        return CheckArrayStore<format>(v1, v2, Types().RefType());
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLenarr()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs = inst_.GetVReg<format>();
        Sync();
        if (!CheckRegTypes(vs, {Types().ArrayType()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        SetAcc(I32);
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleNewarr()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vd = inst_.GetVReg<format, 0>();
        uint16_t vs = inst_.GetVReg<format, 1>();
        Sync();
        if (!CheckRegTypes(vs, {Types().Integral32Type()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        const CachedClass *cached_class = GetCachedClass();
        if (cached_class == nullptr) {
            SET_STATUS_FOR_MSG(CannotResolveClassId);
            return false;
        }
        auto type = Types().TypeOf(*cached_class);
        if (!type.IsValid()) {
            status_ = VerificationStatus::ERROR;
            return false;
        }
        SHOW_MSG(DebugType)
        LOG_VERIFIER_DEBUG_TYPE(ImageOf(type));
        END_SHOW_MSG();
        if (!(type <= Types().ArrayType())) {
            SHOW_MSG(ArrayOfNonArrayType)
            LOG_VERIFIER_ARRAY_OF_NON_ARRAY_TYPE(ImageOf(type), ImagesOf(SubtypesOf({Types().ArrayType()})));
            END_SHOW_MSG();
            SET_STATUS_FOR_MSG(ArrayOfNonArrayType);
            return false;
        }
        SetReg(vd, type);
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleNewobj()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vd = inst_.GetVReg<format, 0>();
        Sync();
        const CachedClass *cached_class = GetCachedClass();
        if (cached_class == nullptr) {
            status_ = VerificationStatus::ERROR;
            return false;
        }
        auto type = Types().TypeOf(*cached_class);
        if (!type.IsValid()) {
            status_ = VerificationStatus::ERROR;
            return false;
        }
        SHOW_MSG(DebugType)
        LOG_VERIFIER_DEBUG_TYPE(ImageOf(type));
        END_SHOW_MSG();
        if (!(type <= Types().ObjectType())) {
            SHOW_MSG(ObjectOfNonObjectType)
            LOG_VERIFIER_OBJECT_OF_NON_OBJECT_TYPE(ImageOf(type), ImagesOf(SubtypesOf({type})));
            END_SHOW_MSG();
            SET_STATUS_FOR_MSG(ObjectOfNonObjectType);
            return false;
        }
        SetReg(vd, type);
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleNewobjDyn()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        MoveToNextInst<format>();
        status_ = VerificationStatus::ERROR;
        return false;
    }

    template <BytecodeInstructionSafe::Format format, typename RegsFetcher>
    bool CheckCallCtor(const CachedMethod &ctor, RegsFetcher regs)
    {
        Type obj_type = Types().TypeOf(ctor.GetClass());

        {
            if (SKIP_VERIFICATION_OF_CALL(ctor.id)) {
                SetAcc(obj_type);
                MoveToNextInst<format>();
                return true;
            }
        }

        auto ctor_name_getter = [&ctor]() { return ctor.GetName(); };

        bool check =
            CheckMethodArgs(ctor_name_getter, ctor,
                            [self_arg = true, obj_type, regs]() mutable -> std::optional<std::tuple<int, Type>> {
                                if (self_arg) {
                                    self_arg = false;
                                    return std::make_tuple(INVALID_REG, obj_type);
                                }
                                auto reg = regs();
                                if (reg) {
                                    return std::make_tuple(*reg, Type {});
                                }
                                return {};
                            });
        if (check) {
            SetAcc(obj_type);
            MoveToNextInst<format>();
        }
        return check;
    }

    template <BytecodeInstructionSafe::Format format, typename Fetcher>
    bool CheckCtor(Fetcher regs)
    {
        const CachedClass *klass = GetCachedClass();
        const CachedMethod *ctor = GetCachedMethod();

        if (klass == nullptr || ctor == nullptr) {
            SET_STATUS_FOR_MSG(CannotResolveMethodId);
            SET_STATUS_FOR_MSG(CannotResolveClassId);
            return false;
        }

        if (klass->flags[CachedClass::Flag::ARRAY_CLASS] && ctor->flags[CachedMethod::Flag::ARRAY_CONSTRUCTOR]) {
            SHOW_MSG(DebugArrayConstructor)
            LOG_VERIFIER_DEBUG_ARRAY_CONSTRUCTOR();
            END_SHOW_MSG();
            return CheckArrayCtor<format>(*ctor, regs);
        }

        SHOW_MSG(DebugConstructor)
        LOG_VERIFIER_DEBUG_CONSTRUCTOR(ctor->GetName());
        END_SHOW_MSG();

        return CheckCallCtor<format>(*ctor, regs);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleInitobj()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs1 = inst_.GetVReg<format, 0x00>();
        uint16_t vs2 = inst_.GetVReg<format, 0x01>();
        uint16_t vs3 = inst_.GetVReg<format, 0x02>();
        uint16_t vs4 = inst_.GetVReg<format, 0x03>();
        Sync();
        const std::array<int, 4> regs {vs1, vs2, vs3, vs4};
        auto fetcher = LazyFetch(regs);
        return CheckCtor<format>(fetcher);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleInitobjShort()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs1 = inst_.GetVReg<format, 0x00>();
        uint16_t vs2 = inst_.GetVReg<format, 0x01>();
        Sync();
        const std::array<int, 4> regs {vs1, vs2};
        auto fetcher = LazyFetch(regs);
        return CheckCtor<format>(fetcher);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleInitobjRange()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs = inst_.GetVReg<format, 0x00>();
        Sync();
        auto fetcher = [this, reg_idx = vs]() mutable -> std::optional<int> {
            if (!ExecCtx().CurrentRegContext().IsRegDefined(reg_idx)) {
                return std::nullopt;
            }
            return reg_idx++;
        };
        return CheckCtor<format>(fetcher);
    }

    Type GetFieldType()
    {
        const CachedField *field = GetCachedField();

        if (field == nullptr) {
            SET_STATUS_FOR_MSG(CannotResolveFieldId);
            return {};
        }

        return Types().TypeOf(field->GetType());
    }

    Type GetFieldObject()
    {
        const CachedField *field = GetCachedField();

        if (field == nullptr) {
            SET_STATUS_FOR_MSG(CannotResolveFieldId);
            return {};
        }
        return Types().TypeOf(field->GetClass());
    }

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_SIZE)
    bool CheckFieldAccess(int reg_idx, Type expected_field_type, bool is_static)
    {
        const CachedField *field = GetCachedField();

        if (field == nullptr) {
            SET_STATUS_FOR_MSG(CannotResolveFieldId);
            return false;
        }

        if (is_static != field->flags[CachedField::Flag::STATIC]) {
            SHOW_MSG(ExpectedStaticOrInstanceField)
            LOG_VERIFIER_EXPECTED_STATIC_OR_INSTANCE_FIELD(is_static);
            END_SHOW_MSG();
            SET_STATUS_FOR_MSG(ExpectedStaticOrInstanceField);
            return false;
        }

        Type field_obj_type = GetFieldObject();
        Type field_type = GetFieldType();
        if (!field_type.IsValid()) {
            LOG_VERIFIER_CANNOT_RESOLVE_FIELD_TYPE(field->GetName());
            return false;
        }

        if (!is_static) {
            if (!IsRegDefined(reg_idx)) {
                SET_STATUS_FOR_MSG(UndefinedRegister);
                return false;
            }
            const AbstractType &obj_type = GetRegType(reg_idx);
            if (obj_type.ForAllTypes([&](Type obj_type1) { return obj_type1 == Types().NullRefType(); })) {
                // treat it as always throw NPE
                SHOW_MSG(AlwaysNpe)
                LOG_VERIFIER_ALWAYS_NPE(reg_idx);
                END_SHOW_MSG();
                SET_STATUS_FOR_MSG(AlwaysNpe);
                return false;
            }
            if (!obj_type.ExistsType([&](Type obj_type1) { return obj_type1 <= field_obj_type; })) {
                SHOW_MSG(InconsistentRegisterAndFieldTypes)
                LOG_VERIFIER_INCONSISTENT_REGISTER_AND_FIELD_TYPES(field->GetName(), reg_idx, ImageOf(obj_type),
                                                                   ImageOf(field_obj_type),
                                                                   ImagesOf(SubtypesOf({field_obj_type})));
                END_SHOW_MSG();
                SET_STATUS_FOR_MSG(InconsistentRegisterAndFieldTypes);
            }
        }

        if (!(field_type <= expected_field_type)) {
            SHOW_MSG(UnexpectedFieldType)
            LOG_VERIFIER_UNEXPECTED_FIELD_TYPE(field->GetName(), ImageOf(field_type), ImageOf(expected_field_type),
                                               ImagesOf(SubtypesOf({expected_field_type})));
            END_SHOW_MSG();
            SET_STATUS_FOR_MSG(UnexpectedFieldType);
            return false;
        }

        Type method_class_type = context_.GetMethodClass();
        TypeRelationship relation = GetRelationship(method_class_type, field_obj_type);
        AccessModifier access_mode = GetAccessMode(field);

        auto result = panda::verifier::CheckFieldAccess(relation, access_mode);

        if (!result.IsOk()) {
            const auto &verif_opts = Runtime::GetCurrent()->GetVerificationOptions();
            if (verif_opts.Debug.Allow.FieldAccessViolation && result.IsError()) {
                result.status = VerificationStatus::WARNING;
            }
            LogInnerMessage(result);
            LOG_VERIFIER_DEBUG_FIELD2(field->GetName());
            status_ = result.status;
            return status_ != VerificationStatus::ERROR;
        }

        return !result.IsError();
    }

    template <BytecodeInstructionSafe::Format format>
    bool ProcessFieldLoad(int reg_dest, int reg_src, Type expected_field_type, bool is_static)
    {
        if (!CheckFieldAccess(reg_src, expected_field_type, is_static)) {
            return false;
        }
        const CachedField *field = GetCachedField();

        if (field == nullptr) {
            SET_STATUS_FOR_MSG(CannotResolveFieldId);
            return false;
        }

        auto type = GetFieldType();
        if (!type.IsValid()) {
            return false;
        }
        SetReg(reg_dest, type);
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool ProcessFieldLoad(int reg_idx, Type expected_field_type, bool is_static)
    {
        return ProcessFieldLoad<format>(ACC, reg_idx, expected_field_type, is_static);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLdobj()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs = inst_.GetVReg<format>();
        Sync();
        return ProcessFieldLoad<format>(vs, Types().Integral32Type(), false);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLdobjWide()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs = inst_.GetVReg<format>();
        Sync();
        return ProcessFieldLoad<format>(vs, Types().Bits64Type(), false);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLdobjObj()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs = inst_.GetVReg<format>();
        Sync();
        return ProcessFieldLoad<format>(vs, Types().RefType(), false);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLdobjV()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vd = inst_.GetVReg<format, 0>();
        uint16_t vs = inst_.GetVReg<format, 1>();
        Sync();
        return ProcessFieldLoad<format>(vd, vs, Types().Integral32Type(), false);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLdobjVWide()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vd = inst_.GetVReg<format, 0>();
        uint16_t vs = inst_.GetVReg<format, 1>();
        Sync();
        return ProcessFieldLoad<format>(vd, vs, Types().Bits64Type(), false);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLdobjVObj()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vd = inst_.GetVReg<format, 0>();
        uint16_t vs = inst_.GetVReg<format, 1>();
        Sync();
        return ProcessFieldLoad<format>(vd, vs, Types().RefType(), false);
    }

    template <BytecodeInstructionSafe::Format format, typename Check>
    bool ProcessStoreField(int vd, int vs, Type expected_field_type, bool is_static, Check check)
    {
        if (!CheckFieldAccess(vs, expected_field_type, is_static)) {
            return false;
        }

        if (!CheckRegTypes(vd, {Types().Bits32Type(), Types().Bits64Type()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }

        const CachedField *field = GetCachedField();

        if (field == nullptr) {
            SET_STATUS_FOR_MSG(CannotResolveFieldId);
            return false;
        }
        Type field_type = GetFieldType();
        if (!field_type.IsValid()) {
            return false;
        }

        const AbstractType &vd_type = GetRegType(vd);

        TypeId field_type_id = Types().TypeIdOf(field_type);

        PandaVector<CheckResult> results;

        vd_type.ForAllTypes(
            [&](auto vd_type1) { return AddCheckResult(results, check(field_type_id, Types().TypeIdOf(vd_type1))); });

        // results is empty if there was an OK, contains all warnings if there were any warnings, all errors if there
        // were only errors
        if (!results.empty()) {
            for (const auto &result : results) {
                LogInnerMessage(result);
            }
            LOG_VERIFIER_DEBUG_STORE_FIELD(field->GetName(), ImageOf(field_type), ImageOf(vd_type),
                                           ImagesOf(SubtypesOf({Types().Integral32Type(), Types().Integral64Type()})),
                                           ImagesOf(SubtypesOf({field_type})));
            status_ = results[0].status;
            if (results[0].IsError()) {
                return false;
            }
        }

        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool ProcessStobj(int vd, int vs, bool is_static)
    {
        return ProcessStoreField<format>(vd, vs, Types().Bits32Type(), is_static, CheckStobj);
    }

    template <BytecodeInstructionSafe::Format format>
    bool ProcessStobj(int vs, bool is_static)
    {
        return ProcessStobj<format>(ACC, vs, is_static);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleStobj()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs = inst_.GetVReg<format>();
        Sync();

        return ProcessStobj<format>(vs, false);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleStobjV()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vd = inst_.GetVReg<format, 0>();
        uint16_t vs = inst_.GetVReg<format, 1>();
        Sync();

        return ProcessStobj<format>(vd, vs, false);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleStobjDyn()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        MoveToNextInst<format>();
        status_ = VerificationStatus::ERROR;
        return false;
    }

    template <BytecodeInstructionSafe::Format format>
    bool ProcessStobjWide(int vd, int vs, bool is_static)
    {
        return ProcessStoreField<format>(vd, vs, Types().Bits64Type(), is_static, CheckStobjWide);
    }

    template <BytecodeInstructionSafe::Format format>
    bool ProcessStobjWide(int vs, bool is_static)
    {
        return ProcessStobjWide<format>(ACC, vs, is_static);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleStobjWide()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs = inst_.GetVReg<format>();
        Sync();

        return ProcessStobjWide<format>(vs, false);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleStobjVWide()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vd = inst_.GetVReg<format, 0>();
        uint16_t vs = inst_.GetVReg<format, 1>();
        Sync();

        return ProcessStobjWide<format>(vd, vs, false);
    }

    template <BytecodeInstructionSafe::Format format>
    bool ProcessStobjObj(int vd, int vs, bool is_static)
    {
        if (!CheckFieldAccess(vs, Types().RefType(), is_static)) {
            return false;
        }

        const CachedField *field = GetCachedField();

        if (field == nullptr) {
            SET_STATUS_FOR_MSG(CannotResolveFieldId);
            return false;
        }

        Type field_type = GetFieldType();
        if (!field_type.IsValid()) {
            return false;
        }

        if (!CheckRegTypes(vd, {Types().RefType()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }

        const AbstractType &vd_type = GetRegType(vd);

        if (vd_type.ForAllTypes([&](Type vd_type1) { return !(vd_type1 <= field_type); })) {
            LOG_VERIFIER_BAD_ACCUMULATOR_TYPE(ImageOf(vd_type), ImageOf(field_type),
                                              ImagesOf(SubtypesOf({field_type})));
            status_ = VerificationStatus::ERROR;
            return false;
        }

        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool ProcessStobjObj(int vs, bool is_static)
    {
        return ProcessStobjObj<format>(ACC, vs, is_static);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleStobjObj()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs = inst_.GetVReg<format>();
        Sync();
        return ProcessStobjObj<format>(vs, false);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleStobjVObj()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vd = inst_.GetVReg<format, 0>();
        uint16_t vs = inst_.GetVReg<format, 1>();
        Sync();
        return ProcessStobjObj<format>(vd, vs, false);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLdstatic()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return ProcessFieldLoad<format>(INVALID_REG, Types().Bits32Type(), true);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLdstaticWide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return ProcessFieldLoad<format>(INVALID_REG, Types().Bits64Type(), true);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleLdstaticObj()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return ProcessFieldLoad<format>(INVALID_REG, Types().RefType(), true);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleStstatic()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return ProcessStobj<format>(INVALID_REG, true);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleStstaticWide()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return ProcessStobjWide<format>(INVALID_REG, true);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleStstaticObj()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        return ProcessStobjObj<format>(INVALID_REG, true);
    }

    template <typename Check>
    bool CheckReturn(Type ret_type, Type acc_type, Check check)
    {
        TypeId ret_type_id = Types().TypeIdOf(ret_type);

        PandaVector<Type> compatible_acc_types;
        for (size_t acc_idx = 0; acc_idx < static_cast<size_t>(TypeId::REFERENCE) + 1; ++acc_idx) {
            auto acc_type_id = static_cast<TypeId>(acc_idx);
            const CheckResult &info = check(ret_type_id, acc_type_id);
            if (!info.IsError()) {
                compatible_acc_types.push_back(Types().TypeOf(acc_type_id));
            }
        }

        if (!CheckTypes(acc_type, {Types().PrimitiveType()}) || acc_type == Types().PrimitiveType()) {
            LOG_VERIFIER_BAD_ACCUMULATOR_RETURN_VALUE_TYPE(ImageOf(acc_type), ImagesOf(compatible_acc_types));
            status_ = VerificationStatus::ERROR;
            return false;
        }

        TypeId acc_type_id = Types().TypeIdOf(acc_type);

        const auto &result = check(ret_type_id, acc_type_id);

        if (!result.IsOk()) {
            LogInnerMessage(result);
            if (result.IsError()) {
                LOG_VERIFIER_DEBUG_FUNCTION_RETURN_AND_ACCUMULATOR_TYPES_WITH_COMPATIBLE_TYPES(
                    ImageOf(ReturnType()), ImageOf(acc_type), ImagesOf(compatible_acc_types));
            } else {
                LOG_VERIFIER_DEBUG_FUNCTION_RETURN_AND_ACCUMULATOR_TYPES(ImageOf(ReturnType()), ImageOf(acc_type));
            }
        }

        status_ = result.status;
        return status_ != VerificationStatus::ERROR;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleReturn()
    {
        LOG_INST();
        DBGBRK();
        Sync();

        if (!CheckTypes(ReturnType(), {Types().Bits32Type()})) {
            LOG_VERIFIER_BAD_RETURN_INSTRUCTION_TYPE("", ImageOf(ReturnType()), ImageOf(Types().Bits32Type()),
                                                     ImagesOf(Types().SubtypesOf(Types().Bits32Type())));
            status_ = VerificationStatus::ERROR;
            return false;
        }

        if (!IsRegDefined(ACC)) {
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }

        if (!CheckTypes(GetAccType(), {Types().Bits32Type()})) {
            LOG_VERIFIER_BAD_ACCUMULATOR_RETURN_VALUE_TYPE(ImageOf(GetAccType()),
                                                           ImagesOf(Types().SubtypesOf(Types().Bits32Type())));
            status_ = VerificationStatus::ERROR;
        }
        return false;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleReturnDyn()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        status_ = VerificationStatus::ERROR;
        return false;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleReturnWide()
    {
        LOG_INST();
        DBGBRK();
        Sync();

        if (!CheckTypes(ReturnType(), {Types().Bits64Type()})) {
            LOG_VERIFIER_BAD_RETURN_INSTRUCTION_TYPE(".64", ImageOf(ReturnType()), ImageOf(Types().Bits64Type()),
                                                     ImagesOf(Types().SubtypesOf(Types().Bits64Type())));
            status_ = VerificationStatus::ERROR;
            return false;
        }

        if (!IsRegDefined(ACC)) {
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }

        if (!CheckTypes(GetAccType(), {Types().Bits64Type()})) {
            LOG_VERIFIER_BAD_ACCUMULATOR_RETURN_VALUE_TYPE(ImageOf(GetAccType()),
                                                           ImagesOf(Types().SubtypesOf(Types().Bits64Type())));
            status_ = VerificationStatus::ERROR;
        }
        return false;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleReturnObj()
    {
        LOG_INST();
        DBGBRK();
        Sync();

        if (!CheckTypes(ReturnType(), {Types().RefType()})) {
            LOG_VERIFIER_BAD_RETURN_INSTRUCTION_TYPE(".obj", ImageOf(ReturnType()), ImageOf(Types().RefType()),
                                                     ImagesOf(Types().SubtypesOf(Types().Bits32Type())));
            status_ = VerificationStatus::ERROR;
            return false;
        }

        if (!IsRegDefined(ACC)) {
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }

        auto &&acc_type = GetAccType();

        if (!CheckTypes(acc_type, {ReturnType()})) {
            LOG_VERIFIER_BAD_ACCUMULATOR_RETURN_VALUE_TYPE_WITH_SUBTYPE(ImageOf(acc_type), ImageOf(ReturnType()),
                                                                        ImagesOf(Types().SubtypesOf(ReturnType())));
            status_ = VerificationStatus::WARNING;
        }

        return false;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleReturnVoid()
    {
        LOG_INST();
        DBGBRK();
        Sync();

        if (ReturnType() != Types().Top()) {
            LOG_VERIFIER_BAD_RETURN_VOID_INSTRUCTION_TYPE(ImageOf(ReturnType()));
            status_ = VerificationStatus::ERROR;
        }

        return false;
    }

    template <BytecodeInstructionSafe::Format format>
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_SIZE)
    bool HandleCheckcast()
    {
        LOG_INST();
        DBGBRK();
        ExecCtx().SetTypecastPoint(inst_.GetAddress());
        Sync();
        const CachedClass *cached_class = GetCachedClass();
        if (cached_class == nullptr) {
            return false;
        }
        auto type = Types().TypeOf(*cached_class);
        if (!type.IsValid()) {
            return false;
        }
        LOG_VERIFIER_DEBUG_TYPE(ImageOf(type));
        if (!(type <= Types().ObjectType()) && !(type <= Types().ArrayType())) {
            LOG_VERIFIER_CHECK_CAST_TO_NON_OBJECT_TYPE(ImageOf(type), ImagesOf(SubtypesOf({Types().ObjectType()})));
            status_ = VerificationStatus::ERROR;
            return false;
        }
        if (!IsRegDefined(ACC)) {
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        auto acc_type = GetAccType();
        if (acc_type.ForAllTypes([&](Type acc_type1) {
                return !(acc_type1 <= Types().RefType() || acc_type1 <= Types().ArrayType());
            })) {
            LOG_VERIFIER_NON_OBJECT_ACCUMULATOR_TYPE();
            status_ = VerificationStatus::ERROR;
            return false;
        }

        if (acc_type.ForAllTypes([&](Type acc_type1) { return acc_type1 <= Types().NullRefType(); })) {
            LOG_VERIFIER_ACCUMULATOR_ALWAYS_NULL();
            status_ = VerificationStatus::WARNING;
        } else if (acc_type.ForAllTypes([&](Type acc_type1) { return acc_type1 <= type; })) {
            LOG_VERIFIER_REDUNDANT_CHECK_CAST(ImageOf(acc_type), ImageOf(type));
            status_ = VerificationStatus::WARNING;
        } else if (type <= Types().ArrayType()) {
            auto &&elt_type = GetArrayEltType(type);
            auto res = acc_type.ForAllTypes(
                [&](auto acc_type1) { return !(acc_type1 <= Types().ArrayType() || type <= acc_type1); });
            if (res) {
                LOG_VERIFIER_IMPOSSIBLE_CHECK_CAST(ImageOf(acc_type), ImagesOf(SubSupTypesOf(type)));
                status_ = VerificationStatus::WARNING;
            } else {
                auto result = acc_type.ForAllTypes([&](Type acc_type1) {
                    if (IsConcreteArrayType(acc_type1)) {
                        auto &&acc_elt_type = GetArrayEltType(acc_type1);
                        return !(acc_elt_type <= elt_type || elt_type <= acc_elt_type);
                    } else {
                        return true;
                    }
                });
                if (result) {
                    LOG_VERIFIER_IMPOSSIBLE_ARRAY_CHECK_CAST(ImageOf(GetArrayEltType(acc_type)),
                                                             ImagesOf(SubSupTypesOf(elt_type)));
                    status_ = VerificationStatus::WARNING;
                }
            }
        } else if (acc_type.ForAllTypes([&](Type acc_type1) { return !(type <= acc_type1); })) {
            // NB: accumulator may be checked several times via checkcast and interface types,
            // so incompatibility here should be just a warning
            // type in acc and given type should be on same line in type hierarchy
            // ACC may be a supertype of given type, because of impresicion of absint,
            // real type in ACC during execution may be a subtype of ACC type during absint
            LOG_VERIFIER_POSSIBLY_INCOMPATIBLE_ACCUMULATOR_TYPE(ImageOf(acc_type), ImagesOf(SubSupTypesOf(type)));
            status_ = VerificationStatus::WARNING;
        }

        if (status_ == VerificationStatus::ERROR) {
            SetAcc(Types().Top());
            return false;
        }

        SetAccAndOthersOfSameOrigin(type);

        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_SIZE)
    bool HandleIsinstance()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        const CachedClass *cached_class = GetCachedClass();
        if (cached_class == nullptr) {
            return false;
        }
        auto type = Types().TypeOf(*cached_class);
        if (!type.IsValid()) {
            return false;
        }
        LOG_VERIFIER_DEBUG_TYPE(ImageOf(type));
        if (!(type <= Types().ObjectType()) && !(type <= Types().ArrayType())) {
            // !(type <= Types().ArrayType()) is redundant, because all arrays
            // are subtypes of either panda.Object <: ObjectType or java.lang.Object <: ObjectType
            // depending on selected language context
            LOG_VERIFIER_BAD_IS_INSTANCE_INSTRUCTION(ImageOf(type), ImagesOf(SubtypesOf({Types().ObjectType()})));
            status_ = VerificationStatus::ERROR;
            return false;
        }
        if (!IsRegDefined(ACC)) {
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        auto acc_type = GetAccType();
        if (acc_type.ForAllTypes([&](Type acc_type1) {
                return !(acc_type1 <= Types().RefType() || acc_type1 <= Types().ArrayType());
            })) {
            LOG_VERIFIER_NON_OBJECT_ACCUMULATOR_TYPE();
            status_ = VerificationStatus::ERROR;
            return false;
        }

        if (acc_type.ForAllTypes([&](Type acc_type1) { return acc_type1 <= Types().NullRefType(); })) {
            LOG_VERIFIER_ACCUMULATOR_ALWAYS_NULL();
            status_ = VerificationStatus::WARNING;
        } else if (acc_type.ForAllTypes([&](Type acc_type1) { return acc_type1 <= type; })) {
            LOG_VERIFIER_REDUNDANT_IS_INSTANCE(ImageOf(acc_type), ImageOf(type));
            status_ = VerificationStatus::WARNING;
        } else if (type <= Types().ArrayType()) {
            auto &&elt_type = GetArrayEltType(type);
            auto &&acc_elt_type = GetArrayEltType(acc_type);
            bool acc_elt_type_is_empty = true;
            auto res = acc_elt_type.ForAllTypes([&](Type acc_elt_type1) {
                acc_elt_type_is_empty = false;
                return !(acc_elt_type1 <= elt_type || elt_type <= acc_elt_type1);
            });
            if (res) {
                if (acc_elt_type_is_empty) {
                    LOG_VERIFIER_IMPOSSIBLE_IS_INSTANCE(ImageOf(acc_type), ImagesOf(SubSupTypesOf(type)));
                } else {
                    LOG_VERIFIER_IMPOSSIBLE_ARRAY_IS_INSTANCE(ImageOf(acc_elt_type), ImagesOf(SubSupTypesOf(elt_type)));
                }
                status_ = VerificationStatus::WARNING;
            }
        } else if (acc_type.ForAllTypes([&](Type acc_type1) { return !(type <= acc_type1); })) {
            // type in acc and given type should be on same line in type hierarchy
            // ACC may be a supertype of given type, because of impresicion of absint,
            // real type in ACC during execution may be a subtype of ACC type during absint
            LOG_VERIFIER_IMPOSSIBLE_IS_INSTANCE(ImageOf(acc_type), ImagesOf(SubSupTypesOf(type)));
            status_ = VerificationStatus::WARNING;
        }  // else {
        SetAcc(I32);
        MoveToNextInst<format>();
        return true;
    }

    template <typename Fetcher, typename NameGetter>
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_SIZE)
    bool CheckMethodArgs(NameGetter name_getter, const CachedMethod &method, Fetcher regs_and_types)
    {
        const auto &sig = Types().MethodSignature(method);
        const auto &norm_sig = Types().NormalizedMethodSignature(method);
        auto formal_args = ConstLazyFetch(sig);
        auto norm_formal_args = ConstLazyFetch(norm_sig);
        bool result = false;
        size_t args_num = sig.size() - 1;
        if (args_num == 0) {
            return true;
        }
        ForEachCond(JoinStreams(regs_and_types, formal_args, norm_formal_args), [this, &result, &name_getter,
                                                                                 &args_num](auto &&types) mutable {
            do {
                // damn clang-tidy
                auto &&reg_and_type = std::get<0x0>(types);
                auto &&formal_param = std::get<0x1>(types);
                auto &&norm_param = std::get<0x2>(types);
                int reg_num = std::get<0x0>(reg_and_type);
                if (reg_num != INVALID_REG && !IsRegDefined(reg_num)) {
                    LOG_VERIFIER_BAD_CALL_UNDEFINED_REGISTER(name_getter(), reg_num);
                    SET_STATUS_FOR_MSG(UndefinedRegister);
                    return result = false;
                }
                Type formal_type {Types().TypeOf(formal_param)};
                Type norm_type {Types().TypeOf(norm_param)};
                ASSERT(formal_param.Variance() == norm_param.Variance());
                const AbstractType &actual_type =
                    reg_num == INVALID_REG ? std::get<0x1>(reg_and_type) : GetRegType(reg_num);
                TypeSet norm_actual_type(Types().GetKind());
                actual_type.ForAllTypes([&](Type actual_type1) {
                    norm_actual_type.Insert(Types().NormalizedTypeOf(actual_type1));
                    return true;
                });
                // arg: NormalizedTypeOf(actual_type) <= norm_type
                // check of physical compatibility
                bool incompatible_types = false;
                if (reg_num != INVALID_REG && formal_type <= Types().RefType() && !formal_type.IsBot() &&
                    actual_type.ExistsType([&](auto actual_type1) {
                        return actual_type1 <= Types().RefType() && !actual_type1.IsBot();
                    })) {
                    if (CheckRegTypesTakingIntoAccountTypecasts(reg_num, formal_type)) {
                        break;
                    } else {
                        if (!Runtime::GetCurrent()->GetVerificationOptions().Debug.Allow.WrongSubclassingInMethodArgs) {
                            incompatible_types = true;
                        }
                    }
                } else if (!formal_type.IsBot() && !formal_type.IsTop() &&
                           !norm_actual_type.Exists(
                               [&](auto norm_actual_type1) { return norm_actual_type1 <= norm_type; })) {
                    incompatible_types = true;
                }
                if (incompatible_types) {
                    PandaString reg_or_param =
                        reg_num == INVALID_REG ? "Actual parameter" : RegisterName(reg_num, true);
                    SHOW_MSG(BadCallIncompatibleParameter)
                    LOG_VERIFIER_BAD_CALL_INCOMPATIBLE_PARAMETER(name_getter(), reg_or_param, ImageOf(norm_actual_type),
                                                                 ImageOf(norm_type));
                    END_SHOW_MSG();
                    SET_STATUS_FOR_MSG(BadCallIncompatibleParameter);
                    return result = false;
                }
                if (formal_type.IsBot()) {
                    if (actual_type.ExistsType([](auto actual_type1) { return actual_type1.IsBot(); })) {
                        LOG_VERIFIER_CALL_FORMAL_ACTUAL_BOTH_BOT_OR_TOP("Bot");
                        break;
                    } else {
                        SHOW_MSG(BadCallFormalIsBot)
                        LOG_VERIFIER_BAD_CALL_FORMAL_IS_BOT(name_getter(), ImageOf(actual_type));
                        END_SHOW_MSG();
                        SET_STATUS_FOR_MSG(BadCallFormalIsBot);
                        return result = false;
                    }
                } else if (formal_type.IsTop()) {
                    if (actual_type.ExistsType([](auto actual_type1) { return actual_type1.IsTop(); })) {
                        LOG_VERIFIER_CALL_FORMAL_ACTUAL_BOTH_BOT_OR_TOP("Top");
                        break;
                    } else {
                        SHOW_MSG(CallFormalTop)
                        LOG_VERIFIER_CALL_FORMAL_TOP();
                        END_SHOW_MSG();
                        break;
                    }
                } else if (formal_type <= Types().PrimitiveType()) {
                    // check implicit conversion of primitive types
                    TypeId formal_id = Types().TypeIdOf(formal_type);
                    const Type &integral32_type = Types().Integral32Type();
                    const Type &integral64_type = Types().Integral64Type();
                    const Type &float64_type = Types().Float64Type();
                    const Type &primitive_type = Types().PrimitiveType();
                    bool actual_type_has_primitives = false;
                    bool need_to_break = false;
                    PandaVector<CheckResult> results;
                    actual_type.ForAllTypes([&](Type actual_type1) {
                        if (!(actual_type1 <= primitive_type)) {
                            return true;
                        }
                        actual_type_has_primitives = true;
                        TypeId actual_id = Types().TypeIdOf(actual_type1);
                        if (actual_id != TypeId::INVALID) {
                            return AddCheckResult(results, panda::verifier::CheckMethodArgs(formal_id, actual_id));
                        }

                        // special case, where type after contexts LUB operation is inexact one, like
                        // Integral32Type()
                        if ((formal_type <= integral32_type && actual_type1 <= integral32_type) ||
                            (formal_type <= integral64_type && actual_type1 <= integral64_type) ||
                            (formal_type <= float64_type && actual_type1 <= float64_type)) {
                            SHOW_MSG(CallFormalActualDifferent)
                            LOG_VERIFIER_CALL_FORMAL_ACTUAL_DIFFERENT(ImageOf(formal_type), ImageOf(actual_type1),
                                                                      ImagesOf(SubtypesOf({actual_type1})));
                            END_SHOW_MSG();
                            need_to_break = true;
                            return false;
                        }

                        return AddCheckResult(results, panda::verifier::CheckMethodArgs(formal_id, actual_id));
                    });

                    if (!actual_type_has_primitives) {
                        return result = false;
                    }
                    if (need_to_break || results.empty()) {
                        break;
                    }
                    for (const auto &res : results) {
                        SHOW_MSG(DebugCallParameterTypes)
                        LogInnerMessage(res);
                        LOG_VERIFIER_DEBUG_CALL_PARAMETER_TYPES(
                            name_getter(),
                            (reg_num == INVALID_REG
                                 ? ""
                                 : PandaString {"Actual parameter in "} + RegisterName(reg_num) + ". "),
                            ImageOf(actual_type), ImageOf(formal_type));
                        END_SHOW_MSG();
                        status_ = res.status;
                    }
                    status_ = results[0].status;
                    if (status_ == VerificationStatus::WARNING) {
                        break;
                    } else {
                        return result = false;
                    }
                } else if (formal_type <= Types().MethodType()) {
                    auto res =
                        norm_actual_type.Exists([&](auto norm_actual_type1) { return norm_actual_type1 <= norm_type; });
                    if (!res) {
                        SHOW_MSG(BadCallIncompatibleLambdaType)
                        LOG_VERIFIER_BAD_CALL_INCOMPATIBLE_LAMBDA_TYPE(
                            name_getter(),
                            (reg_num == INVALID_REG ? "" : PandaString {"in "} + RegisterName(reg_num) + " "),
                            ImageOf(actual_type), ImageOf(norm_actual_type), ImageOf(formal_type), ImageOf(norm_type));
                        END_SHOW_MSG();
                        SET_STATUS_FOR_MSG(BadCallIncompatibleLambdaType);
                        return result = false;
                    }
                    break;
                }
                if (!CheckTypes(actual_type, {formal_type})) {
                    if (reg_num == INVALID_REG) {
                        SHOW_MSG(BadCallWrongParameter)
                        LOG_VERIFIER_BAD_CALL_WRONG_PARAMETER(name_getter(), ImageOf(actual_type),
                                                              ImageOf(formal_type));
                        END_SHOW_MSG();
                        SET_STATUS_FOR_MSG(BadCallWrongParameter);
                    } else {
                        SHOW_MSG(BadCallWrongRegister)
                        LOG_VERIFIER_BAD_CALL_WRONG_REGISTER(name_getter(), reg_num);
                        END_SHOW_MSG();
                        SET_STATUS_FOR_MSG(BadCallWrongRegister);
                    }
                    if (!Runtime::GetCurrent()->GetVerificationOptions().Debug.Allow.WrongSubclassingInMethodArgs) {
                        status_ = VerificationStatus::ERROR;
                        return result = false;
                    }
                }
            } while (false);
            if (--args_num == 0) {
                result = true;
                return false;
            }
            return true;
        });
        if (!result && status_ == VerificationStatus::OK) {
            // premature end of arguments
            SHOW_MSG(BadCallTooFewParameters)
            LOG_VERIFIER_BAD_CALL_TOO_FEW_PARAMETERS(name_getter());
            END_SHOW_MSG();
            SET_STATUS_FOR_MSG(BadCallTooFewParameters);
        }
        return result;
    }

    template <BytecodeInstructionSafe::Format format, typename Fetcher>
    bool CheckCall(const CachedMethod *method, Fetcher regs)
    {
        if (method == nullptr) {
            SET_STATUS_FOR_MSG(CannotResolveMethodId);
            return false;
        }
        auto callee_method_class_type = Types().TypeOf(method->GetClass());
        auto caller_method_class_type = context_.GetMethodClass();
        auto relation = GetRelationship(caller_method_class_type, callee_method_class_type);
        auto access_mode = GetAccessMode(method);

        auto result = panda::verifier::CheckCall(relation, access_mode);

        if (!result.IsOk()) {
            const auto &verif_opts = Runtime::GetCurrent()->GetVerificationOptions();
            if (verif_opts.Debug.Allow.MethodAccessViolation && result.IsError()) {
                result.status = VerificationStatus::WARNING;
            }
            LogInnerMessage(result);
            LOG_VERIFIER_DEBUG_CALL_FROM_TO(CurrentJob.JobCachedMethod().GetName(), method->GetName());
            status_ = result.status;
            if (status_ == VerificationStatus::ERROR) {
                return false;
            }
        }

        const auto &method_sig = Types().MethodSignature(*method);
        auto method_name_getter = [&method]() { return method->GetName(); };
        Type result_type {Types().TypeOf(method_sig.back())};

        if (!SKIP_VERIFICATION_OF_CALL(method->id) &&
            !CheckMethodArgs(method_name_getter, *method,
                             Transform(regs, [](int reg) { return std::make_tuple(reg, Type {}); }))) {
            return false;
        }
        SetAcc(result_type);
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleCallShort()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs1 = inst_.GetVReg<format, 0x00>();
        uint16_t vs2 = inst_.GetVReg<format, 0x01>();
        const CachedMethod *method = GetCachedMethod();
        if (method != nullptr) {
            LOG_VERIFIER_DEBUG_METHOD(method->GetName());
        }
        Sync();
        const std::array<int, 2> regs {vs1, vs2};
        auto fetcher = LazyFetch(regs);
        return CheckCall<format>(method, fetcher);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleCallAccShort()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs1 = inst_.GetVReg<format, 0x00>();
        unsigned acc_pos = inst_.GetImm<format, 0x00>();
        static constexpr auto NUM_ARGS = 2;
        const CachedMethod *method = GetCachedMethod();
        if (method != nullptr) {
            LOG_VERIFIER_DEBUG_METHOD(method->GetName());
        }
        Sync();
        std::array<int, NUM_ARGS> regs;
        if (acc_pos == 0) {
            regs = {ACC, vs1};
        } else {
            regs = {vs1, ACC};
        }
        auto fetcher = LazyFetch(regs);
        return CheckCall<format>(method, fetcher);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleCallDynShort()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        status_ = VerificationStatus::ERROR;
        return false;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleCalliDynShort()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        status_ = VerificationStatus::ERROR;
        return false;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleCallDyn()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        status_ = VerificationStatus::ERROR;
        return false;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleCalliDyn()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        status_ = VerificationStatus::ERROR;
        return false;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleCallDynRange()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        status_ = VerificationStatus::ERROR;
        return false;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleCalliDynRange()
    {
        LOG_INST();
        DBGBRK();
        Sync();
        status_ = VerificationStatus::ERROR;
        return false;
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleCall()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs1 = inst_.GetVReg<format, 0x00>();
        uint16_t vs2 = inst_.GetVReg<format, 0x01>();
        uint16_t vs3 = inst_.GetVReg<format, 0x02>();
        uint16_t vs4 = inst_.GetVReg<format, 0x03>();
        const CachedMethod *method = GetCachedMethod();
        if (method != nullptr) {
            LOG_VERIFIER_DEBUG_METHOD(method->GetName());
        }
        Sync();
        const std::array<int, 4> regs {vs1, vs2, vs3, vs4};
        auto fetcher = LazyFetch(regs);
        return CheckCall<format>(method, fetcher);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleCallAcc()
    {
        LOG_INST();
        DBGBRK();
        unsigned acc_pos = inst_.GetImm<format, 0x0>();
        static constexpr auto NUM_ARGS = 4;
        const CachedMethod *method = GetCachedMethod();
        if (method != nullptr) {
            LOG_VERIFIER_DEBUG_METHOD(method->GetName());
        }
        Sync();
        std::array<int, NUM_ARGS> regs;
        auto reg_idx = 0;
        for (unsigned i = 0; i < NUM_ARGS; ++i) {
            if (i == acc_pos) {
                regs[i] = ACC;
            } else {
                regs[i] = inst_.GetVReg(reg_idx++);
            }
        }
        auto fetcher = LazyFetch(regs);
        return CheckCall<format>(method, fetcher);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleCallRange()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs = inst_.GetVReg<format, 0x00>();
        const CachedMethod *method = GetCachedMethod();
        if (method != nullptr) {
            LOG_VERIFIER_DEBUG_METHOD(method->GetName());
        }
        Sync();
        return CheckCall<format>(method, [this, reg_idx = vs]() mutable -> std::optional<int> {
            if (!ExecCtx().CurrentRegContext().IsRegDefined(reg_idx)) {
                return {};
            }
            return reg_idx++;
        });
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleCallVirtShort()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs1 = inst_.GetVReg<format, 0x00>();
        uint16_t vs2 = inst_.GetVReg<format, 0x01>();
        const CachedMethod *method = GetCachedMethod();
        if (method != nullptr) {
            LOG_VERIFIER_DEBUG_METHOD(method->GetName());
        }
        if (method != nullptr && method->flags[CachedMethod::Flag::STATIC]) {
            LOG_VERIFIER_BAD_CALL_STATIC_METHOD_AS_VIRTUAL(method->GetName());
            status_ = VerificationStatus::ERROR;
            return false;
        }

        Sync();
        const std::array<int, 4> regs {vs1, vs2};
        auto fetcher = LazyFetch(regs);
        return CheckCall<format>(method, fetcher);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleCallVirtAccShort()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs1 = inst_.GetVReg<format, 0x00>();
        unsigned acc_pos = inst_.GetImm<format, 0x00>();
        static constexpr auto NUM_ARGS = 2;
        const CachedMethod *method = GetCachedMethod();
        if (method != nullptr) {
            LOG_VERIFIER_DEBUG_METHOD(method->GetName());
        }
        if (method != nullptr && method->flags[CachedMethod::Flag::STATIC]) {
            LOG_VERIFIER_BAD_CALL_STATIC_METHOD_AS_VIRTUAL(method->GetName());
            status_ = VerificationStatus::ERROR;
            return false;
        }
        Sync();
        std::array<int, NUM_ARGS> regs;
        if (acc_pos == 0) {
            regs = {ACC, vs1};
        } else {
            regs = {vs1, ACC};
        }
        auto fetcher = LazyFetch(regs);
        return CheckCall<format>(method, fetcher);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleCallVirt()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs1 = inst_.GetVReg<format, 0x00>();
        uint16_t vs2 = inst_.GetVReg<format, 0x01>();
        uint16_t vs3 = inst_.GetVReg<format, 0x02>();
        uint16_t vs4 = inst_.GetVReg<format, 0x03>();
        const CachedMethod *method = GetCachedMethod();
        if (method != nullptr) {
            LOG_VERIFIER_DEBUG_METHOD(method->GetName());
        }
        if (method != nullptr && method->flags[CachedMethod::Flag::STATIC]) {
            LOG_VERIFIER_BAD_CALL_STATIC_METHOD_AS_VIRTUAL(method->GetName());
            status_ = VerificationStatus::ERROR;
            return false;
        }

        Sync();
        const std::array<int, 4> regs {vs1, vs2, vs3, vs4};
        auto fetcher = LazyFetch(regs);
        return CheckCall<format>(method, fetcher);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleCallVirtAcc()
    {
        LOG_INST();
        DBGBRK();
        unsigned acc_pos = inst_.GetImm<format, 0x0>();
        static constexpr auto NUM_ARGS = 4;
        const CachedMethod *method = GetCachedMethod();
        if (method != nullptr) {
            LOG_VERIFIER_DEBUG_METHOD(method->GetName());
        }
        if (method != nullptr && method->flags[CachedMethod::Flag::STATIC]) {
            LOG_VERIFIER_BAD_CALL_STATIC_METHOD_AS_VIRTUAL(method->GetName());
            status_ = VerificationStatus::ERROR;
            return false;
        }
        Sync();
        std::array<int, NUM_ARGS> regs;
        auto reg_idx = 0;
        for (unsigned i = 0; i < NUM_ARGS; ++i) {
            if (i == acc_pos) {
                regs[i] = ACC;
            } else {
                regs[i] = inst_.GetVReg(reg_idx++);
            }
        }
        auto fetcher = LazyFetch(regs);
        return CheckCall<format>(method, fetcher);
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleCallVirtRange()
    {
        LOG_INST();
        DBGBRK();
        uint16_t vs = inst_.GetVReg<format, 0x00>();

        const CachedMethod *method = GetCachedMethod();
        if (method != nullptr) {
            LOG_VERIFIER_DEBUG_METHOD(method->GetName());
        }
        if (method != nullptr && method->flags[CachedMethod::Flag::STATIC]) {
            LOG_VERIFIER_BAD_CALL_STATIC_METHOD_AS_VIRTUAL(method->GetName());
            status_ = VerificationStatus::ERROR;
            return false;
        }

        Sync();
        return CheckCall<format>(method, [this, reg_idx = vs]() mutable -> std::optional<int> {
            if (!ExecCtx().CurrentRegContext().IsRegDefined(reg_idx)) {
                return {};
            }
            return reg_idx++;
        });
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleThrow()
    {
        LOG_INST();
        DBGBRK();
        ExecCtx().SetCheckPoint(inst_.GetAddress());
        Sync();
        uint16_t vs = inst_.GetVReg<format>();
        if (!CheckRegTypes(vs, {Types().ObjectType()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        // possible implementation:
        // on stage of building checkpoints:
        // - add all catch block starts as checkpoints/entries
        // on absint stage:
        // - find corresponding catch block/checkpoint/entry
        // - add context to checkpoint/entry
        // - add entry to entry list
        // - stop absint
        return false;
    }

    BytecodeInstructionSafe GetInst()
    {
        return inst_;
    }

    static PandaString RegisterName(int reg_idx, bool capitalize = false)
    {
        if (reg_idx == ACC) {
            return capitalize ? "Accumulator" : "accumulator";
        } else {
            return PandaString {capitalize ? "Register v" : "register v"} + NumToStr<PandaString>(reg_idx);
        }
    }

private:
    const CachedClass *GetCachedClass() const
    {
        auto offset = inst_.GetOffset();
        if (!CurrentJob.IsClassPresentForOffset(offset)) {
            SHOW_MSG(CacheMissForClassAtOffset)
            LOG_VERIFIER_CACHE_MISS_FOR_CLASS_AT_OFFSET(offset);
            END_SHOW_MSG();

            SHOW_MSG(CannotResolveClassId)
            LOG_VERIFIER_CANNOT_RESOLVE_CLASS_ID(NumToStr<PandaString>(
                static_cast<size_t>(inst_.GetId().AsFileId().GetOffset()), static_cast<size_t>(16U)))
            END_SHOW_MSG();
            return nullptr;
        }
        return &CurrentJob.GetClass(offset);
    }

    const CachedMethod *GetCachedMethod() const
    {
        auto offset = inst_.GetOffset();
        if (!CurrentJob.IsMethodPresentForOffset(offset)) {
            SHOW_MSG(CacheMissForMethodAtOffset)
            LOG_VERIFIER_CACHE_MISS_FOR_METHOD_AT_OFFSET(offset);
            END_SHOW_MSG();

            SHOW_MSG(CannotResolveMethodId)
            LOG_VERIFIER_CANNOT_RESOLVE_METHOD_ID(NumToStr<PandaString>(
                static_cast<size_t>(inst_.GetId().AsFileId().GetOffset()), static_cast<size_t>(16U)))
            END_SHOW_MSG();
            return nullptr;
        }
        return &CurrentJob.GetMethod(offset);
    }

    const CachedField *GetCachedField() const
    {
        auto offset = inst_.GetOffset();
        if (!CurrentJob.IsFieldPresentForOffset(offset)) {
            SHOW_MSG(CacheMissForFieldAtOffset)
            LOG_VERIFIER_CACHE_MISS_FOR_FIELD_AT_OFFSET(offset);
            END_SHOW_MSG();

            SHOW_MSG(CannotResolveFieldId)
            LOG_VERIFIER_CANNOT_RESOLVE_FIELD_ID(NumToStr<PandaString>(
                static_cast<size_t>(inst_.GetId().AsFileId().GetOffset()), static_cast<size_t>(16U)))
            END_SHOW_MSG();
            return nullptr;
        }
        return &CurrentJob.GetField(offset);
    }

    template <BytecodeInstructionSafe::Format format>
    void MoveToNextInst()
    {
        inst_ = inst_.GetNext<format>();
    }

    template <BytecodeInstructionSafe::Format format>
    bool CheckArrayStore(int v1, int v2, const Type &expected_elt_type)
    {
        /*
        main rules:
        1. instruction should be strict in array element size, so for primitive types type equality is used
        2. accumulator may be subtype of array element type (under question)
        */
        if (!CheckRegTypes(v2, {Types().Integral32Type()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        if (!CheckRegTypes(v1, {Types().ArrayType()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        if (!IsRegDefined(ACC)) {
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        auto &&reg_type = GetRegType(v1);

        if (reg_type.ForAllTypes([&](Type reg_type1) { return reg_type1 == Types().NullRefType(); })) {
            // treat it as always throw NPE
            SHOW_MSG(AlwaysNpe)
            LOG_VERIFIER_ALWAYS_NPE(v1);
            END_SHOW_MSG();
            SetAcc(Types().Top());
            SET_STATUS_FOR_MSG(AlwaysNpe);
            return false;
        }

        auto &&arr_elt_type = GetArrayEltType(reg_type);

        if (arr_elt_type.ForAllTypes([&](Type arr_elt_type1) { return !(arr_elt_type1 <= expected_elt_type); })) {
            SHOW_MSG(BadArrayElementType2)
            LOG_VERIFIER_BAD_ARRAY_ELEMENT_TYPE2(ImageOf(arr_elt_type), ImageOf(expected_elt_type));
            END_SHOW_MSG();
            SET_STATUS_FOR_MSG(BadArrayElementType2);
            return false;
        }

        auto &&acc_type = GetAccType();

        // since there is no problems with storage (all refs are of the same size)
        // and no problems with information losses, it seems fine at first sight.
        if (acc_type.ForAllTypes([&](Type acc_type1) {
                return arr_elt_type.ForAllTypes([&](Type arr_elt_type1) { return !(acc_type1 <= arr_elt_type1); });
            })) {
            PandaVector<Type> arr_elt_type_members;
            arr_elt_type.ForAllTypes([&](Type arr_elt_type1) {
                arr_elt_type_members.push_back(arr_elt_type1);
                return true;
            });
            // accumulator is of wrong type
            SHOW_MSG(BadAccumulatorType)
            LOG_VERIFIER_BAD_ACCUMULATOR_TYPE(ImageOf(acc_type), ImageOf(arr_elt_type),
                                              ImagesOf(SubtypesOf(arr_elt_type_members)));
            END_SHOW_MSG();
            SET_STATUS_FOR_MSG(BadAccumulatorType);
            return false;
        };

        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format>
    bool CheckArrayStoreExact(int v1, int v2, const Type &acc_supertype,
                              const std::initializer_list<Type> &expected_elt_types)
    {
        status_ = VerificationStatus::ERROR;

        if (!CheckRegTypes(v2, {Types().Integral32Type()}) || !CheckRegTypes(v1, {Types().ArrayType()}) ||
            !IsRegDefined(ACC)) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        auto &&reg_type = GetRegType(v1);

        if (reg_type.ForAllTypes([&](Type reg_type1) { return reg_type1 == Types().NullRefType(); })) {
            SHOW_MSG(AlwaysNpe)
            LOG_VERIFIER_ALWAYS_NPE(v1);
            END_SHOW_MSG();
            SetAcc(Types().Top());
            SET_STATUS_FOR_MSG(AlwaysNpe);
            return false;
        }

        auto &&arr_elt_type = GetArrayEltType(reg_type);

        auto find = [&expected_elt_types](const auto &type) {
            return type.ExistsType([&](auto type1) {
                for (const auto &t : expected_elt_types) {
                    if (type1 == t) {
                        return true;
                    }
                }
                return false;
            });
        };

        if (!find(arr_elt_type)) {
            // array elt type is not expected one
            LOG_VERIFIER_BAD_ARRAY_ELEMENT_TYPE3(ImageOf(arr_elt_type), ImagesOf(expected_elt_types));
            return false;
        }

        auto &&acc_type = GetAccType();

        if (acc_type.ForAllTypes([&](Type acc_type1) { return !(acc_type1 <= acc_supertype); })) {
            LOG_VERIFIER_BAD_ACCUMULATOR_TYPE2(ImageOf(acc_type), ImagesOf(SubtypesOf({acc_supertype})));
            return false;
        }

        status_ = VerificationStatus::OK;
        if (!find(acc_type)) {
            // array elt type is not expected one
            LOG_VERIFIER_BAD_ACCUMULATOR_TYPE3(ImageOf(acc_type), ImagesOf(expected_elt_types));
            status_ = VerificationStatus::WARNING;
        }

        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format, typename T1, typename T2, typename T3>
    bool CheckBinaryOp2(T1 acc_in, T2 reg_in, T3 acc_out)
    {
        uint16_t vs = inst_.GetVReg<format>();
        if (!CheckRegTypes(ACC, {acc_in})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        if (!CheckRegTypes(vs, {reg_in})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        SetAcc(acc_out);
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format, typename T1, typename T2>
    bool CheckBinaryOp2(T1 acc_in, T2 reg_in)
    {
        if (!IsRegDefined(ACC)) {
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        return CheckBinaryOp2<format>(acc_in, reg_in, GetAccType());
    }

    template <BytecodeInstructionSafe::Format format, typename T1, typename T2>
    bool CheckBinaryOp2Imm(T1 acc_in, T2 acc_out)
    {
        if (!CheckRegTypes(ACC, {acc_in})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        SetAcc(acc_out);
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format, typename T1>
    bool CheckBinaryOp2Imm(T1 acc_in)
    {
        if (!IsRegDefined(ACC)) {
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        return CheckBinaryOp2Imm<format>(acc_in, GetAccType());
    }

    template <BytecodeInstructionSafe::Format format, typename T1, typename T2>
    bool CheckUnaryOp(T1 acc_in, T2 acc_out)
    {
        if (!CheckRegTypes(ACC, {acc_in})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        SetAcc(acc_out);
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format, typename T1>
    bool CheckUnaryOp(T1 acc_in)
    {
        if (!IsRegDefined(ACC)) {
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        return CheckUnaryOp<format>(acc_in, GetAccType());
    }

    template <BytecodeInstructionSafe::Format format, typename T1, typename T2, typename T3>
    bool CheckBinaryOp(T1 vs1_in, T2 vs2_in, T3 acc_out)
    {
        uint16_t vs1 = inst_.GetVReg<format, 0x00>();
        uint16_t vs2 = inst_.GetVReg<format, 0x01>();
        if (!CheckRegTypes(vs1, {vs1_in})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        if (!CheckRegTypes(vs2, {vs2_in})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        SetAcc(acc_out);
        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format, typename T1, typename T2>
    bool CheckBinaryOp(T1 vs1_in, T2 vs2_in)
    {
        if (!IsRegDefined(ACC)) {
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        return CheckBinaryOp<format>(vs1_in, vs2_in, GetAccType());
    }

    template <BytecodeInstructionSafe::Format format>
    bool HandleConversion(Type from, Type to)
    {
        if (!CheckRegTypes(ACC, {from})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        SetAcc(to);
        MoveToNextInst<format>();
        return true;
    }

    bool IsConcreteArrayType(Type type)
    {
        return type <= Types().ArrayType() && type.ParamsSize() == 1;
    }

    Type GetArrayEltType(Type arr_type)
    {
        if (arr_type <= Types().ArrayType()) {
            auto &&type_params = arr_type.Params();
            ASSERT(type_params.size() == 1);
            return Types().TypeOf(type_params[0]);
        } else {
            return Types().Top();
        }
    }

    AbstractType GetArrayEltType(const AbstractType &arr_type)
    {
        if (arr_type.IsType()) {
            return GetArrayEltType(arr_type.GetType());
        } else if (arr_type.IsTypeSet()) {
            TypeSet result(Types().GetKind());
            arr_type.GetTypeSet().ForAll([&](Type type1) {
                if (IsConcreteArrayType(type1)) {
                    result.Insert(GetArrayEltType(type1));
                }
                return true;
            });
            return result;
        }
        return {};
    }

    template <BytecodeInstructionSafe::Format format>
    bool CheckArrayLoad(int vs, const PandaVector<Type> &elt_types)
    {
        if (!CheckRegTypes(ACC, {Types().Integral32Type()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        if (!CheckRegTypes(vs, {Types().ArrayType()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }
        auto &&reg_type = GetRegType(vs);
        if (reg_type.ForAllTypes([&](Type reg_type1) { return reg_type1 == Types().NullRefType(); })) {
            // treat it as always throw NPE
            SHOW_MSG(AlwaysNpe)
            LOG_VERIFIER_ALWAYS_NPE(vs);
            END_SHOW_MSG();
            SetAcc(Types().Top());
            SET_STATUS_FOR_MSG(AlwaysNpe);
            return false;
        }
        auto &&arr_elt_type = GetArrayEltType(reg_type);
        if (!IsItemPresent(elt_types, [&arr_elt_type](const auto &type) {
                return arr_elt_type.ExistsType([&](auto arr_elt_type1) { return type == arr_elt_type1; });
            })) {
            LOG_VERIFIER_BAD_ARRAY_ELEMENT_TYPE3(ImageOf(arr_elt_type), ImagesOf(elt_types));
            status_ = VerificationStatus::ERROR;
            return false;
        }
        SetAcc(arr_elt_type);
        MoveToNextInst<format>();
        return true;
    }

    bool ProcessBranching(int32_t offset)
    {
        auto new_inst = inst_.JumpTo(offset);
        const uint8_t *target = new_inst.GetAddress();
        if (!context_.CflowInfo().InstMap().CanJumpTo(target)) {
            LOG_VERIFIER_INCORRECT_JUMP();
            status_ = VerificationStatus::ERROR;
            return false;
        }

#ifndef NDEBUG
        ExecCtx().ProcessJump(
            inst_.GetAddress(), target,
            [this, print_hdr = true](int reg_idx, const auto &src_reg, const auto &dst_reg) mutable {
                if (print_hdr) {
                    LOG_VERIFIER_REGISTER_CONFLICT_HEADER();
                    print_hdr = false;
                }
                LOG_VERIFIER_REGISTER_CONFLICT(RegisterName(reg_idx), ImageOf(src_reg.GetAbstractType()),
                                               ImageOf(dst_reg.GetAbstractType()));
                return true;
            },
            code_type_);
#else
        ExecCtx().ProcessJump(inst_.GetAddress(), target, code_type_);
#endif
        return true;
    }

    template <BytecodeInstructionSafe::Format format, template <typename OpT> class Op>
    bool HandleCondJmpz()
    {
        auto imm = inst_.GetImm<format>();

        if (!CheckRegTypes(ACC, {Types().Integral32Type()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }

        if (!ProcessBranching(imm)) {
            return false;
        }

        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format, template <typename OpT> class Op>
    bool HandleCondJmp()
    {
        auto imm = inst_.GetImm<format>();
        uint16_t vs = inst_.GetVReg<format>();

        if (!CheckRegTypes(ACC, {Types().Integral32Type()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }

        if (!CheckRegTypes(vs, {Types().Integral32Type()})) {
            SET_STATUS_FOR_MSG(BadRegisterType);
            SET_STATUS_FOR_MSG(UndefinedRegister);
            return false;
        }

        if (!ProcessBranching(imm)) {
            return false;
        }

        MoveToNextInst<format>();
        return true;
    }

    template <BytecodeInstructionSafe::Format format, typename Fetcher>
    bool CheckArrayCtor(const CachedMethod &ctor, Fetcher reg_nums)
    {
        Type klass = Types().TypeOf(ctor.GetClass());
        if (!klass.IsValid()) {
            return false;
        }
        auto args_num = ctor.signature.size() - 1;
        bool result = false;
        ForEachCond(reg_nums, [&args_num, &result, this](int reg) {
            if (!IsRegDefined(reg)) {
                SET_STATUS_FOR_MSG(UndefinedRegister);
                return result = false;
            }
            result = CheckRegTypes(reg, {I32});
            if (!result) {
                status_ = VerificationStatus::ERROR;
            }
            --args_num;
            return result && args_num > 0;
        });
        if (result && args_num > 0) {
            SHOW_MSG(TooFewArrayConstructorArgs)
            LOG_VERIFIER_TOO_FEW_ARRAY_CONSTRUCTOR_ARGS(args_num);
            END_SHOW_MSG();
            SET_STATUS_FOR_MSG(TooFewArrayConstructorArgs);
            result = false;
        }
        if (result) {
            SetAcc(klass);
            MoveToNextInst<format>();
        }
        return result;
    }

    void LogInnerMessage(const CheckResult &elt)
    {
        if (elt.IsError()) {
            LOG(ERROR, VERIFIER) << "Error: " << elt.msg << ". ";
        } else if (elt.IsWarning()) {
            LOG(WARNING, VERIFIER) << "Warning: " << elt.msg << ". ";
        }
    }

    TypeRelationship GetRelationship(const Type &type1, const Type &type2)
    {
        if (type1 == type2) {
            return TypeRelationship::SAME;
        } else if (type1 <= type2) {
            return TypeRelationship::DESCENDANT;
        } else {
            return TypeRelationship::OTHER;
        }
    }

    // works for both fields and methods
    template <typename T>
    AccessModifier GetAccessMode(const T *x)
    {
        if (x->flags[T::Flag::PRIVATE]) {
            return AccessModifier::PRIVATE;
        } else if (x->flags[T::Flag::PROTECTED]) {
            return AccessModifier::PROTECTED;
        } else {
            return AccessModifier::PUBLIC;
        }
    }

    PandaVector<Type> SubSupTypesOf(Type type)
    {
        PandaVector<Type> result;
        auto callback = [&result](auto t) {
            if (!t.IsBot() && !t.IsTop()) {
                result.push_back(t);
            }
            return true;
        };
        type.ForAllSubtypes(callback);
        type.ForAllSupertypes(callback);
        return result;
    }

private:
    BytecodeInstructionSafe inst_;
    VerificationContext &context_;
    VerificationStatus status_;
    // #ifndef NDEBUG
    bool debug_ {false};
    uint32_t debug_offset_ {0};
    // #endif
    EntryPointType code_type_;

    static bool AddCheckResult(PandaVector<CheckResult> &results, const CheckResult &result)
    {
        if (result.IsOk()) {
            results.clear();
            return false;
        } else if (result.IsWarning() && results.size() > 0 && results[0].IsError()) {
            results.clear();
        }
        results.push_back(std::move(result));
        return true;
    }
};
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_ABSINT_ABS_INT_INL_H_
