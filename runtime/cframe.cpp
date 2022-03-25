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

#include "runtime/include/cframe.h"
#include "runtime/include/runtime.h"
#include "runtime/include/stack_walker.h"

namespace panda {

bool CFrame::IsNativeMethod() const
{
    return GetMethod()->IsNative();
}

void CFrame::SetVRegValue(const VRegInfo &vreg, uint64_t value, SlotType **callee_stack)
{
    auto location_value = static_cast<int>(vreg.GetValue());
    switch (vreg.GetLocation()) {
        case VRegInfo::Location::SLOT: {
            SetValueToSlot(location_value, value);
            if (!ArchTraits<ARCH>::IS_64_BITS && vreg.Has64BitValue()) {
                SetValueToSlot(location_value - 1, value >> BITS_PER_UINT32);
            }
            break;
        }
        case VRegInfo::Location::REGISTER:
        case VRegInfo::Location::FP_REGISTER: {
            bool is_fp = vreg.GetLocation() == VRegInfo::Location::FP_REGISTER;
            if ((GetCallerRegsMask(ARCH, is_fp) & (1U << vreg.GetValue())) != 0) {
                CFrameLayout fl(ARCH, 0);
                auto reg_num = location_value - GetFirstCallerReg(ARCH, is_fp);
                reg_num = fl.GetCallerLastSlot(is_fp) - reg_num;
                SetValueToSlot(reg_num, value);
                if (!ArchTraits<ARCH>::IS_64_BITS && vreg.Has64BitValue()) {
                    SetValueToSlot(static_cast<int>(reg_num) - 1, value >> BITS_PER_UINT32);
                }
                break;
            }
            WriteCalleeSavedRegister(location_value, value, is_fp, callee_stack);
            if constexpr (!ArchTraits<ARCH>::IS_64_BITS) {  // NOLINT
                if (vreg.Has64BitValue()) {
                    WriteCalleeSavedRegister(location_value + 1, value >> BITS_PER_UINT32, is_fp, callee_stack);
                }
                break;
            }
            break;
        }
        case VRegInfo::Location::CONSTANT:
            ASSERT(false && "Modifying constants is not permitted");  // NOLINT(misc-static-assert)
            break;
        default:
            UNREACHABLE();
    }
}

void CFrame::Dump(std::ostream &os, uint32_t max_slot)
{
    if (IsJni()) {
        os << "JNI CFRAME: fp=" << fp_ << std::endl;
        return;
    }
    auto spill_start_slot = GetCalleeRegsCount(ARCH, false) + GetCalleeRegsCount(ARCH, true) +
                            GetCallerRegsCount(ARCH, false) + GetCallerRegsCount(ARCH, true);
    max_slot = max_slot > spill_start_slot ? (max_slot - spill_start_slot) : 0;

    auto print_mem = [](std::ostream &stream, void *addr, std::string_view dscr, uintptr_t value) {
        constexpr size_t WIDTH = 16;
        stream << ' ' << addr << ": " << std::setw(WIDTH) << std::setfill(' ') << dscr << " 0x" << std::hex << value
               << std::dec << std::endl;
    };
    os << "****************************************\n";
    os << "* CFRAME: fp=" << fp_ << ", max_spill_slot=" << max_slot << '\n';
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    print_mem(os, fp_ - CFrameLayout::LrSlot::Start(), "lr", GetLr());
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    print_mem(os, fp_ - CFrameLayout::PrevFrameSlot::Start(), "prev", reinterpret_cast<uintptr_t>(GetPrevFrame()));
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    print_mem(os, fp_ - CFrameLayout::MethodSlot::Start(), "method", reinterpret_cast<uintptr_t>(GetMethod()));
    PandaString dscr;
    size_t slot = 0;
    DumpCalleeRegs(os, print_mem, &dscr, &slot);
    DumpCalleeFPRegs(os, print_mem, &dscr, &slot);
    DumpCallerRegs(os, print_mem, &dscr, &slot);
    DumpCallerFPRegs(os, print_mem, &dscr, &slot);
    DumpLocals(os, print_mem, &dscr, &slot, max_slot);

    os << "* CFRAME END\n";
    os << "****************************************\n";
}

void CFrame::DumpCalleeRegs(std::ostream &os, MemPrinter print_mem, PandaString *dscr, size_t *slot)
{
    os << " [Callee saved registers]\n";
    for (auto i = panda::helpers::ToSigned(GetLastCalleeReg(ARCH, false));
         i >= panda::helpers::ToSigned(GetFirstCalleeReg(ARCH, false)); i--, (*slot)++) {
        *dscr = "x" + ToPandaString(i) + ":" + ToPandaString(*slot);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        print_mem(os, fp_ - CFrameLayout::STACK_START_SLOT - *slot, *dscr, GetValueFromSlot(*slot));
    }
}

void CFrame::DumpCalleeFPRegs(std::ostream &os, MemPrinter print_mem, PandaString *dscr, size_t *slot)
{
    os << " [Callee saved FP registers]\n";
    for (auto i = panda::helpers::ToSigned(GetLastCalleeReg(ARCH, true));
         i >= panda::helpers::ToSigned(GetFirstCalleeReg(ARCH, true)); i--, (*slot)++) {
        *dscr = "d" + ToPandaString(i) + ":" + ToPandaString(*slot);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        print_mem(os, fp_ - CFrameLayout::STACK_START_SLOT - *slot, *dscr, GetValueFromSlot(*slot));
    }
}

void CFrame::DumpCallerRegs(std::ostream &os, MemPrinter print_mem, PandaString *dscr, size_t *slot)
{
    os << " [Caller saved registers] " << GetLastCallerReg(ARCH, false) << " " << GetFirstCallerReg(ARCH, false)
       << "\n";
    for (auto i = panda::helpers::ToSigned(GetLastCallerReg(ARCH, false));
         i >= panda::helpers::ToSigned(GetFirstCallerReg(ARCH, false)); i--, (*slot)++) {
        *dscr = "x" + ToPandaString(i) + ":" + ToPandaString(*slot);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        print_mem(os, fp_ - CFrameLayout::STACK_START_SLOT - *slot, *dscr, GetValueFromSlot(*slot));
    }
}

void CFrame::DumpCallerFPRegs(std::ostream &os, MemPrinter print_mem, PandaString *dscr, size_t *slot)
{
    os << " [Caller saved FP registers]\n";
    for (auto i = panda::helpers::ToSigned(GetLastCallerReg(ARCH, true));
         i >= panda::helpers::ToSigned(GetFirstCallerReg(ARCH, true)); i--, (*slot)++) {
        *dscr = "d" + ToPandaString(i) + ":" + ToPandaString(*slot);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        print_mem(os, fp_ - CFrameLayout::STACK_START_SLOT - *slot, *dscr, GetValueFromSlot(*slot));
    }
}

void CFrame::DumpLocals(std::ostream &os, MemPrinter print_mem, PandaString *dscr, size_t *slot, int32_t max_slot)
{
    os << " [Locals]\n";
    for (auto i = 0; i <= max_slot; i++, (*slot)++) {
        *dscr = "s" + ToPandaString(i) + ":" + ToPandaString(*slot);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        print_mem(os, fp_ - CFrameLayout::STACK_START_SLOT - *slot, *dscr, GetValueFromSlot(*slot));
    }
}

}  // namespace panda
