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

#ifndef PANDA_RUNTIME_INTERPRETER_ARCH_AARCH64_MACROS_H_
#define PANDA_RUNTIME_INTERPRETER_ARCH_AARCH64_MACROS_H_

#ifndef __clang_analyzer__
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define RESTORE_GLOBAL_REGS() asm volatile("" ::: "x20", "x21", "x22", "x23", "x24", "x28")
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define RESTORE_GLOBAL_REGS()
#endif

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DISPATCH(DISPATCH_TABLE, OPCODE, LABEL)                            \
    do {                                                                   \
        asm("ldr %[label], [%[dispatch_table], %w[opcode], uxtw #3]"       \
            : [label] "=r"(LABEL)                                          \
            : [dispatch_table] "r"(DISPATCH_TABLE), [opcode] "r"(OPCODE)); \
        goto *LABEL;                                                       \
    } while (0)

#endif  // PANDA_RUNTIME_INTERPRETER_ARCH_AARCH64_MACROS_H_
