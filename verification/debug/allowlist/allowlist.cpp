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

#include "allowlist_private.h"

#include "verification/debug/context/context.h"

#include "utils/logger.h"

namespace panda::verifier::debug {

bool InAllowlist(AllowlistKind kind, uint64_t id)
{
    auto &id_local = DebugContext::GetCurrent().Allowlist.Id;
    return id_local[static_cast<size_t>(kind)]->count(id) > 0;
}

void AddAllowlistMethodConfig(AllowlistKind kind, uint32_t name_hash)
{
    auto &name_hash_local = DebugContext::GetCurrent().Allowlist.NameHash;
    name_hash_local[static_cast<size_t>(kind)]->insert(name_hash);
}

void AllowlistMethodIdCalculationHandler(uint32_t class_hash, uint32_t method_hash, uint64_t id)
{
    auto &allowlist = DebugContext::GetCurrent().Allowlist;
    auto &id_local = allowlist.Id;
    auto &name_hash_local = allowlist.NameHash;
    for (size_t k = 0; k < static_cast<size_t>(AllowlistKind::__LAST__); ++k) {
        if (static_cast<AllowlistKind>(k) == AllowlistKind::CLASS) {
            if (name_hash_local[k]->count(class_hash) > 0) {
                LOG(DEBUG, VERIFIER) << "Method with class hash 0x" << std::hex << class_hash << ", id 0x" << id
                                     << " was successfully added to allowlist";
                id_local[k]->insert(id);
            }
        } else {
            if (name_hash_local[k]->count(method_hash) > 0) {
                LOG(DEBUG, VERIFIER) << "Method with hash 0x" << std::hex << method_hash << ", id 0x" << id
                                     << " was successfully added to allowlist";
                id_local[k]->insert(id);
            }
        }
    }
}

}  // namespace panda::verifier::debug
