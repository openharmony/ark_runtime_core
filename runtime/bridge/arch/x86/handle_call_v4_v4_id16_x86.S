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

    // handle call.short || call || call.virt.short || call.virt
.Lloop_handle_call:
    NEXT_SHORTY %ecx
    cmpl $0, %ecx
    je .Linvoke_from_bridge

    // read the pair of arg indexes into %ebx and advance insn_ptr
    movl (%esp), %esi // fill insn*
    movzbl (%esi), %ebx
    addl $1, %esi
    movl %esi, (%esp) // spill insn*

    movl %ebx, %esi
    andl $0xF, %esi
    shll $4, %esi
    addl 4(%esp), %esi

    movl %eax, 8(%esp)
    movl %edx, 12(%esp)
    // handle the first arg index
    PUSH_ARG %esi, %edi, %eax, %edx
    movl 8(%esp), %eax
    movl 12(%esp), %edx

    NEXT_SHORTY %ecx
    cmpl $0, %ecx
    je .Linvoke_from_bridge

    movl %ebx, %esi
    sarl $4, %esi
    shll $4, %esi
    addl 4(%esp), %esi

    movl %eax, 8(%esp)
    movl %edx, 12(%esp)
    // handle the second arg index
    PUSH_ARG %esi, %edi, %eax, %edx 
    movl 8(%esp), %eax
    movl 12(%esp), %edx

    jmp .Lloop_handle_call
