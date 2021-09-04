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

#include "arch/asm_support.h"
#include "arch/amd64/shorty.S"
#include "shorty_values.h"

#define SHORTY_PTR_REG DEFAULT_SHORTY_PTR_REG
#define SHORTY_REG DEFAULT_SHORTY_REG

#include "arch/amd64/helpers_amd64.S"

// Frame* CreateFrameForMethod(Method* method, Frame* prev);
.extern CreateFrameForMethod
// void InterpreterEntryPoint(const uint8_t* pc, Frame* frame);
.extern InterpreterEntryPoint

.extern IncrementHotnessCounter

.global CompiledCodeToInterpreterBridge
.type CompiledCodeToInterpreterBridge, %function
CompiledCodeToInterpreterBridge:
    CFI_STARTPROC
    CFI_DEF_CFA(rsp, 8)

    // Save return address to the TLS field
    movq (%rsp), %rax
    movq %rax, THREAD_NATIVE_PC(%THREAD_REG)

    pushq $COMPILED_CODE_TO_INTERPRETER_BRIDGE
    CFI_ADJUST_CFA_OFFSET(8)

    // construct the frame
    pushq %rbp
    CFI_ADJUST_CFA_OFFSET(8)
    CFI_REL_OFFSET(rbp, 0)
    movq %rsp, %rbp // set frame pointer
    CFI_DEF_CFA_REGISTER(rbp)

    subq $8, %rsp

    // save all the callee saved registers to the stack
    // stack walker will read them during stack unwinding
    pushq %r15
    CFI_REL_OFFSET(r15, -(2 * 8))
    pushq %r14
    CFI_REL_OFFSET(r14, -(3 * 8))
    pushq %r13
    CFI_REL_OFFSET(r13, -(4 * 8))
    pushq %r12
    CFI_REL_OFFSET(r12, -(5 * 8))
    pushq %rbx
    CFI_REL_OFFSET(rbx, -(6 * 8))

    subq $8, %rsp

    // Before we call IncrementHotnessCounter we should set pointer to C2I frame in the TLS,
    // because compilation may fall into safepoint, so we need to make caller's callee registers
    // visible for the stack walker.
    movq %rbp, THREAD_CURRENT_FRAME(%THREAD_REG)

    PUSH_GENERAL_REGS
    PUSH_FP_REGS

    callq IncrementHotnessCounter@plt

    cmpb $0, %al
    je .Lnot_compiled

    // Compilation successfully done, so recover caller's frame in the TLS, since C2I is not needed.
    movq (%rbp), %r8
    movq %r8, THREAD_CURRENT_FRAME(%THREAD_REG)

    POP_FP_REGS
    POP_GENERAL_REGS

    movq %rbp, %rsp
    addq $16, %rsp
    movq (%rbp), %rbp
    CFI_REMEMBER_STATE
    CFI_RESTORE(rbp)
    CFI_DEF_CFA(rsp, 8)

    movq METHOD_COMPILED_ENTRY_POINT_OFFSET(%rdi), %rax
    jmp *%rax
    CFI_RESTORE_STATE
    CFI_DEF_CFA(rbp, (3 * 8))

