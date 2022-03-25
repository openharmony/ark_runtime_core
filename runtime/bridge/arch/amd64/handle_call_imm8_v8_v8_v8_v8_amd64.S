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

// regs set as follow
// %rdi, %rsi, %rdx, %rcx, %r8, %r9 - args
// %rax - insn_ptr, %rbx - frame.vregs, %r12 - method, %r13, %r14, %r15 - temp

// Handler of CALL3DYN_IMM8_V8_V8_V8_V8 (builtin.quin5)

    // get number of args in %esi
    movl $5, %esi // new.target, this, arg1, arg2, arg3

    // get method in %rdi
    movq %r12, %rdi

    // get function object and tag in %rdx, %rcx
    movzbl 1(%rax), %r15d
    shlq $4, %r15
    addq %rbx, %r15
    movq (%r15), %rdx
    movq 8(%r15), %rcx

    // get undefined in %r8, %r9 (new.target) and stack (this)
    xorl %r8d, %r8d
    xorl %r9d, %r9d

    subq $(4 * FRAME_VREGISTER_SIZE), %rsp

    movq $0, (%rsp)
    movq $0, 8(%rsp)

    // get arg1
    movzbl 2(%rax), %r15d
    shlq $4, %r15
    addq %rbx, %r15
    movq (%r15), %r10
    movq 8(%r15), %r11
    movq %r10, 16(%rsp)
    movq %r11, 24(%rsp)

    // get arg2
    movzbl 3(%rax), %r15d
    shlq $4, %r15
    addq %rbx, %r15
    movq (%r15), %r10
    movq 8(%r15), %r11
    movq %r10, 32(%rsp)
    movq %r11, 40(%rsp)

    // get arg3
    movzbl 4(%rax), %r15d
    shlq $4, %r15
    addq %rbx, %r15
    movq (%r15), %r10
    movq 8(%r15), %r11
    movq %r10, 48(%rsp)
    movq %r11, 56(%rsp)

    jmp .Linvoke
