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

#ifndef PANDA_RUNTIME_INCLUDE_CFRAME_ITERATORS_H_
#define PANDA_RUNTIME_INCLUDE_CFRAME_ITERATORS_H_

#include "libpandabase/utils/arch.h"
#include "libpandafile/shorty_iterator.h"
#include "runtime/arch/helpers.h"
#include "runtime/include/cframe.h"
#include "runtime/include/method.h"
#include "utils/bit_utils.h"

namespace panda {

template <typename It>
class Range {
public:
    Range(It begin, It end) : begin_(begin), end_(end) {}

    It begin()  // NOLINT(readability-identifier-naming)
    {
        return begin_;
    }

    It end()  // NOLINT(readability-identifier-naming)
    {
        return end_;
    }

private:
    It begin_;
    It end_;
};

template <Arch arch = RUNTIME_ARCH>
class CFrameJniMethodIterator {
    using SlotType = typename CFrame::SlotType;

    static constexpr size_t ARG_FP_REGS_COUNG = arch::ExtArchTraits<arch>::NUM_FP_ARG_REGS;
    static constexpr size_t ARG_GP_REGS_COUNG = arch::ExtArchTraits<arch>::NUM_GP_ARG_REGS;

public:
    static auto MakeRange(CFrame *cframe)
    {
        CFrameLayout cframe_layout(arch, 0);

        const ptrdiff_t IN_REGS_START_SLOT =
            cframe_layout.GetCallerRegsStartSlot() -
            cframe_layout.GetStackStartSlot()
            // skipped the slot to align the stack, CODECHECK-NOLINT(C_RULE_ID_INDENT_CHECK)
            + (arch == Arch::X86_64 ? 1 : 0);
        const ptrdiff_t IN_STACK_START_SLOT = cframe_layout.GetStackArgsStartSlot() - cframe_layout.GetStackStartSlot();

        ptrdiff_t fp_end_slot = IN_REGS_START_SLOT - 1;
        ptrdiff_t fp_begin_slot = fp_end_slot + ARG_FP_REGS_COUNG;
        ptrdiff_t gpr_end_slot = fp_begin_slot;
        ptrdiff_t gpr_begin_slot = gpr_end_slot + ARG_GP_REGS_COUNG;
        ptrdiff_t stack_begin_slot = IN_STACK_START_SLOT + 1;

        Method *method = cframe->GetMethod();
        bool is_static = method->IsStatic();
        if (!is_static) {
            --gpr_begin_slot;  // skip Method*
        }

        uint32_t num_args = method->GetNumArgs();
        uint32_t vreg_num = num_args + (is_static ? 1 : 0);

        return Range<CFrameJniMethodIterator>(
            CFrameJniMethodIterator(0, vreg_num, method->GetShorty(), gpr_begin_slot, gpr_end_slot, fp_begin_slot + 1,
                                    fp_end_slot, stack_begin_slot),
            CFrameJniMethodIterator(vreg_num, vreg_num, method->GetShorty(), 0, 0, 0, 0, 0));
    }

    VRegInfo operator*()
    {
        return VRegInfo(current_slot_, VRegInfo::Location::SLOT, vreg_type_, false, vreg_index_);
    }

    auto operator++()
    {
        if (++vreg_index_ >= vreg_num_) {
            return *this;
        }

        // Update vreg_type_
        vreg_type_ = ConvertType(*shorty_it_);
        ++shorty_it_;

        // Update current_slot_
        if (vreg_type_ == VRegInfo::Type::FLOAT32 || vreg_type_ == VRegInfo::Type::FLOAT64) {
            if ((fp_current_slot_ - 1) > fp_end_slot_) {
                --fp_current_slot_;
                current_slot_ = fp_current_slot_;
            } else {
                --stack_current_slot_;
                current_slot_ = stack_current_slot_;
            }
        } else {
            if ((gpr_current_slot_ - 1) > gpr_end_slot_) {
                --gpr_current_slot_;
                current_slot_ = gpr_current_slot_;
            } else {
                --stack_current_slot_;
                current_slot_ = stack_current_slot_;
            }
        }

        return *this;
    }

