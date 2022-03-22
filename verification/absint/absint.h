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

#ifndef PANDA_VERIFICATION_ABSINT_ABSINT_H_
#define PANDA_VERIFICATION_ABSINT_ABSINT_H_

#include "verification_context.h"
#include "panda_types.h"

#include "verification_status.h"

#include "cflow/cflow_info.h"

#include "runtime/include/method.h"

#include <optional>

namespace panda::verifier {
VerificationContext PrepareVerificationContext(PandaTypes *panda_types, const Job &job);

VerificationStatus AbstractInterpret(VerificationLevel v_level, VerificationContext *v_ctx, const uint8_t *pc);

VerificationStatus VerifyMethod(VerificationLevel v_level, VerificationContext *v_ctx);
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_ABSINT_ABSINT_H_
