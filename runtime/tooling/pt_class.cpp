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

#include "runtime/include/tooling/pt_class.h"

#include <limits>

#include "runtime/include/runtime.h"
#include "libpandabase/utils/utf.h"
#include "pt_class_private.h"
#include "pt_lang_ext_private.h"

namespace panda::tooling {
static auto *g_invalidRef = reinterpret_cast<PtReference *>(std::numeric_limits<uintptr_t>::max());

// NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
static const PtClass DYNAMIC_CLASS = PtClass(g_invalidRef);

PtClass GetDynamicClass()
{
    return DYNAMIC_CLASS;
}

const char *PtClass::GetDescriptor() const
{
    if (GetReference() == DYNAMIC_CLASS.GetReference()) {
        return nullptr;
    }

    auto *ext = static_cast<PtLangExtPrivate *>(Runtime::GetCurrent()->GetPtLangExt());
    Class *runtimeClass = ext->PtClassToClass(*this);
    return utf::Mutf8AsCString(runtimeClass->GetDescriptor());
}
}  // namespace panda::tooling
