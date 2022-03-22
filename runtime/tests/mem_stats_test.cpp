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

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <thread>

#include "gtest/gtest.h"
#include "iostream"
#include "runtime/include/coretypes/string.h"
#include "runtime/include/runtime.h"
#include "runtime/include/panda_vm.h"
#include "runtime/mem/malloc-proxy-allocator-inl.h"
#include "runtime/mem/mem_stats.h"
#include "runtime/mem/mem_stats_default.h"
#include "runtime/mem/runslots_allocator-inl.h"

namespace panda::mem::test {

#ifndef PANDA_NIGHTLY_TEST_ON
constexpr uint64_t ITERATION = 256;
constexpr size_t NUM_THREADS = 2;
#else
constexpr uint64_t ITERATION = 1 << 17;
constexpr size_t NUM_THREADS = 8;
#endif

using NonObjectAllocator = RunSlotsAllocator<RawMemoryConfig>;

class MemStatsTest : public testing::Test {
public:
    MemStatsTest()
    {
        // we need runtime for creating objects
        RuntimeOptions options;
        options.SetShouldLoadBootPandaFiles(false);
        options.SetShouldInitializeIntrinsics(false);
        options.SetGcType("stw");
        options.SetRunGcInPlace(true);
        Runtime::Create(options);
        thread_ = panda::MTManagedThread::GetCurrent();
        thread_->ManagedCodeBegin();
    }

    ~MemStatsTest() override
    {
        thread_->ManagedCodeEnd();
        Runtime::Destroy();
    }

protected:
    panda::MTManagedThread *thread_ {nullptr};
};

using MallocProxyNonObjectAllocator = MallocProxyAllocator<RawMemoryConfig>;

class RawStatsBeforeTest {
    size_t raw_bytes_allocated_before_test;
    size_t raw_bytes_freed_before_test;
    size_t raw_bytes_footprint_before_rest;

public:
    explicit RawStatsBeforeTest(MemStatsType *stats)
        : raw_bytes_allocated_before_test(stats->GetAllocated(SpaceType::SPACE_TYPE_INTERNAL)),
          raw_bytes_freed_before_test(stats->GetFreed(SpaceType::SPACE_TYPE_INTERNAL)),
          raw_bytes_footprint_before_rest(stats->GetFootprint(SpaceType::SPACE_TYPE_INTERNAL))
    {
    }

    [[nodiscard]] size_t GetRawBytesAllocatedBeforeTest() const
    {
        return raw_bytes_allocated_before_test;
    }

    [[nodiscard]] size_t GetRawBytesFreedBeforeTest() const
    {
        return raw_bytes_freed_before_test;
    }

