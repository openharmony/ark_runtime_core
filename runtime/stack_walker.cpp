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

#include "runtime/include/stack_walker-inl.h"
#include "runtime/entrypoints/entrypoints.h"
#include "runtime/include/runtime.h"
#include "runtime/include/thread.h"
#include "runtime/include/panda_vm.h"

#include <iomanip>

namespace panda {

StackWalker::StackWalker(ManagedThread *thread, UnwindPolicy policy)
    : StackWalker(thread->GetCurrentFrame(), thread->IsCurrentFrameCompiled(), thread->GetNativePc(), policy)
{
#ifndef NDEBUG
    if (Runtime::GetOptions().IsVerifyCallStack()) {
        StackWalker(thread->GetCurrentFrame(), thread->IsCurrentFrameCompiled(), thread->GetNativePc(), policy)
            .Verify();
    }
#endif
}

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
StackWalker::StackWalker(void *fp, bool is_frame_compiled, uintptr_t npc, UnwindPolicy policy)
{
    frame_ = GetTopFrameFromFp(fp, is_frame_compiled, npc);
    if (policy == UnwindPolicy::SKIP_INLINED) {
        inline_depth_ = -1;
    }
}

void StackWalker::Reset(ManagedThread *thread)
{
    frame_ = GetTopFrameFromFp(thread->GetCurrentFrame(), thread->IsCurrentFrameCompiled(), thread->GetNativePc());
}

/* static */
typename StackWalker::FrameVariant StackWalker::GetTopFrameFromFp(void *ptr, bool is_frame_compiled, uintptr_t npc)
{
    if (!is_frame_compiled) {
        return reinterpret_cast<Frame *>(ptr);
    }

    if (IsBoundaryFrame<FrameKind::INTERPRETER>(ptr)) {
        auto bp = GetPrevFromBoundary<FrameKind::INTERPRETER>(ptr);
        if (GetBoundaryFrameMethod<FrameKind::COMPILER>(bp) == BYPASS) {
            return CreateCFrame(GetPrevFromBoundary<FrameKind::COMPILER>(bp),
                                GetReturnAddressFromBoundary<FrameKind::COMPILER>(bp),
                                GetCalleeStackFromBoundary<FrameKind::COMPILER>(bp));
        }
        return CreateCFrame(
            GetPrevFromBoundary<FrameKind::INTERPRETER>(ptr), GetReturnAddressFromBoundary<FrameKind::INTERPRETER>(ptr),
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            reinterpret_cast<SlotType *>(ptr) + BoundaryFrame<FrameKind::INTERPRETER>::CALLEES_OFFSET);  // NOLINT
    }
    return CreateCFrame(reinterpret_cast<SlotType *>(ptr), npc, nullptr);
}

Method *StackWalker::GetMethod()
{
    ASSERT(HasFrame());
    if (!IsCFrame()) {
        return GetIFrame()->GetMethod();
    }
    auto &cframe = GetCFrame();
    ASSERT(cframe.IsJni());
    return cframe.GetMethod();
}

template <bool create>
StackWalker::CFrameType StackWalker::CreateCFrameForC2IBridge(Frame *frame)
{
    auto prev = GetPrevFromBoundary<FrameKind::INTERPRETER>(frame);
    ASSERT(GetBoundaryFrameMethod<FrameKind::COMPILER>(prev) != FrameBridgeKind::BYPASS);
    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if constexpr (create) {
        return CreateCFrame(reinterpret_cast<SlotType *>(prev),
                            GetReturnAddressFromBoundary<FrameKind::INTERPRETER>(frame),
                            GetCalleeStackFromBoundary<FrameKind::INTERPRETER>(frame));
    }
    return CFrameType(prev);
}

StackWalker::CFrameType StackWalker::CreateCFrame(void *ptr, [[maybe_unused]] uintptr_t npc,
                                                  [[maybe_unused]] SlotType *callee_stack,
                                                  [[maybe_unused]] CalleeStorage *prev_callees)
{
    CFrameType cframe(ptr);
    ASSERT(cframe.IsNativeMethod());
    return cframe;
}

void StackWalker::InitCalleeBuffer(SlotType *callee_stack, CalleeStorage *prev_callees)
{
    if (callee_stack == nullptr && prev_callees == nullptr) {
        return;
    }

    bool prev_is_jni = IsCFrame() ? GetCFrame().IsJni() : false;
    size_t callee_regs_count = GetCalleeRegsCount(ARCH, false);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    auto start_slot = callee_stack - callee_regs_count;
    for (size_t reg = GetFirstCalleeReg(ARCH, false); reg <= GetLastCalleeReg(ARCH, false); reg++) {
        size_t offset = reg - GetFirstCalleeReg(ARCH, false);
        // if it's a top cframe or previous frame has saved the register, then copy it from previous frame's stack
        if (prev_callees == nullptr || prev_is_jni || (prev_callees->callee_regs_mask & (1U << reg)) != 0) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            callee_stack_.stack[offset] =
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                start_slot + (callee_regs_count - static_cast<size_t>(Popcount(callee_stack_.callee_regs_mask >> reg)));
        } else {
            callee_stack_.stack[offset] = prev_callees->stack[offset];
        }
    }
    size_t callee_vregs_count = GetCalleeRegsCount(ARCH, true);
    start_slot -= callee_vregs_count;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    for (size_t reg = GetFirstCalleeReg(ARCH, true); reg <= GetLastCalleeReg(ARCH, true); reg++) {
        size_t offset = callee_regs_count + reg - GetFirstCalleeReg(ARCH, true);
        // if it's a top cframe or previous frame has saved the register, then copy it from previous frame's stack
        if (prev_callees == nullptr || prev_is_jni || (prev_callees->callee_fp_regs_mask & (1U << reg)) != 0) {
            callee_stack_.stack[offset] =
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                start_slot +
                (callee_vregs_count - static_cast<size_t>(Popcount(callee_stack_.callee_fp_regs_mask >> reg)));
        } else {
            callee_stack_.stack[offset] = prev_callees->stack[offset];
        }
    }
}

