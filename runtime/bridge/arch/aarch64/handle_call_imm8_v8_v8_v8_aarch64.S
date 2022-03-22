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

// Handler of CALL2DYN_IMM8_V8_V8_V8 (builtin.quatern4)

    // get number of args in w1
    mov w1, 4 // new.target, this, arg1, arg2
    // get method in x0
    mov x0, x12
    //get function object and tag in x2, x3
    ldrb w2, [x10, 1]
    add x2, x9, x2, lsl 4
    ldp x2, x3, [x2]
    // get undefined in x4,x5 (new.target) and x6, x7 (this)
    mov x4, 0
    mov x5, 0
    mov x6, 0
    mov x7, 0
    // get arg2
    ldrb w15, [x10, 3]
    add x15, x9, x15, lsl 4
    ldp x13, x14, [x15]
    stp x13, x14, [sp, -16]!
    // get arg1
    ldrb w15, [x10, 2]
    add x15, x9, x15, lsl 4
    ldp x13, x14, [x15]
    stp x13, x14, [sp, -16]!
    b .Linvoke
