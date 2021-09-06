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

#include "runtime/include/vtable_builder.h"
#include "runtime/include/class_linker.h"

namespace panda {

bool MethodInfo::Proto::AreTypesEqual(const Proto &other, panda_file::Type t1, panda_file::Type t2,
                                      size_t ref_idx) const
{
    if (t1 != t2) {
        return false;
    }

    if (!t1.IsPrimitive()) {
        auto &pf1 = pda_.GetPandaFile();
        auto &pf2 = other.pda_.GetPandaFile();

        auto name1 = pf1.GetStringData(pda_.GetReferenceType(ref_idx));
        auto name2 = pf2.GetStringData(other.pda_.GetReferenceType(ref_idx));

        if (name1 != name2) {
            return false;
        }
    }

    return true;
}

bool MethodInfo::Proto::IsEqualBySignatureAndReturnType(const Proto &other) const
{
    if (pda_.GetNumArgs() != other.pda_.GetNumArgs()) {
        return false;
    }

    auto rt1 = pda_.GetReturnType();
    auto rt2 = other.pda_.GetReturnType();
    if (!AreTypesEqual(other, rt1, rt2, 0)) {
        return false;
    }

    size_t ref_idx = rt1.IsPrimitive() ? 0 : 1;
    for (size_t i = 0; i < pda_.GetNumArgs(); i++) {
        auto t1 = pda_.GetArgType(i);
        auto t2 = other.pda_.GetArgType(i);

        if (!AreTypesEqual(other, t1, t2, ref_idx)) {
            return false;
        }

        if (!t1.IsPrimitive()) {
            ++ref_idx;
        }
    }

    return true;
}

}  // namespace panda
