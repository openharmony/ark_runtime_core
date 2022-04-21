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

#include "arch/asm_support.h"

.macro ENTRYPOINT name, entry, paramsnum
.global \name
TYPE_FUNCTION(\name)
\name:
    CFI_STARTPROC
    CFI_DEF_CFA(rsp, 8)

    // Bridge frame:
    //   [1] native_pc = retaddr
    //   [2] parent frame pointer
    //   [3] COMPILED_CODE_TO_INTERPRETER_BRIDGE flag

    // Bridge frame, slot 1 = npc = retaddr (StackMap stays just after the bridge call)
    mov (%rsp), %r14
    // ManagedThread.npc update
    mov  %r14, THREAD_NATIVE_PC(%THREAD_REG)

    // Bridge frame, slot 2 = COMPILED_CODE_TO_INTERPRETER_BRIDGE flag
    movq $COMPILED_CODE_TO_INTERPRETER_BRIDGE, -0x8(%rsp)
    // Bridge frame, slot 3 = parent frame pointer
    mov  %rbp, -0x10(%rsp)
    CFI_REL_OFFSET(rbp, -(2 * 8))

    subq $0x10, %rsp
    CFI_ADJUST_CFA_OFFSET(0x10)
    // ManagedThread._frame = this boundary frame
    mov %rsp, THREAD_CURRENT_FRAME(%THREAD_REG)

    subq $(BRIDGE_FRAME_SIZE - 0x18), %rsp
    CFI_ADJUST_CFA_OFFSET((BRIDGE_FRAME_SIZE - 0x18))

    // call to BoundaryFrame bridge
    call \entry@plt

    addq $(BRIDGE_FRAME_SIZE - 0x8), %rsp
    CFI_ADJUST_CFA_OFFSET(-(BRIDGE_FRAME_SIZE - 0x8))

    movq -0x10(%rsp), %r14
    // ManagedThread._frame = parent frame pointer
    mov %r14, THREAD_CURRENT_FRAME(%THREAD_REG)

    // return to the caller
    retq
    CFI_ENDPROC
.endm

#include "entrypoints_gen.S"
#include "entrypoints_bridge_asm_macro.inl"

.global AbstractMethodStub
TYPE_FUNCTION(AbstractMethodStub)
AbstractMethodStub:
    CFI_STARTPROC
    CFI_DEF_CFA(rsp, 8)

    movq (%rsp), %r14
    movq  %r14, THREAD_NATIVE_PC(%THREAD_REG)
    movq  %rbp,  -0x10(%rsp)
    CFI_REL_OFFSET(rbp, -(2 * 8))
    movq $COMPILED_CODE_TO_INTERPRETER_BRIDGE, -0x8(%rsp)
    lea -0x10(%rsp), %r14
    movq %r14, THREAD_CURRENT_FRAME(%THREAD_REG)

    movq %r15, -0x20(%rsp)
    CFI_REL_OFFSET(r15, -(4 * 8))
    movq %r14, -0x28(%rsp)
    CFI_REL_OFFSET(r14, -(5 * 8))
    movq %r13, -0x30(%rsp)
    CFI_REL_OFFSET(r13, -(6 * 8))
    movq %r12, -0x38(%rsp)
    CFI_REL_OFFSET(r12, -(7 * 8))
    movq %rbx, -0x40(%rsp)
    CFI_REL_OFFSET(rbx, -(8 * 8))

    // ------------- header
    // %rsp        : ret addr
    // %rsp -0x08  : frame pointer
    // %rsp -0x10  : $COMPILED_CODE_TO_INTERPRETER_BRIDGE
    // %rsp -0x18  : UNUSED
    // ------------- callee-saved regs
    // %rsp -0x20  : %r15
    // %rsp -0x28  : %r14
    // %rsp -0x30  : %r13
    // %rsp -0x38  : %r12
    // %rsp -0x40  : %rbx
    // %rsp -0x48  : empty slot for alignment
    subq $0x48, %rsp
    CFI_ADJUST_CFA_OFFSET(9 * 8)

    call AbstractMethodErrorEntrypoint@plt
    // we're not going to return back here
    CFI_ENDPROC
