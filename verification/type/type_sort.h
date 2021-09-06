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

#ifndef PANDA_VERIFICATION_TYPE_TYPE_SORT_H_
#define PANDA_VERIFICATION_TYPE_TYPE_SORT_H_

#include "runtime/include/mem/panda_containers.h"
#include "runtime/include/mem/panda_string.h"

namespace panda::verifier {
using SortIdx = size_t;

template <typename Name>
class SortNames {
public:
    using NameType = Name;

    SortNames(const Name &bot, const Name &top)
    {
        operator[](bot);
        operator[](top);
    }

    ~SortNames() = default;

    const Name &operator[](SortIdx sort) const
    {
        return SortToName_[sort];
    }

    SortIdx operator[](const Name &name)
    {
        auto s = NameToSort_.find(name);
        if (s != NameToSort_.end()) {
            return s->second;
        }
        SortIdx sort = SortToName_.size();
        SortToName_.push_back(name);
        NameToSort_[name] = sort;
        return sort;
    }

private:
    PandaUnorderedMap<Name, SortIdx> NameToSort_;
    PandaVector<Name> SortToName_;
};
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_TYPE_TYPE_SORT_H_
