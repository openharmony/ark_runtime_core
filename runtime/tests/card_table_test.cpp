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

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <random>

#include "gtest/gtest.h"
#include "runtime/mem/gc/card_table-inl.h"
#include "runtime/mem/mem_stats_additional_info.h"
#include "runtime/mem/mem_stats_default.h"
#include "runtime/mem/internal_allocator-inl.h"
#include "runtime/include/runtime_options.h"
#include "runtime/include/runtime.h"
#include "runtime/include/panda_vm.h"

namespace panda::mem::test {

class CardTableTest : public testing::Test {
protected:
    // static constexpr size_t kHeapSize = 0xffffffff;
    static constexpr size_t kAllocCount = 1000;
    // static constexpr size_t maxCardIndex = kHeapSize / ::panda::mem::CardTable::GetCardSize();
    std::mt19937 gen;
    std::uniform_int_distribution<uintptr_t> addrDis;
    std::uniform_int_distribution<size_t> cardIndexDis;

    CardTableTest()
    {
#ifdef PANDA_NIGHTLY_TEST_ON
        seed_ = std::time(NULL);
#else
        seed_ = 123456U;
#endif
        RuntimeOptions options;
        options.SetHeapSizeLimit(64_MB);
        options.SetShouldLoadBootPandaFiles(false);
        options.SetShouldInitializeIntrinsics(false);
        options.SetGcType("epsilon");
        Runtime::Create(options);
        thread_ = panda::MTManagedThread::GetCurrent();
        thread_->ManagedCodeBegin();

        internal_allocator_ = thread_->GetVM()->GetHeapManager()->GetInternalAllocator();
        addrDis = std::uniform_int_distribution<uintptr_t>(0, GetPoolSize() - 1);
        ASSERT(GetPoolSize() % CardTable::GetCardSize() == 0);
        cardIndexDis = std::uniform_int_distribution<size_t>(0, GetPoolSize() / CardTable::GetCardSize() - 1);
        card_table_ = std::make_unique<CardTable>(internal_allocator_, GetMinAddress(), GetPoolSize());
        card_table_->Initialize();
    }

    ~CardTableTest()
    {
        card_table_.reset(nullptr);
        thread_->ManagedCodeEnd();
        Runtime::Destroy();
    }

    void SetUp() override
    {
        gen.seed(seed_);
    }

    void TearDown() override
    {
        const ::testing::TestInfo *const test_info = ::testing::UnitTest::GetInstance()->current_test_info();
        if (test_info->result()->Failed()) {
            std::cout << "CartTableTest seed = " << seed_ << std::endl;
        }
    }

    uintptr_t GetMinAddress()
    {
        return PoolManager::GetMmapMemPool()->GetMinObjectAddress();
    }

    size_t GetPoolSize()
    {
        return (PoolManager::GetMmapMemPool()->GetMaxObjectAddress() -
                PoolManager::GetMmapMemPool()->GetMinObjectAddress());
    }

    uintptr_t GetRandomAddress()
    {
        return PoolManager::GetMmapMemPool()->GetMinObjectAddress() + addrDis(gen) % GetPoolSize();
    }

    size_t GetRandomCardIndex()
    {
        return cardIndexDis(gen) % GetPoolSize();
    }

    // Generate address at the beginning of the card
    uintptr_t GetRandomCardAddress()
    {
        return PoolManager::GetMmapMemPool()->GetMinObjectAddress() + GetRandomCardIndex() * CardTable::GetCardSize();
    }

