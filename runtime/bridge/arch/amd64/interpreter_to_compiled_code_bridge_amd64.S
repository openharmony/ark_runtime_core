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

.macro LOAD_GPR_ARGS begin_ptr, end_ptr
    // load arguments into %rdi, %rsi, %rdx, %rcx, %r8, %r9 while begin_ptr != end_ptr
    movq (\begin_ptr), %rdi
    addq $8, \begin_ptr
    cmpq \end_ptr, \begin_ptr
    je 1f

    movq (\begin_ptr), %rsi
    addq $8, \begin_ptr
    cmpq \end_ptr, \begin_ptr
    je 1f

    movq (\begin_ptr), %rdx
    addq $8, \begin_ptr
    cmpq \end_ptr, \begin_ptr
    je 1f

    movq (\begin_ptr), %rcx
    addq $8, \begin_ptr
    cmpq \end_ptr, \begin_ptr
    je 1f

    movq (\begin_ptr), %r8
    addq $8, \begin_ptr
    cmpq \end_ptr, \begin_ptr
    je 1f

    movq (\begin_ptr), %r9
    addq $8, \begin_ptr
1: // end
.endm

.macro LOAD_FPR_ARGS begin_ptr, end_ptr
    // load arguments into xmm0-xmm7 while \begin_ptr != \end_ptr
    cmpq \end_ptr, \begin_ptr
    je 1f
    subq $8, \begin_ptr
    movq (\begin_ptr), %xmm0

    cmpq \end_ptr, \begin_ptr
    je 1f
    subq $8, \begin_ptr
    movq (\begin_ptr), %xmm1

    cmpq \end_ptr, \begin_ptr
    je 1f
    subq $8, \begin_ptr
    movq (\begin_ptr), %xmm2

    cmpq \end_ptr, \begin_ptr
    je 1f
    subq $8, \begin_ptr
    movq (\begin_ptr), %xmm3

    cmpq \end_ptr, \begin_ptr
    je 1f
    subq $8, \begin_ptr
    movq (\begin_ptr), %xmm4

    cmpq \end_ptr, \begin_ptr
    je 1f
    subq $8, \begin_ptr
    movq (\begin_ptr), %xmm5

    cmpq \end_ptr, \begin_ptr
    je 1f
    subq $8, \begin_ptr
    movq (\begin_ptr), %xmm6

    cmpq \end_ptr, \begin_ptr
    je 1f
    subq $8, \begin_ptr
    movq (\begin_ptr), %xmm7
1: // end
.endm

// The macro works with the stack prepared by 'PrepareArgStack' macro
// It takes an argument in arg_reg and stores it to the corresponding stack space
// depends on its type.
// After the arg gets stored the macro advances the corresponding pointer
// The macro assumes %r8 contains base address for gpr and fpr args
.macro PUSH_ARG arg_reg, tag_reg, gpr_ptr, fpr_ptr, stack_ptr, tmp_reg, next_label
    subl $SHORTY_FIRST_FLOAT, %r11d
    cmpl $(SHORTY_NUM_FLOAT_TYPES - 1), %r11d
    jbe 1f

    // it is an integer arg
    // determine whether there are free gprs
    movq \gpr_ptr, \tmp_reg
    subq %r8, \tmp_reg
    cmpq $48, \tmp_reg
    jge 2f

    //there are free gprs
    movq \arg_reg, (\gpr_ptr)
    addq $8, \gpr_ptr
    // check if the arg has type 'any'
    cmpl $(SHORTY_TAGGED - SHORTY_FIRST_FLOAT), %r11d
    jne \next_label
    // value of type 'any'
    // store the tag if there are free gprs
    cmpq $40, \tmp_reg
    jge 3f
    movq \tag_reg, (\gpr_ptr)
    addq $8, \gpr_ptr
    jmp \next_label
3:  // store the tag to the stack
    movq \tag_reg, (\stack_ptr)
    addq $8, \stack_ptr
    jmp \next_label

1:
    // it is a float arg
    // determine whether there are free float regs
    movq %r8, \tmp_reg
    subq \fpr_ptr, \tmp_reg
    cmpq $64, \tmp_reg
    jge 2f

    // there are free float regs
    subq $8, \fpr_ptr
    movq \arg_reg, (\fpr_ptr)
    jmp \next_label
