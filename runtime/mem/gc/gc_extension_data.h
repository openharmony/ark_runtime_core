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

#ifndef PANDA_RUNTIME_MEM_GC_GC_EXTENSION_DATA_H_
#define PANDA_RUNTIME_MEM_GC_GC_EXTENSION_DATA_H_

#include "macros.h"
#include "runtime/include/language_config.h"

namespace panda::mem {

// Base class for all GC language-specific data holders.
// Can be extended for different language types.
class GCExtensionData {
public:
    GCExtensionData() = default;
    virtual ~GCExtensionData() = default;
    NO_COPY_SEMANTIC(GCExtensionData);
    NO_MOVE_SEMANTIC(GCExtensionData);

#ifndef NDEBUG
    LangTypeT GetLangType()
    {
        return type_;
    }

protected:
    void SetLangType(LangTypeT type)
    {
        type_ = type;
    }

private:
    // Used for assertions in inherited classes to ensure that
    // language extension got the corresponding type of data
    LangTypeT type_ {LANG_TYPE_STATIC};
#endif  // NDEBUG
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_GC_GC_EXTENSION_DATA_H_
