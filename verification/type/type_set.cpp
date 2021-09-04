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

#include "type_set.h"

#include "type_type_inl.h"

namespace panda::verifier {

const Type &TypeSet::operator<<(const Type &st) const
{
    ForAll([&](const Type &t) {
        t << st;
        return true;
    });
    return st;
}

const TypeSet &TypeSet::operator<<(const TypeSet &st) const
{
    ForAll([&](const Type &t) {
        t << st;
        return true;
    });
    return st;
}

TypeSet TypeSet::operator&(const Type &rhs) const
{
    return TypeSet {Kind_, Indices_ & TypeSystems::Get(Kind_).GetDirectlyRelated(rhs.Index())};
}

TypeSet TypeSet::operator&(const TypeSet &rhs) const
{
    return TypeSet {Kind_, Indices_ & rhs.Indices_};
}

}  // namespace panda::verifier
