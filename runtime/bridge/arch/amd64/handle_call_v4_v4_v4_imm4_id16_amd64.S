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

    // call.acc || call.virt.acc
.Lloop_handle_call_acc:

    movzbl (%rbx), %r9d
    movq %r9, %rcx
    addq $1, %rbx  //advance insn_ptr
    movzbl (%rbx), %r9d
    shlq $8, %r9
    orq %r9, %rcx  // rcx now contains v4_v4_v4_imm4
    addq $1, %rbx  // advance insn_ptr
    movq %rbx, -(CFRAME_CALLEE_REGS_START_SLOT * 8 + 8)(%rbp)  // we need one more register, borrow rbx for tmp usage
    movq %rcx, %rbx
    shrq $12, %rbx
    andq $15, %rbx // rbx now holds acc position

.Lacc_loop:
    NEXT_SHORTY %r11d
    cmpl $0, %r11d
    je .Lacc_exit

    cmpq $0, %rbx
    jne .Lacc_vreg

    leaq (FRAME_ACC_OFFSET - FRAME_VREGS_OFFSET)(%r12), %r13
    movq FRAME_VREGISTER_TAG_OFFSET(%r13), %r15
    movq (%r13), %r13
    PUSH_ARG %r13, %r15, %rdx, %rdi, %rsi, %r9, .Lacc_1
.Lacc_1:
    subq $1, %rbx
    jmp .Lacc_loop

.Lacc_vreg:
    // handle arg index
    movl %ecx, %r15d
    andl $0xF, %r15d
    shlq $4, %r15
    shrq $4, %rcx  // prepare rcx for next index read
    movq (%r12,%r15), %r13
    movq FRAME_VREGISTER_TAG_OFFSET(%r12,%r15), %r15
    PUSH_ARG %r13, %r15, %rdx, %rdi, %rsi, %r9, .Lacc_2
.Lacc_2:
    subq $1, %rbx
    jmp .Lacc_loop

.Lacc_exit:
    movq -(CFRAME_CALLEE_REGS_START_SLOT * 8 + 8)(%rbp), %rbx
    jmp .Lload_reg_args
