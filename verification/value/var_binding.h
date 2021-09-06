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

#ifndef PANDA_VERIFICATION_VALUE_VAR_BINDING_H_
#define PANDA_VERIFICATION_VALUE_VAR_BINDING_H_

#include "verifier/type_system.h"
#include "verifier/value/variable.h"
#include "verifier/relation.h"

#include <variant>
#include <optional>
#include <unordered_map>

namespace panda::verifier {
template <typename BoundVarValue>
class VarBindings {
public:
    using EqualityRel = panda::type_system::Realtion<Variables::Var>;

    void Equate(Variables::Var lhs, Variable::Var rhs)
    {
        Equality_.SymmRelate(lhs, rhs);
    }
    void Bind(Variables::Var var, BoundVarValue val);
    bool IsEquated(Variables::Var v);
    bool IsBound(Variables::Var v);
    std::optional<BoundVarValue> operator[](Variable::Var v);
    auto AllInEqualClass(Variables::Var var);

private:
    std::unordered_map<Variables::Var, BoundVarValue> Bindings_;
    EqualityRel Equality_;
};
}  //  namespace panda::verifier

#endif  // PANDA_VERIFICATION_VALUE_VAR_BINDING_H_
