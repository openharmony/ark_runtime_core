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

#include "runtime/tests/class_linker_test_extension.h"

#include "runtime/include/class_linker-inl.h"
#include "runtime/include/runtime.h"

namespace panda::test {

// Runtime::GetCurrent() can not be used in .h files
bool ClassLinkerTestExtension::InitializeImpl([[maybe_unused]] bool compressed_string_enabled)
{
    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(GetLanguage());

    auto *class_class = CreateClass(ctx.GetClassClassDescriptor(), GetClassVTableSize(ClassRoot::CLASS),
                                    GetClassIMTSize(ClassRoot::CLASS), GetClassSize(ClassRoot::CLASS));
    coretypes::Class::FromRuntimeClass(class_class)->SetClass(class_class);
    class_class->SetState(Class::State::LOADED);

    auto *obj_class = CreateClass(ctx.GetObjectClassDescriptor(), GetClassVTableSize(ClassRoot::OBJECT),
                                  GetClassIMTSize(ClassRoot::OBJECT), GetClassSize(ClassRoot::OBJECT));
    obj_class->SetObjectSize(ObjectHeader::ObjectHeaderSize());
    class_class->SetBase(obj_class);
    obj_class->SetState(Class::State::LOADED);

    GetClassLinker()->AddClassRoot(ClassRoot::OBJECT, obj_class);
    GetClassLinker()->AddClassRoot(ClassRoot::CLASS, class_class);

    return true;
}

}  // namespace panda::test
