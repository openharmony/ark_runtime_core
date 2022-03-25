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

#include "breakpoint_private.h"

#include "verification/debug/context/context.h"

#include "verification/util/str.h"

#include "verifier_messages.h"

#include "utils/logger.h"

namespace panda::verifier::debug {

void AddBreakpointConfig(const DebugManagedBrkCfg &cfg)
{
    auto &managed_breakpoints = DebugContext::GetCurrent().ManagedBreakpoints;
    managed_breakpoints.Config->operator[](cfg.NameHash).push_back(cfg);
}

void BreakpointMethodIdCalculationHandler([[maybe_unused]] uint32_t class_hash, uint32_t name_hash, uint64_t id)
{
    auto &managed_breakpoints = DebugContext::GetCurrent().ManagedBreakpoints;
    if (managed_breakpoints.Config->count(name_hash) > 0) {
        for (const auto &cfg : managed_breakpoints.Config->at(name_hash)) {
            LOG_VERIFIER_DEBUG_BREAKPOINT_SET_INFO(name_hash, id, cfg.Offset);
            auto &breakpoint = managed_breakpoints.Breakpoint[static_cast<size_t>(cfg.Comp)];
            breakpoint->operator[](id).insert(cfg.Offset);
        }
    }
}

bool CheckManagedBreakpoint(Component component, uint64_t id, uint32_t offset)
{
    auto &managed_breakpoints = DebugContext::GetCurrent().ManagedBreakpoints;
    const auto &breakpoints = managed_breakpoints.Breakpoint[static_cast<size_t>(component)];
    auto count = breakpoints->count(id);
    if (count == 0) {
        return false;
    }
    const auto offset_hit = breakpoints->at(id).count(offset);
    return offset_hit > 0;
}

bool ManagedBreakpointPresent(Component component, uint64_t id)
{
    auto &managed_breakpoints = DebugContext::GetCurrent().ManagedBreakpoints;
    const auto &breakpoints = managed_breakpoints.Breakpoint[static_cast<size_t>(component)];
    return breakpoints->count(id) > 0;
}

}  // namespace panda::verifier::debug
