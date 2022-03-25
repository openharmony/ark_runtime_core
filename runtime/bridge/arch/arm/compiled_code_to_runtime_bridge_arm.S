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
.type \name, %function
\name:
      CFI_STARTPROC
      CFI_DEF_CFA(sp, 0)
      // Setup BoundaryFrame
      str lr, [sp, #-4]     // Bridge frame, slot 1 = npc = LR (the StackMap stays just after the bridge call)
      CFI_REL_OFFSET(lr, -4)
      str lr, [THREAD_REG, #THREAD_NATIVE_PC] // ManagedThread.npc update
      mov lr, #COMPILED_CODE_TO_INTERPRETER_BRIDGE
      str lr, [sp, #-8]     // Bridge frame, slot 2 = COMPILED_CODE_TO_INTERPRETER_BRIDGE flag
      str fp, [sp, #-12]    // Bridge frame, slot 3 = parent frame pointer
      CFI_REL_OFFSET(fp, -12)
      sub lr, sp, #12
      str lr, [THREAD_REG, #THREAD_CURRENT_FRAME]    // ManagedThread._frame = this boundary frame
      sub sp, sp, #BRIDGE_FRAME_SIZE
      CFI_ADJUST_CFA_OFFSET(BRIDGE_FRAME_SIZE)

    bl \entry
      // State restore
      add sp, sp, #BRIDGE_FRAME_SIZE
      CFI_ADJUST_CFA_OFFSET(-BRIDGE_FRAME_SIZE)
      ldr fp, [sp, #-12]
      CFI_RESTORE(fp)
      str fp, [THREAD_REG, #THREAD_CURRENT_FRAME]
      ldr lr, [sp, #-4]
      CFI_RESTORE(lr)
      bx lr
      CFI_ENDPROC
.endm

#include "entrypoints_gen.S"
#include "entrypoints_bridge_asm_macro.inl"

.global AbstractMethodStub
.type AbstractMethodStub, %function
AbstractMethodStub:
    CFI_STARTPROC
    CFI_DEF_CFA(sp, 0)
    str lr, [sp, #-4]
    CFI_REL_OFFSET(lr, -4)
    str lr, [THREAD_REG, #THREAD_NATIVE_PC]
    mov lr, #COMPILED_CODE_TO_INTERPRETER_BRIDGE
    str lr, [sp, #-8]
    str fp, [sp, #-12]
    CFI_REL_OFFSET(fp, -12)
    sub lr, sp, #12
    str lr, [THREAD_REG, #THREAD_CURRENT_FRAME]
    sub sp, sp, #16
    CFI_ADJUST_CFA_OFFSET(16) 

    push {r4 - r11}
    CFI_ADJUST_CFA_OFFSET(32)
    // CFI_REL_OFFSET(r11, (7 * 4)) fp already marked
    CFI_REL_OFFSET(r10, (6 * 4))
    CFI_REL_OFFSET(r9,  (5 * 4))
    CFI_REL_OFFSET(r8,  (4 * 4))
    CFI_REL_OFFSET(r7,  (3 * 4))
    CFI_REL_OFFSET(r6,  (2 * 4))
    CFI_REL_OFFSET(r5,  (1 * 4))
    CFI_REL_OFFSET(r4,  (0 * 4))
    vpush {d8 - d15}
    CFI_ADJUST_CFA_OFFSET(64)
    CFI_REL_OFFSET(d15, (14 * 4))
    CFI_REL_OFFSET(d14, (12 * 4))
    CFI_REL_OFFSET(d13, (10 * 4))
    CFI_REL_OFFSET(d12, (8 * 4))
    CFI_REL_OFFSET(d11, (6 * 4))
    CFI_REL_OFFSET(d10, (4 * 4))
    CFI_REL_OFFSET(d9,  (2 * 4))
    CFI_REL_OFFSET(d8,  (0 * 4))

    bl AbstractMethodErrorEntrypoint
    // we're not going to return back here
    CFI_ENDPROC
