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

    // handle call.acc.short || call.virt.acc.short
.Lloop_handle_call_acc_short:
    // read reg and acc_pos
    NEXT_SHORTY %r11d
    cmpl $0, %r11d
    je .Lload_reg_args

    // read the pair of arg index and accumulator position into %ecx and advance insn_ptr
    movzbl (%rbx), %ecx
    addq $1, %rbx

    // handle arg index
    movl %ecx, %r15d
    andl $0xF, %r15d
    shlq $4, %r15

    // handle acc position
    shrl $4, %ecx

    cmpl $0, %ecx  // if acc position is 0, jump
    je .Lshort_acc_1

    // pass Reg, then Acc
    movq (%r12,%r15), %r13
    movq FRAME_VREGISTER_TAG_OFFSET(%r12,%r15), %r15
    PUSH_ARG %r13, %r15, %rdx, %rdi, %rsi, %r9, .Lshort_acc_2
.Lshort_acc_2:
    NEXT_SHORTY %r11d
    cmpl $0, %r11d
    je .Lload_reg_args
    leaq (FRAME_ACC_OFFSET - FRAME_VREGS_OFFSET)(%r12), %r13
    movq FRAME_VREGISTER_TAG_OFFSET(%r13), %r15
    movq (%r13), %r13
    PUSH_ARG %r13, %r15, %rdx, %rdi, %rsi, %r9, .Lshort_acc_3
.Lshort_acc_3:
    jmp .Lload_reg_args

.Lshort_acc_1:  // pass Acc, then Reg
    leaq (FRAME_ACC_OFFSET - FRAME_VREGS_OFFSET)(%r12), %r13
    movq FRAME_VREGISTER_TAG_OFFSET(%r13), %rcx
    movq (%r13), %r13
    PUSH_ARG %r13, %rcx, %rdx, %rdi, %rsi, %r9, .Lshort_acc_4
.Lshort_acc_4:
    NEXT_SHORTY %r11d
    cmpl $0, %r11d
    je .Lload_reg_args
    movq (%r12,%r15), %r13
    movq FRAME_VREGISTER_TAG_OFFSET(%r12,%r15), %r15
    PUSH_ARG %r13, %r15, %rdx, %rdi, %rsi, %r9, .Lshort_acc_5
.Lshort_acc_5:
    jmp .Lload_reg_args
