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

    // handle initobj.short || initobj

    NEXT_SHORTY r2
    cmp r2, #0
    beq .Linvoke_from_bridge

    sub r6, r5, #(FRAME_VREGS_OFFSET - FRAME_ACC_OFFSET)
    PUSH_ARG r6, r8, r6, r7, .Lloop_handle_initobj

.Lloop_handle_initobj:
    NEXT_SHORTY r2
    cmp r2, #0
    beq .Linvoke_from_bridge

    // read the pair of arg indexes into r3 and advance insn_ptr
    ldrb r3, [r4], #1
    and r6, r3, #0xF
    add r6, r5, r6, lsl #4

    // handle the first arg index
    PUSH_ARG r6, r8, r6, r7, 10f

10: // handle the second arg index
    NEXT_SHORTY r2
    cmp r2, #0
    beq .Linvoke_from_bridge

    asr r6, r3, #4
    add r6, r5, r6, lsl #4

    PUSH_ARG r6, r8, r6, r7, .Lloop_handle_initobj
