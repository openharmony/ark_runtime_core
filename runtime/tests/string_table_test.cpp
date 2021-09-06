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
#include "runtime/include/coretypes/string.h"
#include "runtime/include/runtime.h"
#include "runtime/include/thread.h"
#include "runtime/include/gc_task.h"
#include "runtime/include/panda_vm.h"
#include "runtime/handle_base-inl.h"

namespace panda::mem::test {

class StringTableTest : public testing::Test {
public:
    StringTableTest()
    {
        RuntimeOptions options;
        options.SetShouldLoadBootPandaFiles(false);
        options.SetShouldInitializeIntrinsics(false);

        options.SetCompilerEnableJit(false);
        Runtime::Create(options);
    }

    ~StringTableTest() override
    {
        Runtime::Destroy();
    }

    static coretypes::String *AllocUtf8String(std::vector<uint8_t> data)
    {
        LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
        return coretypes::String::CreateFromMUtf8(data.data(), utf::MUtf8ToUtf16Size(data.data()), ctx,
                                                  Runtime::GetCurrent()->GetPandaVM());
    }

    void SetUp() override
    {
        thread_ = panda::MTManagedThread::GetCurrent();
        thread_->ManagedCodeBegin();
    }

    void TearDown() override
    {
        thread_->ManagedCodeEnd();
    }

protected:
    panda::MTManagedThread *thread_ {nullptr};
};

TEST_F(StringTableTest, EmptyTable)
{
    auto table = StringTable();
    ASSERT_EQ(table.Size(), 0);
}

TEST_F(StringTableTest, InternCompressedUtf8AndString)
{
    auto table = StringTable();
    std::vector<uint8_t> data {0x01, 0x02, 0x03, 0x00};
    auto *string = AllocUtf8String(data);
    auto *interned_str1 =
        table.GetOrInternString(data.data(), data.size() - 1,
                                Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY));
    auto *interned_str2 = table.GetOrInternString(
        string, Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY));
    ASSERT_EQ(interned_str1, interned_str2);
    ASSERT_EQ(table.Size(), 1);
}

TEST_F(StringTableTest, InternUncompressedUtf8AndString)
{
    auto table = StringTable();
    std::vector<uint8_t> data {0xc2, 0xa7, 0x34, 0x00};
    auto *string = AllocUtf8String(data);
    auto *interned_str1 = table.GetOrInternString(
        data.data(), 2, Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY));
    auto *interned_str2 = table.GetOrInternString(
        string, Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY));
    ASSERT_EQ(interned_str1, interned_str2);
    ASSERT_EQ(table.Size(), 1);
}

TEST_F(StringTableTest, InternTheSameUtf16String)
{
    auto table = StringTable();
    std::vector<uint16_t> data {0xffc3, 0x33, 0x00};

    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto *first_string =
        coretypes::String::CreateFromUtf16(data.data(), data.size(), ctx, Runtime::GetCurrent()->GetPandaVM());
    auto *second_string =
        coretypes::String::CreateFromUtf16(data.data(), data.size(), ctx, Runtime::GetCurrent()->GetPandaVM());

    auto *interned_str1 = table.GetOrInternString(first_string, ctx);
    auto *interned_str2 = table.GetOrInternString(second_string, ctx);
    ASSERT_EQ(interned_str1, interned_str2);
    ASSERT_EQ(table.Size(), 1);
}

TEST_F(StringTableTest, InternManyStrings)
{
    static constexpr size_t ITERATIONS = 50;
    auto table = StringTable();
    std::vector<uint8_t> data {0x00};

    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
    for (size_t i = 0; i < ITERATIONS; i++) {
        data.insert(data.begin(), (('a' + i) % 25) + 1);
        [[maybe_unused]] auto *first_pointer = table.GetOrInternString(AllocUtf8String(data), ctx);
        [[maybe_unused]] auto *second_pointer =
            table.GetOrInternString(data.data(), utf::MUtf8ToUtf16Size(data.data()), ctx);
        auto *third_pointer = table.GetOrInternString(AllocUtf8String(data), ctx);
        ASSERT_EQ(first_pointer, second_pointer);
        ASSERT_EQ(second_pointer, third_pointer);
    }
    ASSERT_EQ(table.Size(), ITERATIONS);
}

TEST_F(StringTableTest, SweepObjectInTable)
{
    auto table = thread_->GetVM()->GetStringTable();
    auto table_init_size = table->Size();
    std::vector<uint8_t> data1 {0x01, 0x00};
    std::vector<uint8_t> data2 {0x02, 0x00};
    std::vector<uint8_t> data3 {0x03, 0x00};
    auto *s1 = AllocUtf8String(data1);
    auto *s2 = AllocUtf8String(data2);
    auto *s3 = AllocUtf8String(data3);
    auto thread = ManagedThread::GetCurrent();
    [[maybe_unused]] HandleScope<ObjectHeader *> scope(thread);
    VMHandle<coretypes::String> s2h(thread, s2);
    VMHandle<coretypes::String> s3h(thread, s3);
    table->GetOrInternString(s1, Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY));
    table->GetOrInternString(s2, Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY));
    table->GetOrInternString(s3, Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY));
    s1->SetMarkedForGC();
    s3->SetMarkedForGC();
    ASSERT_FALSE(s2->IsMarkedForGC());
    thread_->GetVM()->GetGC()->WaitForGCInManaged(panda::GCTask(panda::GCTaskCause::EXPLICIT_CAUSE));
    // There is no guarantee that Tenured GC will be called - so GE instead of EQ
    ASSERT_GE(table->Size(), table_init_size + 2);
}

}  // namespace panda::mem::test
