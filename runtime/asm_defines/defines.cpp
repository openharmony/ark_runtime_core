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

#include <cstdint>
#include "runtime/include/method.h"
#include "runtime/include/thread.h"
#include "utils/cframe_layout.h"
#include "runtime/include/coretypes/array.h"
#include "runtime/include/coretypes/string.h"

namespace panda {

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_DEFINE_MULTILINE)
#define DEFINE_VALUE(name, value)                                                         \
    void AsmDefinition_##name()                                                           \
    {                                                                                     \
        asm volatile("\n.ascii \"^^" #name " %0^^\"" ::"i"(static_cast<int64_t>(value))); \
    }
#include "asm_defines.def"

}  // namespace panda
