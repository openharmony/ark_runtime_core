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

#include "type_params.h"

#include "type_systems.h"

#include "type_system_kind.h"

#include "type_system.h"

namespace panda::verifier {

TypeSystem &TypeParams::GetTypeSystem() const
{
    return TypeSystems::Get(kind_);
}

bool TypeParams::operator<=(const TypeParams &rhs) const
{
    ASSERT(kind_ == rhs.kind_);
    if (empty()) {
        return true;
    }
    return GetTypeSystem().CheckIfLhsParamsSubtypeOfRhs(*this, rhs);
}

TypeParams &TypeParams::operator>>(const TypeParam &p)
{
    push_back(p);
    return *this;
}

}  // namespace panda::verifier
