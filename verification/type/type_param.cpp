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

#include "type_param.h"
#include "type_system.h"
#include "type_type.h"

namespace panda::verifier {

TypeParam::TypeParam(const Type &t, TypeVariance v) : TypeParamIdx {t.Index(), v}, kind_ {t.GetTypeSystem().GetKind()}
{
}

TypeParam::TypeParam(TypeSystemKind kind, const TypeParamIdx &p) : TypeParamIdx {p}, kind_ {kind} {}

TypeParams TypeParam::operator>>(const TypeParam &p) const
{
    return TypeParams {kind_} >> *this >> p;
}

TypeParam::operator TypeParams() const  // NOLINT(google-explicit-constructor)
{
    return TypeParams {kind_} >> *this;
}

TypeParam::operator Type() const
{
    return {kind_, *this};
}

}  // namespace panda::verifier
