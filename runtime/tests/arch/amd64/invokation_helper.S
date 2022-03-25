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

// T InvokeHelper(const uint8_t* gprs, const uint8_t* fprs, const uint8_t* stack, size_t stack_size, ManagedThread* thread);
.global InvokeHelper
.type InvokeHelper, %function
InvokeHelper:
    CFI_STARTPROC
    CFI_DEF_CFA(rsp, 8)

    // regs:
    // rdi - gprs, rsi - fprs, rdx - stack, rcx - stack_size, r8 - thread

    // setup stack frame
    pushq %rbp
    CFI_ADJUST_CFA_OFFSET(8)
    CFI_REL_OFFSET(rbp, 0)

    movq %rsp, %rbp
    CFI_DEF_CFA_REGISTER(rbp)

    pushq %THREAD_REG
    CFI_REL_OFFSET(THREAD_REG, -(1 * 8))
    pushq %r14
    CFI_REL_OFFSET(r14, -(2 * 8))

    movq %r8, %THREAD_REG

    // load fp regs
    movsd (%rsi), %xmm0
    movsd 8(%rsi), %xmm1
    movsd 16(%rsi), %xmm2
    movsd 24(%rsi), %xmm3
    movsd 32(%rsi), %xmm4
    movsd 40(%rsi), %xmm5
    movsd 48(%rsi), %xmm6
    movsd 56(%rsi), %xmm7

    // check stack arguments
    test %rcx, %rcx
    je 2f
    // reserve stack space
    movq %rcx, %r8
    shlq $3, %r8
    sub %r8, %rsp
    movq %rsp, %r8
    // push stack args
1:  test %rcx, %rcx
    je 2f
    movq (%rdx), %r14
    movq %r14, (%r8)
    addq $8, %rdx
    addq $8, %r8
    sub $1, %rcx
    jmp 1b

2:  // load gprs
    movq %rdi, %r14
    movq (%r14), %rdi
    movq 8(%r14), %rsi
    movq 16(%r14), %rdx
    movq 24(%r14), %rcx
    movq 32(%r14), %r8
    movq 40(%r14), %r9

    // invoke
    movq METHOD_COMPILED_ENTRY_POINT_OFFSET(%rdi), %rax
    callq *%rax

    // rax, rdx contain the result
    // restore regs and pop frame
    leaq -16(%rbp), %rsp
    popq %r14
    CFI_RESTORE(r14)
    popq %THREAD_REG
    CFI_RESTORE(THREAD_REG)
    popq %rbp
    CFI_RESTORE(rbp)
    CFI_DEF_CFA(rsp, (1 * 8))
    retq
    CFI_ENDPROC
