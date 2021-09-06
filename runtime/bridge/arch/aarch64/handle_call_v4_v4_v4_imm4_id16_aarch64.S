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

// handle call.acc || call.virt.acc
// regs contain the following follow (see runtime/bridge/arch/aarch64/interpreter_to_compiled_code_bridge_aarch64.S)
// x0 - SHORTY_PTR_REG, w1 - SHORTY_REG, w2 - shorty value, x3 - stack arg ptr,
// x4 - float arg ptr, x5 - insn, x6 - insn_ptr, x7 - frame.vregs, x9 - arg base ptr
// x10 - gpr arg ptr, x11, x12, x13, x14 - temps, x19 - method.shorty, lr - method
.Lloop_handle_call_acc:
    // read the pair of reg indexes into w14 and advance insn_ptr
    ldrb w14, [x6], #1
    // read reg index and acc_pos into w5 and advance insn_ptr
    ldrb w5, [x6], #1
    orr w5, w14, w5, lsl #8  // w5 now contains v4_v4_v4_imm4
    lsr w14, w5, #12
    and w14, w14, #0xF       // w14 now contains acc position

.Lacc_loop:
    NEXT_SHORTY w2
    cbz w2, .Lload_reg_args

    // check acc pos and branch to acc/vreg index handling
    cbz w14, .Lacc

    // handle vreg index
    and w11, w5, #0xF
    lsr w5, w5, #0x4  // prepare w5 for next index read
    add x11, x7, x11, lsl #4
    ldp x11, x12, [x11]
    PUSH_ARG x11, x12, x10, x4, x3, x13, .Lreg_1
.Lreg_1:
    sub w14, w14, 1
    b .Lacc_loop

.Lacc:
    // handle acc
    sub x11, x7, #(FRAME_VREGS_OFFSET - FRAME_ACC_OFFSET)
    ldp x11, x12, [x11]
    PUSH_ARG x11, x12, x10, x4, x3, x13, .Lacc_1
.Lacc_1:
    sub w14, w14, 1
    b .Lacc_loop
