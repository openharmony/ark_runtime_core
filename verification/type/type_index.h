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

#ifndef PANDA_VERIFICATION_TYPE_TYPE_INDEX_H_
#define PANDA_VERIFICATION_TYPE_TYPE_INDEX_H_

#include "verification/util/lazy.h"
#include "verification/util/relation.h"
#include "verification/util/tagged_index.h"

#include "runtime/include/mem/panda_containers.h"

namespace panda::verifier {
enum class TypeVariance { INVARIANT, COVARIANT, CONTRVARIANT, __LAST__ = CONTRVARIANT };

using TypeIdx = size_t;
using VectorIdx = PandaVector<TypeIdx>;

class TypeParamIdx : public TaggedIndex<TypeVariance> {
    using Base = TaggedIndex<TypeVariance>;

public:
    TypeParamIdx(TypeIdx idx, TypeVariance variance) : Base {variance, idx} {}
    ~TypeParamIdx() = default;
    TypeParamIdx &operator+()
    {
        Base::SetTag(TypeVariance::COVARIANT);
        return *this;
    }
    TypeParamIdx &operator-()
    {
        Base::SetTag(TypeVariance::CONTRVARIANT);
        return *this;
    }
    TypeParamIdx &operator~()
    {
        Base::SetTag(TypeVariance::INVARIANT);
        return *this;
    }
    TypeVariance Variance() const
    {
        return Base::GetTag();
    }
};

using TypeParamsIdx = PandaVector<TypeParamIdx>;
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_TYPE_TYPE_INDEX_H_
