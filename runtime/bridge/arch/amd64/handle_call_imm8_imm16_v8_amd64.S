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

// regs set as follow
// %rdi, %rsi, %rdx, %rcx, %r8, %r9 - args
// %rax - insn_ptr, %rbx - frame.vregs, %r12 - method, %r13, %r14, %r15 - temp

// Handler of CALLIRANGEDYN_IMM8_IMM16_V8 and CALLITHISRANGEDYN_IMM8_IMM16_V8 (builtin.r2i)

    // get builtin_id in %r15
    movzbl (%rax), %r15d

    // get number of args in %esi
    movzwl 1(%rax), %esi
    addl $2, %esi // new.target, this

    // get method in %rdi
    movq %r12, %rdi

    //get function object and tag in %rdx, %rcx
    movzbl 3(%rax), %r14d
    shlq $4, %r14
    addq %r14, %rbx
    leal -1(%esi), %r14d // ignore 'new.target' since it is in registers
    movq (%rbx), %rdx
    movq 8(%rbx), %rcx
    addq $FRAME_VREGISTER_SIZE, %rbx

    // get undefined in %r8, %r9 (new.target)
    xorl %r8d, %r8d
    xorl %r9d, %r9d

    // get the rest arguments
    // setup regs as follow
    // %rdi, %rsi, %rdx, %rcx, %r8, %r9 - args, %rbx - frame.vregs + vreg_idx, %r14 - stack arg counter
    // reserve stack space
    // %r13 - args ptr
    movq %r14, %r12
    shlq $4, %r12
    subq %r12, %rsp
    movq %rsp, %r13

    // get this
    // set undefined to 'this'
    xorl %r10d, %r10d
    xorl %r11d, %r11d

    cmpl $4, %r15d // 4 - builtin_id of CALLITHISRANGEDYN_IMM8_IMM16_V8
    jne 2f

    addq $FRAME_VREGISTER_SIZE, %rsp
    addq $FRAME_VREGISTER_SIZE, %r13
    subl $1, %esi
    subl $1, %r14d

1:
    je .Linvoke
    movq (%rbx), %r10
    movq 8(%rbx), %r11
    addq $FRAME_VREGISTER_SIZE, %rbx

2:
    movq %r10, (%r13)
    movq %r11, 8(%r13)
    addq $FRAME_VREGISTER_SIZE, %r13
    subl $1, %r14d
    jmp 1b