    [[nodiscard]] size_t GetRawBytesFootprintBeforeTest() const
    {
        return raw_bytes_footprint_before_rest;
    }
};

void AssertHeapStats(MemStatsType *stats, size_t bytes_in_heap, size_t heap_bytes_allocated_, size_t heap_bytes_freed_)
{
    ASSERT_EQ(heap_bytes_allocated_, stats->GetAllocated(SpaceType::SPACE_TYPE_OBJECT));
    ASSERT_EQ(heap_bytes_freed_, stats->GetFreed(SpaceType::SPACE_TYPE_OBJECT));
    ASSERT_EQ(bytes_in_heap, stats->GetFootprint(SpaceType::SPACE_TYPE_OBJECT));
}

void AssertHeapHumongousStats(MemStatsType *stats, size_t bytes_in_heap, size_t heap_bytes_allocated_,
                              size_t heap_bytes_freed_)
{
    ASSERT_EQ(heap_bytes_allocated_, stats->GetAllocated(SpaceType::SPACE_TYPE_HUMONGOUS_OBJECT));
    ASSERT_EQ(heap_bytes_freed_, stats->GetFreed(SpaceType::SPACE_TYPE_HUMONGOUS_OBJECT));
    ASSERT_EQ(bytes_in_heap, stats->GetFootprint(SpaceType::SPACE_TYPE_HUMONGOUS_OBJECT));
}

void AssertHeapObjectsStats(MemStatsType *stats, size_t heap_objects_allocated_, size_t heap_objects_freed_,
                            size_t heap_humungous_objects_allocated_, size_t heap_humungous_objects_freed_)
{
    ASSERT_EQ(heap_objects_allocated_, stats->GetTotalObjectsAllocated());
    ASSERT_EQ(heap_objects_freed_, stats->GetTotalObjectsFreed());

    // On arm-32 platform, we should cast the uint64_t(-1) to size_t(-1)
    ASSERT_EQ(heap_objects_allocated_ - heap_humungous_objects_allocated_,
              static_cast<size_t>(stats->GetTotalRegularObjectsAllocated()));
    ASSERT_EQ(heap_objects_freed_ - heap_humungous_objects_freed_,
              static_cast<size_t>(stats->GetTotalRegularObjectsFreed()));

    ASSERT_EQ(heap_humungous_objects_allocated_, stats->GetTotalHumongousObjectsAllocated());
    ASSERT_EQ(heap_humungous_objects_freed_, stats->GetTotalHumongousObjectsFreed());

    ASSERT_EQ(heap_objects_allocated_ - heap_objects_freed_, stats->GetObjectsCountAlive());
    ASSERT_EQ(heap_objects_allocated_ - heap_objects_freed_ + heap_humungous_objects_allocated_ -
                  heap_humungous_objects_freed_,
              stats->GetRegularObjectsCountAlive());
    ASSERT_EQ(heap_humungous_objects_allocated_ - heap_humungous_objects_freed_,
              stats->GetHumonguousObjectsCountAlive());
}

/**
 * We add bytes which we allocated before tests for our internal structures, but we don't add it to `freed` because
 * destructors haven't be called yet.
 */
void AssertRawStats(MemStatsType *stats, size_t raw_bytes_allocated, size_t raw_bytes_freed, size_t raw_bytes_footprint,
                    RawStatsBeforeTest &statsBeforeTest)
{
    ASSERT_EQ(raw_bytes_allocated + statsBeforeTest.GetRawBytesAllocatedBeforeTest(),
              stats->GetAllocated(SpaceType::SPACE_TYPE_INTERNAL));
    ASSERT_EQ(raw_bytes_freed + statsBeforeTest.GetRawBytesFreedBeforeTest(),
              stats->GetFreed(SpaceType::SPACE_TYPE_INTERNAL));
    ASSERT_EQ(raw_bytes_footprint + statsBeforeTest.GetRawBytesFootprintBeforeTest(),
              stats->GetFootprint(SpaceType::SPACE_TYPE_INTERNAL));
}

TEST_F(MemStatsTest, SimpleTest)
{
    static constexpr size_t BYTES_OBJECT1 = 10;
    static constexpr size_t BYTES_OBJECT2 = 12;
    static constexpr size_t BYTES_RAW_MEMORY_ALLOC1 = 20;
    static constexpr size_t BYTES_RAW_MEMORY_ALLOC2 = 30002;
    static constexpr size_t RAW_MEMORY_FREED = 5;

    auto *stats = thread_->GetVM()->GetMemStats();
    size_t init_heap_bytes = stats->GetAllocated(SpaceType::SPACE_TYPE_OBJECT);
    size_t init_heap_objects = stats->GetTotalObjectsAllocated();
    RawStatsBeforeTest raw_stats_before_test(stats);
    stats->RecordAllocateObject(BYTES_OBJECT1, SpaceType::SPACE_TYPE_OBJECT);
    stats->RecordAllocateObject(BYTES_OBJECT2, SpaceType::SPACE_TYPE_OBJECT);
    stats->RecordAllocateRaw(BYTES_RAW_MEMORY_ALLOC1, SpaceType::SPACE_TYPE_INTERNAL);
    stats->RecordAllocateRaw(BYTES_RAW_MEMORY_ALLOC2, SpaceType::SPACE_TYPE_INTERNAL);
    stats->RecordFreeRaw(RAW_MEMORY_FREED, SpaceType::SPACE_TYPE_INTERNAL);

    AssertHeapStats(stats, init_heap_bytes + BYTES_OBJECT1 + BYTES_OBJECT2,
                    init_heap_bytes + BYTES_OBJECT1 + BYTES_OBJECT2, 0);
    AssertHeapObjectsStats(stats, init_heap_objects + 2, 0, 0, 0);
    ASSERT_EQ(init_heap_bytes + BYTES_OBJECT1 + BYTES_OBJECT2, stats->GetFootprint(SpaceType::SPACE_TYPE_OBJECT));
    AssertRawStats(stats, BYTES_RAW_MEMORY_ALLOC1 + BYTES_RAW_MEMORY_ALLOC2, RAW_MEMORY_FREED,
                   BYTES_RAW_MEMORY_ALLOC1 + BYTES_RAW_MEMORY_ALLOC2 - RAW_MEMORY_FREED, raw_stats_before_test);
    stats->RecordFreeRaw(BYTES_RAW_MEMORY_ALLOC1 + BYTES_RAW_MEMORY_ALLOC2 - RAW_MEMORY_FREED,
                         SpaceType::SPACE_TYPE_INTERNAL);
}

// testing MemStats via allocators.
TEST_F(MemStatsTest, NonObjectTestViaMallocAllocator)
{
    static constexpr size_t BYTES_ALLOC1 = 23;
    static constexpr size_t BYTES_ALLOC2 = 42;

    mem::MemStatsType *stats = thread_->GetVM()->GetMemStats();
    RawStatsBeforeTest raw_stats_before_test(stats);
    size_t init_heap_bytes = stats->GetAllocated(SpaceType::SPACE_TYPE_OBJECT);
    size_t init_heap_objects = stats->GetTotalObjectsAllocated();
    MallocProxyNonObjectAllocator allocator(stats, SpaceType::SPACE_TYPE_INTERNAL);

    void *a1 = allocator.Alloc(BYTES_ALLOC1);
    allocator.Free(a1);
    void *a2 = allocator.Alloc(BYTES_ALLOC2);

    AssertHeapStats(stats, init_heap_bytes, init_heap_bytes, 0);
    AssertHeapObjectsStats(stats, init_heap_objects, 0, 0, 0);
    AssertRawStats(stats, BYTES_ALLOC1 + BYTES_ALLOC2, BYTES_ALLOC1, BYTES_ALLOC2, raw_stats_before_test);
    allocator.Free(a2);
}

// testing MemStats via allocators.
TEST_F(MemStatsTest, NonObjectTestViaSlotsAllocator)
{
    static constexpr uint64_t poolSize = SIZE_1M * 4;
    static constexpr size_t REAL_BYTES_ALLOC1 = 23;
    // RunSlotsAllocator uses 32 bytes for allocation 23 bytes
    static constexpr size_t BYTES_IN_ALLOCATOR_ALLOC1 = 32;
    static constexpr size_t REAL_BYTES_ALLOC2 = 42;
    static constexpr size_t BYTES_IN_ALLOCATOR_ALLOC2 = 64;

    mem::MemStatsType *stats = thread_->GetVM()->GetMemStats();
    size_t init_heap_bytes = stats->GetAllocated(SpaceType::SPACE_TYPE_OBJECT);
    size_t init_heap_objects = stats->GetTotalObjectsAllocated();
    RawStatsBeforeTest raw_stats_before_test(stats);

    auto *allocator = new NonObjectAllocator(stats, SpaceType::SPACE_TYPE_INTERNAL);
    void *mem = aligned_alloc(RUNSLOTS_ALIGNMENT_IN_BYTES, poolSize);
    allocator->AddMemoryPool(mem, poolSize);

    void *a1 = allocator->Alloc(REAL_BYTES_ALLOC1);
    allocator->Free(a1);
    void *a2 = allocator->Alloc(REAL_BYTES_ALLOC2);

    AssertHeapStats(stats, init_heap_bytes, init_heap_bytes, 0);
    AssertHeapObjectsStats(stats, init_heap_objects, 0, 0, 0);
    AssertRawStats(stats, BYTES_IN_ALLOCATOR_ALLOC1 + BYTES_IN_ALLOCATOR_ALLOC2, BYTES_IN_ALLOCATOR_ALLOC1,
                   BYTES_IN_ALLOCATOR_ALLOC2, raw_stats_before_test);
    allocator->Free(a2);
    delete allocator;
    std::free(mem);
}

// allocate and free small object in the heap
TEST_F(MemStatsTest, SmallObject)
{
    mem::MemStatsType *stats = thread_->GetVM()->GetMemStats();
    size_t init_heap_bytes = stats->GetAllocated(SpaceType::SPACE_TYPE_OBJECT);
    size_t init_heap_objects = stats->GetTotalObjectsAllocated();
    RawStatsBeforeTest raw_stats_before_test(stats);
    std::string simple_string = "abcdef12345";
    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
    [[maybe_unused]] coretypes::String *string_object =
        coretypes::String::CreateFromMUtf8(reinterpret_cast<const uint8_t *>(&simple_string[0]), simple_string.length(),
                                           ctx, Runtime::GetCurrent()->GetPandaVM());
    ASSERT_TRUE(string_object != nullptr);
    thread_->GetVM()->GetGC()->WaitForGCInManaged(GCTask(GCTaskCause::EXPLICIT_CAUSE));
    size_t alloc_size = simple_string.size() + sizeof(coretypes::String);
    size_t aligment_size = 1UL << RunSlots<>::ConvertToPowerOfTwoUnsafe(alloc_size);
    AssertHeapStats(stats, init_heap_bytes, init_heap_bytes + aligment_size, aligment_size);
    AssertHeapObjectsStats(stats, init_heap_objects + 1, 1, 0, 0);
    ASSERT_EQ(raw_stats_before_test.GetRawBytesFootprintBeforeTest(),
              stats->GetFootprint(SpaceType::SPACE_TYPE_INTERNAL));
}

// allocate and free big object in the heap
TEST_F(MemStatsTest, BigObject)
{
    mem::MemStatsType *stats = thread_->GetVM()->GetMemStats();
    RawStatsBeforeTest raw_stats_before_test(stats);
    size_t init_heap_bytes = stats->GetAllocated(SpaceType::SPACE_TYPE_OBJECT);
    size_t init_heap_objects = stats->GetTotalObjectsAllocated();
    std::string simple_string;
    auto object_allocator = thread_->GetVM()->GetHeapManager()->GetObjectAllocator().AsObjectAllocator();
    size_t alloc_size = object_allocator->GetRegularObjectMaxSize() + 1;
    for (size_t j = 0; j < alloc_size; j++) {
        simple_string.append("x");
    }
    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
    [[maybe_unused]] coretypes::String *string_object =
        coretypes::String::CreateFromMUtf8(reinterpret_cast<const uint8_t *>(&simple_string[0]), simple_string.length(),
                                           ctx, Runtime::GetCurrent()->GetPandaVM());
    ASSERT_TRUE(string_object != nullptr);
    thread_->GetVM()->GetGC()->WaitForGCInManaged(GCTask(GCTaskCause::EXPLICIT_CAUSE));
    alloc_size += sizeof(coretypes::String);
    size_t aligment_size = AlignUp(alloc_size, GetAlignmentInBytes(FREELIST_DEFAULT_ALIGNMENT));

    AssertHeapStats(stats, init_heap_bytes, init_heap_bytes + aligment_size, aligment_size);
    AssertHeapObjectsStats(stats, init_heap_objects + 1, 1, 0, 0);
    ASSERT_EQ(raw_stats_before_test.GetRawBytesFootprintBeforeTest(),
              stats->GetFootprint(SpaceType::SPACE_TYPE_INTERNAL));
}

// allocate and free humongous object in the heap
TEST_F(MemStatsTest, HumongousObject)
{
    mem::MemStatsType *stats = thread_->GetVM()->GetMemStats();
    RawStatsBeforeTest raw_stats_before_test(stats);
    size_t init_heap_bytes = stats->GetAllocated(SpaceType::SPACE_TYPE_HUMONGOUS_OBJECT);
    size_t init_heap_objects = stats->GetTotalObjectsAllocated();
    std::string simple_string;
    auto object_allocator = thread_->GetVM()->GetHeapManager()->GetObjectAllocator().AsObjectAllocator();
    size_t alloc_size = object_allocator->GetLargeObjectMaxSize() + 1;
    for (size_t j = 0; j < alloc_size; j++) {
        simple_string.append("x");
    }
    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
    [[maybe_unused]] coretypes::String *string_object =
        coretypes::String::CreateFromMUtf8(reinterpret_cast<const uint8_t *>(&simple_string[0]), simple_string.length(),
                                           ctx, Runtime::GetCurrent()->GetPandaVM());
    ASSERT_TRUE(string_object != nullptr);
    thread_->GetVM()->GetGC()->WaitForGCInManaged(GCTask(GCTaskCause::EXPLICIT_CAUSE));
    AssertHeapHumongousStats(stats, init_heap_bytes, init_heap_bytes + 2359296, 2359296);
    AssertHeapObjectsStats(stats, init_heap_objects, 0, 1, 1);
    ASSERT_EQ(raw_stats_before_test.GetRawBytesFootprintBeforeTest(),
              stats->GetFootprint(SpaceType::SPACE_TYPE_INTERNAL));

    ASSERT_EQ(2359296, stats->GetAllocated(SpaceType::SPACE_TYPE_HUMONGOUS_OBJECT));
    ASSERT_EQ(2359296, stats->GetFreed(SpaceType::SPACE_TYPE_HUMONGOUS_OBJECT));
    ASSERT_EQ(0, stats->GetFootprint(SpaceType::SPACE_TYPE_HUMONGOUS_OBJECT));
}

TEST_F(MemStatsTest, TotalFootprint)
{
    static constexpr size_t BYTES_ALLOC1 = 2;
    static constexpr size_t BYTES_ALLOC2 = 5;
    static constexpr size_t RAW_ALLOC1 = 15;
    static constexpr size_t RAW_ALLOC2 = 30;

    MemStatsDefault stats;
    stats.RecordAllocateObject(BYTES_ALLOC1, SpaceType::SPACE_TYPE_OBJECT);
    stats.RecordAllocateObject(BYTES_ALLOC2, SpaceType::SPACE_TYPE_OBJECT);
    stats.RecordAllocateRaw(RAW_ALLOC1, SpaceType::SPACE_TYPE_INTERNAL);
    stats.RecordAllocateRaw(RAW_ALLOC2, SpaceType::SPACE_TYPE_INTERNAL);

    ASSERT_EQ(BYTES_ALLOC1 + BYTES_ALLOC2, stats.GetFootprint(SpaceType::SPACE_TYPE_OBJECT));
    ASSERT_EQ(BYTES_ALLOC1 + BYTES_ALLOC2 + RAW_ALLOC1 + RAW_ALLOC2, stats.GetTotalFootprint());
    ASSERT_EQ(RAW_ALLOC1 + RAW_ALLOC2, stats.GetFootprint(SpaceType::SPACE_TYPE_INTERNAL));

    stats.RecordFreeRaw(RAW_ALLOC1, SpaceType::SPACE_TYPE_INTERNAL);

    ASSERT_EQ(BYTES_ALLOC1 + BYTES_ALLOC2, stats.GetFootprint(SpaceType::SPACE_TYPE_OBJECT));
    ASSERT_EQ(BYTES_ALLOC1 + BYTES_ALLOC2 + RAW_ALLOC2, stats.GetTotalFootprint());
    ASSERT_EQ(RAW_ALLOC2, stats.GetFootprint(SpaceType::SPACE_TYPE_INTERNAL));
}

TEST_F(MemStatsTest, Statistics)
{
    static constexpr size_t BYTES_OBJECT = 10;
    static constexpr size_t BYTES_ALLOC1 = 23;
    static constexpr size_t BYTES_ALLOC2 = 42;

    MemStatsDefault stats;
    stats.RecordAllocateObject(BYTES_OBJECT, SpaceType::SPACE_TYPE_OBJECT);
    stats.RecordAllocateRaw(BYTES_ALLOC1, SpaceType::SPACE_TYPE_INTERNAL);
    stats.RecordAllocateRaw(BYTES_ALLOC2, SpaceType::SPACE_TYPE_INTERNAL);

    auto statistics = stats.GetStatistics(thread_->GetVM()->GetHeapManager());
    ASSERT_TRUE(statistics.find(std::to_string(BYTES_OBJECT)) != std::string::npos);
    ASSERT_TRUE(statistics.find(std::to_string(BYTES_ALLOC1 + BYTES_ALLOC2)) != std::string::npos);
    stats.RecordFreeRaw(BYTES_ALLOC1 + BYTES_ALLOC2, SpaceType::SPACE_TYPE_INTERNAL);
}

void FillMemStatsForConcurrency(MemStatsDefault &stats, std::condition_variable &ready_to_start, std::mutex &cv_mutex,
                                std::atomic_size_t &threads_ready, coretypes::String *string_object)
{
    {
        std::unique_lock<std::mutex> lock_for_ready_to_start(cv_mutex);
        threads_ready++;
        if (threads_ready.load() == NUM_THREADS) {
            // Unlock all threads
            ready_to_start.notify_all();
        } else {
            ready_to_start.wait(lock_for_ready_to_start,
                                [&threads_ready] { return threads_ready.load() == NUM_THREADS; });
        }
    }
    for (size_t i = 1; i <= ITERATION; i++) {
        for (size_t index = 0; index < SPACE_TYPE_SIZE; index++) {
            SpaceType type = ToSpaceType(index);
            if (IsHeapSpace(type)) {
                stats.RecordAllocateObject(string_object->ObjectSize(), type);
            } else {
                stats.RecordAllocateRaw(i * (index + 1), type);
            }
        }
    }

    for (size_t index = 0; index < SPACE_TYPE_SIZE; index++) {
        SpaceType type = ToSpaceType(index);
        if (IsHeapSpace(type)) {
            stats.RecordFreeObject(string_object->ObjectSize(), type);
        } else {
            stats.RecordFreeRaw(ITERATION * (index + 1), type);
        }
    }
}

TEST_F(MemStatsTest, TestThreadSafety)
{
    std::string simple_string = "smallData";
    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
    coretypes::String *string_object =
        coretypes::String::CreateFromMUtf8(reinterpret_cast<const uint8_t *>(&simple_string[0]), simple_string.length(),
                                           ctx, Runtime::GetCurrent()->GetPandaVM());

    MemStatsDefault stats;

    std::array<std::thread, NUM_THREADS> threads;

    std::atomic_size_t threads_ready = 0;
    std::mutex cv_mutex;
    std::condition_variable ready_to_start;
    for (size_t i = 0; i < NUM_THREADS; i++) {
        threads[i] = std::thread(FillMemStatsForConcurrency, std::ref(stats), std::ref(ready_to_start),
                                 std::ref(cv_mutex), std::ref(threads_ready), string_object);
    }

    for (size_t i = 0; i < NUM_THREADS; i++) {
        threads[i].join();
    }

    constexpr uint64_t SUM = (ITERATION + 1) * ITERATION / 2U;
    constexpr uint64_t TOTAL_ITERATION_COUNT = NUM_THREADS * ITERATION;

    for (size_t index = 0; index < SPACE_TYPE_SIZE; index++) {
        SpaceType type = ToSpaceType(index);
        if (IsHeapSpace(type)) {
            ASSERT_EQ(stats.GetAllocated(type), TOTAL_ITERATION_COUNT * string_object->ObjectSize());
            ASSERT_EQ(stats.GetFreed(type), NUM_THREADS * string_object->ObjectSize());
            ASSERT_EQ(stats.GetFootprint(type), (TOTAL_ITERATION_COUNT - NUM_THREADS) * string_object->ObjectSize());
        } else {
            ASSERT_EQ(stats.GetAllocated(type), SUM * NUM_THREADS * (index + 1));
            ASSERT_EQ(stats.GetFreed(type), TOTAL_ITERATION_COUNT * (index + 1));
            ASSERT_EQ(stats.GetFootprint(type), (SUM - ITERATION) * NUM_THREADS * (index + 1));
        }
    }
}

// test correct pauses measurment
TEST_F(MemStatsTest, GCPauseTest)
{
    // pauses in milliseconds
    constexpr uint64_t PAUSES[] = {10, 20, 30, 5, 40, 15, 50, 20, 10, 30};
    constexpr uint64_t MIN_PAUSE = 5;
    constexpr uint64_t MAX_PAUSE = 50;
    constexpr uint64_t TOTAL_PAUSE = 230;
    constexpr uint PAUSES_COUNT = 10;
    constexpr uint64_t AVG_PAUSE = TOTAL_PAUSE / PAUSES_COUNT;

    MemStatsDefault stats;
    for (uint i = 0; i < PAUSES_COUNT; i++) {
        stats.RecordGCPauseStart();
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long int>(PAUSES[i])));
        stats.RecordGCPauseEnd();
    }

    ASSERT_LE(MIN_PAUSE, stats.GetMinGCPause());
    ASSERT_LE(MAX_PAUSE, stats.GetMaxGCPause());
    ASSERT_LE(AVG_PAUSE, stats.GetAverageGCPause());
    ASSERT_LE(TOTAL_PAUSE, stats.GetTotalGCPause());

    ASSERT_LE(stats.GetMinGCPause(), stats.GetAverageGCPause());
    ASSERT_LE(stats.GetAverageGCPause(), stats.GetMaxGCPause());
    ASSERT_LE(stats.GetMaxGCPause(), stats.GetTotalGCPause());

    // test empty case
    MemStatsDefault stats_empty;
    ASSERT_EQ(0, stats_empty.GetMaxGCPause());
    ASSERT_EQ(0, stats_empty.GetMinGCPause());
    ASSERT_EQ(0, stats_empty.GetAverageGCPause());
    ASSERT_EQ(0, stats_empty.GetTotalGCPause());
}

}  // namespace panda::mem::test
