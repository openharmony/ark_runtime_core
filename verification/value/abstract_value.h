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

#ifndef PANDA_VERIFICATION_VALUE_ABSTRACT_VALUE_H_
#define PANDA_VERIFICATION_VALUE_ABSTRACT_VALUE_H_

#include "verification/value/variables.h"

#include "macros.h"

#include <variant>

namespace panda::verifier {
class AbstractValue {
public:
    struct None {
    };
    using ContentsData = std::variant<None, Variables::Var>;
    AbstractValue() = default;
    AbstractValue(const AbstractValue &) = default;
    AbstractValue(AbstractValue &&) = default;
    AbstractValue &operator=(const AbstractValue &) = default;
    AbstractValue &operator=(AbstractValue &&) = default;
    ~AbstractValue() = default;
    AbstractValue(const Variables::Var &var) : Contents_ {var} {}
    Variables::Var GetVar() const
    {
        ASSERT(IsVar());
        return std::get<Variables::Var>(Contents_);
    }
    AbstractValue &operator=(const None &)
    {
        Contents_ = None {};
        return *this;
    }
    AbstractValue &operator=(Variables::Var var)
    {
        Contents_ = var;
        return *this;
    }
    bool IsNone() const
    {
        return std::holds_alternative<None>(Contents_);
    }
    bool IsVar() const
    {
        return std::holds_alternative<Variables::Var>(Contents_);
    }
    void Clear()
    {
        Contents_ = None {};
    }
    AbstractValue operator&([[maybe_unused]] const AbstractValue &rhs) const
    {
        // currently only None is supported
        // ASSERT(IsNone() && rhs.IsNone())
        return {};
    }

private:
    ContentsData Contents_ {None {}};
};
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_VALUE_ABSTRACT_VALUE_H_
