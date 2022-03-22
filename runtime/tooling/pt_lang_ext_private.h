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

#ifndef PANDA_RUNTIME_TOOLING_PT_LANG_EXT_PRIVATE_H_
#define PANDA_RUNTIME_TOOLING_PT_LANG_EXT_PRIVATE_H_

#include <string_view>

#include "runtime/interpreter/frame.h"
#include "runtime/include/mem/panda_smart_pointers.h"
#include "runtime/include/tooling/debug_interface.h"
#include "runtime/include/tooling/pt_lang_extension.h"

namespace panda::tooling {
class PtLangExtPrivate : public PtLangExt {
public:
    virtual std::optional<Error> GetPtValueFromManaged(const Frame::VRegister &vreg, PtValue *outValue) const = 0;
    virtual void ReleasePtValueFromManaged(const PtValue *value) const = 0;
    virtual std::optional<Error> StorePtValueFromManaged(const PtValue &value, Frame::VRegister *inOutVreg) const = 0;

    virtual Class *PtClassToClass(const PtClass &klass) const = 0;
    virtual PtClass ClassToPtClass(BaseClass *klass) const = 0;

    virtual Field *PtPropertyToField(const PtProperty &property) const = 0;
    virtual PtProperty FieldToPtProperty(Field *field) const = 0;
};

PandaUniquePtr<PtLangExt> CreatePtLangExt(std::string_view language);

}  // namespace panda::tooling

#endif  // PANDA_RUNTIME_TOOLING_PT_LANG_EXT_PRIVATE_H_
