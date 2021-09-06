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

#ifndef PANDA_RUNTIME_INCLUDE_TOOLING_PT_LANG_EXTENSION_H_
#define PANDA_RUNTIME_INCLUDE_TOOLING_PT_LANG_EXTENSION_H_

#include "libpandabase/macros.h"
#include "runtime/include/mem/panda_containers.h"
#include "runtime/include/tooling/pt_class.h"
#include "runtime/include/tooling/pt_object.h"
#include "runtime/include/tooling/pt_property.h"
#include "runtime/include/tooling/pt_value.h"

namespace panda::tooling {
class PtLangExt {
public:
    PtLangExt() = default;
    virtual ~PtLangExt() = default;

    // PtValue API
    virtual PtObject ValueToObject(PtValue value) const = 0;

    // PtClass API
    virtual PtClass GetClass(PtObject object) const = 0;
    virtual PtClass GetClass(PtProperty property) const = 0;
    virtual void ReleaseClass(PtClass klass) const = 0;
    virtual const char *GetClassDescriptor(PtClass klass) const = 0;

    // PtObject API
    virtual PandaList<PtProperty> GetProperties(PtObject object) const = 0;
    virtual PtProperty GetProperty(PtObject object, const char *propertyName) const = 0;
    virtual bool AddProperty(PtObject object, const char *propertyName, PtValue value) const = 0;
    virtual bool RemoveProperty(PtObject object, const char *propertyName) const = 0;

    // PtProperty API
    virtual const char *GetPropertyName(PtProperty property) const = 0;
    virtual PtValue GetPropertyValue(PtProperty property) const = 0;
    virtual void SetPropertyPtValue(PtProperty property, PtValue value) const = 0;
    virtual void ReleasePtValue(const PtValue *value) const = 0;

    NO_COPY_SEMANTIC(PtLangExt);
    NO_MOVE_SEMANTIC(PtLangExt);
};

class PtScopedValue {
public:
    PtScopedValue(PtLangExt *ext, PtValue value) : ext_(ext), value_(value) {}
    ~PtScopedValue()
    {
        ext_->ReleasePtValue(&value_);
    }

    PtValue &GetValue()
    {
        return value_;
    }

    NO_COPY_SEMANTIC(PtScopedValue);
    NO_MOVE_SEMANTIC(PtScopedValue);

private:
    PtLangExt *ext_;
    PtValue value_;
};

}  // namespace panda::tooling

#endif  // PANDA_RUNTIME_INCLUDE_TOOLING_PT_LANG_EXTENSION_H_
