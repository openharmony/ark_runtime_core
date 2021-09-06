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

.type ExecuteImplStub, @function
.global ExecuteImplStub
.balign 16
ExecuteImplStub:
    .cfi_startproc
    .cfi_def_cfa rsp, __SIZEOF_POINTER__
    // arg0: %rdi: thread
    // arg1: %rsi: PC
    // arg2: %rdx: I-frame
    // arg3: %rcx: impl
    pushq %rbx
    .cfi_adjust_cfa_offset 8
    .cfi_rel_offset %rbx, 0

     /*
     * using CFI opcode to get the interpreter pc from
     * saved register with offset.
     *
     * 0x16 dw_cfa_val_expression
     * 0    tempreg
     * 0x09 size of expression
     * 0x0c dw_op_const4u
     * 0x41, 0x45, 0x58, 0x31 magic = "aex1"
     * 0x13 dw_op_drop
     * 0x92 dw_op_bregx
     * 0x03   the register which store interpreter pc
     * 0    offset
     */
    .cfi_escape 0x16, 0, 0x09, 0x0C, 0x41, 0x45, 0x58, 0x31, 0x13, 0x92, 0x03, 0

    movq %rsi, %rbx
    call *%rcx

    popq %rbx
    .cfi_adjust_cfa_offset -8
    .cfi_restore %rbx
    ret
    .cfi_endproc

