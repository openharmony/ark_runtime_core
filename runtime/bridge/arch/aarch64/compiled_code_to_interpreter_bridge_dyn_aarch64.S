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
#include "arch/aarch64/helpers_aarch64.S"

// Frame* CreateFrameForMethodWithActualArgs(uint32_t num_actual_args, Method* method, Frame* prev);
.extern CreateFrameForMethodWithActualArgs
// void FreeFrame(Frame* frame)
.extern FreeFrame
// void InterpreterEntryPoint(Method *method, Frame* frame);
.extern InterpreterEntryPoint
// bool IncrementHotnessCounter(Method*)
.extern IncrementHotnessCounter
// DecodedTaggedValue GetInitialTaggedValue(Method*)
.extern GetInitialTaggedValue

.global CompiledCodeToInterpreterBridgeDyn
.type CompiledCodeToInterpreterBridgeDyn, %function
CompiledCodeToInterpreterBridgeDyn:
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

    PUSH_ARGS_REGS

    bl IncrementHotnessCounter

    // Compilation finished, so recover caller's frame in the TLS.
    ldr lr, [fp]
    str lr, [THREAD_REG, #THREAD_CURRENT_FRAME]

    uxtb x0, w0
    cbz x0, .Lnot_compiled

    POP_ARGS_REGS

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
    // Restore x0 and x1 args since they contain Method* and actual_num_args
    ldp x0, x1, [sp]

    // setup regs as follow to survive the call
    // x19 - actual_num_args, x20 - method
    mov x20, x0
    add w0, w1, #1 // count function object
    mov w23, w0

    // create an interpreter frame
    // get max(method->num_args, num_actual_args) in x0
    // and rest_num_args in w22 (rest_num_args = max(method->num_args - actual_num_args, 0))
    ldr w19, [x20, #METHOD_NUM_ARGS_OFFSET]
    cmp w19, w0
    csel w19, w0, w19, ls
    sub w22, w19, w0
    mov x1, x20
    mov x2, fp
    bl CreateFrameForMethodWithActualArgs

    // setup regs as follow
    // w0 - actual_num_args, x1 - args, x9, x10 - temp,
    // x19 - iframe.vregs_ + num_vregs_, x20 - method,
    // x21 - iframe, w22 - rest_num_args
    mov x21, x0
    ldr w9, [x21, #FRAME_NUM_VREGS_OFFSET]
    ldr w0, [x20, #METHOD_NUM_ARGS_OFFSET]
    sub w9, w9, w0
    mov w0, w23
    add x19, x21, #FRAME_VREGS_OFFSET
    add x19, x19, x9, lsl 4
    add x1, fp, #24

    // copy function object (value and tag)
    ldp x9, x10, [sp, #16]
    stp x9, x10, [x19], #16
    sub w0, w0, #1

    // fill in the iframe
    // copy register arguments to the frame
    cbz w0, 2f
    // copy x4, x5
    ldp x9, x10, [sp, #32]
    stp x9, x10, [x19], #16
    sub w0, w0, #1

    cbz w0, 2f
    // copy x6, x7
    ldp x9, x10, [sp, #48]
    stp x9, x10, [x19], #16
    sub w0, w0, #1

    // copy stack arguments into the frame
1:  cbz w0, 2f
    ldp x9, x10, [x1], #16
    stp x9, x10, [x19], #16
    sub w0, w0, #1
    b 1b

    // fill in the rest args (the arguments which are declared but not specified in the call instruction)
2:  cbz w22, 4f
    // setup regs as follow
    // x0, x1 - initial value, x19 - iframe.vregs_ + num_vregs_, x20 - method,
    // x21 - iframe, w22 - rest_num_args

    // get initial value in x0, x1
    mov x0, x20
    bl GetInitialTaggedValue
3:  cbz w22, 4f
    stp x0, x1, [x19], #16
    sub w22, w22, #1
    b 3b

4:  // call InterpreterEntryPoint
    mov x0, x20
    mov x1, x21
    bl InterpreterEntryPoint

    // handle the result
    // setup regs as follow
    // x0 - iframe, x19, x20 - result, x21 - iframe.acc_
    mov x0, x21
    add x21, x21, #FRAME_ACC_OFFSET
    ldp x19, x20, [x21]

    bl FreeFrame
    mov x0, x19
    mov x1, x20

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
