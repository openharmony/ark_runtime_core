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

#ifndef PANDA_VERIFICATION_DEBUG_ALLOWLIST_ALLOWLIST_H_
#define PANDA_VERIFICATION_DEBUG_ALLOWLIST_ALLOWLIST_H_

#include <cstdint>
#include <cstddef>

namespace panda::verifier::debug {
enum class AllowlistKind : size_t { METHOD, METHOD_CALL, CLASS, __LAST__ };

bool InAllowlist(AllowlistKind kind, uint64_t id);

#define SKIP_VERIFICATION(id)                                                                  \
    (panda::verifier::debug::InAllowlist(panda::verifier::debug::AllowlistKind::METHOD, id) || \
     panda::verifier::debug::InAllowlist(panda::verifier::debug::AllowlistKind::CLASS, id))
#define SKIP_VERIFICATION_OF_CALL(id)                                                               \
    (panda::verifier::debug::InAllowlist(panda::verifier::debug::AllowlistKind::METHOD_CALL, id) || \
     panda::verifier::debug::InAllowlist(panda::verifier::debug::AllowlistKind::CLASS, id))
}  // namespace panda::verifier::debug

#endif  // PANDA_VERIFICATION_DEBUG_ALLOWLIST_ALLOWLIST_H_
