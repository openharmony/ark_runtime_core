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

.macro LOAD_GPR_ARGS begin_ptr
    ldp x0, x1, [\begin_ptr], #16
    ldp x2, x3, [\begin_ptr], #16
    ldp x4, x5, [\begin_ptr], #16
    ldp x6, x7, [\begin_ptr], #16
.endm

.macro LOAD_FPR_ARGS begin_ptr
    ldp d1, d0, [\begin_ptr, #-16]!
    ldp d3, d2, [\begin_ptr, #-16]!
    ldp d5, d4, [\begin_ptr, #-16]!
    ldp d7, d6, [\begin_ptr, #-16]!
.endm

// The macro works with the stack prepared by 'PrepareArgStack' procedure
// It takes a pair (argument, type tag) and stores to the corresponding stack space
// depends on its type.
// After the arg gets storeg the macro advances the corresponding pointer
// The macro assumes x9 contains base address for gpr and fpr args
.macro PUSH_ARG arg_reg, tag_reg, gpr_ptr, fpr_ptr, stack_ptr, tmp_reg, next_label
    sub w2, w2, #SHORTY_FIRST_FLOAT
    cmp w2, #(SHORTY_NUM_FLOAT_TYPES - 1)
    bls 1f
    // it is an integer arg
    // determine whether there are free gprs
        sub \tmp_reg, \gpr_ptr, x9
    cmp \tmp_reg, #64
    bge 2f
    // there are free gprs
    str \arg_reg, [\gpr_ptr], #8
    // check if the arg has type 'any'
    cmp w2, #(SHORTY_TAGGED - SHORTY_FIRST_FLOAT)
    bne \next_label
    // value of type 'any'
    // store tag if there are free gprs
    cmp \tmp_reg, #56
    bge 3f
    str \tag_reg, [\gpr_ptr], #8
    b \next_label
3:  str \tag_reg, [\stack_ptr], #8
    b \next_label

1:  // it is a float arg
    // determine whether there are free float regs
    sub \tmp_reg, x9, \fpr_ptr
    cmp \tmp_reg, #64
    bge 2f
    // there are free float regs
    str \arg_reg, [\fpr_ptr, #-8]!
    b \next_label

2:  // there are no free regs. It is a stack arg
    str \arg_reg, [\stack_ptr], #8
    // check if the arg has type 'any'
    cmp w2, #(SHORTY_TAGGED - SHORTY_FIRST_FLOAT)
    bne \next_label
    str \tag_reg, [\stack_ptr], #8
.endm

// The procedure reserves stack space for arguments
// The reserved stack space is divided into 3 part:
// 1. the part for arguments passed via the stack
// 2. the part for the arguments passed via GPRs
// 3. the part for the arguments passed via the float registers
// These parts are located as follow:
// +--------------+
// |  fpreg arg7  |
// +--------------+
// |      ...     |
// +--------------+
// |  fpreg arg0  |
// +--------------+ <- x9 (base)
// |   gpr arg0   |
// +--------------+
// |      ...     |
// +--------------+
// |   gpr arg7   |
// +--------------+ <- sp
// |  stack arg0  |
// +--------------+
// |      ...     |
// +--------------+
// |  stack argN  |
// +--------------+
// |      x19     |
// +--------------+
// |  THREAD_REG  |
// +--------------+
// |     fp       |
// +--------------+
// | INTERPRETER_ |
// | TO_COMPILED_ |
// | CODE_BRIDGE  |
// +--------------+ <- fp
// |    iframe    |
// +--------------+
// |      lr      |
// +--------------+
// Input (the following registers must be setup before the call):
// x0 - SHORTY_PTR_REG
// w1 - SHORTY_REG
// w2 - shorty value
// Output (as on the picture)
// x9 - gpr base pointer. Points to the beginning of GPR and FPR args space.
// sp - Points to the last stack argument.
PrepareArgStack:
    // x0 - SHORTY_PTR_REG, w1 - SHORTY_REG, w2 - shorty value, w3 - GPR arg counter,
    // w4 - float arg counter, x5 - stack pointer, w11 - temp
    // As long as we always reserve space for all GP regs we can skip
    // precise arguments counting if there are 3 or less arguments.
    tst w1, 0xf00
    beq .Lfast_path
    // To speed up bridge execution we always reserve space for all 8 GP regs (and 8 FP regs).
    // The stack space should be reserved only for extra parameters (i.e. only for positive w3 and w4 values).
    mov w3, #-7 // 1 register reserved for Method*
    mov w4, #-8

.Lloop_shorty:
    NEXT_SHORTY w2
    cbz w2, .Lexit

    sub w2, w2, #SHORTY_FIRST_FLOAT
    cmp w2, #(SHORTY_NUM_FLOAT_TYPES - 1)
    cinc w4, w4, ls // inc FP regs counter if it's float
    cinc w3, w3, hi // inc GP regs counter if it's not float

    cmp w2, #(SHORTY_TAGGED - SHORTY_FIRST_FLOAT)
    cinc w3, w3, eq // tagged value, use additional GP reg
    b .Lloop_shorty

.Lexit:
    // truncate negative value to 0, leave positive value as is
    bic w5, w3, w3, asr 31
    bic w4, w4, w4, asr 31
    add w5, w5, w4
    sub x5, sp, x5, lsl 3
    // make sp 16 bytes aligned
    bic x5, x5, 0xF
    mov sp, x5
.Lfast_path:
    // reserve stack space for GP regs
    sub x9, sp, #64
    ret

// void InterpreterToCompiledCodeBridge(const BytecodeInstruction* insn, const Frame *iframe, const Method *method, ManagedThread* thread)
.global InterpreterToCompiledCodeBridge
.type InterpreterToCompiledCodeBridge, %function
InterpreterToCompiledCodeBridge:
    CFI_STARTPROC
    CFI_DEF_CFA(sp, 0)

    // Construct bridge frame:
    // lr
    // iframe    <- fp
    // INTERPRETER_TO_COMPILED_CODE_BRIDGE/BYPASS_BRIDGE
    // fp
    // THREAD_REG
    // x19   <- sp

    stp x1, lr, [sp, #-16]!
    CFI_ADJUST_CFA_OFFSET(2 * 8)
    CFI_REL_OFFSET(lr, 8)
    stp x19, THREAD_REG, [sp, #-32]!
    CFI_ADJUST_CFA_OFFSET(4 * 8)
    CFI_REL_OFFSET(THREAD_REG, 8)
    CFI_REL_OFFSET(x19, 0)
    str fp, [sp, #16]
    CFI_REL_OFFSET(fp, 16)
    add fp, sp, #32
    CFI_DEF_CFA(fp, (2 * 8))
    mov THREAD_REG, x3

    // According to the current frame kind set the bridge type
    ldrb w3, [THREAD_REG, #THREAD_FRAME_KIND]
    tst w3, #0x1
    mov w4, #BYPASS_BRIDGE
    mov w5, #INTERPRETER_TO_COMPILED_CODE_BRIDGE
    csel w3, w4, w5, ne
    str x3, [sp, #24]
    // sp should be 16 byte aligned.

    // setup regs as follow
    // x0 - SHORTY_PTR_REG, w1 - SHORTY_REG, w2 - shorty value, x6 - insn_ptr,
    // x7 - frame.vregs, x12 - method, w13 - shorty reg, x19 - method.shorty
    mov x12, x2 // save method to x12 to survive the call
    ldr x19, [x12, #METHOD_SHORTY_OFFSET]
    add x7, x1, #FRAME_VREGS_OFFSET
    mov x6, x0
    mov SHORTY_PTR_REG, x19
    INIT_SHORTY_REG

    // parameter 'this' of instance methods is not encoded in the shorty
    // in case of instance method hack SHORTY_REG by replacing the return type by REF
    // in the another case just skip the return type
    ldr w2, [x12, #METHOD_ACCESS_FLAGS_OFFSET]
    tst w2, #ACCESS_STATIC
    bne 1f
    // it is an instance method
    and w1, w1, #-16 // clear the least significant 4 bits
    mov w2, #SHORTY_REFERENCE
    orr w1, w1, w2
    b 2f
1:  SKIP_SHORTY

2:  mov w13, SHORTY_REG // save value of the shorty reg
    bl PrepareArgStack

    // setup regs as follow
    // x0 - SHORTY_PTR_REG, w1 - SHORTY_REG, w2 - shorty value, x3 - stack arg ptr,
    // x4 - float arg ptr, x5 - insn, x6 - insn_ptr, x7 - frame.vregs, x9 - arg base ptr
    // x10 - gpr arg ptr, x11, x12, x13, x14 - temps, x19 - method.shorty, lr - method
    mov lr, x12 // move method to lr
    mov x3, sp
    mov x4, x9
    mov x10, x9
    add SHORTY_PTR_REG, x19, #2 // since SHORTY_REG contains already read value SHORTY_REG_PTR should be shifted
    mov SHORTY_REG, w13 // restore the value of the shorty reg

    // store method (the first arg)
    str lr, [x10], #8
    ldrb w5, [x6], #1 // read opcode and advance insn_ptr

    // The file contains code which checks opcode and jumps
    // to the corresponding handler.
    // At the end each handler jumps to .Lload_reg_args label.
    // The file is autogenerated from runtime/templates/bridge_dispatch.S.erb
    // Handlers are distinguished by format and located in the corresponding files with name:
    // handle_call_<format>.S
    // If you get a compilation error that there is no such file it seems
    // new call format was introduced and you have to implement the corresponding handler.
#include "bridge_dispatch_aarch64.S"

.Lload_reg_args:
    mov x11, x9
    LOAD_FPR_ARGS x11
    LOAD_GPR_ARGS x9

    // invoke the method
    ldr lr, [lr, #METHOD_COMPILED_ENTRY_POINT_OFFSET]
    blr lr

    // handle the result
    // setup registers as follow
    // x0 - result value, x1 - tag, w2 - shorty[0] & 0xF, x3 - frame.acc, x4 - temp
    ldrb w2, [x19]
    and w2, w2, #0xF

    cmp w2, #SHORTY_VOID
    beq .Lreturn
    ldr x3, [fp] // load iframe from the stack
    add x3, x3, #FRAME_ACC_OFFSET

    // get tag in x1
    cmp w2, #SHORTY_TAGGED
    beq 1f // tag already in x1
    cmp w2, #SHORTY_REFERENCE
    cset x1, eq

1:  sub w4, w2, #SHORTY_FIRST_FLOAT
    cmp w4, #(SHORTY_NUM_FLOAT_TYPES - 1)
    bls .LFLOAT
    sub w4, w2, #SHORTY_FIRST_32
    cmp w4, #(SHORTY_NUM_MIN32_TYPES - 1)
    bls .L32_64 // 64-bit int / ref or 32-bit int
    cmp w2, #SHORTY_I16
    beq .LI16
    bpl .LU16
    cmp w2, #SHORTY_I8
    beq .LI8
    cmp w2, #SHORTY_U1
    beq .LU1
    uxtb x0, w0
    b .L32_64
.LU1:
    and w0, w0 , #0x1
    b .L32_64
.LI8:
    sxtb x0, w0
    b .L32_64
.LI16:
    sxth x0, w0
    b .L32_64
.LU16:
    uxth x0, w0
.L32_64: // store integer value and tag into the acc
    stp x0, x1, [x3]
    b .Lreturn
.LFLOAT:
    // store float value and tag into the acc
    str d0, [x3]
    str x1, [x3, #FRAME_VREGISTER_TAG_OFFSET]

.Lreturn:
    sub sp, fp, #32
    ldp x19, THREAD_REG, [sp], #48
    CFI_RESTORE(THREAD_REG)
    CFI_RESTORE(x19)
    ldr fp, [sp, #-32]
    CFI_RESTORE(fp)
    CFI_DEF_CFA(sp, 0)
    ldr lr, [sp, #-8]
    CFI_RESTORE(lr)
    ret
    CFI_ENDPROC

// (int64_t, int64_t) InvokeCompiledCodeWithArguments(const int64_t* args, const Frame *iframe, const Method *method, ManagedThread* thread)
.global InvokeCompiledCodeWithArgArray
.type InvokeCompiledCodeWithArgArray, %function
InvokeCompiledCodeWithArgArray:
    CFI_STARTPROC
    CFI_DEF_CFA(sp, 0)

    stp x1, lr, [sp, #-16]!
    CFI_ADJUST_CFA_OFFSET(2 * 8)
    CFI_REL_OFFSET(lr, 8)
    stp x19, THREAD_REG, [sp, #-32]!
    CFI_ADJUST_CFA_OFFSET(4 * 8)
    CFI_REL_OFFSET(THREAD_REG, 8)
    CFI_REL_OFFSET(x19, 0)
    str fp, [sp, #16]
    CFI_REL_OFFSET(fp, 16)
    add fp, sp, #32
    CFI_DEF_CFA(fp, (2 * 8))
    mov THREAD_REG, x3

    // According to the current frame kind set the bridge type
    ldrb w3, [THREAD_REG, #THREAD_FRAME_KIND]
    tst w3, #0x1
    mov w4, #BYPASS_BRIDGE
    mov w5, #INTERPRETER_TO_COMPILED_CODE_BRIDGE
    csel w3, w4, w5, ne
    str x3, [sp, #24]
    // sp should be 16 bytes aligned

    // store method.shorty_ to x19
    ldr x19, [x2, #METHOD_SHORTY_OFFSET]

    // check args array
    // it could be null in case the method has no args
    cbnz x0, 1f
    mov lr, x2
    mov x0, x2
    b .Linvoke_with_args

1:  // setup regs as follow
    // x0 - SHORTY_PTR_REG, w1 - SHORTY_REG, w2 - shorty value, x6 - arg_ptr
    // x12 - method, w13 - shorty reg, x19 - method.shorty
    mov x6, x0
    mov SHORTY_PTR_REG, x19
    INIT_SHORTY_REG
    mov x12, x2 // move method to x11 to survive the calls.

    // parameter 'this' of instance methods is not encoded in the shorty
    // in case of instance method hack SHORTY_REG by replacing the return type by REF
    // in the another case just skip the return type
    ldr w2, [x12, #METHOD_ACCESS_FLAGS_OFFSET]
    tst w2, #ACCESS_STATIC
    bne 1f
    // it is an instance method
    and w1, w1, #-16 // clear the least significant 4 bits
    mov w2, #SHORTY_REFERENCE
    orr w1, w1, w2
    b 2f
1:  SKIP_SHORTY

2:  mov w13, SHORTY_REG // save value of the shorty reg
    bl PrepareArgStack

    // setup regs as follow
    // x0 - SHORTY_PTR_REG, w1 - SHORTY_REG, w2 - shorty value, x3 - stack arg ptr,
    // x4 - float arg ptr, x6 - arg_ptr, x9 - arg base ptr, x10 - gpr arg ptr,
    // x11, x12, x13 - temps, x19 - method.shorty, lr - method
    mov lr, x12 // move method to lr
    mov x3, sp
    mov x4, x9
    mov x10, x9
    add SHORTY_PTR_REG, x19, #2 // since SHORTY_REG contains already read value SHORTY_REG_PTR should be shifted
    mov SHORTY_REG, w13 // restore the value of the shorty reg

    // store method (the last arg)
    str lr, [x10], #8

.Lloop_args_push:
    NEXT_SHORTY w2
    cbz w2, .Lloopend_args_push

    ldr x11, [x6], #8
    cmp w2, #SHORTY_TAGGED
    bne 1f
    ldr x12, [x6], #8
1:  PUSH_ARG x11, x12, x10, x4, x3, x13, .Lloop_args_push
    b .Lloop_args_push
.Lloopend_args_push:
    // load arguments into d0-d7 while x12 != x6
    mov x11, x9
    LOAD_FPR_ARGS x11
    LOAD_GPR_ARGS x9

.Linvoke_with_args:
    // invoke the method
    ldr lr, [lr, #METHOD_COMPILED_ENTRY_POINT_OFFSET]
    blr lr

    // handle the result
    // we should return it in x0, x1
    // setup registers as follow
    // x0 - result value, x1 - result tag, w2 - shorty[0] & 0xF, w3 - temp
    ldrb w2, [x19]
    and w2, w2, #0xF

    // get tag in x1
    cmp w2, #SHORTY_TAGGED
    beq 1f
    cmp w2, #SHORTY_REFERENCE
    cset x1, eq

1:  sub w3, w2, #SHORTY_FIRST_FLOAT
    cmp w3, #(SHORTY_NUM_FLOAT_TYPES - 1)
    bls .LFLOAT_
    sub w3, w2, #SHORTY_FIRST_32
    cmp w3, #(SHORTY_NUM_MIN32_TYPES - 1)
    bls .Lreturn_
    cmp w2, #SHORTY_I16
    beq .LI16_
    bpl .LU16_
    cmp w2, #SHORTY_I8
    beq .LI8_
    cmp w2, #SHORTY_U1
    beq .LU1_
    uxtb x0, w0
    b .Lreturn_
.LU1_:
    and w0, w0 , #0x1
    b .Lreturn_
.LI8_:
    sxtb x0, w0
    b .Lreturn_
.LI16_:
    sxth x0, w0
    b .Lreturn_
.LU16_:
    uxth x0, w0
    b .Lreturn_
.LFLOAT_:
    fmov x0, d0
.Lreturn_:
    sub sp, fp, #32
    ldp x19, THREAD_REG, [sp], #48
    CFI_RESTORE(THREAD_REG)
    CFI_RESTORE(x19)
    ldr fp, [sp, #-32]
    CFI_RESTORE(fp)
    CFI_DEF_CFA(sp, 0)
    ldr lr, [sp, #-8]
    CFI_RESTORE(lr)
    ret
    CFI_ENDPROC
