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
.extern CreateFrameForMethod
// void InterpreterEntryPoint(Method *method, Frame* frame);
.extern InterpreterEntryPoint
// bool IncrementHotnessCounter(Method *method);
.extern IncrementHotnessCounter

.global CompiledCodeToInterpreterBridge
.type CompiledCodeToInterpreterBridge, %function
CompiledCodeToInterpreterBridge:
    CFI_STARTPROC
    CFI_DEF_CFA(sp, 0)

    // store r0-r3 before the frame to make arg array continuos with stack args
    push {r0-r3}
    CFI_ADJUST_CFA_OFFSET(16)

    // construct the frame
    mov r1, #COMPILED_CODE_TO_INTERPRETER_BRIDGE
    push {r1, lr}
    CFI_ADJUST_CFA_OFFSET(8)
    CFI_REL_OFFSET(lr, 4)
    push {r1, fp}
    CFI_ADJUST_CFA_OFFSET(8)
    CFI_REL_OFFSET(fp, 4)
    add fp, sp, #4
    CFI_ADJUST_CFA_OFFSET(-4)
    CFI_DEF_CFA_REGISTER(fp)

    // Save return address to the TLS field
    str lr, [THREAD_REG, #THREAD_NATIVE_PC]

    // save all the callee saved registers to the stack
    // stack walker will read them during stack unwinding
    push {r4 - r11}
    // CFI_REL_OFFSET(r11, -(2 * 4)) - fp already pushed
    CFI_REL_OFFSET(r10, -(3 * 4))
    CFI_REL_OFFSET(r9,  -(4 * 4))
    CFI_REL_OFFSET(r8,  -(5 * 4))
    CFI_REL_OFFSET(r7,  -(6 * 4))
    CFI_REL_OFFSET(r6,  -(7 * 4))
    CFI_REL_OFFSET(r5,  -(8 * 4))
    CFI_REL_OFFSET(r4,  -(9 * 4))
    vpush {d8 - d15}
    CFI_REL_OFFSET(d15, -(10 * 4))
    CFI_REL_OFFSET(d14, -(12 * 4))
    CFI_REL_OFFSET(d13, -(14 * 4))
    CFI_REL_OFFSET(d12, -(16 * 4))
    CFI_REL_OFFSET(d11, -(18 * 4))
    CFI_REL_OFFSET(d10, -(20 * 4))
    CFI_REL_OFFSET(d9,  -(22 * 4))
    CFI_REL_OFFSET(d8,  -(24 * 4))

    // Before we call IncrementHotnessCounter we should set pointer to C2I frame in the TLS,
    // because compilation may fall into safepoint, so we need to make caller's callee registers
    // visible for the stack walker.
    str fp, [THREAD_REG, #THREAD_CURRENT_FRAME]

    push {r0 - r3}
    blx IncrementHotnessCounter

    cmp r0, #0
    beq .Lnot_compiled

    pop {r0 - r3}

    // Compilation finished, so recover caller's frame in the TLS.
    sub sp, fp, #4
    CFI_REMEMBER_STATE
    CFI_DEF_CFA(sp, 32)

    pop {r1, fp}
    CFI_ADJUST_CFA_OFFSET(-8)
    CFI_RESTORE(fp)

    str fp, [THREAD_REG, #THREAD_CURRENT_FRAME]
    pop {r1, lr}
    CFI_ADJUST_CFA_OFFSET(-8)
    CFI_RESTORE(lr)
    pop {r0-r3}
    CFI_ADJUST_CFA_OFFSET(-16)

    ldr pc, [r0, #METHOD_COMPILED_ENTRY_POINT_OFFSET]
    CFI_RESTORE_STATE
    CFI_DEF_CFA(fp, 28)

.Lnot_compiled:
    pop {r0 - r3}

    // setup regs as follow
    // r7 - method
    mov r7, r0 // save method to survive the call

    // create an interpreter frame
    mov r1, fp
    blx CreateFrameForMethod

    // setup regs as follow
    // r0 - SHORTY_PTR_REG, r1 - SHORTY_REG, r2 - iframe.vregs_ + num_vregs_,
    // r3 - args, r4, r5 - temp, r6 - iframe, r7 - method, r8 - method.shorty
    mov r6, r0
    ldr r5, [r6, #FRAME_NUM_VREGS_OFFSET]
    ldr r0, [r7, #METHOD_NUM_ARGS_OFFSET]
    sub r5, r5, r0
    ldr r8, [r7, #METHOD_SHORTY_OFFSET]
    mov SHORTY_PTR_REG, r8
    INIT_SHORTY_REG
    add r2, r6, #FRAME_VREGS_OFFSET
    add r2, r2, r5, lsl #4
    add r3, fp, #16

    // parameter 'this' of instance methods is not encoded in the shorty
    // in case of instance method hack SHORTY_REG by replacing the return type by REF
    // in the another case just skip the return type

    // check whether the method is an instance
    ldr r4, [r7, #METHOD_ACCESS_FLAGS_OFFSET]
    tst r4, #ACCESS_STATIC
    bne 1f
    // it is an instance method
    // replace the return type by REF
    bic r1, r1, #0xF // clear the the least significant 4  bits
    orr r1, r1, #SHORTY_REFERENCE
    b .Lloop_copy_args
1:  SKIP_SHORTY

    // fill in the iframe
.Lloop_copy_args:
    NEXT_SHORTY r4
    cmp r4, #0
    beq .Lloopend_copy_args

    sub r4, r4, #SHORTY_FIRST_64
    cmp r4, #(SHORTY_NUM_64BIT_TYPES - 1)
    bls 1f
    // it is a 32bit value of reference
    // store the tag
    cmp r4, #(SHORTY_REFERENCE - SHORTY_FIRST_64)
    movne r5, #FRAME_VREGISTER_PRIMITIVE_TAG
    moveq r5, #FRAME_VREGISTER_OBJECT_TAG
    str r5, [r2, #FRAME_VREGISTER_TAG_OFFSET]
    // store the value
    ldr r4, [r3], #4
    str r4, [r2]
    b 2f
1:  // it is a 64bit value
    // make r3 8 bytes aligned
    add r3, r3, #7
    bic r3, r3, #7
    ldm r3!, {r4, r5}
    stm r2, {r4, r5}
    mov r4, #FRAME_VREGISTER_PRIMITIVE_TAG
    str r4, [r2, #FRAME_VREGISTER_TAG_OFFSET]
2:
    add r2, r2, #FRAME_VREGISTER_SIZE
    b .Lloop_copy_args
.Lloopend_copy_args:

    // call InterpreterEntryPoint
    mov r0, r7
    mov r1, r6
    blx InterpreterEntryPoint

    // handle the result
    // setup regs as follow
    // r0 - iframe, r1 - *method.shorty, r2 - &iframe.acc_
    // r4, r5 - result
    mov r0, r6
    ldrb r1, [r8]
    add r2, r6, #FRAME_ACC_OFFSET
    and r1, r1, #0xF

    cmp r0, #SHORTY_VOID
    bne 1f
    // void method
    blx FreeFrame
    b 3f
1:
    sub r1, r1, #SHORTY_FIRST_64
    cmp r1, #(SHORTY_NUM_64BIT_TYPES - 1)
    bls 2f
    // result is 32 bit integer or reference
    ldr r4, [r2]
    blx FreeFrame
    mov r0, r4
    b 3f
2:  // result is 64bit integer
    ldm r2, {r4, r5}
    blx FreeFrame
    mov r0, r4
    mov r1, r5

3:
    sub sp, fp, #(4 + 8 * 4 + 8 * 8)
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
    // CFI_RESTORE(r11) fp - will restore down
    CFI_RESTORE(r10)
    CFI_RESTORE(r9)
    CFI_RESTORE(r8)
    CFI_RESTORE(r7)
    CFI_RESTORE(r6)
    CFI_RESTORE(r5)
    CFI_RESTORE(r4)

    ldr fp, [sp, #4]
    CFI_DEF_CFA(sp, 32)
    CFI_RESTORE(fp)

    ldr lr, [sp, #12]
    CFI_RESTORE(lr)
    add sp, sp, #32 // c2i + r0 - r3
    CFI_ADJUST_CFA_OFFSET(-32)
    bx lr
    CFI_ENDPROC
