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

#ifndef PANDA_ASSEMBLER_MANGLING_H_
#define PANDA_ASSEMBLER_MANGLING_H_

#include "assembly-function.h"

#include <string>
#include <vector>

namespace panda::pandasm {
static std::string MANGLE_BEGIN = ":";
static std::string MANGLE_SEPARATOR = ";";

inline std::string MangleFunctionName(const std::string &name, const std::vector<pandasm::Function::Parameter> &params,
                                      const pandasm::Type &return_type)
{
    std::string mangle_name {name};
    mangle_name += MANGLE_BEGIN;
    for (const auto &p : params) {
        mangle_name += p.type.GetName() + MANGLE_SEPARATOR;
    }
    mangle_name += return_type.GetName() + MANGLE_SEPARATOR;

    return mangle_name;
}

inline std::string DeMangleName(const std::string &name)
{
    auto iter = name.find_first_of(MANGLE_BEGIN);
    if (iter != std::string::npos) {
        return name.substr(0, name.find_first_of(MANGLE_BEGIN));
    }
    return name;
}

inline std::string MangleFieldName(const std::string &name, const pandasm::Type &type)
{
    std::string mangle_name {name};
    mangle_name += MANGLE_BEGIN;
    mangle_name += type.GetName() + MANGLE_SEPARATOR;
    return mangle_name;
}

}  // namespace panda::pandasm

#endif  // PANDA_ASSEMBLER_MANGLING_H_
