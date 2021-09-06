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
    // r0 - SHORTY_PTR_REG, r1 - SHORTY_REG, r2, r3 - shorty value and temp,
    // r4 - gpr and fpr base ptr, r5 - gpr arg ptr, r6 - fpr arg ptr,
    // r7 - stack arg ptr, r8 - insn ptr, r9 - iframe, r10 - insn,
    // ip - thread, lr - method
.Lloop_handle_call_acc:

    // need one more register, temporarily use ip
    str ip, [fp, #-40]

    // read the pair of reg indexes into r9 and advance insn_ptr
    ldrb ip, [r8], #1
    // read reg index and acc position into r10 and advance insn_ptr
    ldrb r10, [r8], #1
    lsl r10, r10, #8
    orr r10, ip, r10     // r10 now contains v4_v4_v4_imm4
    lsr ip, r10, #12
    and ip, ip, #0xF   // ip now contains acc position

.Lacc_loop:
    NEXT_SHORTY r2
    cmp r2, #0
    beq .Lcall_acc_exit

    // check acc position and branch to acc/vreg index handling
    cmp ip, #0
    beq .Lacc

    // handle vreg index
    and r3, r10, #0xF
    lsr r10, r10, #0x4  // prepare r10 for next index read
    add r3, r9, r3, lsl #4
    PUSH_ARG r3, r5, r6, r7, r2, r3, .Lreg_1
.Lreg_1:
    sub ip, ip, 1
    b .Lacc_loop

.Lacc:
    // handle acc
    sub r3, r9, #(FRAME_VREGS_OFFSET - FRAME_ACC_OFFSET)
    PUSH_ARG r3, r5, r6, r7, r2, r3, .Lacc_1
.Lacc_1:
    sub ip, ip, 1
    b .Lacc_loop

.Lcall_acc_exit:
    ldr ip, [fp, #-40]
    b .Linvoke_from_bridge
