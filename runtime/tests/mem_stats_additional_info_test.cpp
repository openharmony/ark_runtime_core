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

#include <array>
#include <atomic>
#include <condition_variable>
#include <thread>

#include "gtest/gtest.h"
#include "iostream"
#include "runtime/handle_base-inl.h"
#include "runtime/include/coretypes/string.h"
#include "runtime/include/runtime.h"
#include "runtime/include/panda_vm.h"
#include "runtime/mem/gc/gc_phase.h"
#include "runtime/mem/mem_stats.h"
#include "runtime/mem/mem_stats_additional_info.h"

namespace panda::mem::test {

class MemStatsAdditionalInfoTest : public testing::Test {
public:
    MemStatsAdditionalInfoTest()
    {
        RuntimeOptions options;
        options.SetShouldLoadBootPandaFiles(false);
        options.SetShouldInitializeIntrinsics(false);
        Runtime::Create(options);
        thread_ = panda::MTManagedThread::GetCurrent();
        thread_->ManagedCodeBegin();
    }

    ~MemStatsAdditionalInfoTest() override
    {
        thread_->ManagedCodeEnd();
        Runtime::Destroy();
    }

protected:
    panda::MTManagedThread *thread_ {nullptr};
};

TEST_F(MemStatsAdditionalInfoTest, HeapAllocatedMaxAndTotal)
{
    static constexpr size_t BYTES_ALLOC1 = 2;
    static constexpr size_t BYTES_ALLOC2 = 5;
    static constexpr size_t RAW_ALLOC1 = 15;

    std::string simple_string = "smallData";
    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
    coretypes::String *string_object = coretypes::String::CreateFromMUtf8(
        reinterpret_cast<const uint8_t *>(&simple_string[0]), simple_string.length(), ctx, thread_->GetVM());

    size_t string_size = string_object->ObjectSize();

    MemStatsAdditionalInfo stats;
    stats.RecordAllocateObject(BYTES_ALLOC1, SpaceType::SPACE_TYPE_OBJECT);
    stats.RecordAllocateObject(BYTES_ALLOC2, SpaceType::SPACE_TYPE_OBJECT);
    stats.RecordAllocateRaw(RAW_ALLOC1, SpaceType::SPACE_TYPE_INTERNAL);

    stats.RecordAllocateObject(string_size, SpaceType::SPACE_TYPE_OBJECT);
    ASSERT_EQ(BYTES_ALLOC1 + BYTES_ALLOC2 + string_size, stats.GetAllocated(SpaceType::SPACE_TYPE_OBJECT));
    stats.RecordFreeObject(string_size, SpaceType::SPACE_TYPE_OBJECT);
    ASSERT_EQ(BYTES_ALLOC1 + BYTES_ALLOC2, stats.GetFootprint(SpaceType::SPACE_TYPE_OBJECT));
    ASSERT_EQ(BYTES_ALLOC1 + BYTES_ALLOC2 + string_size, stats.GetAllocated(SpaceType::SPACE_TYPE_OBJECT));
    ASSERT_EQ(string_size, stats.GetFreed(SpaceType::SPACE_TYPE_OBJECT));
}

TEST_F(MemStatsAdditionalInfoTest, AdditionalStatistic)
{
    PandaVM *vm = thread_->GetVM();
    std::string simple_string = "smallData";
    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
    [[maybe_unused]] coretypes::String *string_object = coretypes::String::CreateFromMUtf8(
        reinterpret_cast<const uint8_t *>(&simple_string[0]), simple_string.length(), ctx, vm);
    [[maybe_unused]] HandleScope<ObjectHeader *> scope(thread_);
    [[maybe_unused]] VMHandle<ObjectHeader> handle(thread_, string_object);
#ifndef NDEBUG
    Class *stringClass = Runtime::GetCurrent()->GetClassLinker()->GetExtension(ctx)->GetClassRoot(ClassRoot::STRING);
    auto statistics = thread_->GetVM()->GetMemStats()->GetStatistics(vm->GetHeapManager());
    // allocated
    ASSERT_TRUE(statistics.find(stringClass->GetName()) != std::string::npos);
    ASSERT_TRUE(statistics.find("footprint") != std::string::npos);
    ASSERT_TRUE(statistics.find("1") != std::string::npos);
#endif
}

// test correct pauses measurment
TEST_F(MemStatsAdditionalInfoTest, GCPhaseTimeTest)
{
    // pauses in milliseconds
    constexpr uint64_t PAUSES[] = {20, 10, 30};
    constexpr uint64_t MIN_PAUSE = 10;
    constexpr uint64_t MAX_PAUSE = 30;
    constexpr uint64_t TOTAL_PAUSE = 60;
    constexpr uint PAUSES_COUNT = 3;
    constexpr uint64_t AVG_PAUSE = TOTAL_PAUSE / PAUSES_COUNT;

    MemStatsAdditionalInfo stats;
    for (uint i = 0; i < PAUSES_COUNT; i++) {
        for (uint ph = 0; ph < static_cast<uint>(GCPhase::GC_PHASE_LAST); ph++) {
            stats.RecordGCPhaseStart(ToGCPhase(ph));
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long int>(PAUSES[i])));
            stats.RecordGCPhaseEnd();
        }
    }

    GCPhase phase;
    for (uint ph = 0; ph < static_cast<uint>(GCPhase::GC_PHASE_LAST); ph++) {
        phase = ToGCPhase(ph);
        ASSERT_LE(MIN_PAUSE, stats.GetMinGCPhaseTime(phase));
        ASSERT_LE(MAX_PAUSE, stats.GetMaxGCPhaseTime(phase));
        ASSERT_LE(AVG_PAUSE, stats.GetAverageGCPhaseTime(phase));
        ASSERT_LE(TOTAL_PAUSE, stats.GetTotalGCPhaseTime(phase));

        ASSERT_LE(stats.GetMinGCPhaseTime(phase), stats.GetAverageGCPhaseTime(phase));
        ASSERT_LE(stats.GetAverageGCPhaseTime(phase), stats.GetMaxGCPhaseTime(phase));
        ASSERT_LE(stats.GetMaxGCPhaseTime(phase), stats.GetTotalGCPhaseTime(phase));
    }

    // test empty case
    MemStatsAdditionalInfo stats_empty;
    for (uint ph = 0; ph < static_cast<uint>(GCPhase::GC_PHASE_LAST); ph++) {
        ASSERT_EQ(0, stats_empty.GetMinGCPhaseTime(ToGCPhase(ph)));
        ASSERT_EQ(0, stats_empty.GetMaxGCPhaseTime(ToGCPhase(ph)));
        ASSERT_EQ(0, stats_empty.GetAverageGCPhaseTime(ToGCPhase(ph)));
        ASSERT_EQ(0, stats_empty.GetTotalGCPhaseTime(ToGCPhase(ph)));
    }
}

}  // namespace panda::mem::test
