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

#ifndef PANDA_RUNTIME_INCLUDE_CFRAME_H_
#define PANDA_RUNTIME_INCLUDE_CFRAME_H_

#include <array>

#include "libpandabase/utils/cframe_layout.h"
#include "runtime/interpreter/frame.h"
#include "runtime/vreg_info.h"
#include "libpandabase/macros.h"
#include "libpandabase/utils/bit_field.h"
namespace panda {

class Method;

struct alignas(2U * alignof(uintptr_t)) C2IBridge {
    std::array<uintptr_t, 4> v_;  // 4: means array length
};

/**
 * CFrame layout (in descending order):
 *
 * -----------------
 *    LR
 *    PREV_FRAME   <-- `fp_` points here
 *    METHOD
 *    PROPERTIES: [0]: Should deoptimize (1 - deoptimize)
 *                [1..2]: Frame type - JNI, OSR or DEFAULT
 * -----------------
 *   LOCALS     several slots used for internal needs
 * -----------------
 *    R_N       = CALLEE SAVED REGS <--- `callee_stack_` of the caller's frame points here
 *    ...
 *    R_0
 * -----------------
 *    VR_N      = CALLEE SAVED FP REGS
 *    ...
 *    VR_0
 * -----------------
 *    R_N       = CALLER SAVED REGS
 *    ...
 *    R_0
 * -----------------
 *    VR_N      = CALLER SAVED FP REGS
 *    ...
 *    VR_0
 * -----------------
 *    SLOT_0    = SPILL/FILLS
 *    ...
 *    SLOT_N
 * -----------------
 */
class CFrame final {
public:
    enum FrameKind : uint8_t { DEFAULT = 0, OSR = 1, JNI = 2, LAST = JNI };
    static constexpr Arch ARCH = RUNTIME_ARCH;

    using SlotType = std::conditional_t<ArchTraits<ARCH>::IS_64_BITS, uint64_t, uint32_t>;

    using ShouldDeoptimizeFlag = BitField<bool, 0, 1>;
    using FrameKindField =
        ShouldDeoptimizeFlag::NextField<FrameKind, MinimumBitsToStore(static_cast<unsigned>(FrameKind::LAST))>;

public:
    explicit CFrame(void *frame_data) : fp_(reinterpret_cast<SlotType *>(frame_data)) {}
    ~CFrame() = default;

    DEFAULT_COPY_SEMANTIC(CFrame);
    DEFAULT_NOEXCEPT_MOVE_SEMANTIC(CFrame);

    bool IsOsr() const
    {
        return FrameKindField::Get(*GetPtr<SlotType>(CFrameLayout::FlagsSlot::Start())) == FrameKind::OSR;
    }

    bool IsJni() const
    {
        return FrameKindField::Get(*GetPtr<SlotType>(CFrameLayout::FlagsSlot::Start())) == FrameKind::JNI;
    }

    bool IsNativeMethod() const;

    bool ShouldDeoptimize() const
    {
        return ShouldDeoptimizeFlag::Get(*GetPtr<SlotType>(CFrameLayout::FlagsSlot::Start()));
    }

    void SetShouldDeoptimize(bool v)
    {
        ShouldDeoptimizeFlag::Set(v, GetPtr<SlotType>(CFrameLayout::FlagsSlot::Start()));
    }

    SlotType *GetPrevFrame()
    {
        return *GetPtr<SlotType *>(CFrameLayout::PrevFrameSlot::Start());
    }

    const SlotType *GetPrevFrame() const
    {
        return *GetPtr<SlotType *>(CFrameLayout::PrevFrameSlot::Start());
    }

    void SetPrevFrame(void *prev_frame)
    {
        *GetPtr<SlotType>(CFrameLayout::PrevFrameSlot::Start()) = bit_cast<SlotType>(prev_frame);
    }

    Method *GetMethod()
    {
        return *GetPtr<Method *>(CFrameLayout::MethodSlot::Start());
    }

    const Method *GetMethod() const
    {
        return const_cast<CFrame *>(this)->GetMethod();
    }

    void SetMethod(Method *method)
    {
        *GetPtr<SlotType>(CFrameLayout::MethodSlot::Start()) = bit_cast<SlotType>(method);
    }

