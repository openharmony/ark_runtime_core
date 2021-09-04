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

#include "runtime/interpreter/runtime_interface.h"

#include "runtime/include/runtime.h"

namespace panda::interpreter {

// Runtime::GetCurrent() can not be used in .h files
ObjectHeader *RuntimeInterface::CreateObject(Class *klass)
{
    ASSERT(!klass->IsArrayClass());

    if (klass->IsStringClass()) {
        LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(*klass);
        return coretypes::String::CreateEmptyString(ctx, Runtime::GetCurrent()->GetPandaVM());
    }

    if (LIKELY(klass->IsInstantiable())) {
        return ObjectHeader::Create(klass);
    }

    const auto &name = klass->GetName();
    PandaString pname(name.cbegin(), name.cend());
    panda::ThrowInstantiationError(pname);
    return nullptr;
}

}  // namespace panda::interpreter
