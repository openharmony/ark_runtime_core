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

#ifndef PANDA_LIBPANDABASE_UTILS_ARCH_H_
#define PANDA_LIBPANDABASE_UTILS_ARCH_H_

#include "macros.h"
#include "utils/math_helpers.h"
#include "concepts.h"

namespace panda {

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define ARCH_LIST(D) \
    D(NONE)          \
    D(AARCH32)       \
    D(AARCH64)       \
    D(X86)           \
    D(X86_64)

enum class Arch {
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DEF(v) v,
    ARCH_LIST(DEF)
#undef DEF
};

template <Arch arch>
struct ArchTraits;

template <>
struct ArchTraits<Arch::AARCH32> {
    static constexpr size_t CODE_ALIGNMENT = 8;
    static constexpr size_t INSTRUCTION_ALIGNMENT = 2;
    static constexpr size_t INSTRUCTION_MAX_SIZE_BITS = 32;
    static constexpr size_t POINTER_SIZE = 4;
    static constexpr bool IS_64_BITS = false;
    static constexpr size_t THREAD_REG = 10;
    static constexpr size_t CALLER_REG_MASK = 0x0000000f;
    static constexpr size_t CALLER_FP_REG_MASK = 0x0000ffff;
    static constexpr size_t CALLEE_REG_MASK = 0x000007f0;
    static constexpr size_t CALLEE_FP_REG_MASK = 0x0000ff00;
    static constexpr bool SUPPORT_OSR = false;
    static constexpr bool SUPPORT_DEOPTIMIZATION = true;
    using WordType = uint32_t;
};

template <>
struct ArchTraits<Arch::AARCH64> {
    static constexpr size_t CODE_ALIGNMENT = 16;
    static constexpr size_t INSTRUCTION_ALIGNMENT = 4;
    static constexpr size_t INSTRUCTION_MAX_SIZE_BITS = 32;
    static constexpr size_t POINTER_SIZE = 8;
    static constexpr bool IS_64_BITS = true;
    static constexpr size_t THREAD_REG = 28;
    static constexpr size_t CALLER_REG_MASK = 0x0007ffff;
    static constexpr size_t CALLER_FP_REG_MASK = 0xffff00ff;
    static constexpr size_t CALLEE_REG_MASK = 0x1ff80000;
    static constexpr size_t CALLEE_FP_REG_MASK = 0x0000ff00;
    static constexpr bool SUPPORT_OSR = true;
    static constexpr bool SUPPORT_DEOPTIMIZATION = true;
    using WordType = uint64_t;
};

template <>
struct ArchTraits<Arch::X86> {
    static constexpr size_t CODE_ALIGNMENT = 16;
    static constexpr size_t INSTRUCTION_ALIGNMENT = 1;
    static constexpr size_t INSTRUCTION_MAX_SIZE_BITS = 8;
    static constexpr size_t POINTER_SIZE = 4;
    static constexpr bool IS_64_BITS = false;
    static constexpr size_t THREAD_REG = 0;
    static constexpr size_t CALLER_REG_MASK = 0x00000000;
    static constexpr size_t CALLER_FP_REG_MASK = 0x00000000;
    static constexpr size_t CALLEE_REG_MASK = 0x00000001;
    static constexpr size_t CALLEE_FP_REG_MASK = 0x00000001;
    static constexpr bool SUPPORT_OSR = false;
    static constexpr bool SUPPORT_DEOPTIMIZATION = false;
    using WordType = uint32_t;
};

template <>
struct ArchTraits<Arch::X86_64> {
    static constexpr size_t CODE_ALIGNMENT = 16;
    static constexpr size_t INSTRUCTION_ALIGNMENT = 1;
    static constexpr size_t INSTRUCTION_MAX_SIZE_BITS = 8;
    static constexpr size_t POINTER_SIZE = 8;
    static constexpr bool IS_64_BITS = true;
    static constexpr size_t THREAD_REG = 15;               // %r15
    static constexpr size_t CALLER_REG_MASK = 0x000001FF;  // %rax, %rcx, %rdx, %rsi, %rdi, %r8, %r9, %r10, %r11
    static constexpr size_t CALLER_FP_REG_MASK = 0x0000FFFF;
    static constexpr size_t CALLEE_REG_MASK = 0x0000F800;  // %rbx, %r12, %r13, %r14, %r15
    static constexpr size_t CALLEE_FP_REG_MASK = 0x00000000;
    static constexpr bool SUPPORT_OSR = false;
    static constexpr bool SUPPORT_DEOPTIMIZATION = true;
    using WordType = uint64_t;
};

template <>
struct ArchTraits<Arch::NONE> {
    static constexpr size_t CODE_ALIGNMENT = 0;
    static constexpr size_t INSTRUCTION_ALIGNMENT = 0;
    static constexpr size_t INSTRUCTION_MAX_SIZE_BITS = 1;
    static constexpr size_t POINTER_SIZE = 0;
    static constexpr bool IS_64_BITS = false;
    static constexpr size_t CALLEE_REG_MASK = 0x00000000;
    static constexpr size_t CALLEE_FP_REG_MASK = 0x00000000;
    using WordType = void;
};

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage,-warnings-as-errors)
#define DEF_ARCH_PROPERTY_GETTER(func_name, property)                                                 \
    constexpr std::remove_const_t<decltype(ArchTraits<Arch::AARCH64>::property)> func_name(Arch arch) \
    {                                                                                                 \
        ASSERT(arch != Arch::NONE);                                                                   \
        if (arch == Arch::X86) {                                                                      \
            return ArchTraits<Arch::X86>::property;                                                   \
        }                                                                                             \
        if (arch == Arch::X86_64) {                                                                   \
            return ArchTraits<Arch::X86_64>::property;                                                \
        }                                                                                             \
        if (arch == Arch::AARCH32) {                                                                  \
            return ArchTraits<Arch::AARCH32>::property;                                               \
        }                                                                                             \
        if (arch == Arch::AARCH64) {                                                                  \
            return ArchTraits<Arch::AARCH64>::property;                                               \
        }                                                                                             \
        UNREACHABLE();                                                                                \
    }

