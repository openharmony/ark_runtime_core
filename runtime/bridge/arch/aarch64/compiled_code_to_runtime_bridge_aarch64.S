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

#include "arch/asm_support.h"

.macro ENTRYPOINT name, entry, paramsnum
.global \name
.type \name, %function
\name:
    CFI_STARTPROC
    CFI_DEF_CFA(sp, 0)

      // Save caller saved regs
.ifle \paramsnum-2           // parameter holding registers are saved by the caller function
      stp x2,  x3,  [fp, #-CALLER_REG0_OFFSET+8*2]
.endif
.ifle \paramsnum-4
      stp x4,  x5,  [fp, #-CALLER_REG0_OFFSET+8*4]
.endif
      stp x6,  x7,  [fp, #-CALLER_REG0_OFFSET+8*6]
      stp x8,  x9,  [fp, #-CALLER_REG0_OFFSET+8*8]
      stp x10, x11, [fp, #-CALLER_REG0_OFFSET+8*10]
      stp x12, x13, [fp, #-CALLER_REG0_OFFSET+8*12]
      stp x14, x15, [fp, #-CALLER_REG0_OFFSET+8*14]
      stp x16, x17, [fp, #-CALLER_REG0_OFFSET+8*16]
      str x18, [fp, #-CALLER_REG0_OFFSET+8*18]
      // BoundaryFrame setup
      str lr, [sp, #-8]    // Bridge frame, slot 1 = npc = LR (the StackMap is just after the bridge call)
      CFI_REL_OFFSET(lr, -(1 * 8))
      str lr, [THREAD_REG, #THREAD_NATIVE_PC]   // ManagedThread.npc update
      mov lr, #COMPILED_CODE_TO_INTERPRETER_BRIDGE
      str lr, [sp, #-16] // Bridge frame, slot 2 = COMPILED_CODE_TO_INTERPRETER
      str fp, [sp, #-24] // Bridge frame, slot 3 = parent frame pointer
      CFI_REL_OFFSET(fp, -(3 * 8))
      sub lr, sp, #0x18
      str lr, [THREAD_REG, #THREAD_CURRENT_FRAME]    // ManagedThread._frame = this boundary frame
      sub sp, sp, #BRIDGE_FRAME_SIZE
      CFI_ADJUST_CFA_OFFSET(BRIDGE_FRAME_SIZE)
      // The call
      bl \entry
      // Restore previous state
      add sp, sp, #BRIDGE_FRAME_SIZE
      CFI_ADJUST_CFA_OFFSET(-BRIDGE_FRAME_SIZE)
      ldr x17, [sp, #-24]
      str x17, [THREAD_REG, #THREAD_CURRENT_FRAME]
      // Restore caller saved regs
      // do not load x0/x1 here as it can overwrite the result value
      ldp x2,  x3,  [fp, #-CALLER_REG0_OFFSET+8*2]
      ldp x4,  x5,  [fp, #-CALLER_REG0_OFFSET+8*4]
      ldp x6,  x7,  [fp, #-CALLER_REG0_OFFSET+8*6]
      ldp x8,  x9,  [fp, #-CALLER_REG0_OFFSET+8*8]
      ldp x10, x11, [fp, #-CALLER_REG0_OFFSET+8*10]
      ldp x12, x13, [fp, #-CALLER_REG0_OFFSET+8*12]
      ldp x14, x15, [fp, #-CALLER_REG0_OFFSET+8*14]
      ldp x16, x17, [fp, #-CALLER_REG0_OFFSET+8*16]
      ldr x18, [fp, #-CALLER_REG0_OFFSET+8*18]

    ldr lr, [sp, #-8]
    CFI_RESTORE(lr)
    ret
    CFI_ENDPROC
.endm

#include "entrypoints_gen.S"
#include "entrypoints_bridge_asm_macro.inl"

.global AbstractMethodStub
.type AbstractMethodStub, %function
AbstractMethodStub:
    CFI_STARTPROC
    CFI_DEF_CFA(sp, 0)

    str lr, [sp, #-8]
    CFI_REL_OFFSET(lr, -(1 * 8))
    mov x17, #COMPILED_CODE_TO_INTERPRETER_BRIDGE
    str x17, [sp, #-16]
    str fp, [sp, #-24]
    CFI_REL_OFFSET(fp, -(3 * 8))
    str lr, [THREAD_REG, #THREAD_NATIVE_PC]
    sub x17, sp, #24
    str x17, [THREAD_REG, #THREAD_CURRENT_FRAME]
    sub sp, sp, #32
    CFI_ADJUST_CFA_OFFSET(32)

    // save callee regs
    stp x27, x28, [sp, #-16]!
    CFI_ADJUST_CFA_OFFSET(16)
    CFI_REL_OFFSET(x28, 8)
    CFI_REL_OFFSET(x27, 0)
    stp x25, x26, [sp, #-16]!
    CFI_ADJUST_CFA_OFFSET(16)
    CFI_REL_OFFSET(x26, 8)
    CFI_REL_OFFSET(x25, 0)
    stp x23, x24, [sp, #-16]!
    CFI_ADJUST_CFA_OFFSET(16)
    CFI_REL_OFFSET(x24, 8)
    CFI_REL_OFFSET(x23, 0)
    stp x21, x22, [sp, #-16]!
    CFI_ADJUST_CFA_OFFSET(16)
    CFI_REL_OFFSET(x22, 8)
    CFI_REL_OFFSET(x21, 0)
    stp x19, x20, [sp, #-16]!
    CFI_ADJUST_CFA_OFFSET(16)
    CFI_REL_OFFSET(x20, 8)
    CFI_REL_OFFSET(x19, 0)
    stp d14, d15, [sp, #-16]!
    CFI_ADJUST_CFA_OFFSET(16)
    CFI_REL_OFFSET(d15, 8)
    CFI_REL_OFFSET(d14, 0)
    stp d12, d13, [sp, #-16]!
    CFI_ADJUST_CFA_OFFSET(16)
    CFI_REL_OFFSET(d13, 8)
    CFI_REL_OFFSET(d12, 0)
    stp d10, d11, [sp, #-16]!
    CFI_ADJUST_CFA_OFFSET(16)
    CFI_REL_OFFSET(d11, 8)
    CFI_REL_OFFSET(d10, 0)
    stp d8, d9, [sp, #-16]!
    CFI_ADJUST_CFA_OFFSET(16)
    CFI_REL_OFFSET(d9, 8)
    CFI_REL_OFFSET(d8, 0)

    bl AbstractMethodErrorEntrypoint
    // we're not going to return back here
    CFI_ENDPROC
