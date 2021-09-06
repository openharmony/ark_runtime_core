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
#include <vector>

#include "runtime/include/runtime_options.h"
#include "libpandabase/utils/pandargs.h"

namespace panda::test {

class RuntimeOptionsTest : public testing::Test {
public:
    RuntimeOptionsTest() : runtime_options_("AAA") {}
    ~RuntimeOptionsTest() = default;

    void SetUp() override
    {
        runtime_options_.AddOptions(&pa_parser_);
        LoadCorrectOptionsList();
    }

    void TearDown() override {}

    panda::PandArgParser *GetParser()
    {
        return &pa_parser_;
    }

    const std::vector<std::string> &GetCorrectOptionsList() const
    {
        return correct_options_list_;
    }

private:
    void LoadCorrectOptionsList();
    void AddTestingOption(std::string opt, std::string value);

    RuntimeOptions runtime_options_;
    panda::PandArgParser pa_parser_;
    std::vector<std::string> correct_options_list_;
};

void RuntimeOptionsTest::AddTestingOption(std::string opt, std::string value)
{
    correct_options_list_.push_back("--" + opt + "=" + value);
}

void RuntimeOptionsTest::LoadCorrectOptionsList()
{
    AddTestingOption("runtime-compressed-strings-enabled", "true");
    AddTestingOption("run-gc-in-place", "true");
    AddTestingOption("compiler-enable-jit", "true");
    AddTestingOption("sigquit-flag", "1234");
    AddTestingOption("dfx-log", "1234");
    AddTestingOption("gc-dump-heap", "true");
    AddTestingOption("log-file", "string");
    AddTestingOption("start-as-zygote", "true");
    AddTestingOption("verification-enabled", "true");
    AddTestingOption("pre-gc-heap-verify-enabled", "true");
}

// Testing that generator correctly generate options for different languages
TEST_F(RuntimeOptionsTest, TestCorrectOptions)
{
    ASSERT_TRUE(GetParser()->Parse(GetCorrectOptionsList()));
}

// Testing that panda::PandArgParser detect invalid options and types
TEST_F(RuntimeOptionsTest, TestIncorrectOptions)
{
    std::vector<std::string> invalid_options;
    invalid_options.push_back("--InvalidOptionThatNotExistAndNeverWillBeAdded=true");
    ASSERT_FALSE(GetParser()->Parse(invalid_options));
    ASSERT_EQ(GetParser()->GetErrorString(),
              "pandargs: Invalid option \"InvalidOptionThatNotExistAndNeverWillBeAdded\"\n");

    invalid_options.clear();
    invalid_options.push_back("--run-gc-in-place=1234");
    ASSERT_FALSE(GetParser()->Parse(invalid_options));
    ASSERT_EQ(GetParser()->GetErrorString(),
              "pandargs: Bool argument run-gc-in-place has unexpected parameter value 1234\n");
}

TEST_F(RuntimeOptionsTest, TestTailArgumets)
{
    GetParser()->EnableTail();
    std::vector<std::string> options_vector = GetCorrectOptionsList();
    panda::PandArg<std::string> file("file", "", "path to pandafile");

    options_vector.push_back("tail1");
    GetParser()->PushBackTail(&file);
    ASSERT_TRUE(GetParser()->Parse(options_vector));

    options_vector.push_back("tail2");
    ASSERT_FALSE(GetParser()->Parse(options_vector));
    ASSERT_EQ(GetParser()->GetErrorString(), "pandargs: Too many tail arguments given\n");

    GetParser()->DisableTail();
    options_vector.pop_back();
    ASSERT_FALSE(GetParser()->Parse(options_vector));
    ASSERT_EQ(GetParser()->GetErrorString(),
              "pandargs: Tail arguments are not enabled\npandargs: Tail found at literal \"tail1\"\n");
}

}  // namespace panda::test
