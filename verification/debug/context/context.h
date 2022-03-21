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

#ifndef PANDA_VERIFICATION_DEBUG_CONTEXT_CONTEXT_H_
#define PANDA_VERIFICATION_DEBUG_CONTEXT_CONTEXT_H_

#include "verification/debug/breakpoint/breakpoint_private.h"
#include "verification/debug/allowlist/allowlist_private.h"
#include "verification/debug/config/config.h"
#include "verification/util/callable.h"
#include "verification/util/synchronized.h"

#include "runtime/include/mem/panda_string.h"
#include "runtime/include/mem/panda_containers.h"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <array>

namespace panda::verifier::debug {
struct DebugContext {
    struct {
        PandaUnorderedMap<PandaString, panda::verifier::callable<bool(const config::Section &)>> SectionHandlers;
    } Config;

    struct {
        std::array<Synchronized<PandaUnorderedMap<uint64_t, PandaUnorderedSet<uint32_t>>>,
                   static_cast<size_t>(Component::__LAST__)>
            Breakpoint;
        Synchronized<PandaUnorderedMap<uint32_t, PandaVector<DebugManagedBrkCfg>>> Config;
    } ManagedBreakpoints;

    struct {
        std::array<Synchronized<PandaUnorderedSet<uint32_t>>, static_cast<size_t>(AllowlistKind::__LAST__)> NameHash;
        std::array<Synchronized<PandaUnorderedSet<uint64_t>>, static_cast<size_t>(AllowlistKind::__LAST__)> Id;
    } Allowlist;

    static DebugContext &GetCurrent();
    static void Destroy();

    static DebugContext *instance;
};
}  // namespace panda::verifier::debug

#endif  // PANDA_VERIFICATION_DEBUG_CONTEXT_CONTEXT_H_
