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

#include "gtest/gtest.h"
#include "runtime/include/runtime.h"
#include "runtime/include/runtime_options.h"

namespace panda::mem::test {

class MemLeakTest : public testing::Test {
public:
    void CreateRuntime()
    {
        RuntimeOptions options;
        options.SetShouldLoadBootPandaFiles(false);
        options.SetShouldInitializeIntrinsics(false);
        options.SetRunGcInPlace(true);
        Runtime::Create(options);
    }
};

#ifndef NDEBUG

TEST_F(MemLeakTest, MemLeak4BTest)
{
    EXPECT_DEATH(
        {
            CreateRuntime();
            mem::InternalAllocatorPtr allocator = Runtime::GetCurrent()->GetInternalAllocator();
            auto ptr = allocator->Alloc(4);
            ASSERT_NE(ptr, nullptr);

            Runtime::Destroy();
        },
        "");
}

TEST_F(MemLeakTest, MemLeak1KBTest)
{
    EXPECT_DEATH(
        {
            CreateRuntime();
            mem::InternalAllocatorPtr allocator = Runtime::GetCurrent()->GetInternalAllocator();
            auto ptr = allocator->Alloc(1_KB);
            ASSERT_NE(ptr, nullptr);

            Runtime::Destroy();
        },
        "");
}

TEST_F(MemLeakTest, MemLeak1MBTest)
{
    EXPECT_DEATH(
        {
            CreateRuntime();
            mem::InternalAllocatorPtr allocator = Runtime::GetCurrent()->GetInternalAllocator();
            auto ptr = allocator->Alloc(1_MB);
            ASSERT_NE(ptr, nullptr);

            Runtime::Destroy();
        },
        "");
}

#endif  // NDEBUG

}  // namespace panda::mem::test
