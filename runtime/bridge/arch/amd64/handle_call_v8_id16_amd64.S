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

    // handle call.range || call.virt.range
    // setup regs as follow
    // %rax - SHORTY_PTR_REG, %r10d - SHORTY_REG, %r11d - shorty value, %rsi - stack arg ptr,
    // %rdi - float arg ptr, %rbx - arg_ptr, %r12 - frame.vregs, %r8 - arg base ptr
    // %rdx - gpr arg ptr, %rcx, %r9 - temps, %r14 - method.shorty, %r13 - method
    movzbl (%rbx), %ecx
    shlq $4, %rcx
    leaq (%r12,%rcx), %rbx

.Lloop_handle_call_range:
    NEXT_SHORTY %r11d
    cmpl $0, %r11d
    je .Lload_reg_args

    movq (%rbx), %r15
    movq FRAME_VREGISTER_TAG_OFFSET(%rbx), %r13
    addq $FRAME_VREGISTER_SIZE, %rbx

    // handle the arg
    PUSH_ARG %r15, %r13, %rdx, %rdi, %rsi, %r9, .Lloop_handle_call_range
    jmp .Lloop_handle_call_range

