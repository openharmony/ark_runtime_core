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

#ifndef PANDA_VERIFICATION_TYPE_TYPE_SET_H_
#define PANDA_VERIFICATION_TYPE_TYPE_SET_H_

#include "type_type.h"

#include "runtime/include/mem/panda_containers.h"

namespace panda::verifier {
class TypeSystems;

class TypeSet {
public:
    TypeSet() = delete;

    template <typename... Types>
    explicit TypeSet(const Type &t, Types... types) : Kind_ {t.GetTypeSystemKind()}
    {
        Indices_.Insert(t.Index());
        if constexpr (sizeof...(types) > 0) {
            (Insert(types), ...);
        }
    }

    explicit TypeSet(TypeSystemKind kind, IntSet<TypeIdx> &&indices = {}) : Kind_ {kind}, Indices_ {indices} {};

    ~TypeSet() = default;

    void Insert(const Type &t)
    {
        ASSERT(t.GetTypeSystemKind() == Kind_);
        Indices_.Insert(t.Index());
    }

    TypeSet &operator|(const Type &t)
    {
        Insert(t);
        return *this;
    }

    bool Contains(const Type &t) const
    {
        return t.GetTypeSystemKind() == Kind_ && Indices_.Contains(t.Index());
    }

    const Type &operator<<(const Type &st) const;

    const TypeSet &operator<<(const TypeSet &st) const;

    TypeSet operator&(const Type &rhs) const;

    TypeSet operator&(const TypeSet &rhs) const;

    size_t Size() const
    {
        return Indices_.Size();
    }

    bool IsEmpty() const
    {
        return Size() == 0;
    }

    Type TheOnlyType() const
    {
        if (Size() == 1) {
            return {Kind_, *(Indices_.begin())};
        } else {
            return {};
        }
    }

    template <typename Handler>
    bool ForAll(Handler &&handler) const
    {
        for (TypeIdx index : Indices_) {
            if (!handler(Type(Kind_, index))) {
                return false;
            }
        }
        return true;
    }

    template <typename Handler>
    bool Exists(Handler &&handler) const
    {
        return !ForAll([handler {std::move(handler)}](Type t) { return !handler(t); });
    }

    template <typename StrT, typename TypeImageFunc>
    StrT Image(TypeImageFunc type_img_func) const
    {
        StrT result {"TypeSet{"};
        bool first = true;
        ForAll([&](const Type &type) {
            if (first) {
                first = false;
            } else {
                result += ", ";
            }
            result += type_img_func(type);
            return true;
        });
        result += "}";
        return result;
    }

    bool operator==(const TypeSet &rhs) const
    {
        return Kind_ == rhs.Kind_ && Indices_ == rhs.Indices_;
    }

    bool operator!=(const TypeSet &rhs) const
    {
        return !(*this == rhs);
    }

private:
    TypeSystemKind Kind_;
    IntSet<TypeIdx> Indices_;
};
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_TYPE_TYPE_SET_H_
