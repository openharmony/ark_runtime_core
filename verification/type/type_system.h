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

#ifndef PANDA_VERIFICATION_TYPE_TYPE_SYSTEM_H_
#define PANDA_VERIFICATION_TYPE_TYPE_SYSTEM_H_

#include "verification/util/lazy.h"
#include "verification/util/relation.h"
#include "type_sort.h"
#include "type_index.h"
#include "type_info.h"
#include "type_type.h"
#include "type_set.h"
#include "type_param.h"
#include "type_params.h"
#include "type_parametric.h"
#include "subtyping_closure.h"

#include "type_systems.h"

#include "runtime/include/mem/panda_containers.h"

#include "libpandabase/os/mutex.h"

#include "macros.h"

#include "type_system_kind.h"

#include <memory>
#include <variant>
#include <functional>
#include <algorithm>

namespace panda::verifier {
/*
Design decisions:
1. Subtyping relation is kept flat during types construction
2. Subtyping relation closing may be either incremental/implicit during types construction or explicit
3. Sorts are abstracted in the form of indices (of type size_t)
4. Types internally represented as indices (size_t)
5. There are special initial and final types, named as Bot and Top, and all types are implicitly related
   as Bot <: type <: Top
*/

class TypeSystem {
public:
    TypeSystem(const TypeSystem &) = delete;
    TypeSystem(TypeSystem &&) = delete;
    TypeSystem &operator=(const TypeSystem &) = delete;
    TypeSystem &operator=(TypeSystem &&) = delete;
    ~TypeSystem() = default;

    using TypeUniverse = PandaVector<TypeInfo>;
    using MappingToIdx = PandaUnorderedMap<TypeInfo, TypeIdx>;
    // sort -> arity -> types
    using TypeClasses = PandaVector<PandaUnorderedMap<size_t, VectorIdx>>;

    Relation TypingRel_;
    PandaVector<PandaUnorderedSet<TypeIdx>> ParameterOf_;
    TypeUniverse Universe_;
    MappingToIdx InfoToIdx_;
    mutable TypeClasses TypeClasses_;
    SubtypingClosureInfo SubtypingClosureCurrent_;
    SubtypingClosureInfo SubtypingClosureNext_;
    bool IncrementalSubtypingClosure_ = true;
    bool DeferIncrementalSubtypingClosure_ = false;

    TypeSystemKind kind_;

    Index<TypeIdx> FindIdx(const TypeInfo &ti)
    {
        auto it = InfoToIdx_.find(ti);
        if (it != InfoToIdx_.end()) {
            return it->second;
        }
        return {};
    }

    TypeIdx FindIdxOrCreate(const TypeInfo &ti)
    {
        Index<TypeIdx> existing_idx = FindIdx(ti);
        if (existing_idx.IsValid()) {
            return existing_idx;
        }
        size_t idx = Universe_.size();
        TypingRel_.EnsureMinSize(idx);
        Universe_.push_back(ti);
        ParameterOf_.push_back({});
        const auto &params = ti.ParamsIdx();
        for (const auto &param : params) {
            ParameterOf_[param].insert(idx);
        }
        InfoToIdx_[ti] = idx;
        SortIdx sort = ti.Sort();
        size_t arity = params.size();
        if (sort >= TypeClasses_.size()) {
            TypeClasses_.resize(sort + 1);
        }
        TypeClasses_[sort][arity].push_back(idx);
        Relate(idx, idx);
        return idx;
    }

    const VectorIdx &TypeClassIdx(TypeIdx type) const
    {
        const auto &info = Universe_[type];
        const auto &params = info.ParamsIdx();
        return TypeClasses_[info.Sort()][params.size()];
    }

    void PerformClosingCurrentRelation() NO_THREAD_SAFETY_ANALYSIS
    {
        auto add_to_next = [this](TypeIdx type) NO_THREAD_SAFETY_ANALYSIS {
            const auto &info = Universe_[type];
            SubtypingClosureNext_.AddType(info.Sort(), type, info.Arity());
            return true;
        };
        while (!SubtypingClosureCurrent_.Empty()) {
            SubtypingClosureCurrent_.ForAllTypeClasses([this, &add_to_next](auto &types) {
                for (auto type_lhs : types) {
                    for (auto type_rhs : types) {
                        bool in_direct_relation = TypingRel_.IsInDirectRelation(type_lhs, type_rhs);
                        if (!in_direct_relation && CheckIfLhsSubtypeOfRhs(type_lhs, type_rhs)) {
                            add_to_next(type_lhs);
                            add_to_next(type_rhs);
                            TypingRel_ += {type_lhs, type_rhs};
                            for (const auto &type : ParameterOf_[type_lhs]) {
                                add_to_next(type);
                                TypingRel_.ForAllTo(type, add_to_next);
                                TypingRel_.ForAllFrom(type, add_to_next);
                            }
                            for (const auto &type : ParameterOf_[type_rhs]) {
                                add_to_next(type);
                                TypingRel_.ForAllTo(type, add_to_next);
                                TypingRel_.ForAllFrom(type, add_to_next);
                            }
                        }
                    }
                }
            });
            SubtypingClosureCurrent_.swap(SubtypingClosureNext_);
            SubtypingClosureNext_.Clear();
        }
    }

