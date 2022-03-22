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
    // r4 - gprs, r5 - fprs, r6 - stack args, r7 - number of stack args, r8 - thread

    push {r4 - r8, fp, lr}
    CFI_ADJUST_CFA_OFFSET(28)
    CFI_REL_OFFSET(lr, 24)
    CFI_REL_OFFSET(fp, 20)
    mov fp, sp
    CFI_DEF_CFA_REGISTER(fp)

    mov r4, r0
    mov r5, r1
    mov r6, r2
    mov r7, r3
    ldr r8, [sp, #28]

    sub sp, sp, #4
    push {THREAD_REG}
    CFI_ADJUST_CFA_OFFSET(8)
    CFI_REL_OFFSET(THREAD_REG, 0)
    mov THREAD_REG, r8

    ldmia r4!, {r0 - r3}
    vldmia r5!, {d0 - d7}

    sub sp, sp, r7, lsl #2
    bic sp, sp, #7
    mov r8, sp
1:  cmp r7, #0
    beq .Linvoke
    ldr r4, [r6], #4
    str r4, [r8], #4
    sub r7, r7, #1
    b 1b

.Linvoke:
    ldr lr, [r0, #METHOD_COMPILED_ENTRY_POINT_OFFSET]
    blx lr

    mov sp, fp
    ldr THREAD_REG, [sp, #-8]
    CFI_RESTORE(THREAD_REG)
    pop {r4 - r8, fp, lr}
    CFI_ADJUST_CFA_OFFSET(-28)
    CFI_RESTORE(lr)
    CFI_RESTORE(fp)
    bx lr
    CFI_ENDPROC
