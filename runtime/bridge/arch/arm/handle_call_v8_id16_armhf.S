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

.Lhandle_call_range:
    // handle call.range || call.virt.range
    // setup regs as follow
    // r0 - SHORTY_PTR_REG, r1 - SHORTY_REG, r2, r3 - shorty value and temp,
    // r4 - gpr and fpr base ptr, r5 - gpr arg ptr, r6 - fpr arg ptr,
    // r7 - stack arg ptr, r8 - frame.vregs + arg_idx, ip - thread, lr - method
    ldrb r8, [r8]
    add r8, r9, r8, lsl #4

.Lloop_handle_call_range:
    NEXT_SHORTY r2
    cmp r2, #0
    beq .Linvoke_from_bridge

    // handle the arg
    PUSH_ARG r8, r5, r6, r7, r2, r3, 10f
10: add r8, r8, #FRAME_VREGISTER_SIZE
    b .Lloop_handle_call_range
