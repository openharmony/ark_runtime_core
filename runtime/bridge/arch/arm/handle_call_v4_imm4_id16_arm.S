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
    // regs contain the following (see runtime/bridge/arch/arm/interpreter_to_compiled_code_bridge_arm.S)
    // r0 - SHORTY_PTR_REG, r1 - SHORTY_REG, r2 - shorty value, r3 - insn, r4 - insn_ptr,
    // r5 - iframe, r6 and r7 - temps, r8 - pointer to stack, lr - method
    NEXT_SHORTY r2
    cmp r2, #0
    beq .Linvoke_from_bridge

    // read arg index and acc position into r3 and advance insn_ptr
    ldrb r3, [r4], #1

    // check acc position
    asr r6, r3, #4
    cmp r6, #0
    beq 11f

    // 'call vreg, acc' case

    and r6, r3, #0xF
    add r6, r5, r6, lsl #4
    // handle the first arg index
    PUSH_ARG r6, r8, r6, r7, 10f

10: // handle acc
    NEXT_SHORTY r2
    cmp r2, #0
    beq .Linvoke_from_bridge

    sub r6, r5, #(FRAME_VREGS_OFFSET - FRAME_ACC_OFFSET)
    PUSH_ARG r6, r8, r6, r7, .Linvoke_from_bridge

11:
    // 'call acc, vreg' case
    // handle acc

    sub r6, r5, #(FRAME_VREGS_OFFSET - FRAME_ACC_OFFSET)
    PUSH_ARG r6, r8, r6, r7, 12f

12:
    NEXT_SHORTY r2
    cmp r2, #0
    beq .Linvoke_from_bridge
    // handle arg index
    and r6, r3, #0xF
    add r6, r5, r6, lsl #4
    PUSH_ARG r6, r8, r6, r7, .Linvoke_from_bridge