DEF_ARCH_PROPERTY_GETTER(DoesArchSupportDeoptimization, SUPPORT_DEOPTIMIZATION)
DEF_ARCH_PROPERTY_GETTER(GetCodeAlignment, CODE_ALIGNMENT)
DEF_ARCH_PROPERTY_GETTER(GetInstructionAlignment, INSTRUCTION_ALIGNMENT)
DEF_ARCH_PROPERTY_GETTER(GetInstructionSizeBits, INSTRUCTION_MAX_SIZE_BITS)
DEF_ARCH_PROPERTY_GETTER(Is64BitsArch, IS_64_BITS)
DEF_ARCH_PROPERTY_GETTER(PointerSize, POINTER_SIZE)
DEF_ARCH_PROPERTY_GETTER(GetThreadReg, THREAD_REG)

constexpr const char *GetArchString(Arch arch)
{
    switch (arch) {
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DEF(v)    \
    case Arch::v: \
        return #v;
        ARCH_LIST(DEF)
#undef DEF
        default:
            UNREACHABLE();
            return "NONE";
    }
}

inline constexpr size_t GetCallerRegsMask(Arch arch, bool is_fp)
{
    switch (arch) {
        case Arch::AARCH32:
            return is_fp ? ArchTraits<Arch::AARCH32>::CALLER_FP_REG_MASK : ArchTraits<Arch::AARCH32>::CALLER_REG_MASK;
        case Arch::AARCH64:
            return is_fp ? ArchTraits<Arch::AARCH64>::CALLER_FP_REG_MASK : ArchTraits<Arch::AARCH64>::CALLER_REG_MASK;
        case Arch::X86:
            return is_fp ? ArchTraits<Arch::X86>::CALLER_FP_REG_MASK : ArchTraits<Arch::X86>::CALLER_REG_MASK;
        case Arch::X86_64:
            return is_fp ? ArchTraits<Arch::X86_64>::CALLER_FP_REG_MASK : ArchTraits<Arch::X86_64>::CALLER_REG_MASK;
        default:
            UNREACHABLE();
            return 0;
    }
}