Frame::VRegister StackWalker::GetVRegValue(size_t vreg_num)
{
    ASSERT(!IsCFrame());
    ASSERT(vreg_num < GetIFrame()->GetSize());
    return GetIFrame()->GetVReg(vreg_num);
}

template <typename T>
void StackWalker::SetVRegValue(VRegInfo reg_info, T value)
{
    if (IsCFrame()) {
        auto &cframe = GetCFrame();
        if constexpr (sizeof(T) == sizeof(uint64_t)) {  // NOLINT
            cframe.SetVRegValue(reg_info, bit_cast<uint64_t>(value), callee_stack_.stack.data());
        } else {  // NOLINT
            static_assert(sizeof(T) == sizeof(uint32_t));
            cframe.SetVRegValue(reg_info, static_cast<uint64_t>(bit_cast<uint32_t>(value)), callee_stack_.stack.data());
        }
    } else {
        auto &vreg = GetIFrame()->GetVReg(reg_info.GetIndex());
        if constexpr (std::is_same_v<T, ObjectHeader *>) {  // NOLINT
            ASSERT(vreg.HasObject() && "Trying to change object variable by scalar value");
            vreg.SetReference(value);
        } else {  // NOLINT
            ASSERT(!vreg.HasObject() && "Trying to change object variable by scalar value");
            vreg.Set(value);
        }
    }
}

template void StackWalker::SetVRegValue(VRegInfo reg_info, uint32_t value);
template void StackWalker::SetVRegValue(VRegInfo reg_info, int32_t value);
template void StackWalker::SetVRegValue(VRegInfo reg_info, uint64_t value);
template void StackWalker::SetVRegValue(VRegInfo reg_info, int64_t value);
template void StackWalker::SetVRegValue(VRegInfo reg_info, float value);
template void StackWalker::SetVRegValue(VRegInfo reg_info, double value);
template void StackWalker::SetVRegValue(VRegInfo reg_info, ObjectHeader *value);

void StackWalker::NextFrame()
{
    if (IsCFrame()) {
        NextFromCFrame();
    } else {
        NextFromIFrame();
    }
}