2:
    // there are no free regs. It is a stack arg
    movq \arg_reg, (\stack_ptr)
    addq $8, \stack_ptr
    // check if the arg has type 'any'
    cmpl $(SHORTY_TAGGED - SHORTY_FIRST_FLOAT), %r11d
    jne \next_label
    movq \tag_reg, (\stack_ptr)
    addq $8, \stack_ptr
.endm

// The macro reserves stack space for arguments
// The reserved stack space is divided into 3 part:
// 1. the part for arguments passed via the stack
// 2. the part for the arguments passed via GPRs
// 3. the part for the arguments passed via the float registers
// These parts are located as follow:
// +--------------+
// |  fpreg argN  |
// +--------------+
// |      ...     |
// +--------------+
// |  fpreg arg0  |
// +--------------+ <- %r8 (base)
// |   gpr arg0   |
// +--------------+
// |      ...     |
// +--------------+
// |   gpr argN   |
// +--------------+ <- %rsp
// |  stack arg0  |
// +--------------+
// |      ...     |
// +--------------+
// |  stack argN  |
// +--------------+
// |    callee    |
// +--------------+
// |  THREAD_REG  |
// +--------------+
// |     %rbp     |
// +--------------+
// | INTERPRETER_ |
// | TO_COMPILED_ |
// | CODE_BRIDGE  |
// +--------------+ <- %rbp
// |    iframe    |
// +--------------+
// | return addr  |
// +--------------+
// Input:
// %rax - SHORTY_PTR_REG
// %r10d - SHORTY_REG
// %r11d - shorty value
// Output (as on the picture)
// %r8 - gpr base pointer. Points to the beginning of GPR and FPR args space.
// %rsp - Points to the last stack argument.
.macro PREPARE_ARG_STACK
    // %rax - SHORTY_PTR_REG, %r10d - SHORTY_REG, %r11d - shorty value, %esi - GPR arg counter,
    // %edi - float arg counter, %rdx - stack pointer, %r9 - temp
    movl $1, %esi // Method*
    xorl %edi, %edi
    movq %rsp, %rdx

30:
    NEXT_SHORTY %r11d
    cmpl $0, %r11d
    je 40f

    subl $SHORTY_FIRST_FLOAT, %r11d
    cmpl $(SHORTY_NUM_FLOAT_TYPES - 1), %r11d
    jbe 10f

    // it is an integer value
    xorq %r9, %r9
    cmpl $(SHORTY_TAGGED - SHORTY_FIRST_FLOAT), %r11d
    sete %r9b
    addl $1, %r9d
    addl %r9d, %esi

    cmpl $6, %esi
    jle 30b // there are free GPRs

    // there is no GPRs. Adjust %esi and reserve space on the stack
    movq %rsi, %r9
    subq $6, %r9
    movl $6, %esi
    shlq $3, %r9
    subq %r9, %rdx
    jmp 30b

10:
    // it is a float value
    cmpl $7, %edi
    ja 20f

    // there are free float registers
    addl $1, %edi
    jmp 30b

20:
    // there is no more GP registers
    subq $8, %rdx
    jmp 30b

40:
    // make %rsp 16 bytes aligned
    andq $-16, %rdx
    movq %rdx, %rsp

    // reserve stack space for stack arguments
    movq %rsp, %r8
    shlq $3, %rsi
    subq %rsi, %r8
.endm

