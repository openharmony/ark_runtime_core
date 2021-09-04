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

// Frame* CreateFrameForMethod(Method* method, Frame* prev);
.extern CreateFrame
// void InterpreterEntryPoint(Method *method, Frame* frame);
.extern InterpreterEntryPoint
// bool IncrementHotnessCounter(Method *method);
.extern IncrementHotnessCounter

.global CompiledCodeToInterpreterBridge
.type CompiledCodeToInterpreterBridge, %function
CompiledCodeToInterpreterBridge:
    CFI_STARTPROC
    CFI_DEF_CFA(sp, 0)

    // construct the frame
    sub sp, sp, #16
    CFI_ADJUST_CFA_OFFSET(16)
    str lr, [sp, #12]
    CFI_REL_OFFSET(lr, 12)
    mov ip, #COMPILED_CODE_TO_INTERPRETER_BRIDGE
    str ip, [sp, #8]
    str fp, [sp, #4]
    CFI_REL_OFFSET(fp, 4)
    add fp, sp, #4
    CFI_DEF_CFA(fp, 12)

    // Save return address to the TLS field
    str lr, [THREAD_REG, #THREAD_NATIVE_PC]

    // save all the callee saved registers to the stack
    // stack walker will read them during stack unwinding
    push {r4 - r11}
    // CFI_REL_OFFSET(r11, -(2 * 4)) - fp already saved on stack
    CFI_REL_OFFSET(r10, -(3 * 4))
    CFI_REL_OFFSET(r9,  -(4 * 4))
    CFI_REL_OFFSET(r8,  -(5 * 4))
    CFI_REL_OFFSET(r7,  -(6 * 4))
    CFI_REL_OFFSET(r6,  -(7 * 4))
    CFI_REL_OFFSET(r5,  -(8 * 4))
    CFI_REL_OFFSET(r4,  -(9 * 4))
    vpush {d8 - d15}
    CFI_REL_OFFSET(d15, -(11 * 4))
    CFI_REL_OFFSET(d14, -(13 * 4))
    CFI_REL_OFFSET(d13, -(15 * 4))
    CFI_REL_OFFSET(d12, -(17 * 4))
    CFI_REL_OFFSET(d11, -(19 * 4))
    CFI_REL_OFFSET(d10, -(21 * 4))
    CFI_REL_OFFSET(d9,  -(23 * 4))
    CFI_REL_OFFSET(d8,  -(25 * 4))

    // Before we call IncrementHotnessCounter we should set pointer to C2I frame in the TLS,
    // because compilation may fall into safepoint, so we need to make caller's callee registers
    // visible for the stack walker.
    str fp, [THREAD_REG, #THREAD_CURRENT_FRAME]

    push {r0 - r3}
    vpush {d0 - d7}

    blx IncrementHotnessCounter
    cmp r0, #0
    beq .Lnot_compiled
    vpop {d0 - d7}
    pop {r0 - r3}

    // Compilation finished, so recover caller's frame in the TLS.
    sub sp, fp, #4
    ldr fp, [sp, #4]
    CFI_REMEMBER_STATE
    CFI_RESTORE(fp)
    CFI_DEF_CFA(sp, 16)

    str fp, [THREAD_REG, #THREAD_CURRENT_FRAME]
    ldr lr, [sp, #12]
    CFI_RESTORE(lr)
    add sp, sp, #16 // c2i
    CFI_ADJUST_CFA_OFFSET(-16)

    ldr pc, [r0, #METHOD_COMPILED_ENTRY_POINT_OFFSET]

    CFI_RESTORE_STATE
    CFI_DEF_CFA(fp, 12)

.Lnot_compiled:
    vpop {d0 - d7}
    pop {r0 - r3}

    // save arguments to the stack
    vpush {d0 - d7}
    push {r0 - r3}

    mov r4, r0 // save method to r4 to survive the call

    // create an interpreter frame
    mov r1, fp
    blx CreateFrameForMethod

    // setup regs as follow
    // r0 - SHORTY_PTR_REG, r1 - SHORTY_REG, r2 - iframe.vregs_ + num_vregs_,
    // r3 - gpr args begin, r4 - gpr args end, r5 - fpr args begin, r6 - fpr args end,
    // r7 - stack args, r8 - iframe, r9, r10 - temps, lr - method
    mov r8, r0
    ldr r5, [r8, #FRAME_NUM_VREGS_OFFSET]
    ldr r0, [r4, #METHOD_NUM_ARGS_OFFSET]
    sub r5, r5, r0
    mov lr, r4
    ldr SHORTY_PTR_REG, [lr, #METHOD_SHORTY_OFFSET]
    INIT_SHORTY_REG
    add r2, r8, #FRAME_VREGS_OFFSET
    add r2, r2, r5, lsl #4
    add r3, sp, #4 // skip Method*
    add r4, sp, #16
    mov r5, r4
    add r6, r5, #64
    add r7, fp, #12

    // parameter 'this' of instance methods is not encoded in the shorty
    // in case of instance method hack SHORTY_REG by replacing the return type by REF
    // in the another case just skip the return type

    // check whether the method is an instance
    ldr r9, [lr, #METHOD_ACCESS_FLAGS_OFFSET]
    tst r9, #ACCESS_STATIC
    bne 1f
    // it is an instance method
    // replace the return type by REF
    bic r1, r1, #0xF // clear the the least significant 4  bits
    orr r1, r1, #SHORTY_REFERENCE
    b .Lloop_copy_args
1:  SKIP_SHORTY

    // fill in the iframe
.Lloop_copy_args:
    NEXT_SHORTY r9
    cmp r9, #0
    beq .Lloopend_copy_args

    cmp r9, #SHORTY_REFERENCE
    moveq r10, #1
    movne r10, #0
    str r10, [r2, #FRAME_VREGISTER_TAG_OFFSET]
    cmpne r9, #SHORTY_LAST_INT32
    bhi 1f
    // it is a 32bit int or reference
    sub r9, r4, r3
    cmp r9, #0
    ldrne r9, [r3], #4
    ldreq r9, [r7], #4
    str r9, [r2]
    add r2, r2, #FRAME_VREGISTER_SIZE
    b .Lloop_copy_args

1:  sub r9, r9, #SHORTY_FIRST_FLOAT
    cmp r9, #(SHORTY_NUM_FLOAT_TYPES - 1)
    bls 2f
    // it is a 64bit int
    //align r3
    add r3, r3, #7
    bic r3, r3, #7
    sub r9, r4, r3
    cmp r9, #0
    beq 3f
    ldm r3!, {r9, r10}
    stm r2, {r9, r10}
    add r2, r2, #FRAME_VREGISTER_SIZE
    b .Lloop_copy_args

2:  // it is a float
    sub r9, r6, r5
    cmp r9, #0
    beq 3f
    ldm r5!, {r9, r10}
    stm r2, {r9, r10}
    add r2, r2, #FRAME_VREGISTER_SIZE
    b .Lloop_copy_args

3:  // read arg from the stack
    add r7, r7, #7
    bic r7, r7, #7
    ldm r7!, {r9, r10}
    stm r2, {r9, r10}
    add r2, r2, #FRAME_VREGISTER_SIZE
    b .Lloop_copy_args
.Lloopend_copy_args:
    // load method.shorty to r4 to survive the call
    ldr r4, [lr, #METHOD_SHORTY_OFFSET]

    // call InterpreterEntryPoint
    mov r0, lr
    mov r1, r8
    blx InterpreterEntryPoint

    // handle the result
    // setup regs as follow
    // r0 - iframe, r1 - *method.shorty, r2 - &iframe.acc_
    // r4, r5 - result
    ldrb r1, [r4]
    mov r0, r8
    add r2, r0, #FRAME_ACC_OFFSET
    and r1, r1, #0xF

    cmp r1, #SHORTY_VOID
    bne 1f
    // void method
    blx FreeFrame
    b 4f

1:  cmp r1, #SHORTY_REFERENCE
    cmpne r1, #SHORTY_LAST_INT32
    bhi 2f
    // it is a 32bit int of reference
    ldr r4, [r2]
    blx FreeFrame
    mov r0, r4
    b 4f

2:  ldm r2, {r4, r5}
    sub r1, r1, #SHORTY_FIRST_FLOAT
    cmp r1, #(SHORTY_NUM_FLOAT_TYPES - 1)
    bls 3f
    // it is a 64bit int
    blx FreeFrame
    mov r0, r4
    mov r1, r5
    b 4f

3:  // it is a float value
    vmov.f64 d0, r4, r5
    blx FreeFrame

4:  sub sp, fp, #(4 + 8 * 4 + 8 * 8)
    vpop {d8 - d15}
    CFI_RESTORE(d15)
    CFI_RESTORE(d14)
    CFI_RESTORE(d13)
    CFI_RESTORE(d12)
    CFI_RESTORE(d11)
    CFI_RESTORE(d10)
    CFI_RESTORE(d9)
    CFI_RESTORE(d8)

    pop {r4 - r11}
    // CFI_RESTORE(r11) - fp will be restored after
    CFI_RESTORE(r10)
    CFI_RESTORE(r9)
    CFI_RESTORE(r8)
    CFI_RESTORE(r7)
    CFI_RESTORE(r6)
    CFI_RESTORE(r5)
    CFI_RESTORE(r4)

    ldr fp, [sp, #4]
    CFI_DEF_CFA(sp, 16)
    CFI_RESTORE(fp)

    ldr lr, [sp, #12]
    CFI_RESTORE(lr)
    add sp, sp, #16 // c2i
    CFI_ADJUST_CFA_OFFSET(-16)
    bx lr
    CFI_ENDPROC
