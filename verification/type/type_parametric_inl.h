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

#ifndef PANDA_VERIFICATION_TYPE_TYPE_PARAMETRIC_INL_H_
#define PANDA_VERIFICATION_TYPE_TYPE_PARAMETRIC_INL_H_

#include "type_parametric.h"
#include "type_system.h"

namespace panda::verifier {
template <typename Handler>
void ForAll(Handler &&handler) const
{
    GetTypeSystem().ForAllTypes([this, &handler](const Type &type) {
        if (type.Sort() == Sort_) {
            return handler(type);
        }
        return true;
    });
}
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_TYPE_TYPE_PARAMETRIC_INL_H_