    auto operator++(int)  // NOLINT(cert-dcl21-cpp)
    {
        auto res = *this;
        this->operator++();
        return res;
    }

    bool operator==(const CFrameJniMethodIterator &it) const
    {
        return vreg_index_ == it.vreg_index_;
    }

    bool operator!=(const CFrameJniMethodIterator &it) const
    {
        return !(*this == it);
    }

private:
    CFrameJniMethodIterator(uint32_t vreg_index, uint32_t vreg_num, const uint16_t *shorty, ptrdiff_t gpr_begin_slot,
                            ptrdiff_t gpr_end_slot, ptrdiff_t fp_begin_slot, ptrdiff_t fp_end_slot,
                            ptrdiff_t stack_current_slot)
        : vreg_index_(vreg_index),
          vreg_num_(vreg_num),
          shorty_it_(shorty),
          current_slot_(gpr_begin_slot),
          gpr_current_slot_(gpr_begin_slot),
          gpr_end_slot_(gpr_end_slot),
          fp_current_slot_(fp_begin_slot),
          fp_end_slot_(fp_end_slot),
          stack_current_slot_(stack_current_slot)
    {
        ++shorty_it_;  // skip return value
    }

    VRegInfo::Type ConvertType(panda_file::Type type) const
    {
        switch (type.GetId()) {
            case panda_file::Type::TypeId::U1:
                return VRegInfo::Type::BOOL;
            case panda_file::Type::TypeId::I8:
            case panda_file::Type::TypeId::U8:
            case panda_file::Type::TypeId::I16:
            case panda_file::Type::TypeId::U16:
            case panda_file::Type::TypeId::I32:
            case panda_file::Type::TypeId::U32:
                return VRegInfo::Type::INT32;
            case panda_file::Type::TypeId::F32:
                return VRegInfo::Type::FLOAT32;
            case panda_file::Type::TypeId::F64:
                return VRegInfo::Type::FLOAT64;
            case panda_file::Type::TypeId::I64:
            case panda_file::Type::TypeId::U64:
                return VRegInfo::Type::INT64;
            case panda_file::Type::TypeId::REFERENCE:
                return VRegInfo::Type::OBJECT;
            case panda_file::Type::TypeId::TAGGED:
                return VRegInfo::Type::INT64;
            default:
                UNREACHABLE();
        }
        return VRegInfo::Type::INT32;
    }

private:
    uint32_t vreg_index_;
    uint32_t vreg_num_;
    panda_file::ShortyIterator shorty_it_;
    ptrdiff_t current_slot_;
    ptrdiff_t gpr_current_slot_;
    ptrdiff_t gpr_end_slot_;
    ptrdiff_t fp_current_slot_;
    ptrdiff_t fp_end_slot_;
    ptrdiff_t stack_current_slot_;
    VRegInfo::Type vreg_type_ = VRegInfo::Type::OBJECT;
};

template <>
class CFrameJniMethodIterator<Arch::AARCH32> {
    using SlotType = typename CFrame::SlotType;

    static constexpr size_t ARG_FP_REGS_COUNG = arch::ExtArchTraits<Arch::AARCH32>::NUM_FP_ARG_REGS;
    static constexpr size_t ARG_GP_REGS_COUNG = arch::ExtArchTraits<Arch::AARCH32>::NUM_GP_ARG_REGS;

