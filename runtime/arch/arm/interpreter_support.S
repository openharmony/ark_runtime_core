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

.global ExecuteImplStub
.type ExecuteImplStub, %function
.balign 16
/* parameter:
 * r0 thread
 * r1 PC
 * r2 I-frame
 * r3 impl
 */
ExecuteImplStub:
    .cfi_startproc
	.fnstart

	push {r4, lr}
	.cfi_adjust_cfa_offset 8
	.cfi_rel_offset r4, 0
	.cfi_rel_offset lr, 4

	mov r4, r1
	/*
     * Using CFI opcode to get the interpreter PC from
     * saved register with offset.
     *
     * 0x16 DW_CFA_val_expression
     * 0    tempReg
     * 0x09 size of expression
     * 0x0C DW_OP_const4u
     * 0x41, 0x45, 0x58, 0x31 magic = "AEX1"
     * 0x13 DW_OP_drop
     * 0x92 DW_OP_bregx
     * 4   the register which store interpreter PC
     * 0    offset
     */
    .cfi_escape 0x16, 0x0, 0x09, 0x0C, 0x41, 0x45, 0x58, 0x31, 0x13, 0x92, 4, 0x0

	blx r3
	pop {r4, pc}

	.fnend
	.cfi_endproc

