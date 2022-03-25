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

#define CALLEE_SAVED_GP_REGS_COUNT 10

.extern printf

.macro PUSH_CALLER_REGS
    stp x0, x1, [sp, #-16]!
    stp x2, x3, [sp, #-16]!
    stp x4, x5, [sp, #-16]!
    stp x6, x7, [sp, #-16]!
    stp x8, x9, [sp, #-16]!
    stp x10, x11, [sp, #-16]!
    stp x12, x13, [sp, #-16]!
    stp x14, x15, [sp, #-16]!
.endm

.macro POP_CALLER_REGS
    ldp x14, x15, [sp], #16
    ldp x12, x13, [sp], #16
    ldp x10, x11, [sp], #16
    ldp x8, x9, [sp], #16
    ldp x6, x7, [sp], #16
    ldp x4, x5, [sp], #16
    ldp x2, x3, [sp], #16
    ldp x0, x1, [sp], #16
.endm

.macro PUSH_CALLER_VREGS
    stp d0, d1, [sp, #-16]!
    stp d2, d3, [sp, #-16]!
    stp d4, d5, [sp, #-16]!
    stp d6, d7, [sp, #-16]!
    stp d16, d17, [sp, #-16]!
    stp d18, d19, [sp, #-16]!
    stp d20, d21, [sp, #-16]!
    stp d22, d23, [sp, #-16]!
    stp d24, d25, [sp, #-16]!
    stp d26, d27, [sp, #-16]!
    stp d28, d29, [sp, #-16]!
    stp d30, d31, [sp, #-16]!
.endm

.macro POP_CALLER_VREGS
    ldp d30, d31, [sp], #16
    ldp d28, d29, [sp], #16
    ldp d26, d27, [sp], #16
    ldp d24, d25, [sp], #16
    ldp d22, d23, [sp], #16
    ldp d20, d21, [sp], #16
    ldp d18, d19, [sp], #16
    ldp d16, d17, [sp], #16
    ldp d6, d7, [sp], #16
    ldp d4, d5, [sp], #16
    ldp d2, d3, [sp], #16
    ldp d0, d1, [sp], #16
.endm

.macro PUSH_ARGS_REGS
    stp x6, x7, [sp, #-16]!
    stp x4, x5, [sp, #-16]!
    stp x2, x3, [sp, #-16]!
    stp x0, x1, [sp, #-16]!
.endm

.macro POP_ARGS_REGS
    ldp x0, x1, [sp], #16
    ldp x2, x3, [sp], #16
    ldp x4, x5, [sp], #16
    ldp x6, x7, [sp], #16
.endm

.macro PUSH_ARGS_VREGS
    stp d0, d1, [sp, #-16]!
    stp d2, d3, [sp, #-16]!
    stp d4, d5, [sp, #-16]!
    stp d6, d7, [sp, #-16]!
.endm

.macro POP_ARGS_VREGS
    ldp d6, d7, [sp], #16
    ldp d4, d5, [sp], #16
    ldp d2, d3, [sp], #16
    ldp d0, d1, [sp], #16
.endm

.macro PUSH_CALLEE_REGS base_reg
    stp x27, x28, [\base_reg, #-16]!
    stp x25, x26, [\base_reg, #-16]!
    stp x23, x24, [\base_reg, #-16]!
    stp x21, x22, [\base_reg, #-16]!
    stp x19, x20, [\base_reg, #-16]!
    stp d14, d15, [\base_reg, #-16]!
    stp d12, d13, [\base_reg, #-16]!
    stp d10, d11, [\base_reg, #-16]!
    stp d8, d9, [\base_reg, #-16]!
.endm

.macro BEGIN_CALL
    PUSH_CALLER_REGS
.endm

.macro LARG0 v
    ldr x0, =\v
.endm

.macro ARG0 v
    mov x0, \v
.endm

.macro ARG1 v
    mov x1, \v
.endm

.macro ARG2 v
    mov x2, \v
.endm

.macro END_CALL func
    bl \func
    POP_CALLER_REGS
.endm

.macro LOG3 fmt, arg1, arg2
    BEGIN_CALL
        LARG0 \fmt
        ARG1 \arg1
        ARG2 \arg2
    END_CALL LogEntrypoint
.endm

.macro ABORT
    brk 0
.endm

.macro ASSERT_REGS_CMP reg1, reg2, pred
#ifndef NDEBUG
    cmp \reg1, \reg2
    b\pred 1f
    ABORT
1:
#endif
.endm