void StackWalker::NextFromCFrame()
{
    if (IsInlined()) {
        if (policy_ != UnwindPolicy::SKIP_INLINED) {
            inline_depth_--;
            return;
        }
        inline_depth_ = -1;
    }
    if (policy_ == UnwindPolicy::ONLY_INLINED) {
        frame_ = nullptr;
        return;
    }
    auto prev = GetCFrame().GetPrevFrame();
    if (prev == nullptr) {
        frame_ = nullptr;
        return;
    }
    auto frame_method = GetBoundaryFrameMethod<FrameKind::COMPILER>(prev);
    switch (frame_method) {
        case FrameBridgeKind::INTERPRETER_TO_COMPILED_CODE: {
            auto prev_frame = reinterpret_cast<Frame *>(GetPrevFromBoundary<FrameKind::COMPILER>(prev));
            if (prev_frame != nullptr && IsBoundaryFrame<FrameKind::INTERPRETER>(prev_frame)) {
                frame_ = CreateCFrameForC2IBridge<true>(prev_frame);
                break;
            }

            frame_ = reinterpret_cast<Frame *>(prev_frame);
            break;
        }
        case FrameBridgeKind::BYPASS: {
            auto prev_frame = reinterpret_cast<Frame *>(GetPrevFromBoundary<FrameKind::COMPILER>(prev));
            if (prev_frame != nullptr && IsBoundaryFrame<FrameKind::INTERPRETER>(prev_frame)) {
                frame_ = CreateCFrameForC2IBridge<true>(prev_frame);
                break;
            }
            frame_ = CreateCFrame(reinterpret_cast<SlotType *>(GetPrevFromBoundary<FrameKind::COMPILER>(prev)),
                                  GetReturnAddressFromBoundary<FrameKind::COMPILER>(prev),
                                  GetCalleeStackFromBoundary<FrameKind::COMPILER>(prev));
            break;
        }
        default:
            prev_callee_stack_ = callee_stack_;
            frame_ = CreateCFrame(reinterpret_cast<SlotType *>(prev), GetCFrame().GetLr(),
                                  GetCFrame().GetCalleeSaveStack(), &prev_callee_stack_);
            break;
    }
}

void StackWalker::NextFromIFrame()
{
    if (policy_ == UnwindPolicy::ONLY_INLINED) {
        frame_ = nullptr;
        return;
    }
    auto prev = GetIFrame()->GetPrevFrame();
    if (prev == nullptr) {
        frame_ = nullptr;
        return;
    }
    if (IsBoundaryFrame<FrameKind::INTERPRETER>(prev)) {
        auto bp = GetPrevFromBoundary<FrameKind::INTERPRETER>(prev);
        if (GetBoundaryFrameMethod<FrameKind::COMPILER>(bp) == BYPASS) {
            frame_ = CreateCFrame(GetPrevFromBoundary<FrameKind::COMPILER>(bp),
                                  GetReturnAddressFromBoundary<FrameKind::COMPILER>(bp),
                                  GetCalleeStackFromBoundary<FrameKind::COMPILER>(bp));
        } else {
            frame_ = CreateCFrameForC2IBridge<true>(prev);
        }
    } else {
        frame_ = reinterpret_cast<Frame *>(prev);
    }
}

