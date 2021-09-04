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

#include "type_parametric.h"
#include "type_system.h"
#include "type_systems.h"

namespace panda::verifier {

TypeSystem &ParametricType::GetTypeSystem() const
{
    return TypeSystems::Get(kind_);
}

bool ParametricType::operator[](TypeParamsIdx params) const
{
    Index<TypeIdx> idx = GetTypeSystem().FindIdx({Sort_, std::move(params)});
    return idx.IsValid();
}

Type ParametricType::operator()(TypeParamsIdx params) const
{
    auto idx = GetTypeSystem().FindIdxOrCreate({Sort_, std::move(params)});
    GetTypeSystem().Relate(GetTypeSystem().BotIdx_, idx);
    GetTypeSystem().Relate(idx, GetTypeSystem().TopIdx_);
    return {kind_, idx};
}

bool ParametricType::operator[](const TypeParams &params) const
{
    TypeParamsIdx params_idx {params};  // NOLINT(cppcoreguidelines-slicing)
    return operator[](std::move(params_idx));
}

Type ParametricType::operator()(const TypeParams &params) const
{
    TypeParamsIdx params_idx {params};  // NOLINT(cppcoreguidelines-slicing)
    return operator()(std::move(params_idx));
}

template <typename Handler>
void ParametricType::ForAll(Handler &&handler) const
{
    GetTypeSystem().ForAllTypes([this, &handler](const Type &type) {
        if (type.Sort() == Sort_) {
            return handler(type);
        };
        return true;
    });
}

}  // namespace panda::verifier
