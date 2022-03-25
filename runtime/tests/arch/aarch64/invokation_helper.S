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

#include "arch/asm_support.h"

// DecodedTaggedValue InvokeEntryPointImpl(const uint8_t* gprs, const uint8_t* fprs, const uint8_t* stack, size_t stack_size, ManagedThread* thread);
.global InvokeHelper
.type InvokeHelper, %function
InvokeHelper:
    CFI_STARTPROC
    CFI_DEF_CFA(sp, 0)

    // setup regs as follow
    // x9 - gprs, x10 - fprs, x11 - stack args, x12 - number of stack args, x13 - thread
    mov x9, x0
    mov x10, x1
    mov x11, x2
    mov x12, x3
    mov x13, x4

    stp fp, lr, [sp, #-16]!
    CFI_ADJUST_CFA_OFFSET(2 * 8)
    CFI_REL_OFFSET(lr, 8)
    CFI_REL_OFFSET(fp, 0)
    mov fp, sp
    CFI_DEF_CFA_REGISTER(fp)
    str THREAD_REG, [sp, #-16]!
    CFI_REL_OFFSET(THREAD_REG, -(2 * 8))
    mov THREAD_REG, x13

    ldp x0, x1, [x9], #16
    ldp x2, x3, [x9], #16
    ldp x4, x5, [x9], #16
    ldp x6, x7, [x9], #16
    ldp d0, d1, [x10], #16
    ldp d2, d3, [x10], #16
    ldp d4, d5, [x10], #16
    ldp d6, d7, [x10], #16

    sub sp, sp, x12, lsl #3
    mov x14, sp
1:  cbz x12, .Linvoke
    ldp x9, x10, [x11], #16
    stp x9, x10, [x14], #16
    sub x12, x12, #2
    b 1b

.Linvoke:
    ldr lr, [x0, #METHOD_COMPILED_ENTRY_POINT_OFFSET]
    blr lr

    mov sp, fp
    ldr THREAD_REG, [sp, #-16]
    CFI_RESTORE(THREAD_REG)
    ldp fp, lr, [sp], #16
    CFI_RESTORE(lr)
    CFI_RESTORE(fp)
    CFI_DEF_CFA(sp, 0)
    ret
    CFI_ENDPROC
