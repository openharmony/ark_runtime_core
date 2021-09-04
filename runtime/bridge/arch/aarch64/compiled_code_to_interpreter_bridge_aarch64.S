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
#include "arch/aarch64/shorty.S"
#include "shorty_values.h"

#define SHORTY_PTR_REG DEFAULT_SHORTY_PTR_REG
#define SHORTY_REG DEFAULT_SHORTY_REG

#include "arch/aarch64/helpers_aarch64.S"

// Frame* CreateFrameForMethod(Method* method, Frame* prev);
.extern CreateFrame
// void InterpreterEntryPoint(Method *method, Frame* frame);
.extern InterpreterEntryPoint

.extern IncrementHotnessCounter

.global CompiledCodeToInterpreterBridge
.type CompiledCodeToInterpreterBridge, %function
CompiledCodeToInterpreterBridge:
    CFI_STARTPROC
    CFI_DEF_CFA(sp, 0)

    // Construct bridge frame:
    // lr
    // COMPILED_CODE_TO_INTERPRETER_BRIDGE
    // fp  <- fp
    // ==  <- sp

    sub sp, sp, #32
    CFI_ADJUST_CFA_OFFSET(4 * 8)
    str lr, [sp, #24]
    CFI_REL_OFFSET(lr, 24)
    mov lr, #COMPILED_CODE_TO_INTERPRETER_BRIDGE
    stp fp, lr, [sp, #8]
    CFI_REL_OFFSET(fp, 8)
    add fp, sp, #8
    CFI_DEF_CFA(fp, (3 * 8))

    // save all the callee saved registers to the stack
    // stack walker will read them during stack unwinding
    PUSH_CALLEE_REGS sp
    CFI_REL_OFFSET(x28, -(2 * 8))
    CFI_REL_OFFSET(x27, -(3 * 8))
    CFI_REL_OFFSET(x26, -(4 * 8))
    CFI_REL_OFFSET(x25, -(5 * 8))
    CFI_REL_OFFSET(x24, -(6 * 8))
    CFI_REL_OFFSET(x23, -(7 * 8))
    CFI_REL_OFFSET(x22, -(8 * 8))
    CFI_REL_OFFSET(x21, -(9 * 8))
    CFI_REL_OFFSET(x20, -(10 * 8))
    CFI_REL_OFFSET(x19, -(11 * 8))
    CFI_REL_OFFSET(d15, -(12 * 8))
    CFI_REL_OFFSET(d14, -(13 * 8))
    CFI_REL_OFFSET(d13, -(14 * 8))
    CFI_REL_OFFSET(d12, -(15 * 8))
    CFI_REL_OFFSET(d11, -(16 * 8))
    CFI_REL_OFFSET(d10, -(17 * 8))
    CFI_REL_OFFSET(d9, -(18 * 8))
    CFI_REL_OFFSET(d8, -(19 * 8))

    // Before we call IncrementHotnessCounter we should set pointer to C2I frame in the TLS,
    // because compilation may fall into safepoint, so we need to make caller's callee registers
    // visible for the stack walker.
    str fp, [THREAD_REG, #THREAD_CURRENT_FRAME]

    PUSH_CALLER_REGS
    PUSH_ARGS_VREGS
    bl IncrementHotnessCounter

    uxtb x0, w0
    cbz x0, .Lnot_compiled

    // Compilation finished, so recover caller's frame in the TLS.
    ldr lr, [fp]
    str lr, [THREAD_REG, #THREAD_CURRENT_FRAME]

    POP_ARGS_VREGS
    POP_CALLER_REGS

    sub sp, fp, #8
    ldr fp, [sp, #8]
    CFI_REMEMBER_STATE
    CFI_RESTORE(fp)
    CFI_DEF_CFA(sp, (4 * 8))

    ldr lr, [sp, #24]
    CFI_RESTORE(lr)
    add sp, sp, #32
    CFI_ADJUST_CFA_OFFSET(-(4 * 8))

    // invoke the method
    ldr x16, [x0, #METHOD_COMPILED_ENTRY_POINT_OFFSET]
    br  x16
    CFI_RESTORE_STATE
    CFI_DEF_CFA(fp, (3 * 8))

.Lnot_compiled:

    POP_ARGS_VREGS
    POP_CALLER_REGS

    // save arguments to the stack
    stp d6, d7, [sp, #-16]!
    stp d4, d5, [sp, #-16]!
    stp d2, d3, [sp, #-16]!
    stp d0, d1, [sp, #-16]!
    stp x6, x7, [sp, #-16]!
    stp x4, x5, [sp, #-16]!
    stp x2, x3, [sp, #-16]!
    stp x0, x1, [sp, #-16]!

    // location of values is the following
    // x20 - method
    mov x20, x0 // save method to survive the call

    // create an interpreter frame
    mov x1, fp
    bl CreateFrameForMethod

    // setup regs as follow
    // x0 - SHORTY_PTR_REG, x1 - SHORTY_REG, x2 - iframe.vregs_ + num_vregs_,
    // x3 - gpr args, x4 - float args, x5 - stack args, w6 - gpr arg counter,
    // w7 - float arg counter, x9, x10 - temp, x19 - iframe, x20 - method,
    // x21 - method.shorty
    mov x19, x0
    ldr x21, [x20, #METHOD_SHORTY_OFFSET]
    ldr w9, [x19, #FRAME_NUM_VREGS_OFFSET]
    ldr w0, [x20, #METHOD_NUM_ARGS_OFFSET]
    sub w9, w9, w0
    mov SHORTY_PTR_REG, x21
    INIT_SHORTY_REG
    add x2, x19, #FRAME_VREGS_OFFSET
    add x2, x2, x9, lsl 4
    add x3, sp, #8 // skip Method*
    add x4, sp, #64
    add x5, fp, #24
    mov w6, #7 // 7 because we skip Method* arg
    mov w7, #8

    // parameter 'this' of instance methods is not encoded in the shorty
    // in case of instance method hack SHORTY_REG by replacing the return type by REF
    // in the another case just skip the return type

    // check whether the method is an instance
    ldr w9, [x20, #METHOD_ACCESS_FLAGS_OFFSET]
    tst w9, #ACCESS_STATIC
    bne 1f
    // it is an instance method
    // replace the return type by REF
    and w1, w1, #-16 // clear the the least significant 4  bits
    mov w9, #SHORTY_REFERENCE
    orr w1, w1, w9
    b .Lloop_copy_args
1:  SKIP_SHORTY

    // fill in the iframe
.Lloop_copy_args:
    NEXT_SHORTY w9
    cbz w9, .Lloopend_copy_args

    // store the tag
    cmp w9, #SHORTY_REFERENCE
    cset w10, eq
    str x10, [x2, #FRAME_VREGISTER_TAG_OFFSET]

    sub w9, w9, #SHORTY_FIRST_FLOAT
    cmp w9, #(SHORTY_NUM_FLOAT_TYPES - 1)
    bls 1f
    // it is an int value of reference
    cbz w6, 2f
    // the arg in a gpr
    ldr x9, [x3], #8
    sub w6, w6, #1
    b 3f
1:  // it is a float value
    cbz w7, 2f
    // the arg in a float reg
    ldr x9, [x4], #8
    sub w7, w7, #1
    b 3f
2:  // it is a stack arg
    ldr x9, [x5], #8
3:
    str x9, [x2], #FRAME_VREGISTER_SIZE
    b .Lloop_copy_args
.Lloopend_copy_args:

    // call InterpreterEntryPoint
    mov x0, x20
    mov x1, x19
    bl InterpreterEntryPoint

    // handle the result
    // setup regs as follow
    // x0 - iframe, x1 - *method.shorty, x2 - &iframe.acc_
    // x19, x20 - result
    ldr w1, [x21]
    mov x0, x19
    add x2, x0, #FRAME_ACC_OFFSET
    and w1, w1, #0xF

    cmp w1, #SHORTY_VOID
    bne 1f
    // void method
    bl FreeFrame
    b 3f
1:
    ldp x19, x20, [x2]
    sub w1, w1, #SHORTY_FIRST_FLOAT
    cmp w1, #(SHORTY_NUM_FLOAT_TYPES - 1)
    bls 2f
    // result is an integer, reference or tagged value
    bl FreeFrame
    mov x0, x19
    mov x1, x20
    b 3f
2:  // result is a float value
    bl FreeFrame
    fmov d0, x19

3:
    sub sp, fp, #8
    // Restore callee registers, since GC may change its values while moving objects.
    mov x16, sp
    ldp x27, x28, [x16, #-16]!
    CFI_RESTORE(x28)
    CFI_RESTORE(x27)
    ldp x25, x26, [x16, #-16]!
    CFI_RESTORE(x26)
    CFI_RESTORE(x25)
    ldp x23, x24, [x16, #-16]!
    CFI_RESTORE(x24)
    CFI_RESTORE(x23)
    ldp x21, x22, [x16, #-16]!
    CFI_RESTORE(x22)
    CFI_RESTORE(x21)
    ldp x19, x20, [x16, #-16]!
    CFI_RESTORE(x20)
    CFI_RESTORE(x19)

    ldr fp, [sp, #8]
    CFI_RESTORE(fp)
    CFI_DEF_CFA(sp, (4 * 8))
    ldr lr, [sp, #24]
    CFI_RESTORE(lr)
    add sp, sp, #32
    CFI_ADJUST_CFA_OFFSET(-(4 * 8))
    ret
    CFI_ENDPROC
