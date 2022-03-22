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

// handle call.acc.short || call.virt.acc.short
// regs contain the following follow (see runtime/bridge/arch/aarch64/interpreter_to_compiled_code_bridge_aarch64.S)
// x0 - SHORTY_PTR_REG, w1 - SHORTY_REG, w2 - shorty value, x3 - stack arg ptr,
// x4 - float arg ptr, x5 - insn, x6 - insn_ptr, x7 - frame.vregs, x9 - arg base ptr
// x10 - gpr arg ptr, x11, x12, x13 - temps, x19 - method.shorty, lr - method
    NEXT_SHORTY w2
    cbz w2, .Lload_reg_args

    // read arg index and acc position into w5 and advance insn_ptr
    ldrb w5, [x6], #1

    // check acc index
    asr w11, w5, #4

    cbz w11, 11f

    // the case call vreg, acc
    // handle the first arg index
    and w11, w5, #0xF
    add x11, x7, x11, lsl #4
    ldp x11, x12, [x11]
    PUSH_ARG x11, x12, x10, x4, x3, x13, 10f

10:
    NEXT_SHORTY w2
    cbz w2, .Lload_reg_args

    sub x11, x7, #(FRAME_VREGS_OFFSET - FRAME_ACC_OFFSET)
    ldp x11, x12, [x11]
    PUSH_ARG x11, x12, x10, x4, x3, x13, .Lload_reg_args
    b .Lload_reg_args

11: // the case call acc, vreg
    // handle the acc
    sub x11, x7, #(FRAME_VREGS_OFFSET - FRAME_ACC_OFFSET)
    ldp x11, x12, [x11]
    PUSH_ARG x11, x12, x10, x4, x3, x13, 12f

12:
    NEXT_SHORTY w2
    cbz w2, .Lload_reg_args

    // handle the reg
    and w11, w5, #0xF
    add x11, x7, x11, lsl #4
    ldp x11, x12, [x11]
    PUSH_ARG x11, x12, x10, x4, x3, x13, .Lload_reg_args
    b .Lload_reg_args