    static constexpr ptrdiff_t IN_REGS_START_SLOT = 24;
    static constexpr ptrdiff_t IN_STACK_START_SLOT = -11;
    static constexpr ptrdiff_t FP_END_SLOT = IN_REGS_START_SLOT - 1;
    static constexpr ptrdiff_t FP_BEGIN_SLOT = FP_END_SLOT + ARG_FP_REGS_COUNG;
    static constexpr ptrdiff_t GPR_END_SLOT = FP_BEGIN_SLOT;
    static constexpr ptrdiff_t GPR_BEGIN_SLOT = GPR_END_SLOT + ARG_GP_REGS_COUNG;
    static constexpr ptrdiff_t STACK_BEGIN_SLOT = IN_STACK_START_SLOT + 1;

public:
    static auto MakeRange(CFrame *cframe)
    {
        ptrdiff_t gpr_begin_slot = GPR_BEGIN_SLOT;
        Method *method = cframe->GetMethod();
        bool is_static = method->IsStatic();
        if (!is_static) {
            --gpr_begin_slot;  // skip Method*
        }

        uint32_t num_args = method->GetNumArgs();
        uint32_t vreg_num = num_args + (is_static ? 1 : 0);

        return Range<CFrameJniMethodIterator>(
            CFrameJniMethodIterator(0, vreg_num, method->GetShorty(), gpr_begin_slot, GPR_END_SLOT, FP_BEGIN_SLOT,
                                    FP_END_SLOT, STACK_BEGIN_SLOT),
            CFrameJniMethodIterator(vreg_num, vreg_num, method->GetShorty(), 0, 0, 0, 0, 0));
    }

    VRegInfo operator*()
    {
        return VRegInfo(current_slot_, VRegInfo::Location::SLOT, vreg_type_, false, vreg_index_);
    }

    uint32_t GetSlotsCountForType(VRegInfo::Type vreg_type)
    {
        static_assert(arch::ExtArchTraits<Arch::AARCH32>::GPR_SIZE == 4);  // 4 bytes -- register size on AARCH32

        if (vreg_type == VRegInfo::Type::INT64 || vreg_type == VRegInfo::Type::FLOAT64) {
            return 2;  // 2 slots
        }
        return 1;
    }

    auto operator++()
    {
        if (++vreg_index_ >= vreg_num_) {
            return *this;
        }

        // Update type
        vreg_type_ = ConvertType(*shorty_it_);
        ++shorty_it_;

        // Update slots
        auto inc = static_cast<ptrdiff_t>(GetSlotsCountForType(vreg_type_));
        ASSERT(inc == 1 || inc == 2);  // 1 or 2 slots
        if (inc == 1) {
            if constexpr (arch::ExtArchTraits<Arch::AARCH32>::HARDFP) {
                if (vreg_type_ == VRegInfo::Type::FLOAT32) {  // in this case one takes 1 slots
                    return HandleHardFloat();
                }
            }
            if ((gpr_current_slot_ - 1) > gpr_end_slot_) {
                --gpr_current_slot_;
                current_slot_ = gpr_current_slot_;
            } else {
                gpr_current_slot_ = gpr_end_slot_;

                --stack_current_slot_;
                current_slot_ = stack_current_slot_;
            }
        } else {
            if constexpr (arch::ExtArchTraits<Arch::AARCH32>::HARDFP) {
                if (vreg_type_ == VRegInfo::Type::FLOAT64) {  // in this case one takes 2 slots
                    return HandleHardDouble();
                }
            }
            gpr_current_slot_ = RoundUp(gpr_current_slot_ - 1, 2) - 1;  // 2
            if (gpr_current_slot_ > gpr_end_slot_) {
                current_slot_ = gpr_current_slot_;
                gpr_current_slot_ -= 1;
            } else {
                stack_current_slot_ = RoundUp(stack_current_slot_ - 1, 2) - 1;  // 2
                current_slot_ = stack_current_slot_;
                stack_current_slot_ -= 1;
            }
        }

        return *this;
    }

    auto operator++(int)  // NOLINT(cert-dcl21-cpp)
    {
        auto res = *this;
        this->operator++();
        return res;
    }

    bool operator==(const CFrameJniMethodIterator &it) const
    {
        return vreg_index_ == it.vreg_index_;
    }

