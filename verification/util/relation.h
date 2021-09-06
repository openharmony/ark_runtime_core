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

#ifndef PANDA_VERIFICATION_UTIL_RELATION_H_
#define PANDA_VERIFICATION_UTIL_RELATION_H_

#include "lazy.h"
#include "index.h"

#include "runtime/include/mem/panda_containers.h"
#include "int_set.h"

#include "macros.h"

#include <limits>
#include <initializer_list>
#include <type_traits>

namespace panda::verifier {

class Relation {
public:
    using RelIndex = size_t;
    using MapIndexFromTo = PandaVector<IntSet<size_t>>;

    void Relate(RelIndex from, RelIndex to)
    {
        ASSERT(from < Direct_.size());
        ASSERT(to < Inverse_.size());
        Inverse_[to].Insert(from);
        Inverse_[to] |= Inverse_[from];
        Direct_[from].Insert(to);
        Direct_[from] |= Direct_[to];
        // flatten relation
        for (RelIndex dst : Direct_[to]) {
            Inverse_[dst].Insert(from);
            Inverse_[dst] |= Inverse_[from];
        }
        for (RelIndex src : Inverse_[from]) {
            Direct_[src].Insert(to);
            Direct_[src] |= Direct_[to];
        }
    }

    void SymmRelate(RelIndex lhs, RelIndex rhs)
    {
        Relate(lhs, rhs);
        Relate(rhs, lhs);
    }

    Relation &operator+=(const std::pair<RelIndex, RelIndex> &pair)
    {
        Relate(pair.first, pair.second);
        return *this;
    }

    Relation &operator+=(std::initializer_list<std::pair<RelIndex, RelIndex>> pairs)
    {
        for (const auto &p : pairs) {
            *this += p;
        }
        return *this;
    }

    void EnsureMinSize(size_t idx)
    {
        if (idx >= Direct_.size()) {
            size_t i = idx + 1;
            Direct_.resize(i);
            Inverse_.resize(i);
        }
    }

    template <typename Handler>
    void ForAllFrom(RelIndex from, Handler &&handler) const
    {
        ASSERT(from < Direct_.size());
        Direct_[from].ForAll(handler);
    }

    template <typename Handler>
    void ForAllTo(RelIndex to, Handler &&handler) const
    {
        ASSERT(to < Inverse_.size());
        Inverse_[to].ForAll(handler);
    }

    template <typename Handler>
    void ForAllBetween(RelIndex from, RelIndex to, Handler &&handler) const
    {
        if (IsInInverseRelation(from, to)) {
            auto tmp = to;
            to = from;
            from = tmp;
        }
        auto stream = Direct_[from].LazyIntersect(Inverse_[to]);
        auto value = stream();
        while (value.IsValid()) {
            if (!handler(value)) {
                return;
            }
            value = stream();
        }
    }

    bool IsInDirectRelation(RelIndex from, RelIndex to) const
    {
        return from < Direct_.size() && Direct_[from].Contains(to);
    }

    bool IsInInverseRelation(RelIndex from, RelIndex to) const
    {
        return from < Inverse_.size() && Inverse_[from].Contains(to);
    }

    bool IsInAnyRelation(RelIndex from, RelIndex to) const
    {
        return IsInDirectRelation(from, to) || IsInInverseRelation(from, to);
    }

    bool IsInIsoRelation(RelIndex from, RelIndex to) const
    {
        return IsInDirectRelation(from, to) && IsInInverseRelation(from, to);
    }

    const IntSet<RelIndex> &GetDirectlyRelated(RelIndex from) const
    {
        return Direct_[from];
    }

    const IntSet<RelIndex> &GetInverselyRelated(RelIndex to) const
    {
        return Inverse_[to];
    }

private:
    MapIndexFromTo Direct_, Inverse_;
};
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_UTIL_RELATION_H_