inline constexpr size_t GetCalleeRegsMask(Arch arch, bool is_fp)
{
    switch (arch) {
        case Arch::AARCH32:
            return is_fp ? ArchTraits<Arch::AARCH32>::CALLEE_FP_REG_MASK : ArchTraits<Arch::AARCH32>::CALLEE_REG_MASK;
        case Arch::AARCH64:
            return is_fp ? ArchTraits<Arch::AARCH64>::CALLEE_FP_REG_MASK : ArchTraits<Arch::AARCH64>::CALLEE_REG_MASK;
        case Arch::X86:
            return is_fp ? ArchTraits<Arch::X86>::CALLEE_FP_REG_MASK : ArchTraits<Arch::X86>::CALLEE_REG_MASK;
        case Arch::X86_64:
            return is_fp ? ArchTraits<Arch::X86_64>::CALLEE_FP_REG_MASK : ArchTraits<Arch::X86_64>::CALLEE_REG_MASK;
        default:
            UNREACHABLE();
            return 0;
    }
}

static constexpr size_t LAST_BIT_IN_MASK = 63;

inline constexpr size_t GetFirstCalleeReg(Arch arch, bool is_fp)
{
    if (arch == Arch::X86_64 && is_fp) {
        // in amd64 xmm regs are volatile, so we return first reg (1) > last reg(0) to imitate empty list;
        // also number of registers = last reg (0) - first reg (1) + 1 == 0
        return 1;
    }

    size_t mask = GetCalleeRegsMask(arch, is_fp);
    return mask == 0 ? 0 : helpers::math::Ctz(mask);
}

inline constexpr size_t GetLastCalleeReg(Arch arch, bool is_fp)
{
    if (arch == Arch::X86_64 && is_fp) {
        return 0;
    }

    size_t mask = GetCalleeRegsMask(arch, is_fp);
    constexpr size_t BIT32 = 32;
    return BIT32 - 1 - helpers::math::Clz(mask);
}

inline constexpr size_t GetCalleeRegsCount(Arch arch, bool is_fp)
{
    return (GetLastCalleeReg(arch, is_fp) + 1) - GetFirstCalleeReg(arch, is_fp);
}

inline constexpr size_t GetFirstCallerReg(Arch arch, bool is_fp)
{
    size_t mask = GetCallerRegsMask(arch, is_fp);
    return mask == 0 ? 0 : helpers::math::Ctz(mask);
}

inline constexpr size_t GetLastCallerReg(Arch arch, bool is_fp)
{
    size_t mask = GetCallerRegsMask(arch, is_fp);
    constexpr size_t BIT32 = 32;
    return BIT32 - 1 - helpers::math::Clz(mask);
}

inline constexpr size_t GetCallerRegsCount(Arch arch, bool is_fp)
{
    return GetLastCallerReg(arch, is_fp) - GetFirstCallerReg(arch, is_fp) + 1;
}

#ifdef PANDA_TARGET_ARM32
static constexpr Arch RUNTIME_ARCH = Arch::AARCH32;
#elif defined(PANDA_TARGET_ARM64)
static constexpr Arch RUNTIME_ARCH = Arch::AARCH64;
#elif defined(PANDA_TARGET_X86)
static constexpr Arch RUNTIME_ARCH = Arch::X86;
#elif defined(PANDA_TARGET_AMD64)
static constexpr Arch RUNTIME_ARCH = Arch::X86_64;
#else
static constexpr Arch RUNTIME_ARCH = Arch::NONE;
#endif

template <class String = std::string>
std::enable_if_t<is_stringable_v<String>, Arch> GetArchFromString(const String &str)
{
    if (str == "arm64") {
        return Arch::AARCH64;
    }
    if (str == "arm") {
        return Arch::AARCH32;
    }
    if (str == "x86") {
        return Arch::X86;
    }
    if (str == "x86_64") {
        return Arch::X86_64;
    }
    return Arch::NONE;
}

template <class String = std::string>
std::enable_if_t<is_stringable_v<String>, String> GetStringFromArch(const Arch &arch)
{
    if (arch == Arch::AARCH64) {
        return "arm64";
    }
    if (arch == Arch::AARCH32) {
        return "arm";
    }
    if (arch == Arch::X86) {
        return "x86";
    }
    if (arch == Arch::X86_64) {
        return "x86_64";
    }
    return "none";
}

}  // namespace panda

#endif  // PANDA_LIBPANDABASE_UTILS_ARCH_H_
