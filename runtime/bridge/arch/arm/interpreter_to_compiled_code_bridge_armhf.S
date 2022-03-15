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
#include "arch/arm/shorty.S"
#include "shorty_values.h"

#define SHORTY_PTR_REG DEFAULT_SHORTY_PTR_REG
#define SHORTY_REG DEFAULT_SHORTY_REG

.macro LOAD_GPR_ARGS begin_ptr, end_ptr
    // load arguments into r0-r4 while begin_ptr != end_ptr
    ldr r0, [\begin_ptr, #-4]!
    cmp \begin_ptr, \end_ptr
    beq 1f
    ldr r1, [\begin_ptr, #-4]!
    cmp \begin_ptr, \end_ptr
    beq 1f
    ldr r2, [\begin_ptr, #-4]!
    cmp \begin_ptr, \end_ptr
    beq 1f
    ldr r3, [\begin_ptr, #-4]!
1: // end
.endm

.macro LOAD_FPR_ARGS begin_ptr, end_ptr
    // load arguments into d0-d7 while \begin_ptr != \end_ptr
    cmp \begin_ptr, \end_ptr
    beq 1f
    vldmia \begin_ptr!, {d0}
    cmp \begin_ptr, \end_ptr
    beq 1f
    vldmia \begin_ptr!, {d1}
    cmp \begin_ptr, \end_ptr
    beq 1f
    vldmia \begin_ptr!, {d2}
    cmp \begin_ptr, \end_ptr
    beq 1f
    vldmia \begin_ptr!, {d3}
    cmp \begin_ptr, \end_ptr
    beq 1f
    vldmia \begin_ptr!, {d4}
    cmp \begin_ptr, \end_ptr
    beq 1f
    vldmia \begin_ptr!, {d5}
    cmp \begin_ptr, \end_ptr
    beq 1f
    vldmia \begin_ptr!, {d6}
    cmp \begin_ptr, \end_ptr
    beq 1f
    vldmia \begin_ptr!, {d7}
1:
.endm

// The macro works with the stack prepared by 'PrepareArgStack' procedure
// It takes an argument in arg_reg and stores it to the corresponding stack space
// depends on its type.
// After the arg gets storeg the macro advances the corresponding pointer
// The macro assumes x9 contains base address for gpr and fpr args

.macro PUSH_ARG arg_ptr, gpr_ptr, fpr_ptr, stack_ptr, tmp1, tmp2, next_label
    cmp r2, #SHORTY_TAGGED
    beq 4f

    cmp r2, #SHORTY_REFERENCE
    cmpne r2, #SHORTY_LAST_INT32
    bhi 1f
    // it is a 32bit int or reference
    ldr \tmp1, [\arg_ptr]
    // determine whether there are free gprs
    sub \tmp2, r4, \gpr_ptr
    cmp \tmp2, #16
    strlt \tmp1, [\gpr_ptr, #-4]!
    strge \tmp1, [\stack_ptr], #4
    b \next_label

1:  cmp r2, #SHORTY_F32
    beq 21f
    cmp r2, #SHORTY_F64
    beq 22f
    // it is a 64bit int
    // align
    bic \gpr_ptr, \gpr_ptr, #7
    // determine whether there are free gprs
    sub \tmp1, r4, \gpr_ptr
    cmp \tmp1, #16
    ldm \arg_ptr, {\tmp1, \tmp2}
    sub \gpr_ptr, \gpr_ptr, #8
    // store tmp1 and tmp2 in the reverse order
    // in this order they will be read from the stack
    str \tmp2, [\gpr_ptr]
    str \tmp1, [\gpr_ptr, #4]
    blt \next_label
    bge 3f

21: // it is a float
    // determine whether there are free fp regs
    sub \tmp1, \fpr_ptr, r4
    cmp \tmp1, #64
    ldr \tmp1, [\arg_ptr]
    stmlt \fpr_ptr!, {\tmp1}
    stmge \stack_ptr!, {\tmp1}
    b \next_label

22: // it is a double
    // determine whether there are free fp regs
    add \fpr_ptr, \fpr_ptr, #7
    bic \fpr_ptr, \fpr_ptr, #7
    sub \tmp1, \fpr_ptr, r4
    cmp \tmp1, #64
    ldm \arg_ptr, {\tmp1, \tmp2}
    stmlt \fpr_ptr!, {\tmp1, \tmp2}
    blt \next_label

3:  // it is a 8 byte stack arg
    // align sp
    add \stack_ptr, \stack_ptr, #7
    bic \stack_ptr, \stack_ptr, #7
    stm \stack_ptr!, {\tmp1, \tmp2}
    b \next_label

4:  // it is value of type 'any'
    // in the code below we cannot overwrite
    // tmp2 because it is the same register
    // as arg_ptr
    // align gpr_ptr to 8byte boundary
    bic \gpr_ptr, \gpr_ptr, #7
    // determine whether there are free gprs
    // at least for the first int64_t
    sub \tmp1, r4, \gpr_ptr
    cmp \tmp1, #16
    ldr \tmp1, [\arg_ptr], #4
    sub \gpr_ptr, \gpr_ptr, #8
    blt 5f
    // there are no free gprs
    // store value to the stack
    // align sp
    add \stack_ptr, \stack_ptr, #7
    bic \stack_ptr, \stack_ptr, #7
    str \tmp1, [\stack_ptr], #4
    ldr \tmp1, [\arg_ptr], #4
    str \tmp1, [\stack_ptr], #4
    b 6f
5:  // there are free grps
    // store value in the reverse order
    // in this order they will be read from the stack
    str \tmp1, [\gpr_ptr, #4]
    ldr \tmp1, [\arg_ptr], #4
    str \tmp1, [\gpr_ptr]
6:  // copy tag to the stack
    ldm \arg_ptr, {\tmp1, \tmp2}
    stm \stack_ptr!, {\tmp1, \tmp2}
.endm

// The procedure reserves stack space for the arguments
// The reserved stack space is divided into 3 part:
// 1. the part for arguments passed via the stack
// 2. the part for the arguments passed via GPRs
// 3. the part for the arguments passed via the float registers
// These parts are located as follow:
// +--------------+
// |   gpr argN   |
// +--------------+
// |      ...     |
// +--------------+
// |   gpr arg0   |
// +--------------+ <- r4 (base)
// |  fpreg arg0  |
// +--------------+
// |      ...     |
// +--------------+
// |  fpreg argN  |
// +--------------+ <- sp
// |  stack arg0  |
// +--------------+
// |      ...     |
// +--------------+
// |  stack argN  |
// +--------------+
// |      r4      |
// +--------------+
// |     ...      |
// +--------------+
// |     r11      |
// +--------------+
// |      fp      |
// +--------------+
// | INTERPRETER_ |
// | TO_COMPILED_ |
// | CODE_BRIDGE  |
// +--------------+
// |    iframe    |
// +--------------+ <- fp
// |      lr      |
// +--------------+
// Input:
// r0 - SHORTY_PTR_REG
// r1 - SHORTY_REG
// r2 - shorty value (no initialization needed)
// r6 - method
// Output (as on the picture)
// r4 - gpr base pointer. Points to the beginning of GPR and FPR args space.
// sp - Points to the last stack argument.
PrepareArgStack:
    // r0 - SHORTY_PTR_REG, r2 - SHORTY_REG, r2 - shorty value, r3 - gpr args counter,
    // r4 - fpr args counter
    mov r3, #1 // Method*
    mov r4, #0

    NEXT_SHORTY r2
    // handle return type
    cmp r2, #SHORTY_TAGGED
    addeq r3, r3, #1 // reserve space for the pointer to the memory where the result will be stored

    // parameter 'this' of instance methods is not encoded in the shorty
    ldr r2, [r6, #METHOD_ACCESS_FLAGS_OFFSET]
    tst r2, #ACCESS_STATIC
    // it is an instance method
    addeq r3, r3, #1 // reserve space for 'this'

.Lloop_shorty:
    NEXT_SHORTY r2
    cmp r2, #0
    beq .Lexit

    cmp r2, #SHORTY_REFERENCE
    cmpne r2, #SHORTY_LAST_INT32
    bhi 1f
    // it is a 32bit value or reference
    cmp r3, #4
    addlt r3, r3, #1
    subge sp, sp, #4
    b .Lloop_shorty

1:  sub r2, r2, #SHORTY_FIRST_FLOAT
    cmp r2, #(SHORTY_NUM_FLOAT_TYPES - 1)
    bls 2f
    // it is a 64bit integer or 'any'
    // align
    add r3, r3, #1
    bic r3, r3, #1
    cmp r3, #4
    addlt r3, r3, #2
    // align sp
    bicge sp, sp, #7
    subge sp, sp, #8
    cmp r2, #(SHORTY_TAGGED - SHORTY_FIRST_FLOAT)
    bne .Lloop_shorty
    // it is 'any'
    // reserve space for the tag
    // align sp
    bic sp, sp, #7
    sub sp, sp, #8
    b .Lloop_shorty

2:  // it is a float
    // align
    cmp r4, #8
    addlt r4, r4, #1
    // align sp
    bicge sp, sp, #7
    subge sp, sp, #8
    b .Lloop_shorty
.Lexit:
    bic sp, sp, #7 // align sp
    sub r4, sp, r4, lsl #3
    mov pc, lr

// void InterpreterToCompiledCodeBridge(const BytecodeInstruction* insn, const Frame *iframe, const Method *method, ManagedThread* thread)
.global InterpreterToCompiledCodeBridge
.type InterpreterToCompiledCodeBridge, %function
InterpreterToCompiledCodeBridge:
    CFI_STARTPROC
    CFI_DEF_CFA(sp, 0)
    push {r1, lr}
    CFI_ADJUST_CFA_OFFSET(8)
    CFI_REL_OFFSET(lr, 4)
    sub sp, sp, #8
    CFI_ADJUST_CFA_OFFSET(8)

    // According to the current frame kind set the bridge type
    ldrb ip, [r3, #THREAD_FRAME_KIND]
    cmp ip, #0
    moveq ip, #INTERPRETER_TO_COMPILED_CODE_BRIDGE
    movne ip, #BYPASS_BRIDGE

    str fp, [sp]
    CFI_REL_OFFSET(fp, 0)
    str ip, [sp, #4]
    add fp, sp, #8
    CFI_DEF_CFA(fp, 8)
    push {r4 - r10}
    sub sp, sp, #4
    // sp should be 8 byte aligned

    // save regs to survive call of PrepareArgStack
    // r6 - method, r7 - method.shorty, r8 - insn_ptr, r9 - iframe
    // ip - thread
    mov r8, r0
    mov r9, r1
    ldr r7, [r2, #METHOD_SHORTY_OFFSET]
    mov r6, r2
    mov ip, r3
    mov SHORTY_PTR_REG, r7
    INIT_SHORTY_REG

    bl PrepareArgStack

    // setup regs as follow
    // r0 - SHORTY_PTR_REG, r1 - SHORTY_REG, r2, r3 - shorty value and temp,
    // r4 - gpr and fpr base ptr, r5 - gpr arg ptr, r6 - fpr arg ptr,
    // r7 - stack arg ptr, r8 - insn ptr, r9 - iframe, r10 - insn,
    // ip - thread, lr - method
    mov SHORTY_PTR_REG, r7
    INIT_SHORTY_REG
    mov lr, r6
    mov r5, r4
    mov r6, r4
    mov r7, sp

    // handle the return type
    NEXT_SHORTY r2
    cmp r2, #SHORTY_TAGGED
    // the return type is any
    // the first arg is the pointer to the caller frame's acc
    addeq r2, r9, #FRAME_ACC_OFFSET
    streq r2, [r5, #-4]!

    str lr, [r5, #-4]! // push method to the stack

    // parameter 'this' of instance methods is not encoded in the shorty
    // in case of instance method hack SHORTY_REG by replacing the return type by REF
    // in the another case just skip the return type
    ldr r2, [lr, #METHOD_ACCESS_FLAGS_OFFSET]
    tst r2, #ACCESS_STATIC
    // it is an instance method
    lsleq SHORTY_REG, SHORTY_REG, #4 // unshift shorty reg back
    orreq r1, r1, #SHORTY_REFERENCE

    ldrb r10, [r8], #1 // read opcode and advance insn_ptr

    // set r9 - frame.vregs
    add r9, r9, #FRAME_VREGS_OFFSET

    // The file contains code which checks opcode and jumps
    // to the corresponding handler.
    // At the end each handler jumps to .Linvoke_from_bridge label.
    // The file is autogenerated from runtime/templates/bridge_dispatch.S.erb
    // Handlers are distinguished by format and located in the corresponding files with name:
    // handle_call_<format>.S
    // If you get a compilation error that there is no such file it seems
    // new call format was introduced and you have to implement the corresponding handler.
#include "bridge_dispatch_armhf.S"

.Linvoke_from_bridge:
    mov r0, r4
    LOAD_FPR_ARGS r0, r6
    LOAD_GPR_ARGS r4, r5

    // move thread into THREAD_REG
    mov THREAD_REG, ip

    ldr r4, [lr, #METHOD_SHORTY_OFFSET] // load Method.shorty_ into r4 to survive the call
    ldr lr, [lr, #METHOD_COMPILED_ENTRY_POINT_OFFSET]
    blx lr

    // handle the result
    // setup registers as follow
    // r0, r1 or d0 - result, r2 - shorty[0] & 0xF, r3 - frame.acc, r4- temp
    ldrb r2, [r4]
    and r2, r2, #0xF

    cmp r2, #SHORTY_VOID
    cmpne r2, #SHORTY_TAGGED
    beq .Lreturn
    ldr r3, [fp]
    add r3, r3, #FRAME_ACC_OFFSET
    // store tag
    cmp r2, #SHORTY_REFERENCE
    moveq r4, #1
    movne r4, #0
    str r4, [r3, #FRAME_VREGISTER_TAG_OFFSET]
    cmpne r2, #SHORTY_LAST_INT32
    // it is a float or 64bit value(i64, u64, f32, f64)
    bhi .L64
    cmp r2, #SHORTY_FIRST_32
    bge .L32
    cmp r2, #SHORTY_I16
    beq .LI16
    bpl .LU16
    cmp r2, #SHORTY_I8
    beq .LI8
    uxtb r0, r0
    b .L32
.LI8:
    sxtb r0, r0
    b .L32
.LI16:
    sxth r0, r0
    b .L32
.LU16:
    uxth r0, r0
.L32:
    // it is a 32bit value or reference
    str r0, [r3]
    b .Lreturn

.L64:
    cmp r2, #SHORTY_F64
    vmoveq.f64 r0, r1, d0
    cmp r2, #SHORTY_F32
    vmoveq.f32 r0, s0
    stm r3, {r0, r1}

.Lreturn:
    sub sp, fp, #36
    pop {r4 - r10, fp}
    CFI_RESTORE(fp)
    CFI_RESTORE(r10)
    CFI_RESTORE(r9)
    CFI_RESTORE(r8)
    CFI_RESTORE(r7)
    CFI_RESTORE(r6)
    CFI_RESTORE(r5)
    CFI_RESTORE(r4)
    CFI_DEF_CFA(sp,12)
    add sp, sp, #12
    CFI_ADJUST_CFA_OFFSET(-12)
    ldr lr, [sp, #-4]
    CFI_RESTORE(lr)
    bx lr
    CFI_ENDPROC

// InvocationResult InvokeCompiledCodeWithArguments(const int64_t* args, const Frame *iframe, const Method *method, ManagedThread* thread)
.global InvokeCompiledCodeWithArgArray
.type InvokeCompiledCodeWithArgArray, %function
InvokeCompiledCodeWithArgArray:
    CFI_STARTPROC
    CFI_DEF_CFA(sp, 0)
    // Since Invocation result is 128bit structure the areguments are located here:
    // r0 - pointer to memory where to store the result
    // r1 - args, r2 - iframe, r3 - method, sp[0] - thread
    push {r2, lr}
    CFI_ADJUST_CFA_OFFSET(8)
    CFI_REL_OFFSET(lr, 4)
    sub sp, sp, #12
    CFI_ADJUST_CFA_OFFSET(12)

    stm sp, {THREAD_REG, fp}
    CFI_REL_OFFSET(fp, 4)
    CFI_REL_OFFSET(THREAD_REG, 0)
    add fp, sp, #12
    CFI_DEF_CFA(fp, 8)
    ldr THREAD_REG, [fp, #8]

    // According to the current frame kind set the bridge type
    ldrb r2, [THREAD_REG, #THREAD_FRAME_KIND]
    cmp r2, #0
    moveq r2, #INTERPRETER_TO_COMPILED_CODE_BRIDGE
    movne r2, #BYPASS_BRIDGE
    str r2, [sp, #8]
    push {r4 - r8}
    // sp should be 8 bytes aligned

    // save regs to survive call of PrepareArgStack
    // r6 - method, r7 - method.shorty, r8 - args, ip - pointer to memory where to store the result
    ldr r7, [r3, #METHOD_SHORTY_OFFSET]
    mov r8, r1
    mov r6, r3
    mov ip, r0
    mov SHORTY_PTR_REG, r7
    INIT_SHORTY_REG

    bl PrepareArgStack

    // push arguments to the stack
    // setup regs as follow
    // r0 - SHORTY_PTR_REG, r1 - SHORTY_REG, r2, r3 - shorty value and temp,
    // r4 - gpr and fpr base ptr, r5 - gpr arg ptr, r6 - fpr arg ptr,
    // r7 - stack arg ptr, r8 - args, ip - pointer to memory where to store the result,
    // lr - method
    mov SHORTY_PTR_REG, r7
    INIT_SHORTY_REG
    mov lr, r6
    mov r5, r4
    mov r6, r4
    mov r7, sp

    // handle the return type
    NEXT_SHORTY r2
    cmp r2, #SHORTY_TAGGED
    // the return type is any
    // the first arg is the pointer to the result
    streq ip, [r5, #-4]!

    str lr, [r5, #-4]! // push method to the stack

    // parameter 'this' of instance methods is not encoded in the shorty
    // in case of instance method hack SHORTY_REG by replacing the return type by REF
    // in the another case just skip the return type
    ldr r2, [lr, #METHOD_ACCESS_FLAGS_OFFSET]
    tst r2, #ACCESS_STATIC
    lsleq SHORTY_REG, SHORTY_REG, #4 // unshift shorty reg back
    orreq r1, r1, #SHORTY_REFERENCE

.Lloop_args_push:
    NEXT_SHORTY r2
    cmp r2, #0
    beq .Lloopend_args_push

    // handle the arg
    PUSH_ARG r8, r5, r6, r7, r2, r3, 10f
10: add r8, r8, #8
    b .Lloop_args_push
.Lloopend_args_push:
    mov r0, r4
    LOAD_FPR_ARGS r0, r6
    LOAD_GPR_ARGS r4, r5

    // setup regs as follow
    // r7 - method.shorty, r8 - pointer to memory where to store the result
    ldr r7, [lr, #METHOD_SHORTY_OFFSET]
    mov r8, ip

    ldr lr, [lr, #METHOD_COMPILED_ENTRY_POINT_OFFSET]
    blx lr

    // handle the result
    // setup regs as follow
    // r0,r1 - result, r2 - *method.shorty & 0xF
    ldrb r2, [r7]
    and r2, r2, #0xF
    cmp r2, #SHORTY_TAGGED
    beq .Lreturn_
    sub r3, r2, #SHORTY_FIRST_FLOAT
    cmp r3, #(SHORTY_NUM_FLOAT_TYPES - 1)
    bls .LFLOAT_

    cmp r2, #SHORTY_REFERENCE
    cmpne r2, #SHORTY_LAST_INT32
    bgt .Lstore_result_ // it is a 64bit type
    // it is a 32bit int
    mov r1, #0 // zero hight part of int64_t
    cmp r2, #SHORTY_FIRST_32
    bge .Lstore_result_
    cmp r2, #SHORTY_I16
    beq .LI16_
    bpl .LU16_
    cmp r2, #SHORTY_I8
    beq .LI8_
    uxtb r0, r0
    b .Lstore_result_
.LI8_:
    sxtb r0, r0
    b .Lstore_result_
.LI16_:
    sxth r0, r0
    b .Lstore_result_
.LU16_:
    uxth r0, r0
    b .Lstore_result_
.LFLOAT_:
    cmp r2, #SHORTY_F64
    vmoveq.f64 r0, r1, d0
    cmp r2, #SHORTY_F32
    vmoveq.f32 r0, s0

.Lstore_result_:
    // store the result r0,r1 into memory [r8]
    stm r8!, {r0, r1}
    // store tag
    cmp r2, #SHORTY_REFERENCE
    moveq r0, #1
    movne r0, #0
    mov r1, #0
    stm r8, {r0, r1}

.Lreturn_:
    sub sp, fp, #32
    pop {r4 - r8, THREAD_REG, fp}
    CFI_DEF_CFA(sp, 12)
    CFI_RESTORE(fp)
    CFI_RESTORE(THREAD_REG)
    CFI_RESTORE(r8)
    CFI_RESTORE(r7)
    CFI_RESTORE(r6)
    CFI_RESTORE(r5)
    CFI_RESTORE(r4)
    add sp, sp, #12
    CFI_ADJUST_CFA_OFFSET(-12)
    ldr lr, [sp, #-4]
    CFI_RESTORE(lr)
    bx lr
    CFI_ENDPROC