FrameAccessor StackWalker::GetNextFrame()
{
    if (IsCFrame()) {
        if (IsInlined()) {
            return FrameAccessor(frame_);
        }
        auto prev = GetCFrame().GetPrevFrame();
        if (prev == nullptr) {
            return FrameAccessor(nullptr);
        }
        auto frame_method = GetBoundaryFrameMethod<FrameKind::COMPILER>(prev);
        switch (frame_method) {
            case FrameBridgeKind::INTERPRETER_TO_COMPILED_CODE: {
                auto prev_frame = reinterpret_cast<Frame *>(GetPrevFromBoundary<FrameKind::COMPILER>(prev));
                if (prev_frame != nullptr && IsBoundaryFrame<FrameKind::INTERPRETER>(prev_frame)) {
                    return FrameAccessor(CreateCFrameForC2IBridge<false>(prev_frame));
                }
                return FrameAccessor(prev_frame);
            }
            case FrameBridgeKind::BYPASS: {
                auto prev_frame = reinterpret_cast<Frame *>(GetPrevFromBoundary<FrameKind::COMPILER>(prev));
                if (prev_frame != nullptr && IsBoundaryFrame<FrameKind::INTERPRETER>(prev_frame)) {
                    return FrameAccessor(CreateCFrameForC2IBridge<false>(prev_frame));
                }
                return FrameAccessor(
                    CFrameType(reinterpret_cast<SlotType *>(GetPrevFromBoundary<FrameKind::COMPILER>(prev))));
            }
            default:
                return FrameAccessor(CFrameType(reinterpret_cast<SlotType *>(prev)));
        }
    } else {
        auto prev = GetIFrame()->GetPrevFrame();
        if (prev == nullptr) {
            return FrameAccessor(nullptr);
        }
        if (IsBoundaryFrame<FrameKind::INTERPRETER>(prev)) {
            auto bp = GetPrevFromBoundary<FrameKind::INTERPRETER>(prev);
            if (GetBoundaryFrameMethod<FrameKind::COMPILER>(bp) == BYPASS) {
                return FrameAccessor(CreateCFrame(GetPrevFromBoundary<FrameKind::COMPILER>(bp),
                                                  GetReturnAddressFromBoundary<FrameKind::COMPILER>(bp),
                                                  GetCalleeStackFromBoundary<FrameKind::COMPILER>(bp)));
            }
            return FrameAccessor(CreateCFrameForC2IBridge<false>(prev));
        }
        return FrameAccessor(reinterpret_cast<Frame *>(prev));
    }
}

FrameKind StackWalker::GetPreviousFrameKind() const
{
    if (IsCFrame()) {
        auto prev = GetCFrame().GetPrevFrame();
        if (prev == nullptr) {
            return FrameKind::NONE;
        }
        if (IsBoundaryFrame<FrameKind::COMPILER>(prev)) {
            return FrameKind::INTERPRETER;
        }
        return FrameKind::COMPILER;
    }
    auto prev = GetIFrame()->GetPrevFrame();
    if (prev == nullptr) {
        return FrameKind::NONE;
    }
    if (IsBoundaryFrame<FrameKind::INTERPRETER>(prev)) {
        return FrameKind::COMPILER;
    }
    return FrameKind::INTERPRETER;
}

bool StackWalker::IsCompilerBoundFrame(SlotType *prev)
{
    if (IsBoundaryFrame<FrameKind::COMPILER>(prev)) {
        return true;
    }
    if (GetBoundaryFrameMethod<FrameKind::COMPILER>(prev) == FrameBridgeKind::BYPASS) {
        auto prev_frame = reinterpret_cast<Frame *>(GetPrevFromBoundary<FrameKind::COMPILER>(prev));
        // Case for clinit:
        // Compiled code -> C2I -> InitializeClass -> call clinit -> I2C -> compiled code for clinit
        if (prev_frame != nullptr && IsBoundaryFrame<FrameKind::INTERPRETER>(prev_frame)) {
            return true;
        }
    }

    return false;
}

Frame *StackWalker::ConvertToIFrame([[maybe_unused]] FrameKind *prev_frame_kind,
                                    [[maybe_unused]] uint32_t *num_inlined_methods)
{
    if (!IsCFrame()) {
        return GetIFrame();
    }

    UNREACHABLE();
}

bool StackWalker::IsDynamicMethod() const
{
    // Dynamic method may have no class
    return GetMethod()->GetClass() == nullptr ||
           Runtime::GetCurrent()->GetLanguageContext(*GetMethod()).IsDynamicLanguage();
}

