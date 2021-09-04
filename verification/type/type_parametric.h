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

#ifndef PANDA_VERIFICATION_TYPE_TYPE_PARAMETRIC_H_
#define PANDA_VERIFICATION_TYPE_TYPE_PARAMETRIC_H_

#include "type_system_kind.h"
#include "type_sort.h"
#include "type_params.h"
#include "type_type.h"

namespace panda::verifier {
class TypeSystem;
class TypeParams;

class ParametricType {
public:
    TypeSystemKind kind_;
    SortIdx Sort_;
    ParametricType(TypeSystemKind kind, SortIdx sort) : kind_ {kind}, Sort_(sort) {}
    friend class TypeSystem;

    ParametricType() = delete;
    ParametricType(const ParametricType &) = default;
    ParametricType(ParametricType &&) = default;
    ParametricType &operator=(const ParametricType &) = default;
    ParametricType &operator=(ParametricType &&) = default;
    ~ParametricType() = default;

    TypeSystem &GetTypeSystem() const;
    bool operator[](TypeParamsIdx params) const;
    Type operator()(TypeParamsIdx params = {}) const;
    bool operator[](const TypeParams &params) const;
    Type operator()(const TypeParams &params) const;

    template <typename Handler>
    void ForAll(Handler &&handler) const;
};
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_TYPE_TYPE_PARAMETRIC_H_
