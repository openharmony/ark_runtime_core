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
#include "type_type.h"

#include "type_system.h"

#include "type_sort.h"
#include "type_index.h"
#include "type_info.h"

#include "type_set.h"
#include "type_param.h"

namespace panda::verifier {

template <>
TypeSet Type::operator|(const Type &t) const
{
    return TypeSet {*this, t};
}

template <>
TypeParam Type::operator+() const
{
    return {*this, TypeVariance::COVARIANT};
}

template <>
TypeParam Type::operator-() const
{
    return {*this, TypeVariance::CONTRVARIANT};
}

template <>
TypeParam Type::operator~() const
{
    return {*this, TypeVariance::INVARIANT};
}

template <>
TypeParams Type::Params() const
{
    return TypeParams {GetTypeSystemKind(), GetTypeSystem().GetParamsIdx(Idx_)};
}

template <>
TypeParam Type::operator*(TypeVariance variance) const
{
    return {*this, variance};
}

bool Type::operator==(const Type &t) const
{
    return t.Idx_ == Idx_;
}

bool Type::operator!=(const Type &t) const
{
    return t.Idx_ != Idx_;
}

const Type &Type::operator<<(const Type &t) const
{
    ASSERT(GetTypeSystemKind() == t.GetTypeSystemKind());
    GetTypeSystem().Relate(Idx_, t.Idx_);
    return t;
}

const TypeSet &Type::operator<<(const TypeSet &s) const
{
    s.ForAll([&](const Type &t) {
        operator<<(t);
        return true;
    });
    return s;
}

bool Type::operator<=(const Type &rhs) const
{
    return GetTypeSystem().IsInDirectRelation(Idx_, rhs.Idx_);
}

bool Type::operator<=(const TypeParams &rhs) const
{
    return TypeParams {GetTypeSystemKind()} <= rhs;
}

SortIdx Type::Sort() const
{
    return GetTypeSystem().GetSort(Idx_);
}

size_t Type::Arity() const
{
    return GetTypeSystem().GetArity(Idx_);
}

size_t Type::ParamsSize() const
{
    return GetTypeSystem().GetParamsIdx(Idx_).size();
}

TypeSystem &Type::GetTypeSystem() const
{
    return TypeSystems::Get(GetTypeSystemKind());
}

TypeSystemKind Type::GetTypeSystemKind() const
{
    return Idx_.GetTag();
}

bool Type::IsValid() const
{
    return Idx_.IsValid();
}

bool Type::IsTop() const
{
    return GetTypeSystem().Top() == *this;
}

bool Type::IsBot() const
{
    return GetTypeSystem().Bot() == *this;
}

TypeSet Type::operator&(const Type &rhs) const
{
    ASSERT(GetTypeSystemKind() == rhs.GetTypeSystemKind());
    const TypeSystem &type_system = GetTypeSystem();
    return TypeSet {GetTypeSystemKind(),
                    type_system.GetDirectlyRelated(Idx_) & type_system.GetDirectlyRelated(rhs.Idx_)};
}

TypeSet Type::operator&(const TypeSet &rhs) const
{
    return rhs & *this;
}

TypeIdx Type::Index() const
{
    return Idx_;
}

}  // namespace panda::verifier
