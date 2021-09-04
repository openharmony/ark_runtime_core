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

    // handle call.range || call.virt.range
    // setup regs as follow
    // x0 - SHORTY_PTR_REG, w1 - SHORTY_REG, w2 - shorty value, x3 - stack arg ptr,
    // x4 - float arg ptr, x6 - arg_ptr, x7 - frame.vregs, x9 - arg base ptr
    // x10 - gpr arg ptr, x11, x12, x13 - temps, x19 - method.shorty, lr - method
    ldrb w6, [x6]
    add x6, x7, x6, lsl #4

.Lloop_handle_call_range:
    NEXT_SHORTY w2
    cbz w2, .Lload_reg_args

    ldp x11, x12, [x6], #FRAME_VREGISTER_SIZE

    // handle the arg
    PUSH_ARG x11, x12, x10, x4, x3, x13, .Lloop_handle_call_range
    b .Lloop_handle_call_range

