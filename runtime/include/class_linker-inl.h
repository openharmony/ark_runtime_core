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

#ifndef PANDA_RUNTIME_INCLUDE_CLASS_LINKER_INL_H_
#define PANDA_RUNTIME_INCLUDE_CLASS_LINKER_INL_H_

#include "libpandafile/panda_cache.h"
#include "runtime/include/class_linker.h"
#include "runtime/include/runtime.h"

namespace panda {

inline Class *ClassLinker::GetClass(const Method &caller, panda_file::File::EntityId id,
                                    ClassLinkerErrorHandler *error_handler /* = nullptr */)
{
    Class *klass = caller.GetPandaFile()->GetPandaCache()->GetClassFromCache(id);
    if (klass != nullptr) {
        return klass;
    }
    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(caller);
    auto *ext = GetExtension(ctx);
    ASSERT(ext != nullptr);
    klass = ext->GetClass(*caller.GetPandaFile(), id, caller.GetClass()->GetLoadContext(),
                          error_handler == nullptr ? ext->GetErrorHandler() : error_handler);
    if (LIKELY(klass != nullptr)) {
        caller.GetPandaFile()->GetPandaCache()->SetClassCache(id, klass);
    }
    return klass;
}

inline void ClassLinker::AddClassRoot(ClassRoot root, Class *klass)
{
    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(*klass);
    auto *ext = GetExtension(ctx);
    ASSERT(ext != nullptr);
    ext->SetClassRoot(root, klass);

    RemoveCreatedClassInExtension(klass);
}

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_CLASS_LINKER_INL_H_
