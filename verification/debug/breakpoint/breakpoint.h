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

#ifndef PANDA_VERIFICATION_DEBUG_BREAKPOINT_BREAKPOINT_H_
#define PANDA_VERIFICATION_DEBUG_BREAKPOINT_BREAKPOINT_H_

#include <cstdint>
#include <cstddef>

namespace panda::verifier::debug {
enum class Component : size_t { VERIFIER, __LAST__ };

bool CheckManagedBreakpoint(Component component, uint64_t id, uint32_t offset);
bool ManagedBreakpointPresent(Component component, uint64_t id);

#ifndef NDEBUG
#define DBG_MANAGED_BRK(component, method_id, method_offset)                                   \
    if (panda::verifier::debug::CheckManagedBreakpoint(component, method_id, method_offset)) { \
        __builtin_trap();                                                                      \
    }
#define DBG_MANAGED_BRK_PRESENT(component, method_id) \
    panda::verifier::debug::ManagedBreakpointPresent(component, method_id)
#else
#define DBG_MANAGED_BRK(a, b, c)
#define DBG_MANAGED_BRK_PRESENT(a, b) false
#endif
}  // namespace panda::verifier::debug

#endif  // PANDA_VERIFICATION_DEBUG_BREAKPOINT_BREAKPOINT_H_
