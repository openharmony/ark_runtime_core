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

#ifndef PANDA_VERIFICATION_TYPE_TYPE_SYSTEMS_H_
#define PANDA_VERIFICATION_TYPE_TYPE_SYSTEMS_H_

#include "type_sort.h"
#include "type_system_kind.h"

#include "verification/value/variables.h"

#include "macros.h"

namespace panda::verifier {
class TypeSystem;
class Type;
class TypeParams;

class TypeSystems {
public:
    static const PandaString &ImageOfType(const Type &type);
    static PandaString ImageOfTypeParams(const TypeParams &type_params);
    static SortIdx GetSort(TypeSystemKind kind, const PandaString &name);
    static TypeSystem &Get(TypeSystemKind kind);
    static Variables::Var GetVar(TypeSystemKind kind);

    static void Initialize() NO_THREAD_SANITIZE;
    static void Destroy() NO_THREAD_SANITIZE;

private:
    struct Impl;
    static Impl *impl;
};
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_TYPE_TYPE_SYSTEMS_H_