    bool operator!=(const CFrameJniMethodIterator &it) const
    {
        return !(*this == it);
    }

private:
    CFrameJniMethodIterator(uint32_t vreg_index, uint32_t vreg_num, const uint16_t *shorty, ptrdiff_t gpr_begin_slot,
                            ptrdiff_t gpr_end_slot, ptrdiff_t fp_begin_slot, ptrdiff_t fp_end_slot,
                            ptrdiff_t stack_current_slot)
        : vreg_index_(vreg_index),
          vreg_num_(vreg_num),
          shorty_it_(shorty),
          current_slot_(gpr_begin_slot),
          gpr_current_slot_(gpr_begin_slot),
          gpr_end_slot_(gpr_end_slot),
          fp_current_slot_(fp_begin_slot),
          fp_end_slot_(fp_end_slot),
          stack_current_slot_(stack_current_slot)
    {
        ++shorty_it_;  // skip return value
    }

    VRegInfo::Type ConvertType(panda_file::Type type) const
    {
        switch (type.GetId()) {
            case panda_file::Type::TypeId::U1:
                return VRegInfo::Type::BOOL;
            case panda_file::Type::TypeId::I8:
            case panda_file::Type::TypeId::U8:
            case panda_file::Type::TypeId::I16:
            case panda_file::Type::TypeId::U16:
            case panda_file::Type::TypeId::I32:
            case panda_file::Type::TypeId::U32:
                return VRegInfo::Type::INT32;
            case panda_file::Type::TypeId::F32:
                return VRegInfo::Type::FLOAT32;
            case panda_file::Type::TypeId::F64:
                return VRegInfo::Type::FLOAT64;
            case panda_file::Type::TypeId::I64:
            case panda_file::Type::TypeId::U64:
                return VRegInfo::Type::INT64;
            case panda_file::Type::TypeId::REFERENCE:
                return VRegInfo::Type::OBJECT;
            case panda_file::Type::TypeId::TAGGED:
                return VRegInfo::Type::INT64;
            default:
                UNREACHABLE();
        }
        return VRegInfo::Type::INT32;
    }

    CFrameJniMethodIterator &HandleHardFloat()
    {
        ASSERT(vreg_type_ == VRegInfo::Type::FLOAT32);
        if (fp_current_slot_ > fp_end_slot_) {
            current_slot_ = fp_current_slot_;
            --fp_current_slot_;
        } else {
            --stack_current_slot_;
            current_slot_ = stack_current_slot_;
        }
        return *this;
    }

    CFrameJniMethodIterator &HandleHardDouble()
    {
        ASSERT(vreg_type_ == VRegInfo::Type::FLOAT64);
        fp_current_slot_ = static_cast<ptrdiff_t>(RoundDown(static_cast<uintptr_t>(fp_current_slot_) + 1, 2U) - 1);
        if (fp_current_slot_ > fp_end_slot_) {
            current_slot_ = fp_current_slot_;
            fp_current_slot_ -= 2U;
        } else {
            stack_current_slot_ = RoundUp(stack_current_slot_ - 1, 2U) - 1;
            current_slot_ = stack_current_slot_;
            stack_current_slot_ -= 1;
        }
        return *this;
    }

private:
    uint32_t vreg_index_;
    uint32_t vreg_num_;
    panda_file::ShortyIterator shorty_it_;
    ptrdiff_t current_slot_;
    ptrdiff_t gpr_current_slot_;
    ptrdiff_t gpr_end_slot_;
    ptrdiff_t fp_current_slot_;
    ptrdiff_t fp_end_slot_;
    ptrdiff_t stack_current_slot_;
    VRegInfo::Type vreg_type_ = VRegInfo::Type::OBJECT;
};

template <Arch arch = RUNTIME_ARCH>
class CFrameDynamicNativeMethodIterator {
    using SlotType = typename CFrame::SlotType;

public:
    static auto MakeRange(CFrame *cframe)
    {
        size_t arg_regs_count = arch::ExtArchTraits<arch>::NUM_GP_ARG_REGS;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        Span<SlotType> callers(cframe->GetCallerSaveStack() - arg_regs_count, arg_regs_count);
        // In dynamic methods the first two args are Method* method and uint32_t num_args
        // Read num_args
        auto num_args = static_cast<uint32_t>(callers[1]);
        ++num_args;  // count function object
        size_t num_arg_slots = num_args * sizeof(Frame::VRegister) / sizeof(SlotType);

        CFrameLayout cframe_layout(arch, 0);
        size_t caller_end_slot = cframe_layout.GetCallerRegsStartSlot();
        size_t caller_start_slot = caller_end_slot + arg_regs_count;
        size_t gpr_arg_start_slot = caller_start_slot - 2;  // skip Method and num_args, 2 - offset
        // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
        if constexpr (arch != Arch::X86_64) {
            gpr_arg_start_slot = RoundDown(gpr_arg_start_slot, sizeof(Frame::VRegister) / sizeof(SlotType));
        }
        size_t num_gpr_arg_slots = std::min(gpr_arg_start_slot - caller_end_slot, num_arg_slots);

        size_t num_stack_arg_slots = num_arg_slots - num_gpr_arg_slots;
        ptrdiff_t stack_arg_start_slot = cframe_layout.GetStackArgsStartSlot();
        ptrdiff_t stack_arg_end_slot = stack_arg_start_slot - num_stack_arg_slots;

        // Since all stack slots are calculated relative STACK_START_SLOT
        // subtract it from each value
        ptrdiff_t stack_start_slot = cframe_layout.GetStackStartSlot();
        gpr_arg_start_slot -= stack_start_slot;
        caller_end_slot -= stack_start_slot;
        stack_arg_start_slot -= stack_start_slot;
        stack_arg_end_slot -= stack_start_slot;
        return Range<CFrameDynamicNativeMethodIterator>(
            CFrameDynamicNativeMethodIterator(cframe, gpr_arg_start_slot - 1, caller_end_slot - 1, stack_arg_start_slot,
                                              stack_arg_end_slot),
            CFrameDynamicNativeMethodIterator(cframe, caller_end_slot - 1, caller_end_slot - 1, stack_arg_end_slot,
                                              stack_arg_end_slot));
    }

