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

    // handle call.short || call || call.virt.short || call.virt
.Lloop_handle_call:
    NEXT_SHORTY %r11d
    cmpl $0, %r11d
    je .Lload_reg_args

    // read the pair of arg indexes into w9 and advance insn_ptr
    movzbl (%rbx), %ecx
    addq $1, %rbx // read opcode and advance insn_ptr

    // handle the first arg index
    movl %ecx, %r15d
    andl $0xF, %r15d
    shlq $4, %r15
    movq (%r12,%r15), %r13
    movq FRAME_VREGISTER_TAG_OFFSET(%r12,%r15), %r15
    PUSH_ARG %r13, %r15, %rdx, %rdi, %rsi, %r9, 4f

4:
    // handle the second arg index
    NEXT_SHORTY %r11d
    cmpl $0, %r11d
    je .Lload_reg_args

    movl %ecx, %r15d
    sarl $4, %r15d
    shlq $4, %r15
    movq (%r12, %r15), %r13
    movq FRAME_VREGISTER_TAG_OFFSET(%r12,%r15), %r15
    PUSH_ARG %r13, %r15, %rdx, %rdi, %rsi, %r9, .Lloop_handle_call
    jmp .Lloop_handle_call
