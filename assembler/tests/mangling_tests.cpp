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

#include <gtest/gtest.h>
#include "mangling.h"
#include "assembly-function.h"
#include "extensions/extensions.h"

namespace panda::test {

using namespace panda::pandasm;

TEST(mangling_tests, MangleFunctionName)
{
    std::vector<Function::Parameter> params;
    extensions::Language language {extensions::Language::PANDA_ASSEMBLY};
    params.emplace_back(Type {"type1", 0}, language);
    params.emplace_back(Type {"type2", 0}, language);
    params.emplace_back(Type {"type3", 0}, language);

    auto return_type = Type("type4", 0);

    std::string name = "Asm.main";
    ASSERT_EQ(MangleFunctionName(name, std::move(params), return_type), "Asm.main:type1;type2;type3;type4;");
}

TEST(mangling_tests, DeMangleFunctionName)
{
    std::string name = "Asm.main:type1;type2;type3;type4;";
    ASSERT_EQ(DeMangleName(name), "Asm.main");
}

}  // namespace panda::test
