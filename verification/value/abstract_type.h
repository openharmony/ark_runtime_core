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

#ifndef PANDA_VERIFICATION_VALUE_ABSTRACT_TYPE_H_
#define PANDA_VERIFICATION_VALUE_ABSTRACT_TYPE_H_

#include "verification/value/variables.h"
#include "verification/type/type_set.h"

#include "macros.h"

#include <variant>

namespace panda::verifier {
class AbstractType {
public:
    struct None {
    };
    using ContentsData = std::variant<None, Variables::Var, Type, TypeSet>;
    AbstractType() = default;
    AbstractType(const AbstractType &) = default;
    AbstractType(AbstractType &&) = default;
    AbstractType(const Type &type) : Contents_ {type} {}
    AbstractType(const Variables::Var &var) : Contents_ {var} {}
    AbstractType(Variables::Var &&var) : Contents_ {std::move(var)} {}
    AbstractType(TypeSet &&type_set)
    {
        auto the_only_type = type_set.TheOnlyType();
        if (the_only_type.IsValid()) {
            Contents_ = the_only_type;
        } else {
            Contents_ = std::move(type_set);
        }
    }

    AbstractType &operator=(const AbstractType &) = default;
    AbstractType &operator=(AbstractType &&) = default;
    ~AbstractType() = default;
    AbstractType &operator=(const None &)
    {
        Contents_ = None {};
        return *this;
    }
    AbstractType &operator=(Variables::Var var)
    {
        Contents_ = var;
        return *this;
    }
    AbstractType &operator=(Type type)
    {
        Contents_ = type;
        return *this;
    }
    AbstractType &operator=(TypeSet &&type_set)
    {
        Contents_ = std::move(type_set);
        return *this;
    }

    Variables::Var GetVar() const
    {
        ASSERT(IsVar());
        return std::get<Variables::Var>(Contents_);
    }
    Type GetType() const
    {
        ASSERT(IsType());
        return std::get<Type>(Contents_);
    }
    const TypeSet &GetTypeSet() const
    {
        ASSERT(IsTypeSet());
        return std::get<TypeSet>(Contents_);
    }

    bool IsNone() const
    {
        return std::holds_alternative<None>(Contents_);
    }
    bool IsVar() const
    {
        return std::holds_alternative<Variables::Var>(Contents_);
    }
    bool IsType() const
    {
        return std::holds_alternative<Type>(Contents_);
    }
    bool IsTypeSet() const
    {
        return std::holds_alternative<TypeSet>(Contents_);
    }

    bool IsConsistent() const
    {
        if (IsType()) {
            return !GetType().IsTop();
        } else if (IsTypeSet()) {
            Type the_only_type = GetTypeSet().TheOnlyType();
            return !(the_only_type.IsValid() && the_only_type.IsTop());
        } else {
            return false;
        }
    }

    AbstractType operator&(const AbstractType &rhs) const
    {
        if (IsType()) {
            if (rhs.IsType()) {
                Type lhs_type = GetType();
                Type rhs_type = rhs.GetType();
                if (lhs_type <= rhs_type) {
                    return rhs_type;
                } else if (rhs_type <= lhs_type) {
                    return lhs_type;
                } else {
                    return lhs_type & rhs_type;
                }
            } else if (rhs.IsTypeSet()) {
                return MergeTypeAndTypeSet(GetType(), rhs.GetTypeSet());
            } else {
                UNREACHABLE();
            }
        } else if (IsTypeSet()) {
            if (rhs.IsType()) {
                return MergeTypeAndTypeSet(rhs.GetType(), GetTypeSet());
            } else if (rhs.IsTypeSet()) {
                return GetTypeSet() & rhs.GetTypeSet();
            } else {
                UNREACHABLE();
            }
        } else {
            UNREACHABLE();
        }
    }

    template <typename StrT, typename TypeImageFunc>
    StrT Image(TypeImageFunc type_img_func) const
    {
        if (IsNone()) {
            return "<none>";
        } else if (IsVar()) {
            return GetVar().Image<StrT>("<TypeVar") + ">";
        } else if (IsType()) {
            StrT result = type_img_func(GetType());
            return result;
        } else if (IsTypeSet()) {
            return GetTypeSet().Image<StrT>(type_img_func);
        }
        return "<unexpected kind of AbstractType>";
    }

    template <typename TypeHandler, typename Default>
    bool ForAllTypes(TypeHandler &&type_handler, Default &&non_type_handler) const
    {
        if (IsType()) {
            return type_handler(GetType());
        } else if (IsTypeSet()) {
            return GetTypeSet().ForAll(std::forward<TypeHandler>(type_handler));
        } else {
            return non_type_handler();
        }
    }

    template <typename TypeHandler>
    bool ForAllTypes(TypeHandler &&type_handler) const
    {
        return ForAllTypes(std::forward<TypeHandler>(type_handler), []() { return true; });
    }

    template <typename TypeHandler>
    bool ExistsType(TypeHandler &&type_handler) const
    {
        return !ForAllTypes([&type_handler](auto t) { return !type_handler(t); });
    }

private:
    ContentsData Contents_;

    AbstractType MergeTypeAndTypeSet(Type type, const TypeSet &type_set) const
    {
        if (type_set.Contains(type)) {
            return type;
        } else {
            return type & type_set;
        }
    }
};

}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_VALUE_ABSTRACT_TYPE_H_
