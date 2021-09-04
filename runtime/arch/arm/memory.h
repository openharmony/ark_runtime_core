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

#ifndef PANDA_RUNTIME_ARCH_ARM_MEMORY_H_
#define PANDA_RUNTIME_ARCH_ARM_MEMORY_H_

namespace panda::archSpecific {

#if (__ARM_ARCH >= 7)
inline void FullMemoryBarrier()
{
    asm volatile("dmb ish" : : : "memory");  // NOLINT(hicpp-no-assembler)
}
#else
#error "Unsupported target"
#endif

}  // namespace panda::archSpecific

#endif  // PANDA_RUNTIME_ARCH_ARM_MEMORY_H_
