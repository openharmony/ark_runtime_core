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

    // handle calli.dyn.short || calli.dyn
    // regs are set as follow
    // r0 - the pointer to the acc, r1 - num args, r2-r3 - function object as int64_t
    // r4 - insn, r5 - iframe.vregs, r6, r7, r8 - temp, lr - method

    // get num args and function object in r1, r2 and r3
    ldrb r1, [r4], #1
    ldrb r2, [r4], #1
    lsl r2, r2, #8
    orr r1, r1, r2
    ldrb r2, [r4], #1
    ldrb r3, [r4], #1
    lsl r3, r3, #8
    orr r2, r2, r3
    // make r5 = iframe.vregs + arg_idx
    add r5, r5, r2, lsl #4
    ldm r5, {r2, r3}
    add r5, r5, #FRAME_VREGISTER_SIZE

    // setup regs are set as follow
    // r0 - the pointer to the acc, r1 - num args, r2-r3 - function object as int64_t
    // r4 - arg counter, r5 - iframe.vregs + arg_idx, r6 - stack pointer, r7,r8 - temp, lr - method
    mov r4, r1
    sub sp, sp, r1, lsl #4
    mov r6, sp

    // get all the arguments
1:  cmp r4, #0
    beq .Linvoke
    ldm r5!, {r7-r8}
    stm r6!, {r7-r8}
    ldm r5!, {r7-r8}
    stm r6!, {r7-r8}
    sub r4, r4, #1
    b 1b