void StackWalker::Verify()
{
#ifndef NDEBUG
    for (; HasFrame(); NextFrame()) {
        ASSERT(GetMethod() != nullptr);
        [[maybe_unused]] bool is_dynamic = IsDynamicMethod();
        IterateVRegsWithInfo([this, is_dynamic]([[maybe_unused]] const auto &reg_info, const auto &vreg) {
            if (vreg.HasObject()) {
                // In dynamic methods all reg_infos are generic values.
                // Use Frame::VRegister::HasObject() to detect objects
                ASSERT(is_dynamic || reg_info.IsObject());
                if (ObjectHeader *object = vreg.GetReference(); object != nullptr && IsCFrame()) {
                    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_NESTING_LEVEL)
                    if (auto *bcls = object->ClassAddr<BaseClass>(); bcls != nullptr && !(bcls->IsDynamicClass())) {
                        auto cls = static_cast<Class *>(bcls);
                        cls->GetName();
                    }
                }
            } else {
                ASSERT(!reg_info.IsObject());
                vreg.GetLong();
            }
            return true;
        });

        if (IsCFrame()) {
            IterateObjects([](const auto &vreg) {
                ASSERT(vreg.HasObject());
                if (ObjectHeader *object = vreg.GetReference(); object != nullptr) {
                    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_NESTING_LEVEL)
                    if (auto *bcls = object->ClassAddr<BaseClass>(); bcls != nullptr && !(bcls->IsDynamicClass())) {
                        auto cls = static_cast<Class *>(bcls);
                        cls->GetName();
                    }
                }
                return true;
            });
        }
    }
#endif  // ifndef NDEBUG
}

// Dump function change StackWalker object-state, that's why it may be called only
// with rvalue reference.
// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_COMMENT_ADDSPASE,C_RULE_ID_FUNCTION_SIZE)
void StackWalker::Dump(std::ostream &os, bool print_vregs /* = false */) &&
{
    [[maybe_unused]] static constexpr size_t WIDTH_INDEX = 4;
    [[maybe_unused]] static constexpr size_t WIDTH_REG = 4;
    [[maybe_unused]] static constexpr size_t WIDTH_FRAME = 8;
    [[maybe_unused]] static constexpr size_t WIDTH_LOCATION = 12;
    [[maybe_unused]] static constexpr size_t WIDTH_TYPE = 20;

    size_t frame_index = 0;
    os << "Panda call stack:\n";
    for (; HasFrame(); NextFrame()) {
        os << std::setw(WIDTH_INDEX) << std::setfill(' ') << std::right << std::dec << frame_index << ": "
           << std::setfill('0');
        os << std::setw(WIDTH_FRAME) << std::hex;
        os << (IsCFrame() ? reinterpret_cast<Frame *>(GetCFrame().GetFrameOrigin()) : GetIFrame()) << " in ";
        DumpFrame(os);
        os << std::endl;
        if (print_vregs) {
            IterateVRegsWithInfo([this, &os](auto reg_info, auto vreg) {
                os << "     " << std::setw(WIDTH_REG) << std::setfill(' ') << std::right
                   << (reg_info.IsAccumulator() ? "acc" : (std::string("v") + std::to_string(reg_info.GetIndex())));
                os << " = " << std::left;
                os << std::setw(WIDTH_TYPE) << std::setfill(' ');
                switch (reg_info.GetType()) {
                    case VRegInfo::Type::INT64:
                    case VRegInfo::Type::INT32:
                        os << std::dec << vreg.GetLong();
                        break;
                    case VRegInfo::Type::FLOAT64:
                    case VRegInfo::Type::FLOAT32:
                        os << vreg.GetDouble();
                        break;
                    case VRegInfo::Type::BOOL:
                        os << (vreg.Get() ? "true" : "false");
                        break;
                    case VRegInfo::Type::OBJECT:
                        os << vreg.GetReference();
                        break;
                    case VRegInfo::Type::UNDEFINED:
                        os << "undefined";
                        break;
                    default:
                        os << "unknown";
                        break;
                }
                os << std::setw(WIDTH_LOCATION) << std::setfill(' ') << reg_info.GetTypeString();  // NOLINT
                if (IsCFrame()) {
                    os << reg_info.GetLocationString() << ":" << std::dec << helpers::ToSigned(reg_info.GetValue());
                } else {
                    os << '-';
                }
                os << std::endl;
                return true;
            });
        }
        frame_index++;
    }
}

void StackWalker::DumpFrame(std::ostream &os)
{
    auto method = GetMethod();
    os << method->GetFullName();
    if (IsCFrame()) {
        if (GetCFrame().IsJni()) {
            os << " (native)";
        } else {
            os << " (compiled" << (GetCFrame().IsOsr() ? "/osr" : "") << ": npc=" << GetNativePc()
               << (IsInlined() ? ", inlined) " : ") ");
        }
    } else {
        os << " (managed)";
    }
}

}  // namespace panda
