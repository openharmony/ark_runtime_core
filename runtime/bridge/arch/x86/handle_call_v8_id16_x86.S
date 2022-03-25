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

    // handle call.range || call.virt.range
    // setup regs and memory as follow
    // %eax - SHORTY_PTR_REG, %edx - SHORTY_REG, %ecx - shorty value, (%esp) - insn_ptr, 
    // 4(%esp) - iframe->vregs[], %edi - pointer to stack
    movl (%esp), %esi // fill insn*
    movzbl (%esi), %ebx
    movl %esi, (%esp) // spill insn*

    shll $4, %ebx
    addl 4(%esp), %ebx     

.Lloop_handle_call_range:
    NEXT_SHORTY %ecx
    cmpl $0, %ecx
    je .Linvoke_from_bridge

    // handle the arg
    movl %eax, 8(%esp)
    movl %edx, 12(%esp)
    PUSH_ARG %ebx, %edi, %eax, %edx 
    movl 8(%esp), %eax
    movl 12(%esp), %edx

    addl $FRAME_VREGISTER_SIZE, %ebx
    jmp .Lloop_handle_call_range
.Lloopend_handle_call_range:

