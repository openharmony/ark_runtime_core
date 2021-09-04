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
    // regs contain the following (see runtime/bridge/arch/arm/interpreter_to_compiled_code_bridge_arm.S)
    // r0 - SHORTY_PTR_REG, r1 - SHORTY_REG, r2 - shorty value, r3 - insn, r4 - insn_ptr,
    // r5 - iframe, r6, r7 and r9 - temps, r8 - pointer to stack, lr - method
.Lloop_handle_call_acc:

    // read the pair of reg indexes into r9 and advance insn_ptr
    ldrb r9, [r4], #1
    // read reg index and acc position into r3 and advance insn_ptr
    ldrb r3, [r4], #1
    lsl r3, r3, #8
    orr r3, r9, r3     // r3 now contains v4_v4_v4_imm4
    lsr r9, r3, #12
    and r9, r9, #0xF   // r9 now contains acc position

.Lacc_loop:
    NEXT_SHORTY r2
    cmp r2, #0
    beq .Linvoke_from_bridge

    // check acc position and branch to acc/vreg index handling
    cmp r9, #0
    beq .Lacc

    // handle vreg index
    and r6, r3, #0xF
    lsr r3, r3, #0x4  // prepare r3 for next index read
    add r6, r5, r6, lsl #4
    PUSH_ARG r6, r8, r6, r7, .Lreg_1
.Lreg_1:
    sub r9, r9, 1
    b .Lacc_loop

.Lacc:
    // handle acc
    sub r6, r5, #(FRAME_VREGS_OFFSET - FRAME_ACC_OFFSET)
    PUSH_ARG r6, r8, r6, r7, .Lacc_1
.Lacc_1:
    sub r9, r9, 1
    b .Lacc_loop