// void InterpreterToCompiledCodeBridge(const BytecodeInstruction* insn, const Frame *iframe, const Method *method, ManagedThread* thread)
.global InterpreterToCompiledCodeBridge
.type InterpreterToCompiledCodeBridge, %function
InterpreterToCompiledCodeBridge:
    CFI_STARTPROC
    CFI_DEF_CFA(rsp, 8)

    pushq %rsi // iframe*
    CFI_ADJUST_CFA_OFFSET(8)

    movq %rsp, %rax
    CFI_DEF_CFA_REGISTER(rax)

    // According to the current frame kind set the bridge type
    movb THREAD_FRAME_KIND(%rcx), %r10b
    testb %r10b, %r10b
    movq $INTERPRETER_TO_COMPILED_CODE_BRIDGE, %r11
    movq $BYPASS_BRIDGE, %r10
    cmovne %r10, %r11
    pushq %r11

    pushq %rbp
    CFI_REL_OFFSET(rbp, -(2 * 8))

    movq %rax, %rbp // set frame pointer
    CFI_DEF_CFA_REGISTER(rbp)

    pushq %THREAD_REG
    CFI_REL_OFFSET(THREAD_REG, -(3 * 8))
    pushq %r14
    CFI_REL_OFFSET(r14, -(4 * 8))
    pushq %r13
    CFI_REL_OFFSET(r13, -(5 * 8))
    pushq %r12
    CFI_REL_OFFSET(r12, -(6 * 8))
    pushq %rbx
    CFI_REL_OFFSET(rbx, -(7 * 8))

    pushq %rcx // thread*
    subq $16, %rsp // do not delete, used to copy args
    // %rsp should be 16-byte aligned here

    // setup regs as follow
    // %rax - SHORTY_PTR_REG, %r10d - SHORTY_REG, %r11d - shorty value, %rbx - insn_ptr,
    // %r9 - temp (used by PREPARE_ARG_STACK), %r12 - frame.vregs, %r13 - method, %r14 - method.shorty
    movq %rdi, %rbx // insn*
    leaq FRAME_VREGS_OFFSET(%rsi), %r12 // frame.vregs
    movq %rdx, %r13 // method
    movq METHOD_SHORTY_OFFSET(%rdx), %r14 // method.shorty

    movq %r14, %SHORTY_PTR_REG
    INIT_SHORTY_REG

    // parameter 'this' of instance methods is not encoded in the shorty
    // in case of instance method hack SHORTY_REG by replacing the return type by REF
    // in the another case just skip the return type
    movl METHOD_ACCESS_FLAGS_OFFSET(%r13), %ecx
    testl $ACCESS_STATIC, %ecx
    jne 1f

    // it is an instance method
    andl $-16, %SHORTY_REG // clear the least significant 4 bits
    orl $SHORTY_REFERENCE, %SHORTY_REG
    jmp 2f

1:
    SKIP_SHORTY
2:
    movl %SHORTY_REG, %r15d // save value of the shorty reg
    PREPARE_ARG_STACK

    // setup regs as follow
    // %rax - SHORTY_PTR_REG, %r10d - SHORTY_REG, %r11d - shorty value, %rsi - stack arg ptr,
    // %rdi - float arg ptr, %rbx - insn_ptr, %r12 - frame.vregs, %r8 - arg base ptr
    // %rdx - gpr arg ptr, %rcx, %rdi, %r9 - temps, %r14 - method.shorty
    movq %rsp, %rsi
    movq %r8, %rdi
    movq %r8, %rdx
    leaq 2(%r14), %SHORTY_PTR_REG // since SHORTY_REG contains already read value SHORTY_REG_PTR should be shifted
    movl %r15d, %SHORTY_REG // restore the value of the shorty reg

    // store method (the first arg)
    movq %r13, (%rdx)
    addq $8, %rdx
    movzbl (%rbx), %ecx
    addq $1, %rbx // read opcode and advance insn_ptr

    // The file contains code which checks opcode and jumps
    // to the corresponding handler.
    // At the end each handler jumps to .Lload_reg_args label.
    // The file is autogenerated from runtime/templates/bridge_dispatch.S.erb
    // Handlers are distinguished by format and located in the corresponding files with name:
    // handle_call_<format>.S
    // If you get a compilation error that there is no such file it seems
    // new call format was introduced and you have to implement the corresponding handler.
#include "bridge_dispatch_amd64.S"

.Lload_reg_args:
    movq %r8, %rax
    movq %r8, %r10
    LOAD_FPR_ARGS %rax, %rdi
    LOAD_GPR_ARGS %r10, %rdx

    // invoke the method
    // since the first argument is Method* it must be in %rdi
    movq METHOD_COMPILED_ENTRY_POINT_OFFSET(%rdi), %rax
    movq -64(%rbp), %THREAD_REG
    callq *%rax

    // handle the result
    // setup registers as follow
    // %rax, %rdx / %xmm0 - result, %r14d - shorty[0] & 0xF, %r12 - frame.acc, %rcx - temp
    movzbl (%r14), %r14d
    andl $0xF, %r14d

    cmpl $SHORTY_VOID, %r14d
    je .Lreturn

    movq (%rbp), %r12 // load iframe from the stack
    addq $FRAME_ACC_OFFSET, %r12

    // get tag in rdx
    cmpl $SHORTY_TAGGED, %r14d
    je 1f // tag already in rdx
    xorq %rdx, %rdx
    cmpl $SHORTY_REFERENCE, %r14d
    sete %dl
