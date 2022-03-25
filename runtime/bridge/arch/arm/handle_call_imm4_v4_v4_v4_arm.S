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
    ldrb r6, [r4], #1
    and r1, r6, #0xF
    and r6, r6, #0xF0
    add r6, r5, r6
    ldm r6, {r2-r3}

    cmp r1, #0
    beq .Linvoke // there is no arguments

    // there are some argumetns
    // push them to the stack
    sub sp, sp, r1, lsl #4

    // get the first argument
    ldrb r6, [r4]
    and r6, r6, #0xF
    add r6, r5, r6, lsl #4
    ldm r6!, {r7-r8}
    stm sp!, {r7, r8}
    ldm r6, {r7-r8}
    stm sp!, {r7-r8}
    cmp r1, #1
    subeq sp, sp, r1, lsl #4
    beq .Linvoke

    // get the second argument
    ldrb r6, [r4], #1
    and r6, r6, #0xF0
    add r6, r5, r6
    ldm r6!, {r7-r8}
    stm sp!, {r7, r8}
    ldm r6, {r7-r8}
    stm sp!, {r7-r8}
    cmp r1, #2
    subeq sp, sp, r1, lsl #4
    beq .Linvoke

    // get the third argument
    ldrb r6, [r4]
    and r6, r6, #0xF
    add r6, r5, r6, lsl #4
    ldm r6!, {r7-r8}
    stm sp!, {r7, r8}
    ldm r6, {r7-r8}
    stm sp!, {r7-r8}
    cmp r1, #3
    subeq sp, sp, r1, lsl #4
    beq .Linvoke

    // get the fourth argument
    ldrb r6, [r4], #1
    and r6, r6, #0xF0
    add r6, r5, r6
    ldm r6!, {r7-r8}
    stm sp!, {r7, r8}
    ldm r6, {r7-r8}
    stm sp!, {r7-r8}
    subeq sp, sp, r1, lsl #4
    beq .Linvoke
