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

// regs set as follow
// x0-x7 - arguments, x9 - frame.vregs, x10 - insn_ptr, x12 - method, x19 - frame
// x13, x14, x15 - temp

// Handler of CALLIRANGEDYN_IMM8_IMM16_V8 and CALLITHISRANGEDYN_IMM8_IMM16_V8 (builtin.r2i)

    // get builtin_id in w15
    ldrb w15, [x10], 1
    // get number of args in w1
    ldrb w1, [x10], 1
    ldrb w2, [x10], 1
    lsl w2, w2, 8
    orr w1, w1, w2
    add w1, w1, 2 // new.target, this
    // get method in x0
    mov x0, x12
    // get function object and tag in x2, x3
    ldrb w2, [x10]
    // setup regs as follow
    // x9 - frame.vregs + vreg_idx, x10 - stack args count, x12 - method, x19 - frame
    // x13, x14, x15 - temp
    add x9, x9, x2, lsl 4
    sub w10, w1, 2 // ignore 'new.target' and 'this' since they are in registers
    ldp x2, x3, [x9], FRAME_VREGISTER_SIZE
    // get undefined in x4,x5 (new.target)
    mov x4, 0
    mov x5, 0
    // get this
    // set undefined to 'this'
    mov x6, 0
    mov x7, 0
    cmp w15, 4 // 4 - builtin_id of CALLITHISRANGEDYN_IMM8_IMM16_V8
    bne 1f
    // it is 'CALLITHISRANGEDYN_IMM8_IMM16_V8'
    sub w1, w1, 1
    sub w10, w10, 1
    ldp x6, x7, [x9], FRAME_VREGISTER_SIZE


1:  // get the rest arguments
    // setup regs as follow
    // x0-x7 - arguments, x9 - frame.vregs + vreg_idx, x10 - stack arg counter, x12 - method, x19 - frame
    // x13, x14, x15 - temp
    // reserve stack space
    // x15 - args ptr
    sub sp, sp, x10, lsl 4
    mov x15, sp
2:  cbz w10, .Linvoke
    ldp x13, x14, [x9], FRAME_VREGISTER_SIZE
    stp x13, x14, [x15], FRAME_VREGISTER_SIZE
    sub w10, w10, 1
    b 2b