1:  // store tag
    movq %rdx, FRAME_VREGISTER_TAG_OFFSET(%r12)

    movl %r14d, %esi
    movl %r14d, %edi

    subl $SHORTY_FIRST_FLOAT, %r14d
    cmpl $(SHORTY_NUM_FLOAT_TYPES - 1), %r14d
    jbe 1f // float

    subl $(SHORTY_FIRST_32), %esi
    cmpl $(SHORTY_NUM_MIN32_TYPES - 1), %esi
    jbe .L64_32 // 64-bit int / ref or 32-bit int

    // less than 32-bit
    cmpl $SHORTY_I16, %edi
    je .LI16
    ja .LU16

    cmpl $SHORTY_I8, %edi
    je .LI8
    jne .LU1_U8

1:
    // store float value into acc
    movsd %xmm0, (%r12)
    jmp .Lreturn
.LU1_U8:
    movzbl %al, %eax
    jmp .L64_32
.LI8:
    movsbl %al, %eax
    jmp .L64_32
.LI16:
    movswl %ax, %eax
    jmp .L64_32
.LU16:
    movzwl %ax, %eax
.L64_32:
    // store integer value into acc
    movq %rax, (%r12)

.Lreturn:
    leaq -56(%rbp), %rsp
    popq %rbx
    CFI_RESTORE(rbx)
    popq %r12
    CFI_RESTORE(r12)
    popq %r13
    CFI_RESTORE(r13)
    popq %r14
    CFI_RESTORE(r14)
    popq %THREAD_REG
    CFI_RESTORE(THREAD_REG)
    popq %rbp
    CFI_RESTORE(rbp)
    CFI_DEF_CFA(rsp, (3 * 8))
    addq $16, %rsp
    CFI_ADJUST_CFA_OFFSET(-(2 * 8))
    retq
    CFI_ENDPROC

// DecodedTaggedValue InvokeCompiledCodeWithArguments(const int64_t* args, const Frame *iframe, const Method *method, ManagedThread* thread)
.global InvokeCompiledCodeWithArgArray
.type InvokeCompiledCodeWithArgArray, %function
InvokeCompiledCodeWithArgArray:
    CFI_STARTPROC
    CFI_DEF_CFA(rsp, 8)

    pushq %rsi // iframe*
    CFI_ADJUST_CFA_OFFSET(8)

    movq %rsp, %rax
    CFI_DEF_CFA_REGISTER(rax)

    // According to the current frame kind set the bridge type
    movb THREAD_FRAME_KIND(%rcx), %r10b
    testb %r10b, %r10b
    movq $INTERPRETER_TO_COMPILED_CODE_BRIDGE, %r11
    movq $BYPASS_BRIDGE, %r10
    cmovne %r10, %r11
    pushq %r11

    pushq %rbp
    CFI_REL_OFFSET(rbp, -(2 * 8))

    movq %rax, %rbp // set frame pointer
    CFI_DEF_CFA_REGISTER(rbp)

    pushq %THREAD_REG
    CFI_REL_OFFSET(THREAD_REG, -(3 * 8))
    pushq %r14
    CFI_REL_OFFSET(r14, -(4 * 8))
    pushq %r13
    CFI_REL_OFFSET(r13, -(5 * 8))
    pushq %r12
    CFI_REL_OFFSET(r12, -(6 * 8))
    pushq %rbx
    CFI_REL_OFFSET(rbx, -(7 * 8))

    pushq %rcx // thread*
    subq $16, %rsp // do not delete, used to copy args
    // %rsp should be 16-byte aligned here

    // store method.shorty_ to %r14
    movq %rdx, %r13 // method
    movq METHOD_SHORTY_OFFSET(%rdx), %r14 // method.shorty

    // check args array
    // it could be null in case the method has no args
    cmpq $0, %rdi
    jne 1f

    movq %r13, %rdi
    jmp .Linvoke_with_args

1:
    // setup regs as follow
    // %rax - SHORTY_PTR_REG, %r10d - SHORTY_REG, %r11d - shorty value, %rbx - arg_ptr
    // %r13 - method, %r14 - method.shorty
    movq %rdi, %rbx
    movq %r14, %SHORTY_PTR_REG
    INIT_SHORTY_REG

    // parameter 'this' of instance methods is not encoded in the shorty
    // in case of instance method hack SHORTY_REG by replacing the return type by REF
    // in the another case just skip the return type
    movl METHOD_ACCESS_FLAGS_OFFSET(%r13), %ecx
    testl $ACCESS_STATIC, %ecx
    jne 1f

    // it is an instance method
    andl $-16, %SHORTY_REG
    orl $SHORTY_REFERENCE, %SHORTY_REG
    jmp 2f