    InternalAllocatorPtr internal_allocator_ {nullptr};
    std::unique_ptr<CardTable> card_table_ {nullptr};
    unsigned int seed_ {0};
    panda::MTManagedThread *thread_ {nullptr};
};

TEST_F(CardTableTest, MarkTest)
{
    size_t markedCnt = 0;
    for (size_t i = 0; i < kAllocCount; i++) {
        uintptr_t addr;
        addr = GetRandomAddress();
        if (!card_table_->IsMarked(addr)) {
            ++markedCnt;
            card_table_->MarkCard(addr);
        }
    }

    for (auto card : *card_table_) {
        if (card->IsMarked()) {
            markedCnt--;
        }
    }
    ASSERT_EQ(markedCnt, 0);
}

TEST_F(CardTableTest, MarkAndClearAllTest)
{
    size_t cnt = 0;
    for (auto card : *card_table_) {
        card->Mark();
        cnt++;
    }
    ASSERT_EQ(cnt, card_table_->GetCardsCount());

    size_t cnt_cleared = 0;
    for (auto card : *card_table_) {
        card->Clear();
        cnt_cleared++;
    }
    ASSERT_EQ(cnt_cleared, card_table_->GetCardsCount());
}

TEST_F(CardTableTest, ClearTest)
{
    std::set<uintptr_t> addrSet;

    // Mark some cards not more than once
    while (addrSet.size() <= kAllocCount) {
        uintptr_t addr;
        addr = GetRandomCardAddress();
        if (addrSet.insert(addr).second == false) {
            continue;
        }
        card_table_->MarkCard(addr);
    }

    size_t cleared_cnt = 0;
    // Clear all marked and count them
    for (auto card : *card_table_) {
        if (card->IsMarked()) {
            card->Clear();
            cleared_cnt++;
        }
    }

    ASSERT_EQ(addrSet.size(), cleared_cnt);
    // Check that there are no marked
    for (auto card : *card_table_) {
        ASSERT_EQ(card->IsMarked(), false);
    }
}

TEST_F(CardTableTest, ClearAllTest)
{
    std::set<uintptr_t> addrSet;

    // Mark some cards not more than once
    while (addrSet.size() < kAllocCount) {
        uintptr_t addr;
        addr = GetRandomCardAddress();
        if (addrSet.insert(addr).second == false) {
            continue;
        }
        card_table_->MarkCard(addr);
    }

    card_table_->ClearAll();
    for (auto card : *card_table_) {
        ASSERT_EQ(card->IsMarked(), false);
    }
}

TEST_F(CardTableTest, double_initialization)
{
    EXPECT_DEATH(card_table_->Initialize(), ".*");
}

TEST_F(CardTableTest, corner_cases)
{
    // Mark 1st byte in the heap
    ASSERT_EQ((*card_table_->begin())->IsMarked(), false);
    card_table_->MarkCard(GetMinAddress());
    ASSERT_EQ((*card_table_->begin())->IsMarked(), true);
    // Mark last byte in the heap
    uintptr_t last = GetMinAddress() + GetPoolSize() - 1;
    ASSERT_EQ(card_table_->IsMarked(last), false);
    card_table_->MarkCard(last);
    ASSERT_EQ(card_table_->IsMarked(last), true);
    // Mark last byte of second card
    uintptr_t secondLast = GetMinAddress() + 2 * card_table_->GetCardSize() - 1;
    ASSERT_EQ(card_table_->IsMarked(secondLast), false);
    card_table_->MarkCard(secondLast);
    ASSERT_EQ(((*card_table_->begin()) + 1)->IsMarked(), true);
}

TEST_F(CardTableTest, VisitMarked)
{
    size_t markedCnt = 0;
    while (markedCnt < kAllocCount) {
        uintptr_t addr;
        addr = GetRandomAddress();
        if (!card_table_->IsMarked(addr)) {
            ++markedCnt;
            card_table_->MarkCard(addr);
        }
    }

    PandaVector<MemRange> mem_ranges;
    card_table_->VisitMarked([&mem_ranges](MemRange mem_range) { mem_ranges.emplace_back(mem_range); },
                             CardTableProcessedFlag::VISIT_MARKED);

    // Got ranges one by one
    PandaVector<MemRange> expected_ranges;
    for (auto card : *card_table_) {
        if (card->IsMarked()) {
            expected_ranges.emplace_back(card_table_->GetMemoryRange(card));
        }
    }

    ASSERT_EQ(expected_ranges.size(), mem_ranges.size());
    for (size_t i = 0; i < expected_ranges.size(); i++) {
        ASSERT(mem_ranges[i].GetStartAddress() == expected_ranges[i].GetStartAddress());
        ASSERT(mem_ranges[i].GetEndAddress() == expected_ranges[i].GetEndAddress());
    }
}

}  // namespace panda::mem::test
