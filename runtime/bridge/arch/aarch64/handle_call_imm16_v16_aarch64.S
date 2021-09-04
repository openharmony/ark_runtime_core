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

// handle calli.dyn.range
// regs set as follow
// x0-x7 - arguments, x9 - frame.vregs, x10 - insn_ptr, x12 - method, x19 - frame
// x13, x14, x15 - temp

    // get number of args in w1
    ldrb w0, [x10], 1
    ldrb w1, [x10], 1
    ldrb w2, [x10], 1
    ldrb w3, [x10]
    lsl w1, w1, 8
    orr w1, w0, w1
    // get frame.vregs + vreg_idx in x9
    lsl w3, w3, 8
    orr w2, w2, w3
    add x9, x9, x2, lsl 4
    // get method in x0
    mov x0, x12
    //get function object and tag in x2, x3
    ldp x2, x3, [x9], FRAME_VREGISTER_SIZE

    // setup regs as follow
    // x0-x7 - arguments, x9 - frame.vregs + vreg_idx, x10 - arg counter, x12 - method, x19 - frame
    // x13, x14, x15 - temp
    mov w10, w1
    // load arguments into the registers
    cbz w10, .Linvoke
    ldp x4, x5, [x9], FRAME_VREGISTER_SIZE
    sub w10, w10, 1
    cbz w10, .Linvoke
    ldp x6, x7, [x9], FRAME_VREGISTER_SIZE
    sub w10, w10, 1
    cbz w10, .Linvoke
    // reserve stack space for the argument
    sub sp, sp, x10, lsl 4
    // x13 - stack pointer
    mov x13, sp
    // push the rest arguments to the stack
1:  ldp x14, x15, [x9], FRAME_VREGISTER_SIZE
    stp x14, x15, [x13], FRAME_VREGISTER_SIZE
    sub w10, w10, 1
    cbnz w10, 1b
    b .Linvoke
