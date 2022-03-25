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
#include "arch/arm/shorty.S"
#include "shorty_values.h"

#define SHORTY_PTR_REG DEFAULT_SHORTY_PTR_REG
#define SHORTY_REG DEFAULT_SHORTY_REG

.macro PUSH_ARG arg_ptr, stack_ptr, tmp1, tmp2, next_label
    cmp r2, #SHORTY_TAGGED
    beq 2f

    sub r2, r2, #SHORTY_FIRST_64
    cmp r2, #(SHORTY_NUM_64BIT_TYPES - 1)
    bls 1f
    // it is a 32bit value
    ldr \tmp1, [\arg_ptr]
    str \tmp1, [\stack_ptr], #4
    b \next_label
1:  // it is a 64bit value
    ldm \arg_ptr, {\tmp1, \tmp2}
    add \stack_ptr, \stack_ptr, #7 // round the address up to 8 bytes boundary
    bic \stack_ptr, \stack_ptr, #7
    stm \stack_ptr!, {\tmp1, \tmp2}
    b \next_label

2:  // the arg has type 'any'
    add \stack_ptr, \stack_ptr, #7 // round the address up to 8 bytes boundary
    bic \stack_ptr, \stack_ptr, #7
    // copy value
    ldr \tmp2, [\arg_ptr], #4
    str \tmp2, [\stack_ptr], #4
    ldr \tmp2, [\arg_ptr], #4
    str \tmp2, [\stack_ptr], #4
    // copy tag
    ldm \arg_ptr, {\tmp1, \tmp2}
    stm \stack_ptr, {\tmp1, \tmp2}
    b \next_label
.endm

.macro LOAD_ARGS begin_ptr, end_ptr
    ldr r0, [\begin_ptr], #4
    cmp \begin_ptr, \end_ptr
    beq 1f
    ldr r1, [\begin_ptr], #4
    cmp \begin_ptr, \end_ptr
    beq 1f
    ldr r2, [\begin_ptr], #4
    cmp \begin_ptr, \end_ptr
    beq 1f
    ldr r3, [\begin_ptr], #4
1:
    bic sp, sp, #7 // round downd sp to 8 bytes boundary
.endm

