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

#ifndef PANDA_RUNTIME_INTERPRETER_ARCH_AARCH64_GLOBAL_REGS_H_
#define PANDA_RUNTIME_INTERPRETER_ARCH_AARCH64_GLOBAL_REGS_H_

#include <cstdint>

namespace panda {
class Frame;
class ManagedThread;
}  //  namespace panda

namespace panda::interpreter::arch::regs {

#ifndef FFIXED_REGISTERS
#error "Need to compile source with -ffixed-x20 -ffixed-x21 -ffixed-x22 -ffixed-x23 -ffixed-x24 -ffixed-x28"
#endif

register const uint8_t *g_pc asm("x20");
register int64_t g_acc_value asm("x21");
register uint64_t g_acc_tag asm("x22");
register Frame *g_frame asm("x23");
register const void *const *g_dispatch_table asm("x24");
register ManagedThread *g_thread asm("x28");

ALWAYS_INLINE inline const uint8_t *GetPc()
{
    return g_pc;
}

ALWAYS_INLINE inline void SetPc(const uint8_t *pc)
{
    g_pc = pc;
}

ALWAYS_INLINE inline int64_t GetAccValue()
{
    return g_acc_value;
}

ALWAYS_INLINE inline void SetAccValue(int64_t value)
{
    g_acc_value = value;
}

ALWAYS_INLINE inline uint64_t GetAccTag()
{
    return g_acc_tag;
}

ALWAYS_INLINE inline void SetAccTag(uint64_t tag)
{
    g_acc_tag = tag;
}

ALWAYS_INLINE inline Frame *GetFrame()
{
    return g_frame;
}

ALWAYS_INLINE inline void SetFrame(Frame *frame)
{
    g_frame = frame;
}

ALWAYS_INLINE inline const void *const *GetDispatchTable()
{
    return g_dispatch_table;
}

ALWAYS_INLINE inline void SetDispatchTable(const void *const *dispatch_table)
{
    g_dispatch_table = dispatch_table;
}

ALWAYS_INLINE inline ManagedThread *GetThread()
{
    return g_thread;
}

ALWAYS_INLINE inline void SetThread(ManagedThread *thread)
{
    g_thread = thread;
}

}  // namespace panda::interpreter::arch::regs

#endif  // PANDA_RUNTIME_INTERPRETER_ARCH_AARCH64_GLOBAL_REGS_H_
