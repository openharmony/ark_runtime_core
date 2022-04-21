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

#ifndef PANDA_RUNTIME_ARCH_ASM_SUPPORT_H_
#define PANDA_RUNTIME_ARCH_ASM_SUPPORT_H_

#include "asm_defines.h"
#include "shorty_values.h"

#ifdef PANDA_TARGET_ARM32
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define THREAD_REG r10
#elif defined(PANDA_TARGET_ARM64)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define THREAD_REG x28
#elif defined(PANDA_TARGET_X86)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define THREAD_REG gs
#elif defined(PANDA_TARGET_AMD64)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define THREAD_REG r15
#else
#error "Unsupported target"
#endif

// clang-format off

#ifndef PANDA_TARGET_WINDOWS
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define TYPE_FUNCTION(name) .type name, %function
#else
#define TYPE_FUNCTION(name)
#endif

#ifndef NDEBUG

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CFI_STARTPROC .cfi_startproc // CODECHECK-NOLINT(C_RULE_ID_HORIZON_SPACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CFI_ENDPROC .cfi_endproc // CODECHECK-NOLINT(C_RULE_ID_HORIZON_SPACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CFI_DEF_CFA(reg, offset) .cfi_def_cfa reg, (offset)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CFI_ADJUST_CFA_OFFSET(offset) .cfi_adjust_cfa_offset (offset)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CFI_DEF_CFA_REGISTER(reg) .cfi_def_cfa_register reg
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CFI_REL_OFFSET(reg, offset) .cfi_rel_offset reg, (offset)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CFI_OFFSET(reg, offset) .cfi_offset reg, (offset)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CFI_REMEMBER_STATE .cfi_remember_state // CODECHECK-NOLINT(C_RULE_ID_HORIZON_SPACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CFI_RESTORE_STATE .cfi_restore_state // CODECHECK-NOLINT(C_RULE_ID_HORIZON_SPACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CFI_RESTORE(reg) .cfi_restore reg
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CFI_REGISTER(reg, old_reg) .cfi_register reg, old_reg

#else

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CFI_STARTPROC
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CFI_ENDPROC
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CFI_DEF_CFA(reg, offset)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CFI_ADJUST_CFA_OFFSET(offset)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CFI_DEF_CFA_REGISTER(reg)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CFI_REL_OFFSET(reg, offset)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CFI_OFFSET(reg, offset)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CFI_REMEMBER_STATE
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CFI_RESTORE_STATE
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CFI_RESTORE(reg)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CFI_REGISTER(reg, old_reg)

#endif

// clang-format on

#endif  // PANDA_RUNTIME_ARCH_ASM_SUPPORT_H_
