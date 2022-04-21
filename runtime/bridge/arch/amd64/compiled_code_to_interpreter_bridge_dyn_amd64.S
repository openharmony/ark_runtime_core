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
#include "arch/amd64/helpers_amd64.S"

// Frame* CreateFrameForMethodWithActualArgs(uint32_t num_actual_args, Method* method, Frame* prev);
.extern CreateFrameForMethodWithActualArgs
// void InterpreterEntryPoint(Method* pc, Frame* frame, bool is_dynamic);
.extern InterpreterEntryPoint
// void FreeFrame(Frame* frame);
.extern FreeFrame
// bool IncrementHotnessCounter(Method* method, bool is_dynamic);
.extern IncrementHotnessCounter
// DecodedTaggedValue GetInitialTaggedValue(Method*)
.extern GetInitialTaggedValue

// CompiledCodeToInterpreterBridgeDyn(Method* method, uint32_t num_args, int64_t func_obj, int64_t func_tag, int64_t arg_i, int64_t tag_i, ...)
.global CompiledCodeToInterpreterBridgeDyn
TYPE_FUNCTION(CompiledCodeToInterpreterBridgeDyn)
CompiledCodeToInterpreterBridgeDyn:
    CFI_STARTPROC
    CFI_DEF_CFA(rsp, 8)

    // method:      %rdi
    // num_args:    %rsi
    // func_obj:    %rdx
    // func_tag:    %rcx
    // arg_0:       %r8
    // tag_0:       %r9
    // arg_i:       8*i(%rbp)
    // tag_i:       8*(i+1)(%rbp)

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

    callq IncrementHotnessCounter@plt
    cmpb $0, %al
    je .Lnot_compiled

    // Compilation successfully done, so recover caller's frame in the TLS, since C2I is not needed.
    movq (%rbp), %r8
    movq %r8, THREAD_CURRENT_FRAME(%THREAD_REG)

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

    POP_GENERAL_REGS

    // save arguments to the stack
    PUSH_GENERAL_REGS

    // %rsp % 16 == 0 here

    // save method* before call
    movq %rdi, %r14

    // create an interpreter frame
    movq %rsi, %rdi
    addq $1, %rdi // count function object
    // get num args in r15d
    // num_args = max(num_args, method->num_args_)
    movl METHOD_NUM_ARGS_OFFSET(%r14), %r15d
    cmpl %r15d, %edi
    cmovge %edi, %r15d
    // get number of rest arguments in r13
    movl %r15d, %r13d
    subl %edi, %r13d

    movq %r14, %rsi
    leaq (%rbp), %rdx
    // Frame* CreateFrameForMethodWithActualArgs(uint32_t num_actual_args, Method* method, Frame* prev);
    callq CreateFrameForMethodWithActualArgs@plt
    movq %rax, %r12 // iframe*

    POP_GENERAL_REGS

    // setup regs as follow
    // %rax         - iframe.vregs_ + num_vregs_
    // %rdi         - stack args
    // %rsi         - num args
    // %rdx/%rcx    - func obj
    // %r8/%r9      - arg0
    // %rbx         - method*
    // %r12         - iframe*
    // %r13         - num of rest arguments
    movl FRAME_NUM_VREGS_OFFSET(%rax), %ebx
    subl %r15d, %ebx
    shll $4, %ebx
    leaq FRAME_VREGS_OFFSET(%r12,%rbx), %rax
    movq %rdi, %rbx // method*
    leaq 24(%rbp), %rdi

    movq %rdx, (%rax)
    movq %rcx, FRAME_VREGISTER_TAG_OFFSET(%rax)
    addq $FRAME_VREGISTER_SIZE, %rax
    cmpl $0, %esi
    je .Lrest_args

    movq %r8, (%rax)
    movq %r9, FRAME_VREGISTER_TAG_OFFSET(%rax)
    addq $FRAME_VREGISTER_SIZE, %rax
    subl $1, %esi
    je .Lrest_args

.Lloop:
    movq (%rdi), %rdx
    movq FRAME_VREGISTER_TAG_OFFSET(%rdi), %rcx
    addq $FRAME_VREGISTER_SIZE, %rdi

    movq %rdx, (%rax)
    movq %rcx, FRAME_VREGISTER_TAG_OFFSET(%rax)
    addq $FRAME_VREGISTER_SIZE, %rax

    subl $1, %esi
    ja .Lloop

.Lrest_args:
    // mov rax to r14 to survive the call
    movq %rax, %r14
    // set the first arg Method*
    movq %rbx, %rdi
    callq GetInitialTaggedValue@plt

    // setup regs as follow
    // rax / rdx - initial value / tag
    // %rbx - method*
    // %r12 - iframe
    // %r13 - num rest args
    // %r14 - pointer to the current arg vreg

1:  // fill in the reset args
    test %r13, %r13
    je .Lcall_interpreter
    movq %rax, (%r14)
    movq %rdx, FRAME_VREGISTER_TAG_OFFSET(%r14)
    addq $FRAME_VREGISTER_SIZE, %r14
    subl $1, %r13d
    jmp 1b

.Lcall_interpreter:
    // call InterpreterEntryPoint
    movq %rbx, %rdi
    movq %r12, %rsi
    // void InterpreterEntryPoint(Method *method, Frame* frame);
    callq InterpreterEntryPoint@plt

    // handle the result
    // setup regs as follow
    // %rax         - &iframe.acc_
    // %r12         - iframe*
    // %r13/%r14    - result
    leaq FRAME_ACC_OFFSET(%r12), %rax

    movq (%rax), %r13
    movq FRAME_VREGISTER_TAG_OFFSET(%rax), %r14

    movq %r12, %rdi
    // void FreeFrame(Frame* frame);
    callq FreeFrame@plt

    movq %r13, %rax
    movq %r14, %rdx

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
