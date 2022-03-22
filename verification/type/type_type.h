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

#ifndef PANDA_VERIFICATION_TYPE_TYPE_TYPE_H_
#define PANDA_VERIFICATION_TYPE_TYPE_TYPE_H_

#include "type_sort.h"
#include "type_index.h"
#include "type_info.h"

#include "type_system_kind.h"

namespace panda::verifier {
class TypeSystem;
class TypeSet;
class TypeParam;
class TypeParams;

class Type {
public:
    Type() = default;
    Type(const Type &) = default;
    Type(Type &&) = default;
    Type &operator=(const Type &) = default;
    Type &operator=(Type &&) = default;
    ~Type() = default;

    bool operator==(const Type &t) const;

    bool operator!=(const Type &t) const;

    const Type &operator<<(const Type &t) const;

    // def subtyping a << (b | c) << d
    const TypeSet &operator<<(const TypeSet &s) const;

    // a workaround for absence of mutualy-recursive classes in C++
    template <typename...>
    TypeSet operator|(const Type &t) const;

    template <typename...>
    TypeParam operator+() const;

    template <typename...>
    TypeParam operator-() const;

    template <typename...>
    TypeParam operator~() const;

    // subtyping relation: <=
    bool operator<=(const Type &rhs) const;
    bool operator<=(const TypeParams &rhs) const;

    SortIdx Sort() const;

    size_t Arity() const;

    template <typename...>
    TypeParams Params() const;

    size_t ParamsSize() const;

    TypeSystem &GetTypeSystem() const;

    TypeSystemKind GetTypeSystemKind() const;

    bool IsValid() const;

    bool IsTop() const;

    bool IsBot() const;

    TypeSet operator&(const Type &rhs) const;

    TypeSet operator&(const TypeSet &rhs) const;

    template <typename...>
    TypeParam operator*(TypeVariance variance) const;

    template <typename Handler>
    void ForAllParams(Handler &&handler) const;

    template <typename Handler>
    void ForAllSupertypes(Handler &&handler) const;

    template <typename Handler>
    void ForAllSupertypesOfSort(SortIdx sort, Handler &&handler) const;

    template <typename Handler>
    void ForAllSubtypes(Handler &&handler) const;

    template <typename Handler>
    void ForAllSubtypesOfSort(SortIdx sort, Handler &&handler) const;

private:
    Type(TypeSystemKind kind, TypeIdx idx) : Idx_ {kind, idx} {}

    TypeIdx Index() const;

    TaggedIndex<TypeSystemKind> Idx_;

    friend class TypeSystem;
    friend class TypeParam;
    friend class ParametricType;
    friend class PandaTypes;
    friend class TypeSet;

    template <typename SortNames>
    friend class TypeImage;

    friend struct std::hash<Type>;
};
}  // namespace panda::verifier

namespace std {
template <>
struct hash<panda::verifier::Type> {
    size_t operator()(const panda::verifier::Type &type) const
    {
        return static_cast<size_t>(type.Index());
    }
};
}  // namespace std

#endif  // PANDA_VERIFICATION_TYPE_TYPE_TYPE_H_