    VRegInfo operator*()
    {
        if (gpr_start_slot_ > gpr_end_slot_) {
            return VRegInfo(gpr_start_slot_, VRegInfo::Location::SLOT, VRegInfo::Type::INT64, false, vreg_num_);
        }
        ASSERT(stack_start_slot_ > stack_end_slot_);
        return VRegInfo(stack_start_slot_, VRegInfo::Location::SLOT, VRegInfo::Type::INT64, false, vreg_num_);
    }

    CFrameDynamicNativeMethodIterator &operator++()
    {
        size_t inc = sizeof(Frame::VRegister) / sizeof(SlotType);
        if (gpr_start_slot_ > gpr_end_slot_) {
            gpr_start_slot_ -= inc;
            ++vreg_num_;
        } else if (stack_start_slot_ > stack_end_slot_) {
            stack_start_slot_ -= inc;
            ++vreg_num_;
        }
        return *this;
    }

    // NOLINTNEXTLINE(cert-dcl21-cpp)
    CFrameDynamicNativeMethodIterator operator++(int)
    {
        auto res = *this;
        this->operator++();
        return res;
    }

    bool operator==(const CFrameDynamicNativeMethodIterator &it) const
    {
        return gpr_start_slot_ == it.gpr_start_slot_ && stack_start_slot_ == it.stack_start_slot_;
    }

    bool operator!=(const CFrameDynamicNativeMethodIterator &it) const
    {
        return !(*this == it);
    }

private:
    CFrameDynamicNativeMethodIterator(CFrame *cframe, ptrdiff_t gpr_start_slot, ptrdiff_t gpr_end_slot,
                                      ptrdiff_t stack_start_slot, ptrdiff_t stack_end_slot)
        : cframe_(cframe),
          gpr_start_slot_(gpr_start_slot),
          gpr_end_slot_(gpr_end_slot),
          stack_start_slot_(stack_start_slot),
          stack_end_slot_(stack_end_slot)
    {
    }

private:
    CFrame *cframe_;
    uint32_t vreg_num_ = 0;
    ptrdiff_t gpr_start_slot_;
    ptrdiff_t gpr_end_slot_;
    ptrdiff_t stack_start_slot_;
    ptrdiff_t stack_end_slot_;
};

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_CFRAME_ITERATORS_H_
