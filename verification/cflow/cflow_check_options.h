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

#ifndef PANDA_VERIFICATION_CFLOW_CFLOW_CHECK_OPTIONS_H_
#define PANDA_VERIFICATION_CFLOW_CFLOW_CHECK_OPTIONS_H_

#include "verification/util/flags.h"

namespace panda::verifier {

enum class CflowCheckOptions {
    ALLOW_JMP_BODY_TO_HANDLER = 0,   // code -> start of exception handler
    ALLOW_JMP_BODY_INTO_HANDLER,     // code -> into body of exception handler
    ALLOW_JMP_HANDLER_TO_HANDLER,    // handler -> start of handler
    ALLOW_JMP_HANDLER_INTO_HANDLER,  // handler -> into body of other handler
    ALLOW_JMP_HANDLER_INTO_BODY,     // handler -> into code
};

using CflowCheckFlags =
    FlagsForEnum<size_t, CflowCheckOptions, CflowCheckOptions::ALLOW_JMP_BODY_TO_HANDLER,
                 CflowCheckOptions::ALLOW_JMP_BODY_INTO_HANDLER, CflowCheckOptions::ALLOW_JMP_HANDLER_TO_HANDLER,
                 CflowCheckOptions::ALLOW_JMP_HANDLER_INTO_HANDLER, CflowCheckOptions::ALLOW_JMP_HANDLER_INTO_BODY>;

}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_CFLOW_CFLOW_CHECK_OPTIONS_H_
