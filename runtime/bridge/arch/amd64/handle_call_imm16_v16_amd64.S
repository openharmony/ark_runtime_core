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

// handle calli.dyn.range
// regs set as follow
// %rax - insn_ptr, %rbx - frame.vregs, %r12 - method, %r13, %r14, %r15 - temp

    // load method* into %rdi
    movq %r12, %rdi

    // load num args into %rsi and function object into %rdx, %rcx
    movzwl (%rax), %esi
    movzwl 2(%rax), %r13d
    shll $4, %r13d
    addq %r13, %rbx
    movq (%rbx), %rdx
    movq FRAME_VREGISTER_TAG_OFFSET(%rbx), %rcx
    addq $FRAME_VREGISTER_SIZE, %rbx

    // setup regs as follow
    // rdi, rsi, rdx, rcx, r8, r9 - arguments, rax - arg counter, rbx - frame.vregs + vreg_idx, r12 - method,
    // r13, r14, r15 - temps
    movl %esi, %eax
    // load arguments into the registers
    cmpl $0, %eax
    je .Linvoke

    movq (%rbx), %r8
    movq FRAME_VREGISTER_TAG_OFFSET(%rbx), %r9
    addq $FRAME_VREGISTER_SIZE, %rbx
    subl $1, %eax
    je .Linvoke

    // reserve stack space for the arguments
    movl %eax, %r15d
    shll $4, %r15d
    subq %r15, %rsp

    // r15 - stack pointer
    movq %rsp, %r15
    // push the rest arguments to the stack

1:  movq (%rbx), %r13
    movq FRAME_VREGISTER_TAG_OFFSET(%rbx), %r14
    movq %r13, (%r15)
    movq %r14, FRAME_VREGISTER_TAG_OFFSET(%r15)
    addq $FRAME_VREGISTER_SIZE, %r15
    addq $FRAME_VREGISTER_SIZE, %rbx
    subl $1, %eax
    ja 1b

    jmp .Linvoke
