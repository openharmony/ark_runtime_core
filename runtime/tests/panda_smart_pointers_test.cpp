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

#include "gtest/gtest.h"
#include "runtime/include/coretypes/string.h"
#include "runtime/include/mem/panda_smart_pointers.h"
#include "runtime/include/runtime.h"

namespace panda::mem::test {

class PandaSmartPointersTest : public testing::Test {
public:
    PandaSmartPointersTest()
    {
        RuntimeOptions options;
        options.SetShouldLoadBootPandaFiles(false);
        options.SetShouldInitializeIntrinsics(false);
        options.SetLimitStandardAlloc(true);
        Runtime::Create(options);
        thread_ = panda::MTManagedThread::GetCurrent();
        thread_->ManagedCodeBegin();
    }

    ~PandaSmartPointersTest() override
    {
        thread_->ManagedCodeEnd();
        Runtime::Destroy();
    }

protected:
    panda::MTManagedThread *thread_ {nullptr};
};

int ReturnValueFromUniqPtr(PandaUniquePtr<int> ptr)
{
    return *ptr.get();
}

TEST_F(PandaSmartPointersTest, MakePandaUniqueTest)
{
    // Not array type

    static constexpr int POINTER_VALUE = 5;

    auto uniq_ptr = MakePandaUnique<int>(POINTER_VALUE);
    ASSERT_NE(uniq_ptr.get(), nullptr);

    int res = ReturnValueFromUniqPtr(std::move(uniq_ptr));
    ASSERT_EQ(res, 5);
    ASSERT_EQ(uniq_ptr.get(), nullptr);

    // Unbounded array type

    static constexpr size_t SIZE = 3;

    auto uniq_ptr_2 = MakePandaUnique<int[]>(SIZE);
    ASSERT_NE(uniq_ptr_2.get(), nullptr);

    for (size_t i = 0; i < SIZE; ++i) {
        uniq_ptr_2[i] = i;
    }

    auto uniq_ptr_3 = std::move(uniq_ptr_2);
    for (size_t i = 0; i < SIZE; ++i) {
        ASSERT_EQ(uniq_ptr_3[i], i);
    }
    ASSERT_EQ(uniq_ptr_2.get(), nullptr);
}

}  // namespace panda::mem::test
