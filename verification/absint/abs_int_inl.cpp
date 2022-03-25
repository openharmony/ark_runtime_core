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

#include "abs_int_inl.h"

namespace panda::verifier {

#ifndef NDEBUG
bool AbsIntInstructionHandler::IsRegDefined(int reg)
{
    bool is_defined = ExecCtx().CurrentRegContext().IsRegDefined(reg);
    if (!is_defined) {
        if (!ExecCtx().CurrentRegContext().WasConflictOnReg(reg)) {
            SHOW_MSG(UndefinedRegister)
            LOG_VERIFIER_UNDEFINED_REGISTER(RegisterName(reg, true));
            END_SHOW_MSG();
        } else {
            SHOW_MSG(RegisterTypeConflict)
            LOG_VERIFIER_REGISTER_TYPE_CONFLICT(RegisterName(reg, false));
            END_SHOW_MSG();
        }
    }
    return is_defined;
}
#else
bool AbsIntInstructionHandler::IsRegDefined(int reg)
{
    return ExecCtx().CurrentRegContext().IsRegDefined(reg);
}
#endif

const PandaString &AbsIntInstructionHandler::ImageOf(const Type &type)
{
    return Types().ImageOf(type);
}

PandaString AbsIntInstructionHandler::ImageOf(const AbstractType &abstract_type)
{
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE)
    return abstract_type.template Image<PandaString>([this](const Type &type) { return ImageOf(type); });
}

PandaString AbsIntInstructionHandler::ImageOf(const TypeSet &types)
{
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE)
    return types.template Image<PandaString>([this](const Type &type) { return ImageOf(type); });
}

PandaVector<Type> AbsIntInstructionHandler::SubtypesOf(const PandaVector<Type> &types)
{
    PandaUnorderedSet<Type> set;
    for (const auto &type : types) {
        type.ForAllSubtypes([&set](const auto &t) {
            set.insert(t);
            return true;
        });
    }
    return PandaVector<Type> {set.cbegin(), set.cend()};
}

PandaVector<Type> AbsIntInstructionHandler::SubtypesOf(std::initializer_list<Type> types)
{
    PandaUnorderedSet<Type> set;
    for (const auto &type : types) {
        type.ForAllSubtypes([&set](const auto &t) {
            set.insert(t);
            return true;
        });
    }
    return PandaVector<Type> {set.cbegin(), set.cend()};
}

PandaVector<Type> AbsIntInstructionHandler::SupertypesOf(const PandaVector<Type> &types)
{
    PandaUnorderedSet<Type> set;
    for (const auto &type : types) {
        type.ForAllSupertypes([&set](const auto &t) {
            set.insert(t);
            return true;
        });
    }
    return PandaVector<Type> {set.cbegin(), set.cend()};
}

PandaVector<Type> AbsIntInstructionHandler::SupertypesOf(std::initializer_list<Type> types)
{
    PandaUnorderedSet<Type> set;
    for (const auto &type : types) {
        type.ForAllSupertypes([&set](const auto &t) {
            set.insert(t);
            return true;
        });
    }
    return PandaVector<Type> {set.cbegin(), set.cend()};
}

bool AbsIntInstructionHandler::CheckRegTypes(int reg, std::initializer_list<Type> types)
{
    return CheckRegTypes<std::initializer_list<Type>>(reg, types);
}

bool AbsIntInstructionHandler::CheckTypes(const Type &type, std::initializer_list<Type> types)
{
    return CheckTypes<std::initializer_list<Type>>(type, types);
}

const AbstractTypedValue &AbsIntInstructionHandler::GetReg(int reg_idx)
{
    return context_.ExecCtx().CurrentRegContext()[reg_idx];
}

const AbstractType &AbsIntInstructionHandler::GetRegType(int reg_idx)
{
    return GetReg(reg_idx).GetAbstractType();
}

void AbsIntInstructionHandler::SetReg(int reg_idx, const AbstractTypedValue &val)
{
    if (CurrentJob.Options().ShowRegChanges()) {
        PandaString prev_atv_image {"<none>"};
        auto img_of = [this](const auto &t) { return ImageOf(t); };
        if (ExecCtx().CurrentRegContext().IsRegDefined(reg_idx)) {
            prev_atv_image = GetReg(reg_idx).Image<PandaString>(img_of);
        }
        auto new_atv_image = val.Image<PandaString>(img_of);
        LOG_VERIFIER_DEBUG_REGISTER_CHANGED(RegisterName(reg_idx), prev_atv_image, new_atv_image);
    }
    context_.ExecCtx().CurrentRegContext()[reg_idx] = val;
}

void AbsIntInstructionHandler::SetReg(int reg_idx, const AbstractType &type)
{
    SetReg(reg_idx, MkVal(type));
}

void AbsIntInstructionHandler::SetRegAndOthersOfSameOrigin(int reg_idx, const AbstractTypedValue &val)
{
    context_.ExecCtx().CurrentRegContext().ChangeValuesOfSameOrigin(reg_idx, val);
}

void AbsIntInstructionHandler::SetRegAndOthersOfSameOrigin(int reg_idx, const AbstractType &type)
{
    SetRegAndOthersOfSameOrigin(reg_idx, MkVal(type));
}

const AbstractTypedValue &AbsIntInstructionHandler::GetAcc()
{
    return context_.ExecCtx().CurrentRegContext()[ACC];
}

const AbstractType &AbsIntInstructionHandler::GetAccType()
{
    return GetAcc().GetAbstractType();
}

void AbsIntInstructionHandler::SetAcc(const AbstractTypedValue &val)
{
    SetReg(ACC, val);
}

void AbsIntInstructionHandler::SetAcc(const AbstractType &type)
{
    SetReg(ACC, type);
}

void AbsIntInstructionHandler::SetAccAndOthersOfSameOrigin(const AbstractTypedValue &val)
{
    SetRegAndOthersOfSameOrigin(ACC, val);
}

void AbsIntInstructionHandler::SetAccAndOthersOfSameOrigin(const AbstractType &type)
{
    SetRegAndOthersOfSameOrigin(ACC, type);
}

AbstractTypedValue AbsIntInstructionHandler::MkVal(const AbstractType &t)
{
    return AbstractTypedValue {t, context_.NewVar(), GetInst()};
}

PandaTypes &AbsIntInstructionHandler::Types()
{
    return context_.Types();
}

const Type &AbsIntInstructionHandler::ReturnType()
{
    return context_.ReturnType();
}

ExecContext &AbsIntInstructionHandler::ExecCtx()
{
    return context_.ExecCtx();
}

void AbsIntInstructionHandler::DumpRegs(const RegContext &ctx)
{
    LOG_VERIFIER_DEBUG_REGISTERS("registers =", ctx.DumpRegs([this](const auto &t) { return ImageOf(t); }));
}

void AbsIntInstructionHandler::Sync()
{
    auto addr = inst_.GetAddress();
    ExecContext &exec_ctx = ExecCtx();
#ifndef NDEBUG
    exec_ctx.StoreCurrentRegContextForAddr(
        addr, [this, print_hdr = true](int reg_idx, const auto &src, const auto &dst) mutable {
            if (print_hdr) {
                LOG_VERIFIER_REGISTER_CONFLICT_HEADER();
                print_hdr = false;
            }
            LOG_VERIFIER_REGISTER_CONFLICT(RegisterName(reg_idx), ImageOf(src.GetAbstractType()),
                                           ImageOf(dst.GetAbstractType()));
            return true;
        });
#else
    exec_ctx.StoreCurrentRegContextForAddr(addr);
#endif
}

}  // namespace panda::verifier