1:
    SKIP_SHORTY
2:
    movl %SHORTY_REG, %r15d // save value of the shorty reg
    PREPARE_ARG_STACK

    // setup regs as follow
    // %rax - SHORTY_PTR_REG, %r10d - SHORTY_REG, %r11d - shorty value, %rsi - stack arg ptr,
    // %rdi - float arg ptr, %rbx - arg_ptr, %r8 - arg base ptr, %rdx - gpr arg ptr,
    // %rcx, %r9 - temps, %r14 - method.shorty, %r13 - method
    movq %rsp, %rsi
    movq %r8, %rdi
    movq %r8, %rdx
    leaq 2(%r14), %SHORTY_PTR_REG // since SHORTY_REG contains already read value SHORTY_REG_PTR should be shifted
    movl %r15d, %SHORTY_REG // restore the value of the shorty reg

    // store method (the last arg)
    movq %r13, (%rdx)
    addq $8, %rdx

.Lloop_args_push:
    NEXT_SHORTY %r11d
    cmpl $0, %r11d
    je .Lloopend_args_push

    movq (%rbx), %rcx
    addq $8, %rbx
    cmpl $SHORTY_TAGGED, %r11d
    jne 1f
    // Load the tag
    movq (%rbx), %r13
    addq $8, %rbx

1:  PUSH_ARG %rcx, %r13, %rdx, %rdi, %rsi, %r9, .Lloop_args_push
    jmp .Lloop_args_push
.Lloopend_args_push:
    // load arguments into regs
    movq %r8, %rax
    movq %r8, %r10
    LOAD_FPR_ARGS %rax, %rdi
    LOAD_GPR_ARGS %r10, %rdx

.Linvoke_with_args:
    // invoke the method
    // since the first argument is Method* it must be in %rdi
    movq METHOD_COMPILED_ENTRY_POINT_OFFSET(%rdi), %rax
    movq -64(%rbp), %THREAD_REG
    callq *%rax

    // handle the result
    // we should return it in %rax
    // setup registers as follow
    // %rax, %rdx / %xmm0 - result, %r14d - shorty[0] & 0xF
    movzbl (%r14), %r14d
    andl $0xF, %r14d

    cmpl $SHORTY_VOID, %r14d
    je .Lreturn_

    // get tag in rdx
    cmpl $SHORTY_TAGGED, %r14d
    je 1f // tag already in rdx
    xorq %rdx, %rdx
    cmpl $SHORTY_REFERENCE, %r14d
    sete %dl

1:
    movl %r14d, %esi
    movl %r14d, %edi

    subl $SHORTY_FIRST_FLOAT, %r14d
    cmpl $(SHORTY_NUM_FLOAT_TYPES - 1), %r14d
    jbe 1f

    subl $(SHORTY_FIRST_32), %esi
    cmpl $(SHORTY_NUM_MIN32_TYPES - 1), %esi
    jbe .Lreturn_ // 64-bit int / ref or 32-bit int

    // less than 32-bit
    cmpl $SHORTY_I16, %edi
    je .LI16_
    ja .LU16_

    cmpl $SHORTY_I8, %edi
    je .LI8_
    jne .LU1_U8_

1:
    movq %xmm0, %rax
    jmp .Lreturn_
.LU1_U8_:
    movzbl %al, %eax
    jmp .Lreturn_
.LI8_:
    movsbl %al, %eax
    jmp .Lreturn_
.LI16_:
    movswl %ax, %eax
    jmp .Lreturn_
.LU16_:
    movzwl %ax, %eax

.Lreturn_:
    leaq -56(%rbp), %rsp
    popq %rbx
    CFI_RESTORE(rbx)
    popq %r12
    CFI_RESTORE(r12)
    popq %r13
    CFI_RESTORE(r13)
    popq %r14
    CFI_RESTORE(r14)
    popq %THREAD_REG
    CFI_RESTORE(THREAD_REG)
    popq %rbp
    CFI_RESTORE(rbp)
    CFI_DEF_CFA(rsp, (3 * 8))
    addq $16, %rsp
    CFI_ADJUST_CFA_OFFSET(-(2 * 8))
    retq
    CFI_ENDPROC