    void Relate(TypeIdx lhs, TypeIdx rhs)
    {
        if (TypingRel_.IsInDirectRelation(lhs, rhs)) {
            return;
        }
        TypingRel_ += {lhs, rhs};
        if (IncrementalSubtypingClosure_) {
            auto process_type = [this](TypeIdx type) {
                auto add_to_current = [this](TypeIdx type_idx) {
                    const auto &info = Universe_[type_idx];
                    SubtypingClosureCurrent_.AddType(info.Sort(), type_idx, info.Arity());
                    return true;
                };
                for (const auto &type_idx : TypeClassIdx(type)) {
                    add_to_current(type_idx);
                }
                for (const auto &type_idx : ParameterOf_[type]) {
                    add_to_current(type_idx);
                    TypingRel_.ForAllTo(type_idx, add_to_current);
                    TypingRel_.ForAllFrom(type_idx, add_to_current);
                }
            };
            process_type(lhs);
            if (lhs != rhs) {
                process_type(rhs);
            }
            if (!DeferIncrementalSubtypingClosure_) {
                PerformClosingCurrentRelation();
            }
        }
    }

    bool CheckIfLhsParamsSubtypeOfRhs(const TypeParamsIdx &lhs, const TypeParamsIdx &rhs) const
    {
        if (lhs.size() != rhs.size()) {
            return false;
        }
        auto lhs_it = lhs.cbegin();
        auto rhs_it = rhs.cbegin();
        for (; lhs_it != lhs.cend(); ++lhs_it, ++rhs_it) {
            switch (lhs_it->Variance()) {
                case TypeVariance::INVARIANT:
                    if (!TypingRel_.IsInIsoRelation(*lhs_it, *rhs_it)) {
                        return false;
                    }
                    break;
                case TypeVariance::COVARIANT:
                    if (!TypingRel_.IsInDirectRelation(*lhs_it, *rhs_it)) {
                        return false;
                    }
                    break;
                case TypeVariance::CONTRVARIANT:
                    if (!TypingRel_.IsInInverseRelation(*lhs_it, *rhs_it)) {
                        return false;
                    }
                    break;
                default:
                    break;
            }
        }
        return true;
    }

    bool CheckIfLhsSubtypeOfRhs(TypeIdx lhs, TypeIdx rhs) const
    {
        const TypeInfo &lhs_info = Universe_[lhs];
        const TypeInfo &rhs_info = Universe_[rhs];
        if (lhs_info.Sort() != rhs_info.Sort()) {
            return false;
        }
        const TypeParamsIdx &lhs_params = lhs_info.ParamsIdx();
        const TypeParamsIdx &rhs_params = rhs_info.ParamsIdx();
        return CheckIfLhsParamsSubtypeOfRhs(lhs_params, rhs_params);
    }

    bool IsInDirectRelation(TypeIdx lhs, TypeIdx rhs) const
    {
        return TypingRel_.IsInDirectRelation(lhs, rhs);
    }

    size_t GetSort(TypeIdx t) const
    {
        return Universe_[t].Sort();
    }

    size_t GetArity(TypeIdx t) const
    {
        return Universe_[t].Arity();
    }

    const TypeParamsIdx &GetParamsIdx(TypeIdx t) const
    {
        return Universe_[t].ParamsIdx();
    }

    friend class Type;
    friend class TypeParams;
    friend class ParametricType;

    void SetIncrementalRelationClosureMode(bool state)
    {
        IncrementalSubtypingClosure_ = state;
    }

    void SetDeferIncrementalRelationClosure(bool state)
    {
        DeferIncrementalSubtypingClosure_ = state;
    }

    template <typename Handler>
    void ForAllTypes(Handler &&handler) const
    {
        for (size_t idx = 0; idx < Universe_.size(); ++idx) {
            if (!handler(Type {kind_, idx})) {
                return;
            }
        }
    }

    template <typename Handler>
    void ForAllSubtypesOf(const Type &t, Handler &&handler) const
    {
        auto idx = t.Index();
        auto callback = [this, &handler](const TypeIdx &index) {
            bool result = handler(Type {kind_, index});
            return result;
        };
        TypingRel_.ForAllTo(idx, callback);
    }

    template <typename Handler>
    void ForAllSupertypesOf(const Type &t, Handler &&handler) const
    {
        auto idx = t.Index();
        auto callback = [this, &handler](const TypeIdx &index) {
            bool result = handler(Type {kind_, index});
            return result;
        };
        TypingRel_.ForAllFrom(idx, callback);
    }

    const IntSet<TypeIdx> &GetDirectlyRelated(TypeIdx from) const
    {
        return TypingRel_.GetDirectlyRelated(from);
    }

    const IntSet<TypeIdx> &GetInverselyRelated(TypeIdx to) const
    {
        return TypingRel_.GetInverselyRelated(to);
    }

    void CloseSubtypingRelation()
    {
        ForAllTypes([this](const Type &type) {
            auto sort = type.Sort();
            auto index = type.Index();
            auto arity = type.Arity();
            SubtypingClosureCurrent_.AddType(sort, index, arity);
            return true;
        });
        PerformClosingCurrentRelation();
    }

    void CloseAccumulatedSubtypingRelation()
    {
        if (IncrementalSubtypingClosure_) {
            if (DeferIncrementalSubtypingClosure_) {
                PerformClosingCurrentRelation();
            }
        } else {
            CloseSubtypingRelation();
        }
    }

    ParametricType Parametric(SortIdx sort)
    {
        return {kind_, sort};
    }

    Type Bot() const
    {
        return {kind_, BotIdx_};
    }

    Type Top() const
    {
        return {kind_, TopIdx_};
    }

    TypeSystemKind GetKind() const
    {
        return kind_;
    }

    TypeSystem(SortIdx bot, SortIdx top, TypeSystemKind kind = TypeSystemKind::PANDA)
        : kind_ {kind}, BotIdx_ {FindIdxOrCreate({bot, {}})}, TopIdx_ {FindIdxOrCreate({top, {}})}
    {
    }

private:
    TypeIdx BotIdx_;
    TypeIdx TopIdx_;
};
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_TYPE_TYPE_SYSTEM_H_
