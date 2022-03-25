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

#ifndef PANDA_VERIFICATION_TYPE_SUBTYPING_CLOSURE_H_
#define PANDA_VERIFICATION_TYPE_SUBTYPING_CLOSURE_H_

#include "type_sort.h"
#include "type_index.h"
#include "type_info.h"

#include "runtime/include/mem/panda_containers.h"

namespace panda::verifier {
class SubtypingClosureInfo {
public:
    SubtypingClosureInfo() = default;
    SubtypingClosureInfo(SubtypingClosureInfo &&) = default;
    SubtypingClosureInfo(const SubtypingClosureInfo &) = delete;
    SubtypingClosureInfo &operator=(SubtypingClosureInfo &&) = default;
    SubtypingClosureInfo &operator=(const SubtypingClosureInfo &) = delete;
    ~SubtypingClosureInfo() = default;
    bool Empty() const
    {
        return Empty_;
    }
    void Clear()
    {
        for (auto &map_sort_to_type : ArityToSortToTypes_) {
            for (auto &sort_to_type : map_sort_to_type) {
                sort_to_type.second.clear();
            }
        }
        Empty_ = true;
    }
    void AddType(SortIdx sort, TypeIdx type, size_t arity)
    {
        if (arity >= ArityToSortToTypes_.size()) {
            ArityToSortToTypes_.resize(arity + 1);
        }
        ArityToSortToTypes_[arity][sort].insert(type);
        Empty_ = false;
    }
    template <typename Proc>
    void ForAllTypeClasses(Proc process) const
    {
        for (auto &map_sort_to_types : ArityToSortToTypes_) {
            for (auto &sort_to_types : map_sort_to_types) {
                process(sort_to_types.second);
            }
        }
    }
    void swap(SubtypingClosureInfo &other)  // NOLINT(readability-identifier-naming)
    {
        std::swap(ArityToSortToTypes_, other.ArityToSortToTypes_);
        std::swap(Empty_, other.Empty_);
    }

private:
    // arity -> sort -> instances
    PandaVector<PandaUnorderedMap<SortIdx, PandaUnorderedSet<TypeIdx>>> ArityToSortToTypes_;
    bool Empty_ = true;
};
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_TYPE_SUBTYPING_CLOSURE_H_
