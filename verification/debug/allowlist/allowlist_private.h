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

#ifndef PANDA_VERIFICATION_DEBUG_ALLOWLIST_ALLOWLIST_PRIVATE_H_
#define PANDA_VERIFICATION_DEBUG_ALLOWLIST_ALLOWLIST_PRIVATE_H_

#include "allowlist.h"

namespace panda::verifier::debug {
void AddAllowlistMethodConfig(AllowlistKind kind, uint32_t name_hash);

void AllowlistMethodIdCalculationHandler(uint32_t class_hash, uint32_t method_hash, uint64_t id);
}  // namespace panda::verifier::debug

#endif  // PANDA_VERIFICATION_DEBUG_ALLOWLIST_ALLOWLIST_PRIVATE_H_
