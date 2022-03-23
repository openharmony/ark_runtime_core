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

#include "runtime/include/field.h"

#include "libpandabase/utils/hash.h"
#include "libpandafile/field_data_accessor.h"
#include "libpandafile/file-inl.h"
#include "runtime/include/class_linker.h"
#include "runtime/include/runtime.h"

namespace panda {

panda_file::File::StringData Field::GetName() const
{
    panda_file::FieldDataAccessor fda(*panda_file_, file_id_);
    return panda_file_->GetStringData(fda.GetNameId());
}

Class *Field::ResolveTypeClass(ClassLinkerErrorHandler *error_handler) const
{
    panda_file::FieldDataAccessor fda(*panda_file_, file_id_);
    auto *class_linker = Runtime::GetCurrent()->GetClassLinker();

    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(*class_);
    auto *ext = class_linker->GetExtension(ctx);
    ASSERT(ext != nullptr);

    switch (type_.GetId()) {
        case panda_file::Type::TypeId::U1:
            return ext->GetClassRoot(ClassRoot::U1);
        case panda_file::Type::TypeId::I8:
            return ext->GetClassRoot(ClassRoot::I8);
        case panda_file::Type::TypeId::U8:
            return ext->GetClassRoot(ClassRoot::U8);
        case panda_file::Type::TypeId::I16:
            return ext->GetClassRoot(ClassRoot::I16);
        case panda_file::Type::TypeId::U16:
            return ext->GetClassRoot(ClassRoot::U16);
        case panda_file::Type::TypeId::I32:
            return ext->GetClassRoot(ClassRoot::I32);
        case panda_file::Type::TypeId::U32:
            return ext->GetClassRoot(ClassRoot::U32);
        case panda_file::Type::TypeId::I64:
            return ext->GetClassRoot(ClassRoot::I64);
        case panda_file::Type::TypeId::U64:
            return ext->GetClassRoot(ClassRoot::U64);
        case panda_file::Type::TypeId::F32:
            return ext->GetClassRoot(ClassRoot::F32);
        case panda_file::Type::TypeId::F64:
            return ext->GetClassRoot(ClassRoot::F64);
        case panda_file::Type::TypeId::TAGGED:
            return ext->GetClassRoot(ClassRoot::TAGGED);
        case panda_file::Type::TypeId::REFERENCE:
            return ext->GetClass(*panda_file_, panda_file::File::EntityId(fda.GetType()), class_->GetLoadContext(),
                                 error_handler);
        default:
            UNREACHABLE();
    }
}

}  // namespace panda
