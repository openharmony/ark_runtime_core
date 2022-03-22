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
// regs set as follow
// x0-x7 - arguments, x9 - frame.vregs, x10 - insn_ptr, x12 - method, x19 - frame
// x13, x14, x15 - temp

    mov x0, x12
    // get number of args in w1 and function object in x2 and tag in x3
    ldrb w13, [x10], 1
    and w1, w13, 0xF
    and w2, w13, -16
    add x2, x9, x2
    ldp x2, x3, [x2]
    cbz w1, .Linvoke

    // there are some arguments
    // load the first argument into x4, x5
    ldrb w13, [x10], 1
    and w4, w13, 0xF
    add x4, x9, x4, lsl 4
    ldp x4, x5, [x4]
    cmp w1, 1
    beq .Linvoke // there is only one argument

    // there are two arguments
    // load the second arg into x6, x7
    and w6, w13, -16
    add x6, x9, x6
    ldp x6, x7, [x6]
    cmp w1, 2
    beq .Linvoke // there are two arguments

    cmp w1, 4
    beq .Lfour_args
 // there are three arguments
    // load the 3rd arg into the stack
    ldrb w13, [x10], 1
    and w14, w13, 0xF
    add x14, x9, x14, lsl 4
    ldp x14, x15, [x14]
    stp x14, x15, [sp, -16]!
    cmp w1, 3
    beq .Linvoke // there are three arguments

.Lfour_args:
    // there are four arguments
    // load the 3rd arg into the stack
    ldrb w13, [x10], 1
    and w14, w13, 0xF
    add x14, x9, x14, lsl 4
    ldp x14, x15, [x14]
    stp x14, x15, [sp, -32]!
    // load the 4th arg into the stack
    and w14, w13, -16
    add x14, x9, x14
    ldp x14, x15, [x14]
    stp x14, x15, [sp, 16]
    b .Linvoke
