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

    // handle call.acc.short || call.virt.acc.short
    // regs contain the following (see runtime/bridge/arch/arm/interpreter_to_compiled_code_bridge_arm.S)
    // r0 - SHORTY_PTR_REG, r1 - SHORTY_REG, r2, r3 - shorty value and temp,
    // r4 - gpr and fpr base ptr, r5 - gpr arg ptr, r6 - fpr arg ptr,
    // r7 - stack arg ptr, r8 - insn ptr, r9 - iframe, r10 - insn,
    // ip - thread, lr - method
    NEXT_SHORTY r2
    cmp r2, #0
    beq .Linvoke_from_bridge

    // read arg index and acc position into r10 and advance insn_ptr
    ldrb r10, [r8], #1

    // check acc position
    asr r3, r10, #4
    cmp r3, #0
    beq 11f

    // 'call vreg, acc' case

    and r3, r10, #0xF
    add r3, r9, r3, lsl #4
    // handle the first arg index
    PUSH_ARG r3, r5, r6, r7, r2, r3, 10f

10: // handle acc
    NEXT_SHORTY r2
    cmp r2, #0
    beq .Linvoke_from_bridge

    sub r3, r9, #(FRAME_VREGS_OFFSET - FRAME_ACC_OFFSET)
    PUSH_ARG r3, r5, r6, r7, r2, r3, .Linvoke_from_bridge

11:
    // 'call acc, vreg' case
    // handle acc

    sub r3, r9, #(FRAME_VREGS_OFFSET - FRAME_ACC_OFFSET)
    PUSH_ARG r3, r5, r6, r7, r2, r3, 12f

12:
    NEXT_SHORTY r2
    cmp r2, #0
    beq .Linvoke_from_bridge
    // handle arg index
    and r3, r10, #0xF
    add r3, r9, r3, lsl #4
    PUSH_ARG r3, r5, r6, r7, r2, r3, .Linvoke_from_bridge
