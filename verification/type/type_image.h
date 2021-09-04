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

#ifndef PANDA_VERIFICATION_TYPE_TYPE_IMAGE_H_
#define PANDA_VERIFICATION_TYPE_TYPE_IMAGE_H_

#include "type_type.h"
#include "type_param.h"
#include "type_params.h"

#include "runtime/include/mem/panda_containers.h"
#include "runtime/include/mem/panda_string.h"

#include "verification/util/synchronized.h"

namespace panda::verifier {
template <typename SortNames>
class TypeImage {
public:
    const SortNames &SNames_;
    using Map = PandaUnorderedMap<TypeIdx, PandaString>;
    Map CachedImages_;

    explicit TypeImage(const SortNames &names) : SNames_ {names} {}
    ~TypeImage() = default;

    PandaString ImageOfVariance(TypeVariance var) const
    {
        switch (var) {
            case TypeVariance::COVARIANT:
                return "+";
            case TypeVariance::CONTRVARIANT:
                return "-";
            case TypeVariance::INVARIANT:
                return "~";
            default:
                break;
        }
        return "?";
    }

    PandaString ImageOfTypeParam(const TypeParam &type_param)
    {
        return ImageOfVariance(type_param.Variance()) + ImageOfType(type_param);
    }

    PandaString ImageOfTypeParams(const TypeParams &params)
    {
        PandaString params_image {""};

        if (params.size() != 0) {
            params.ForEach([this, &params_image](const auto &p) {
                params_image += PandaString {params_image.empty() ? "( " : ", "};
                params_image += ImageOfTypeParam(p);
            });
            params_image += " )";
        }

        return params_image;
    }

    const PandaString &ImageOfType(const Type &type)
    {
        auto cached = CachedImages_.find(type.Index());
        if (cached != CachedImages_.end()) {
            return cached->second;
        }

        PandaString sort_name {SNames_[type.Sort()]};

        const auto &params = type.Params();

        auto &&params_image = ImageOfTypeParams(params);

        PandaString val = sort_name + params_image;

        CachedImages_[type.Index()] = val;

        return CachedImages_[type.Index()];
    }

    const PandaString &operator[](const Type &type)
    {
        return ImageOfType(type);
    }
};
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_TYPE_TYPE_IMAGE_H_
