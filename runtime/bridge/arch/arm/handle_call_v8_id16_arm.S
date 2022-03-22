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
    // r0 - SHORTY_PTR_REG, r1 - SHORTY_REG, r2 - shorty value, r3 - iframe.vregs + arg_idx,
    // r6 and r7 - temps, r8 - pointer to stack, lr - method
    ldrb r3, [r4]
    add r3, r5, r3, lsl #4

.Lloop_handle_call_range:
    NEXT_SHORTY r2
    cmp r2, #0
    beq .Linvoke_from_bridge

    // handle the arg
    mov r6, r3
    add r3, r3, #FRAME_VREGISTER_SIZE
    PUSH_ARG r6, r8, r6, r7, .Lloop_handle_call_range