.Lnot_compiled:

    POP_FP_REGS
    POP_GENERAL_REGS

    // save arguments to the stack
    PUSH_FP_REGS
    PUSH_GENERAL_REGS

    // %rsp % 16 == 0 here

    movq %rdi, %rbx // save method to survive the call

    // create an interpreter frame
    movq %rbx, %rdi
    movq %rbp, %rsi
    callq CreateFrameForMethod@plt
    movq %rax, %r13

    // setup regs as follow
    // %rax - SHORTY_PTR_REG, %r10d - SHORTY_REG, %r12 - iframe.vregs_ + num_vregs_,
    // %rdx - gpr args, %rsi - float args, %rdi - stack args, %r8d - gpr arg counter,
    // %r9d - float arg counter, %rcx, %r15 - temp, %r13 - iframe, %rbx - method,
    // %r14 - method.shorty
    movq METHOD_SHORTY_OFFSET(%rbx), %r14
    movl FRAME_NUM_VREGS_OFFSET(%r13), %r12d
    subl METHOD_NUM_ARGS_OFFSET(%rbx), %r12d
    shlq $4, %r12
    leaq FRAME_VREGS_OFFSET(%r13,%r12), %r12
    leaq 8(%rsp), %rdx // skip Method*
    leaq 48(%rsp), %rsi
    leaq 24(%rbp), %rdi
    movl $5, %r8d  // 5 because we skip Method* arg
    movl $8, %r9d

    movq %r14, %rax
    INIT_SHORTY_REG

    // parameter 'this' of instance methods is not encoded in the shorty
    // in case of instance method hack SHORTY_REG by replacing the return type by REF
    // in the another case just skip the return type

    // check whether the method is an instance
    movl METHOD_ACCESS_FLAGS_OFFSET(%rbx), %ecx
    testl $ACCESS_STATIC, %ecx
    jne 1f

    // it is an instance method
    // replace the return type by REF
    andl $0xFFFFFFF0, %SHORTY_REG // clear the least significant 4  bits
    orl $SHORTY_REFERENCE, %SHORTY_REG
    jmp .Lloop_copy_args

1:
    SKIP_SHORTY

    // fill in the iframe
.Lloop_copy_args:
    NEXT_SHORTY %r11d
    cmpl $0, %r11d
    je .Lloopend_copy_args

    // store the tag
    xorq %rcx, %rcx
    cmpl $SHORTY_REFERENCE, %r11d
    sete %cl
    movq %rcx, FRAME_VREGISTER_TAG_OFFSET(%r12)

    subl $SHORTY_FIRST_FLOAT, %r11d
    cmpl $(SHORTY_NUM_FLOAT_TYPES - 1), %r11d
    jbe 1f

    // it is an int value or reference
    cmpl $0, %r8d
    je 2f

    // the arg in a gpr
    movq (%rdx), %r15
    addq $8, %rdx
    subl $1, %r8d
    jmp 3f
1:
    // it is a float value
    cmpl $0, %r9d
    je 2f

    // the arg in a float reg
    movq (%rsi), %r15
    addq $8, %rsi
    subl $1, %r9d
    jmp 3f
2:
    // it is a stack arg
    movq (%rdi), %r15
    addq $8, %rdi
3:
    movq %r15, (%r12)
    addq $FRAME_VREGISTER_SIZE, %r12
    jmp .Lloop_copy_args
.Lloopend_copy_args:

    // call InterpreterEntryPoint
    movq %rbx, %rdi
    movq %r13, %rsi
    callq InterpreterEntryPoint@plt

    // handle the result
    // setup regs as follow
    // %rdi - iframe, %r14d - *method.shorty, %rax - &iframe.acc_
    // %r12, %r13 - result
    movq %r13, %rdi
    leaq FRAME_ACC_OFFSET(%r13), %rax
    movl (%r14), %r14d
    andl $0xF, %r14d

    cmpl $SHORTY_VOID, %r14d
    jne 1f

    // void method
    callq FreeFrame@plt
    jmp 3f
1:
    movq (%rax), %r12
    movq FRAME_VREGISTER_TAG_OFFSET(%rax), %r13
    subl $SHORTY_FIRST_FLOAT, %r14d
    cmpl $(SHORTY_NUM_FLOAT_TYPES - 1), %r14d
    jbe 2f

    // result is an integer or reference value
    callq FreeFrame@plt
    movq %r12, %rax
    movq %r13, %rdx
    jmp 3f
2:
    // result is a float value
    callq FreeFrame@plt
    movq %r12, %xmm0
3:
    leaq -48(%rbp), %rsp
    popq %rbx
    CFI_RESTORE(rbx)
    popq %r12
    CFI_RESTORE(r12)
    popq %r13
    CFI_RESTORE(r13)
    popq %r14
    CFI_RESTORE(r14)
    popq %r15
    CFI_RESTORE(r15)
    addq $8, %rsp
    popq %rbp
    CFI_RESTORE(rbp)
    CFI_DEF_CFA(rsp, (2 * 8))
    addq $8, %rsp
    CFI_ADJUST_CFA_OFFSET(-(1 * 8))
    retq
    CFI_ENDPROC