// The procedure reserves stack space for the arguments
// Input:
// r0 - SHORTY_PTR_REG
// r1 - SHORTY_REG
// r2 - shorty value (no initialization needed)
// r7 - method
// The procedure musn't use other registers
PrepareArgStack:
    // check result type
    NEXT_SHORTY r2
    cmp r2, #SHORTY_TAGGED
    subeq sp, sp, #4 // r0 contains a pointer to the space to which the result will be stored

    sub sp, sp, #4 // space for Method

    // parameter 'this' of instance methods is not encoded in the shorty
    ldr r2, [r7, #METHOD_ACCESS_FLAGS_OFFSET]
    tst r2, #ACCESS_STATIC
    subeq sp, sp, #4 // it is an instance method

.Lloop_shorty:
    NEXT_SHORTY r2
    cmp r2, #0
    beq .Lexit

    cmp r2, #SHORTY_TAGGED
    beq 1f

    sub r2, r2, #SHORTY_FIRST_64
    cmp r2, #(SHORTY_NUM_64BIT_TYPES - 1)
    // it is a 32bit value
    subhi sp, sp, #4
    // it is a 64bit value
    // align arg slot
    bicls sp, sp, #7
    subls sp, sp, #8
    b .Lloop_shorty

1:  // argument has type 'any'
    bic sp, sp, #7
    sub sp, sp, #16
    b .Lloop_shorty
.Lexit:
    bic sp, sp, #7 // round downd sp to 8 bytes boundary
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

    sub sp, sp, #12
    CFI_ADJUST_CFA_OFFSET(12)

    stm sp, {THREAD_REG, fp}
    CFI_REL_OFFSET(fp, 4)
    CFI_REL_OFFSET(THREAD_REG, 0)
    add fp, sp, #12
    CFI_ADJUST_CFA_OFFSET(-12)
    CFI_DEF_CFA_REGISTER(fp)

    mov THREAD_REG, r3

    // According to the current frame kind set the bridge type
    ldrb r3, [THREAD_REG, #THREAD_FRAME_KIND]
    cmp r3, #0
    moveq r3, #INTERPRETER_TO_COMPILED_CODE_BRIDGE
    movne r3, #BYPASS_BRIDGE

    str r3, [sp, #8]
    push {r4, r5, r6, r7, r8, r9}
    CFI_REL_OFFSET(r9,  -((3 + 1) * 4))
    CFI_REL_OFFSET(r8,  -((3 + 2) * 4))
    CFI_REL_OFFSET(r7,  -((3 + 3) * 4))
    CFI_REL_OFFSET(r6,  -((3 + 4) * 4))
    CFI_REL_OFFSET(r5,  -((3 + 5) * 4))
    CFI_REL_OFFSET(r4,  -((3 + 6) * 4))
    CFI_REL_OFFSET(r3,  -((3 + 7) * 4))
    sub sp, sp, #4
    // sp should be 8 bytes aligned

    // save regs to survive call of PrepareArgStack
    // r4 - insn_ptr, r5 - iframe, r6 - method.shorty, r7 - method
    mov r4, r0
    mov r5, r1
    ldr r6, [r2, #METHOD_SHORTY_OFFSET]
    mov r7, r2
    mov SHORTY_PTR_REG, r6
    INIT_SHORTY_REG

    bl PrepareArgStack

    // setup regs as follow
    // r0 - SHORTY_PTR_REG, r1 - SHORTY_REG, r2 - shorty value, r3 - insn, r4 - insn_ptr,
    // r5 - iframe, r6, r7 and r9 - temps, r8 - pointer to stack, lr - method
    mov SHORTY_PTR_REG, r6
    INIT_SHORTY_REG
    mov r8, sp
    mov lr, r7

    // handle the return type
    NEXT_SHORTY r2
    cmp r2, #SHORTY_TAGGED
    // the return type is any
    // the first arg is the pointer to the caller frame's acc
    addeq r2, r5, #FRAME_ACC_OFFSET
    streq r2, [r8], #4

    str lr, [r8], #4 // push method to the stack

    // parameter 'this' of instance methods is not encoded in the shorty
    // in case of instance method hack SHORTY_REG by replacing the return type by REF
    // in the another case just skip the return type
    ldr r2, [lr, #METHOD_ACCESS_FLAGS_OFFSET]
    tst r2, #ACCESS_STATIC
    // it is an instance method
    lsleq SHORTY_REG, SHORTY_REG, #4 // unshift shorty reg
    orreq SHORTY_REG, SHORTY_REG, #SHORTY_REFERENCE

    // setup r5 as follow
    // r5 - iframe.vregs
    add r5, r5, #FRAME_VREGS_OFFSET
    ldrb r6, [r4], #1 // read opcode and advance insn_ptr

    // The file contains code which checks opcode and jumps
    // to the corresponding handler.
    // At the end each handler jumps to .Linvoke_from_bridge label.
    // The file is autogenerated from runtime/templates/bridge_dispatch.S.erb
    // Handlers are distinguished by format and located in the corresponding files with name:
    // handle_call_<format>.S
    // If you get a compilation error that there is no such file it seems
    // new call format was introduced and you have to implement the corresponding handler.
#include "bridge_dispatch_arm.S"

.Linvoke_from_bridge:
    LOAD_ARGS sp, r8

    ldr r4, [lr, #METHOD_SHORTY_OFFSET] // load Method.shorty_ into r4 to survive the call
    ldr lr, [lr, #METHOD_COMPILED_ENTRY_POINT_OFFSET]
    blx lr

    // handle the result
    // setup registers as follow
    // r0, r1 - result, r2 - shorty[0] & 0xF, r3 - frame.acc, r4- temp
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
    streq r0, [r3]
    beq .Lreturn
    cmp r2, #SHORTY_FIRST_64
    // u64, i64, f64
    stmge r3, {r0, r1}
    bge .Lreturn
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
    str r0, [r3]
.Lreturn:
    sub sp, fp, #36
    pop {r4, r5, r6, r7, r8, r9, THREAD_REG, fp}
    CFI_RESTORE(r4)
    CFI_RESTORE(r5)
    CFI_RESTORE(r6)
    CFI_RESTORE(r7)
    CFI_RESTORE(r8)
    CFI_RESTORE(r9)
    CFI_RESTORE(THREAD_REG)
    CFI_RESTORE(fp)
    CFI_DEF_CFA(sp, 12)
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
    // Since Invocation result is 128bit structure the areguments are located here:
    // r0 - pointer to memory where to store the result
    // r1 - args, r2 - iframe, r3 - method, sp[0] - thread
    CFI_STARTPROC
    CFI_DEF_CFA(sp, 0)

    push {r2, lr}
    CFI_ADJUST_CFA_OFFSET(8)
    CFI_REL_OFFSET(lr, 4)

    sub sp, sp, #12
    CFI_ADJUST_CFA_OFFSET(12)

    stm sp, {THREAD_REG, fp}
    CFI_REL_OFFSET(fp, 4)
    CFI_REL_OFFSET(THREAD_REG, 0)

    add fp, sp, #12
    CFI_DEF_CFA_REGISTER(fp)
    CFI_ADJUST_CFA_OFFSET(-12)

    ldr THREAD_REG, [fp, #8]

    // According to the current frame kind set the bridge type
    ldrb r2, [THREAD_REG, #THREAD_FRAME_KIND]
    cmp r2, #0
    moveq r2, #INTERPRETER_TO_COMPILED_CODE_BRIDGE
    movne r2, #BYPASS_BRIDGE
    str r2, [sp, #8]
    push {r4, r5, r6, r7, r8}
    CFI_REL_OFFSET(r8, -(4 * 4))
    CFI_REL_OFFSET(r7, -(5 * 4))
    CFI_REL_OFFSET(r6, -(6 * 4))
    CFI_REL_OFFSET(r5, -(7 * 4))
    CFI_REL_OFFSET(r4, -(8 * 4))

    // sp should be 8 bytes aligned

    // setup regs as follow
    // r4 - args, r6 - method.shorty, r7 - method, r8 - pointer to memory where to store the result
    mov r4, r1
    mov r7, r3
    ldr r6, [r3, #METHOD_SHORTY_OFFSET]
    mov r8, r0
    mov SHORTY_PTR_REG, r6
    INIT_SHORTY_REG

    bl PrepareArgStack

    // push arguments to the stack
    // setup regs as follow
    // r0 - SHORTY_PTR_REG, r1 - SHORTY_REG, r2 - shorty value, r3 - the pointer to the stack,
    // r4 - args, r5 and r6 - temps, lr - method
    mov SHORTY_PTR_REG, r6
    INIT_SHORTY_REG
    mov r3, sp
    mov lr, r7

    // handle the return type
    NEXT_SHORTY r2
    cmp r2, #SHORTY_TAGGED
    // the return type is any
    // the first arg is the pointer to the result
    streq r8, [r3], #4

    str lr, [r3], #4

    // parameter 'this' of instance methods is not encoded in the shorty
    // in case of instance method hack SHORTY_REG by replacing the return type by REF
    // in the another case just skip the return type
    ldr r2, [lr, #METHOD_ACCESS_FLAGS_OFFSET]
    tst r2, #ACCESS_STATIC
    // it is an instance method
    lsleq SHORTY_REG, SHORTY_REG, #4 // unshift shorty reg
    orreq SHORTY_REG, SHORTY_REG, #SHORTY_REFERENCE

.Lloop_args_push:
    NEXT_SHORTY r2
    cmp r2, #0
    beq .Lloopend_args_push

    // handle the arg
    mov r5, r4
    add r4, r4, #8
    PUSH_ARG r5, r3, r5, r6, .Lloop_args_push
.Lloopend_args_push:
    LOAD_ARGS sp, r3
    // load shorty into r6 again
    ldr r6, [lr, #METHOD_SHORTY_OFFSET]

    ldr lr, [lr, #METHOD_COMPILED_ENTRY_POINT_OFFSET]
    blx lr

    // handle the result
    // setup regs as follow
    // r0,r1 - result, r2 - method.shorty[0] & 0xF
    ldrb r2, [r6]
    and r2, r2, #0xF
    cmp r2, #SHORTY_TAGGED
    beq .Lreturn_
    sub r3, r2, #SHORTY_FIRST_64
    cmp r3, #(SHORTY_NUM_64BIT_TYPES - 1)
    bls .Lstore_result // the result type is a 64bit type
    // the result type is a 32bit type
    mov r1, #0 // set high 32 bits of 64bit value to 0
    cmp r2, #SHORTY_FIRST_32
    bge .Lstore_result
    cmp r2, #SHORTY_I16
    beq .LI16_
    bpl .LU16_
    cmp r2, #SHORTY_I8
    beq .LI8_
    uxtb r0, r0
    b .Lstore_result
.LI8_:
    sxtb r0, r0
    b .Lstore_result
.LI16_:
    sxth r0, r0
    b .Lstore_result
.LU16_:
    uxth r0, r0

.Lstore_result:
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
    pop {r4, r5, r6, r7, r8, THREAD_REG, fp}
    CFI_RESTORE(fp)
    CFI_RESTORE(THREAD_REG)
    CFI_RESTORE(r8)
    CFI_RESTORE(r7)
    CFI_RESTORE(r6)
    CFI_RESTORE(r5)
    CFI_RESTORE(r4)
    CFI_DEF_CFA(sp, 12)
    add sp, sp, #12
    CFI_ADJUST_CFA_OFFSET(-12)
    ldr lr, [sp, #-4]
    CFI_RESTORE(lr)
    bx lr
    CFI_ENDPROC

