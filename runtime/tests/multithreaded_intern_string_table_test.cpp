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
#include "runtime/include/runtime.h"
#include "runtime/include/thread.h"
#include "runtime/include/gc_task.h"
#include "runtime/handle_base-inl.h"

#include <array>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace panda::mem::test {

static constexpr uint32_t TEST_THREADS = 8;
static constexpr uint32_t TEST_ITERS = 100;

class MultithreadedInternStringTableTest : public testing::Test {
public:
    MultithreadedInternStringTableTest()
    {
        RuntimeOptions options;
        options.SetShouldLoadBootPandaFiles(false);
        options.SetShouldInitializeIntrinsics(false);

        options.SetGcType("epsilon");
        options.SetCompilerEnableJit(false);
        Runtime::Create(options);
    }

    ~MultithreadedInternStringTableTest() override
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
        table_ = new StringTable();
        thread_ = panda::MTManagedThread::GetCurrent();
        thread_->ManagedCodeBegin();
    }

    void TearDown() override
    {
        thread_->ManagedCodeEnd();
        delete table_;
        table_ = nullptr;
    }

    StringTable *GetTable() const
    {
        return table_;
    }

    void PreCheck()
    {
        std::unique_lock<std::mutex> lk(pre_lock_);
        counter_pre_++;
        if (counter_pre_ == TEST_THREADS) {
            pre_cv_.notify_all();
            counter_pre_ = 0;
        } else {
            pre_cv_.wait(lk);
        }
    }

    void CheckSameString(coretypes::String *string)
    {
        // Loop until lock is taken
        while (lock_.test_and_set(std::memory_order_seq_cst)) {
        }
        if (string_ != nullptr) {
            ASSERT_EQ(string_, string);
        } else {
            string_ = string;
        }
        lock_.clear(std::memory_order_seq_cst);
    }

    void PostFree()
    {
        std::unique_lock<std::mutex> lk(post_lock_);
        counter_post_++;
        if (counter_post_ == TEST_THREADS) {
            // There should be just one element in table
            ASSERT_EQ(table_->Size(), 1);
            string_ = nullptr;

            {
                os::memory::WriteLockHolder holder(table_->table_.table_lock_);
                table_->table_.table_.clear();
            }
            {
                os::memory::WriteLockHolder holder(table_->internal_table_.table_lock_);
                table_->internal_table_.table_.clear();
            }

            post_cv_.notify_all();
            counter_post_ = 0;
        } else {
            post_cv_.wait(lk);
        }
    }

protected:
    panda::MTManagedThread *thread_ {nullptr};

    std::mutex pre_lock_;
    std::condition_variable pre_cv_;
    int counter_pre_ = 0;
    std::mutex post_lock_;
    std::condition_variable post_cv_;
    int counter_post_ = 0;
    StringTable *table_ {nullptr};

    std::atomic_flag lock_ {0};
    coretypes::String *string_ {nullptr};
};

void TestThreadEntry(MultithreadedInternStringTableTest *test)
{
    auto *this_thread =
        panda::MTManagedThread::Create(panda::Runtime::GetCurrent(), panda::Runtime::GetCurrent()->GetPandaVM());
    this_thread->ManagedCodeBegin();
    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
    std::vector<uint8_t> data {0xc2, 0xa7, 0x34, 0x00};
    auto *table = test->GetTable();
    for (uint32_t i = 0; i < TEST_ITERS; i++) {
        test->PreCheck();
        auto *interned_str = table->GetOrInternString(data.data(), 2U, ctx);
        test->CheckSameString(interned_str);
        test->PostFree();
    }
    this_thread->ManagedCodeEnd();
    this_thread->Destroy();
}

TEST_F(MultithreadedInternStringTableTest, CheckInternReturnsSameString)
{
    std::array<std::thread, TEST_THREADS> threads;
    for (uint32_t i = 0; i < TEST_THREADS; i++) {
        threads[i] = std::thread(TestThreadEntry, this);
    }
    for (uint32_t i = 0; i < TEST_THREADS; i++) {
        threads[i].join();
    }
}

}  // namespace panda::mem::test