    void *GetDeoptCodeEntry() const
    {
        return *GetPtr<void *>(CFrameData::Start());
    }

    /**
     * When method is deoptimizated due to it speculation fatal failure, its code entry is reset.
     * Therefore already executing methods will can't get proper code entry for stack walker,
     * thus we create this backup code entry.
     */
    void SetDeoptCodeEntry(const void *value)
    {
        *GetPtr<const void *>(CFrameData::Start()) = value;
    }

    void SetVRegValue(const VRegInfo &vreg, uint64_t value, SlotType **callee_stack);

    uintptr_t GetLr() const
    {
        return *GetPtr<uintptr_t>(CFrameLayout::LrSlot::Start());
    }

    SlotType *GetStackOrigin()
    {
        return GetPtr<SlotType>(CFrameLayout::STACK_START_SLOT);
    }
    const SlotType *GetStackOrigin() const
    {
        return GetPtr<const SlotType>(CFrameLayout::STACK_START_SLOT);
    }

    SlotType *GetCalleeSaveStack()
    {
        return GetPtr<SlotType>(CFrameLayout::CALLEE_REGS_START_SLOT - 1);
    }

    SlotType *GetCallerSaveStack()
    {
        return GetPtr<SlotType>(CFrameLayout::CALLER_REGS_START_SLOT - 1);
    }

    SlotType *GetFrameOrigin()
    {
        return fp_;
    }

    const SlotType *GetFrameOrigin() const
    {
        return fp_;
    }

    SlotType GetValueFromSlot(int slot) const
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return *(reinterpret_cast<const SlotType *>(GetStackOrigin()) - slot);
    }

    void SetValueToSlot(int slot, SlotType value)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        *(reinterpret_cast<SlotType *>(GetStackOrigin()) - slot) = value;
    }

    void Dump(std::ostream &os, uint32_t max_slot = 0);

private:
    SlotType ReadCalleeSavedRegister(size_t reg, bool is_fp, SlotType **callee_stack) const
    {
        ASSERT(reg >= GetFirstCalleeReg(ARCH, is_fp));
        ASSERT(reg <= GetLastCalleeReg(ARCH, is_fp));
        ASSERT(GetCalleeRegsCount(ARCH, is_fp) != 0);
        size_t start_slot = reg - GetFirstCalleeReg(ARCH, is_fp);
        if (is_fp) {
            start_slot += GetCalleeRegsCount(ARCH, false);
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        ASSERT(callee_stack[start_slot] != nullptr);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return *callee_stack[start_slot];
    }

    void WriteCalleeSavedRegister(size_t reg, SlotType value, bool is_fp, SlotType **callee_stack) const
    {
        ASSERT(reg >= GetFirstCalleeReg(ARCH, is_fp));
        ASSERT(reg <= GetLastCalleeReg(ARCH, is_fp));
        ASSERT(GetCalleeRegsCount(ARCH, is_fp) != 0);
        size_t start_slot = reg - GetFirstCalleeReg(ARCH, is_fp);
        if (is_fp) {
            start_slot += GetCalleeRegsCount(ARCH, false);
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        ASSERT(callee_stack[start_slot] != nullptr);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        *callee_stack[start_slot] = value;
    }

    template <typename T>
    T *GetPtr(ptrdiff_t slot)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return reinterpret_cast<T *>(GetFrameOrigin() - slot);
    }
    template <typename T>
    const T *GetPtr(ptrdiff_t slot) const
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return reinterpret_cast<const T *>(GetFrameOrigin() - slot);
    }

    using MemPrinter = void (*)(std::ostream &, void *, std::string_view, uintptr_t);
    void DumpCalleeRegs(std::ostream &os, MemPrinter print_mem, PandaString *dscr, size_t *slot);
    void DumpCalleeFPRegs(std::ostream &os, MemPrinter print_mem, PandaString *dscr, size_t *slot);
    void DumpCallerRegs(std::ostream &os, MemPrinter print_mem, PandaString *dscr, size_t *slot);
    void DumpCallerFPRegs(std::ostream &os, MemPrinter print_mem, PandaString *dscr, size_t *slot);
    void DumpLocals(std::ostream &os, MemPrinter print_mem, PandaString *dscr, size_t *slot, int32_t max_slot);

private:
    SlotType *fp_ {nullptr};
};

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_CFRAME_H_
