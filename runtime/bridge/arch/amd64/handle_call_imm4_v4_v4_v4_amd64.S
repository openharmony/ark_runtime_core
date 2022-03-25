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

// handle calli.dyn.short || calli.dyn
// regs set as follow
// %rax - insn_ptr, %rbx - frame.vregs, %r12 - method, %r13, %r14, %r15 - temp

    // load method* into %rdi
    movq %r12, %rdi

    // load num args into %rsi and function object into %rdx, %rcx
    movzbl (%rax), %r13d
    addq $1, %rax // advance insn_ptr
    movl %r13d, %esi
    andl $0x0F, %esi
    andl $0xF0, %r13d
    movq (%rbx,%r13), %rdx
    movq FRAME_VREGISTER_TAG_OFFSET(%rbx,%r13), %rcx
    cmpl $0, %esi
    je .Linvoke

    // load the first argument into %r8, %r9
    movzbl (%rax), %r13d
    addq $1, %rax // advance insn_ptr
    movl %r13d, %r14d
    andl $0x0F, %r14d
    shll $4, %r14d
    movq (%rbx,%r14), %r8
    movq FRAME_VREGISTER_TAG_OFFSET(%rbx,%r14), %r9
    cmpl $1, %esi
    je .Linvoke

    // the rest arguments goes to the stack
    // reserve stack space
    leal -1(%esi), %r15d // %r15 contains number of arguments which goes to the stack
    shll $4, %r15d
    subq %r15, %rsp
    // set r15 to be stack pointer
    movq %rsp, %r15

    // load the second argument into the stack
    andl $0xF0, %r13d
    movq (%rbx,%r13), %r14
    movq %r14, (%r15)
    movq FRAME_VREGISTER_TAG_OFFSET(%rbx,%r13), %r14
    movq %r14, FRAME_VREGISTER_TAG_OFFSET(%r15)
    cmpl $2, %esi
    je .Linvoke

    // load the third argument into the stack
    // use %rax here because we don't need insn_ptr any more
    addq $FRAME_VREGISTER_SIZE, %r15
    movzbl (%rax), %r13d
    movl %r13d, %r14d
    andl $0x0F, %r14d
    shll $4, %r14d
    movq (%rbx,%r14), %rax
    movq %rax, (%r15)
    movq FRAME_VREGISTER_TAG_OFFSET(%rbx,%r14), %rax
    movq %rax, FRAME_VREGISTER_TAG_OFFSET(%r15)
    cmpl $3, %esi
    je .Linvoke

    // load the fourth argument into the stack
    addq $FRAME_VREGISTER_SIZE, %r15
    andl $0xF0, %r13d
    movq (%rbx,%r13), %r14
    movq %r14, (%r15)
    movq FRAME_VREGISTER_TAG_OFFSET(%rbx,%r13), %r14
    movq %r14, FRAME_VREGISTER_TAG_OFFSET(%r15)
    jmp .Linvoke
