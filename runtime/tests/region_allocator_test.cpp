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

#include <sys/mman.h>
#include <algorithm>

#include "libpandabase/mem/mem.h"
#include "libpandabase/os/mem.h"
#include "libpandabase/utils/asan_interface.h"
#include "libpandabase/utils/logger.h"
#include "libpandabase/utils/math_helpers.h"
#include "runtime/include/class_linker.h"
#include "runtime/include/runtime.h"
#include "runtime/mem/alloc_config.h"
#include "runtime/mem/tlab.h"
#include "runtime/tests/allocator_test_base.h"
#include "runtime/mem/region_allocator-inl.h"

namespace panda::mem {
using NonObjectRegionAllocator = RegionAllocator<EmptyAllocConfigWithCrossingMap>;

template <typename ObjectAllocator>
class RegionAllocatorTestBase : public AllocatorTest<ObjectAllocator> {
public:
    RegionAllocatorTestBase()
    {
        options_.SetShouldLoadBootPandaFiles(false);
        options_.SetShouldInitializeIntrinsics(false);
        options_.SetObjectPoolSize(256_MB);
        Runtime::Create(options_);
        thread_ = panda::MTManagedThread::GetCurrent();
        thread_->ManagedCodeBegin();
        class_linker_ = Runtime::GetCurrent()->GetClassLinker();
        auto lang = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto *class_linker_ext = Runtime::GetCurrent()->GetClassLinker()->GetExtension(lang);
        test_class_ = class_linker_ext->CreateClass(nullptr, 0, 0, sizeof(panda::Class));
        test_class_->SetObjectSize(OBJECT_SIZE);
    }
    virtual ~RegionAllocatorTestBase()
    {
        thread_->ManagedCodeEnd();
        Runtime::Destroy();
    }

protected:
    static constexpr size_t OBJECT_SIZE = 128;
    void AddMemoryPoolToAllocator([[maybe_unused]] ObjectAllocator &allocator) final {}

    void AddMemoryPoolToAllocatorProtected([[maybe_unused]] ObjectAllocator &allocator) final {}

    bool AllocatedByThisAllocator([[maybe_unused]] ObjectAllocator &allocator, [[maybe_unused]] void *mem)
    {
        return allocator.ContainObject(reinterpret_cast<ObjectHeader *>(mem));
    }

    void InitializeObjectAtMem(ObjectHeader *object)
    {
        object->SetClass(test_class_);
    }

    panda::MTManagedThread *thread_ {nullptr};
    ClassLinker *class_linker_ {nullptr};
    Class *test_class_ {nullptr};
    RuntimeOptions options_;
};

class RegionAllocatorTest : public RegionAllocatorTestBase<NonObjectRegionAllocator> {
public:
    static constexpr size_t TEST_REGION_SPACE_SIZE = 128_MB;

    size_t GetNumFreeRegions(NonObjectRegionAllocator &allocator)
    {
        return allocator.GetSpace()->GetPool()->GetFreeRegionsNumInRegionBlock();
    }

    bool IsTLAB(Region *reg)
    {
        return reg->GetTLAB() != nullptr;
    }

    size_t static constexpr RegionSize()
    {
        return NonObjectRegionAllocator::REGION_SIZE;
    }

    size_t static constexpr GetRegionsNumber()
    {
        return TEST_REGION_SPACE_SIZE / NonObjectRegionAllocator::REGION_SIZE;
    }

    template <RegionFlag alloc_type>
    void *AllocateObjectWithClass(NonObjectRegionAllocator &allocator)
    {
        void *mem = allocator.Alloc<alloc_type>(OBJECT_SIZE);
        if (mem == nullptr) {
            return nullptr;
        }
        InitializeObjectAtMem(static_cast<ObjectHeader *>(mem));
        return mem;
    }

    void AllocateRegularObject(NonObjectRegionAllocator &allocator, size_t &free_regions,
                               size_t &free_bytes_for_cur_reg, size_t size)
    {
        ASSERT_EQ(GetNumFreeRegions(allocator), free_regions);
        size_t align_size = AlignUp(size, GetAlignmentInBytes(DEFAULT_ALIGNMENT));
        if (free_bytes_for_cur_reg >= align_size) {
            ASSERT_TRUE(allocator.Alloc(size) != nullptr)
                << "fail allocate object with size " << align_size << " with free size " << free_bytes_for_cur_reg;
            free_bytes_for_cur_reg -= align_size;
        } else if (free_regions > 0) {
            ASSERT_TRUE(allocator.Alloc(size) != nullptr);
            free_regions -= 1;
            free_bytes_for_cur_reg = NonObjectRegionAllocator::GetMaxRegularObjectSize() - align_size;
        } else {
            ASSERT_TRUE(allocator.Alloc(align_size) == nullptr);
            align_size = free_bytes_for_cur_reg;
            ASSERT(free_bytes_for_cur_reg % GetAlignmentInBytes(DEFAULT_ALIGNMENT) == 0);
            ASSERT_TRUE(allocator.Alloc(align_size) != nullptr);
            free_bytes_for_cur_reg = 0;
        }
        auto reg = allocator.GetCurrentRegion<true, RegionFlag::IS_EDEN>();
        ASSERT_EQ(GetNumFreeRegions(allocator), free_regions);
        ASSERT_EQ(reg->End() - reg->Top(), free_bytes_for_cur_reg);
    }

    void AllocateLargeObject(NonObjectRegionAllocator &allocator, size_t &free_regions, size_t size)
    {
        ASSERT_EQ(GetNumFreeRegions(allocator), free_regions);
        size_t alloc_size = AlignUp(size, GetAlignmentInBytes(DEFAULT_ALIGNMENT));
        if (alloc_size > free_regions * NonObjectRegionAllocator::GetMaxRegularObjectSize()) {
            ASSERT_TRUE(allocator.Alloc(alloc_size) == nullptr);
            alloc_size = std::min(alloc_size, free_regions * NonObjectRegionAllocator::GetMaxRegularObjectSize());
        }
        ASSERT_TRUE(allocator.Alloc(alloc_size) != nullptr);
        free_regions -= (alloc_size + Region::HeadSize() + NonObjectRegionAllocator::REGION_SIZE - 1) /
                        NonObjectRegionAllocator::REGION_SIZE;
        ASSERT_EQ(GetNumFreeRegions(allocator), free_regions);
    }

    static const int LOOP_COUNT = 100;
};

TEST_F(RegionAllocatorTest, AllocateTooMuchRegularObject)
{
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    NonObjectRegionAllocator allocator(mem_stats, SpaceType::SPACE_TYPE_OBJECT, TEST_REGION_SPACE_SIZE, false);
    size_t alloc_times = GetRegionsNumber();
    for (size_t i = 0; i < alloc_times; i++) {
        ASSERT_TRUE(allocator.Alloc(allocator.GetMaxRegularObjectSize() / 2U + 1) != nullptr);
    }
    delete mem_stats;
    ASSERT_TRUE(allocator.Alloc(allocator.GetMaxRegularObjectSize() / 2U + 1) == nullptr);
}

TEST_F(RegionAllocatorTest, AllocateTooMuchRandomRegularObject)
{
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    for (int i = 0; i < RegionAllocatorTest::LOOP_COUNT; i++) {
        NonObjectRegionAllocator allocator(mem_stats, SpaceType::SPACE_TYPE_OBJECT, TEST_REGION_SPACE_SIZE, false);
        size_t free_regions = GetRegionsNumber();
        size_t free_bytes_for_cur_reg = 0;
        while (free_regions != 0 || free_bytes_for_cur_reg != 0) {
            size_t size = RandFromRange(1, allocator.GetMaxRegularObjectSize());
            AllocateRegularObject(allocator, free_regions, free_bytes_for_cur_reg, size);
        }
        ASSERT_TRUE(allocator.Alloc(1) == nullptr);
    }
    delete mem_stats;
}

TEST_F(RegionAllocatorTest, AllocateTooMuchLargeObject)
{
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    NonObjectRegionAllocator allocator(mem_stats, SpaceType::SPACE_TYPE_OBJECT, TEST_REGION_SPACE_SIZE, false);
    ASSERT_TRUE(allocator.Alloc(allocator.GetMaxRegularObjectSize()) != nullptr);
    size_t alloc_times = (GetRegionsNumber() - 1) / 2U;
    for (size_t i = 0; i < alloc_times; i++) {
        ASSERT_TRUE(allocator.Alloc(allocator.GetMaxRegularObjectSize() + 1) != nullptr);
    }
    ASSERT_TRUE(allocator.Alloc(allocator.GetMaxRegularObjectSize() + 1) == nullptr);
    allocator.Alloc(allocator.GetMaxRegularObjectSize());
    ASSERT_TRUE(allocator.Alloc(1) == nullptr);
    delete mem_stats;
}

TEST_F(RegionAllocatorTest, AllocateTooMuchRandomLargeObject)
{
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    for (int i = 0; i < RegionAllocatorTest::LOOP_COUNT; i++) {
        NonObjectRegionAllocator allocator(mem_stats, SpaceType::SPACE_TYPE_OBJECT, TEST_REGION_SPACE_SIZE, false);
        ASSERT_TRUE(allocator.Alloc(allocator.GetMaxRegularObjectSize()) != nullptr);
        size_t free_regions = GetRegionsNumber() - 1;
        while (free_regions > 1) {
            size_t size =
                RandFromRange(allocator.GetMaxRegularObjectSize() + 1, 3 * allocator.GetMaxRegularObjectSize());
            AllocateLargeObject(allocator, free_regions, size);
        }
        if (free_regions == 1) {
            ASSERT_TRUE(allocator.Alloc(allocator.GetMaxRegularObjectSize()) != nullptr);
        }
        ASSERT_TRUE(allocator.Alloc(1) == nullptr);
    }
    delete mem_stats;
}

TEST_F(RegionAllocatorTest, AllocateTooMuchRandomRegularAndLargeObjectTest)
{
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    for (int i = 0; i < RegionAllocatorTest::LOOP_COUNT; i++) {
        NonObjectRegionAllocator allocator(mem_stats, SpaceType::SPACE_TYPE_OBJECT, TEST_REGION_SPACE_SIZE, false);
        size_t free_regions = GetRegionsNumber();
        size_t free_bytes_for_cur_reg = 0;
        while (free_regions != 0 || free_bytes_for_cur_reg != 0) {
            ASSERT(free_bytes_for_cur_reg % GetAlignmentInBytes(DEFAULT_ALIGNMENT) == 0);
            size_t size = RandFromRange(1, 3 * allocator.GetMaxRegularObjectSize());
            size_t align_size = AlignUp(size, GetAlignmentInBytes(DEFAULT_ALIGNMENT));
            if (align_size <= NonObjectRegionAllocator::GetMaxRegularObjectSize()) {
                AllocateRegularObject(allocator, free_regions, free_bytes_for_cur_reg, align_size);
            } else if (free_regions > 1) {
                AllocateLargeObject(allocator, free_regions, align_size);
            }
        }
        ASSERT_TRUE(allocator.Alloc(1) == nullptr);
    }
    delete mem_stats;
}

TEST_F(RegionAllocatorTest, AllocatedByRegionAllocatorTest)
{
    mem::MemStatsType mem_stats;
    NonObjectRegionAllocator allocator(&mem_stats, SpaceType::SPACE_TYPE_OBJECT, TEST_REGION_SPACE_SIZE, false);
    AllocatedByThisAllocatorTest(allocator);
}

TEST_F(RegionAllocatorTest, OneAlignmentAllocTest)
{
    OneAlignedAllocFreeTest<NonObjectRegionAllocator::GetMaxRegularObjectSize() - 128,
                            NonObjectRegionAllocator::GetMaxRegularObjectSize() + 128, DEFAULT_ALIGNMENT>(1);
}

TEST_F(RegionAllocatorTest, AllocateFreeDifferentSizesTest)
{
    AllocateFreeDifferentSizesTest<NonObjectRegionAllocator::GetMaxRegularObjectSize() - 128,
                                   NonObjectRegionAllocator::GetMaxRegularObjectSize() + 128>(256, 1);
}

TEST_F(RegionAllocatorTest, RegionTLABAllocTest)
{
    static constexpr size_t ALLOC_SIZE = 512;
    static constexpr size_t ALLOC_COUNT = 5000000;
    auto thread = ManagedThread::GetCurrent();
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    NonObjectRegionAllocator allocator(mem_stats, SpaceType::SPACE_TYPE_OBJECT, TEST_REGION_SPACE_SIZE, false);
    bool is_oom = false;
    auto tlab = thread->GetTLAB();
    ASSERT_NE(tlab, nullptr);
    tlab = allocator.CreateNewTLAB(thread);
    for (size_t i = 0; i < ALLOC_COUNT; i++) {
        auto old_start_pointer = tlab->GetStartAddr();
        auto old_reg = allocator.GetRegion(reinterpret_cast<ObjectHeader *>(old_start_pointer));
        auto mem = tlab->Alloc(ALLOC_SIZE);
        // Check new tlab address
        if (mem == nullptr) {
            auto new_tlab = allocator.CreateNewTLAB(thread);
            ASSERT_FALSE(IsTLAB(old_reg));
            auto new_start_pointer = tlab->GetStartAddr();
            if (new_start_pointer != 0) {
                ASSERT_NE(new_start_pointer, old_start_pointer);
                auto new_reg = allocator.GetRegion(reinterpret_cast<ObjectHeader *>(new_start_pointer));
                ASSERT_TRUE(IsTLAB(new_reg));
            }
            ASSERT_EQ(new_tlab, tlab);
            mem = tlab->Alloc(ALLOC_SIZE);
        }
        if (mem == nullptr) {
            ASSERT_EQ(GetNumFreeRegions(allocator), 0);
            is_oom = true;
            break;
        }
        ASSERT_NE(mem, nullptr);
    }
    ASSERT_EQ(is_oom, true) << "Increase the size of alloc_count to get OOM";
    delete mem_stats;
}

TEST_F(RegionAllocatorTest, RegionPoolTest)
{
    mem::MemStatsType mem_stats;
    NonObjectRegionAllocator allocator(&mem_stats, SpaceType::SPACE_TYPE_OBJECT, RegionSize() * 2, true);

    // Alloc two small objects in a region
    ASSERT_EQ(GetNumFreeRegions(allocator), 2);
    auto *obj1 = reinterpret_cast<ObjectHeader *>(allocator.Alloc(1));  // one byte
    ASSERT_TRUE(obj1 != nullptr);
    ASSERT_EQ(GetNumFreeRegions(allocator), 1);
    auto *obj2 = reinterpret_cast<ObjectHeader *>(allocator.Alloc(DEFAULT_ALIGNMENT_IN_BYTES + 2));  // two byte
    ASSERT_TRUE(obj2 != nullptr);
    ASSERT_EQ(GetNumFreeRegions(allocator), 1);

    // Check that the two objects should be in a region
    ASSERT_EQ(ToUintPtr(obj2), ToUintPtr(obj1) + DEFAULT_ALIGNMENT_IN_BYTES);
    auto *region1 = allocator.GetRegion(obj1);
    ASSERT_TRUE(region1 != nullptr);
    auto *region2 = allocator.GetRegion(obj2);
    ASSERT_TRUE(region2 != nullptr);
    ASSERT_EQ(region1, region2);
    ASSERT_EQ(region1->Top() - region1->Begin(), 3 * DEFAULT_ALIGNMENT_IN_BYTES);

    // Allocate a large object in pool(not in initial block)
    ASSERT_EQ(GetNumFreeRegions(allocator), 1);
    auto *obj3 = reinterpret_cast<ObjectHeader *>(allocator.Alloc(allocator.GetMaxRegularObjectSize() + 200));
    ASSERT_TRUE(obj3 != nullptr);
    ASSERT_EQ(GetNumFreeRegions(allocator), 1);
    auto *region3 = allocator.GetRegion(obj3);
    ASSERT_TRUE(region3 != nullptr);
    ASSERT_NE(region2, region3);
    auto *region30 = allocator.GetSpace()->GetPool()->GetRegion<true>(
        reinterpret_cast<ObjectHeader *>(ToUintPtr(obj3) + allocator.GetMaxRegularObjectSize()));
    ASSERT_EQ(region3, region30);

    // Allocate a regular object which can't be allocated in current region
    auto *obj4 = reinterpret_cast<ObjectHeader *>(
        allocator.Alloc(allocator.GetMaxRegularObjectSize() - DEFAULT_ALIGNMENT_IN_BYTES));
    ASSERT_TRUE(obj4 != nullptr);
    ASSERT_EQ(GetNumFreeRegions(allocator), 0);
    auto *region4 = allocator.GetRegion(obj4);
    ASSERT_TRUE(region4 != nullptr);
    ASSERT_EQ(ToUintPtr(region4), ToUintPtr(region2) + RegionSize());

    auto *obj5 = reinterpret_cast<ObjectHeader *>(allocator.Alloc(DEFAULT_ALIGNMENT_IN_BYTES));
    ASSERT_TRUE(obj5 != nullptr);
    auto *region5 = allocator.GetRegion(obj5);
    ASSERT_EQ(region4, region5);
}

TEST_F(RegionAllocatorTest, IterateOverObjectsTest)
{
    mem::MemStatsType mem_stats;
    NonObjectRegionAllocator allocator(&mem_stats, SpaceType::SPACE_TYPE_OBJECT, 0, true);
    auto *obj1 = reinterpret_cast<ObjectHeader *>(allocator.Alloc(test_class_->GetObjectSize()));
    obj1->SetClass(test_class_);
    auto *obj2 = reinterpret_cast<ObjectHeader *>(allocator.Alloc(test_class_->GetObjectSize()));
    obj2->SetClass(test_class_);
    auto *region = allocator.GetRegion(obj1);
    size_t obj1_num = 0;
    size_t obj2_num = 0;
    region->IterateOverObjects([this, obj1, obj2, region, &obj1_num, &obj2_num, &allocator](ObjectHeader *object) {
        ASSERT_TRUE(object == obj1 || object == obj2);
        ASSERT_EQ(allocator.GetRegion(object), region);
        ASSERT_EQ(object->ClassAddr<Class>(), test_class_);
        if (object == obj1) {
            obj1_num++;
        } else if (object == obj2) {
            obj2_num++;
        }

#ifndef NDEBUG
        // Can't allocator object while iterating the region
        ASSERT_DEATH(allocator.Alloc(test_class_->GetObjectSize()), "");
#endif
    });
    ASSERT_EQ(obj1_num, 1);
    ASSERT_EQ(obj2_num, 1);

#ifndef NDEBUG
    ASSERT_TRUE(region->SetAllocating(true));
    // Can't iterating the region while allocating
    ASSERT_DEATH(region->IterateOverObjects([]([[maybe_unused]] ObjectHeader *object) {}), "");
    ASSERT_TRUE(region->SetAllocating(false));
#endif
}

TEST_F(RegionAllocatorTest, AllocateAndMoveYoungObjectsToTenured)
{
    static constexpr size_t ALLOCATION_COUNT = 10000;
    static constexpr size_t TENURED_OBJECTS_CREATION_RATE = 4;
    mem::MemStatsType mem_stats;
    NonObjectRegionAllocator allocator(&mem_stats, SpaceType::SPACE_TYPE_OBJECT, TEST_REGION_SPACE_SIZE, false);
    // Allocate some objects (young and tenured) in allocator
    for (size_t i = 0; i < ALLOCATION_COUNT; i++) {
        void *mem = nullptr;
        if (i % TENURED_OBJECTS_CREATION_RATE == 0) {
            mem = AllocateObjectWithClass<RegionFlag::IS_OLD>(allocator);
        } else {
            mem = AllocateObjectWithClass<RegionFlag::IS_EDEN>(allocator);
        }
        ASSERT_TRUE(mem != nullptr);
    }
    // Iterate over young objects and move them into tenured:
    allocator.CompactAllSpecificRegions<RegionFlag::IS_EDEN, RegionFlag::IS_OLD>([&](ObjectHeader *object) {
        (void)object;
        return ObjectStatus::ALIVE_OBJECT;
    });
    allocator.ResetAllSpecificRegions<RegionFlag::IS_EDEN>();
    size_t object_found = 0;
    allocator.IterateOverObjects([&](ObjectHeader *object) {
        (void)object;
        object_found++;
    });
    ASSERT_EQ(object_found, ALLOCATION_COUNT);
}

TEST_F(RegionAllocatorTest, AllocateAndCompactTenuredObjects)
{
    static constexpr size_t ALLOCATION_COUNT = 7000;
    static constexpr size_t YOUNG_OBJECTS_CREATION_RATE = 100;
    mem::MemStatsType mem_stats;
    NonObjectRegionAllocator allocator(&mem_stats, SpaceType::SPACE_TYPE_OBJECT, TEST_REGION_SPACE_SIZE, false);
    PandaVector<Region *> regions_vector;
    size_t tenured_object_count = 0;
    // Allocate some objects (young and tenured) in allocator
    for (size_t i = 0; i < ALLOCATION_COUNT; i++) {
        void *mem = nullptr;
        if (i % YOUNG_OBJECTS_CREATION_RATE != 0) {
            mem = AllocateObjectWithClass<RegionFlag::IS_OLD>(allocator);
            tenured_object_count++;
            Region *region = allocator.GetRegion(static_cast<ObjectHeader *>(mem));
            if (std::find(regions_vector.begin(), regions_vector.end(), region) == regions_vector.end()) {
                regions_vector.insert(regions_vector.begin(), region);
            }
        } else {
            mem = AllocateObjectWithClass<RegionFlag::IS_EDEN>(allocator);
        }
        ASSERT_TRUE(mem != nullptr);
    }
    ASSERT_TRUE(regions_vector.size() > 1);
    ASSERT_EQ(allocator.GetAllSpecificRegions<RegionFlag::IS_OLD>().size(), regions_vector.size());
    // Iterate over some tenured regions and compact them:
    size_t object_found = 0;
    allocator.CompactSeveralSpecificRegions<RegionFlag::IS_OLD, RegionFlag::IS_OLD>(
        regions_vector, [&](ObjectHeader *object) {
            (void)object;
            object_found++;
            return ObjectStatus::ALIVE_OBJECT;
        });
    ASSERT_EQ(object_found, tenured_object_count);
    object_found = 0;
    allocator.IterateOverObjects([&](ObjectHeader *object) {
        (void)object;
        object_found++;
    });
    ASSERT_EQ(object_found, ALLOCATION_COUNT + tenured_object_count);
    allocator.ResetSeveralSpecificRegions<RegionFlag::IS_OLD>(regions_vector);
    // Check that we have the same object amount.
    object_found = 0;
    allocator.IterateOverObjects([&](ObjectHeader *object) {
        (void)object;
        object_found++;
    });
    ASSERT_EQ(object_found, ALLOCATION_COUNT);
    // Check that we can still correctly allocate smth in tenured:
    ASSERT_TRUE(AllocateObjectWithClass<RegionFlag::IS_OLD>(allocator) != nullptr);
    // Reset tenured regions:
    allocator.ResetAllSpecificRegions<RegionFlag::IS_OLD>();
    // Check that we can still correctly allocate smth in tenured:
    ASSERT_TRUE(AllocateObjectWithClass<RegionFlag::IS_OLD>(allocator) != nullptr);
}

TEST_F(RegionAllocatorTest, AllocateAndCompactTenuredObjectsViaMarkedBitmap)
{
    static constexpr size_t ALLOCATION_COUNT = 7000;
    static constexpr size_t MARKED_OBJECTS_RATE = 2;
    mem::MemStatsType mem_stats;
    NonObjectRegionAllocator allocator(&mem_stats, SpaceType::SPACE_TYPE_OBJECT, TEST_REGION_SPACE_SIZE, false);
    PandaVector<Region *> regions_vector;
    size_t marked_tenured_object_count = 0;
    // Allocate some objects (young and tenured) in allocator
    for (size_t i = 0; i < ALLOCATION_COUNT; i++) {
        void *mem = AllocateObjectWithClass<RegionFlag::IS_OLD>(allocator);
        Region *region = allocator.GetRegion(static_cast<ObjectHeader *>(mem));
        if (std::find(regions_vector.begin(), regions_vector.end(), region) == regions_vector.end()) {
            regions_vector.insert(regions_vector.begin(), region);
        }
        if (i % MARKED_OBJECTS_RATE != 0) {
            region->SetMarkBit(static_cast<ObjectHeader *>(mem));
            marked_tenured_object_count++;
        }
        ASSERT_TRUE(mem != nullptr);
    }
    ASSERT_TRUE(regions_vector.size() > 1);
    ASSERT_EQ(allocator.GetAllSpecificRegions<RegionFlag::IS_OLD>().size(), regions_vector.size());
    // Iterate over some tenured regions and compact them:
    size_t object_found = 0;
    allocator.CompactSeveralSpecificRegions<RegionFlag::IS_OLD, RegionFlag::IS_OLD, true>(
        regions_vector, [&](ObjectHeader *object) {
            (void)object;
            object_found++;
            return ObjectStatus::ALIVE_OBJECT;
        });
    ASSERT_EQ(object_found, marked_tenured_object_count);
    object_found = 0;
    allocator.IterateOverObjects([&](ObjectHeader *object) {
        (void)object;
        object_found++;
    });
    ASSERT_EQ(object_found, ALLOCATION_COUNT + marked_tenured_object_count);
    allocator.ResetSeveralSpecificRegions<RegionFlag::IS_OLD>(regions_vector);
    // Check that we have the same object amount.
    object_found = 0;
    allocator.IterateOverObjects([&](ObjectHeader *object) {
        (void)object;
        object_found++;
    });
    ASSERT_EQ(object_found, marked_tenured_object_count);
    // Check that we can still correctly allocate smth in tenured:
    ASSERT_TRUE(AllocateObjectWithClass<RegionFlag::IS_OLD>(allocator) != nullptr);
    // Reset tenured regions:
    allocator.ResetAllSpecificRegions<RegionFlag::IS_OLD>();
    // Check that we can still correctly allocate smth in tenured:
    ASSERT_TRUE(AllocateObjectWithClass<RegionFlag::IS_OLD>(allocator) != nullptr);
}

TEST_F(RegionAllocatorTest, MTAllocTest)
{
#if defined(PANDA_TARGET_ARM64) || defined(PANDA_TARGET_32)
    // We have an issue with QEMU during MT tests. Issue 2852
    static constexpr size_t THREADS_COUNT = 1;
#else
    static constexpr size_t THREADS_COUNT = 10;
#endif
    static constexpr size_t MIN_MT_ALLOC_SIZE = 16;
    static constexpr size_t MAX_MT_ALLOC_SIZE = 256;
    static constexpr size_t MIN_ELEMENTS_COUNT = 500;
    static constexpr size_t MAX_ELEMENTS_COUNT = 1000;
    static constexpr size_t MT_TEST_RUN_COUNT = 20;
    for (size_t i = 0; i < MT_TEST_RUN_COUNT; i++) {
        mem::MemStatsType mem_stats;
        NonObjectRegionAllocator allocator(&mem_stats, SpaceType::SPACE_TYPE_OBJECT, RegionSize() * 128, true);
        MT_AllocTest<MIN_MT_ALLOC_SIZE, MAX_MT_ALLOC_SIZE, THREADS_COUNT>(&allocator, MIN_ELEMENTS_COUNT,
                                                                          MAX_ELEMENTS_COUNT);
    }
}

TEST_F(RegionAllocatorTest, MTAllocLargeTest)
{
#if defined(PANDA_TARGET_ARM64) || defined(PANDA_TARGET_32)
    // We have an issue with QEMU during MT tests. Issue 2852
    static constexpr size_t THREADS_COUNT = 1;
#else
    static constexpr size_t THREADS_COUNT = 10;
#endif
    static constexpr size_t MIN_MT_ALLOC_SIZE = 128;
    static constexpr size_t MAX_MT_ALLOC_SIZE = NonObjectRegionAllocator::GetMaxRegularObjectSize() * 3;
    static constexpr size_t MIN_ELEMENTS_COUNT = 10;
    static constexpr size_t MAX_ELEMENTS_COUNT = 30;
    static constexpr size_t MT_TEST_RUN_COUNT = 20;
    for (size_t i = 0; i < MT_TEST_RUN_COUNT; i++) {
        mem::MemStatsType mem_stats;
        NonObjectRegionAllocator allocator(&mem_stats, SpaceType::SPACE_TYPE_OBJECT, RegionSize() * 256, true);
        MT_AllocTest<MIN_MT_ALLOC_SIZE, MAX_MT_ALLOC_SIZE, THREADS_COUNT>(&allocator, MIN_ELEMENTS_COUNT,
                                                                          MAX_ELEMENTS_COUNT);
    }
}

using RegionNonmovableObjectAllocator =
    RegionRunslotsAllocator<ObjectAllocConfigWithCrossingMap, RegionAllocatorLockConfig::CommonLock>;
class RegionNonmovableObjectAllocatorTest : public RegionAllocatorTestBase<RegionNonmovableObjectAllocator> {
};

using RegionNonmovableLargeObjectAllocator =
    RegionFreeListAllocator<ObjectAllocConfigWithCrossingMap, RegionAllocatorLockConfig::CommonLock>;
class RegionNonmovableLargeObjectAllocatorTest : public RegionAllocatorTestBase<RegionNonmovableLargeObjectAllocator> {
};

TEST_F(RegionNonmovableObjectAllocatorTest, AllocatorTest)
{
    mem::MemStatsType mem_stats;
    RegionNonmovableObjectAllocator allocator(&mem_stats, SpaceType::SPACE_TYPE_NON_MOVABLE_OBJECT);
    for (uint32_t i = 8; i <= RegionNonmovableObjectAllocator::GetMaxSize(); i++) {
        ASSERT_TRUE(allocator.Alloc(i) != nullptr);
    }
}

TEST_F(RegionNonmovableObjectAllocatorTest, MTAllocatorTest)
{
#if defined(PANDA_TARGET_ARM64) || defined(PANDA_TARGET_32)
    // We have an issue with QEMU during MT tests. Issue 2852
    static constexpr size_t THREADS_COUNT = 1;
#else
    static constexpr size_t THREADS_COUNT = 10;
#endif
    static constexpr size_t MIN_MT_ALLOC_SIZE = 8;
    static constexpr size_t MAX_MT_ALLOC_SIZE = RegionNonmovableObjectAllocator::GetMaxSize();
    static constexpr size_t MIN_ELEMENTS_COUNT = 200;
    static constexpr size_t MAX_ELEMENTS_COUNT = 300;
    static constexpr size_t MT_TEST_RUN_COUNT = 20;
    for (size_t i = 0; i < MT_TEST_RUN_COUNT; i++) {
        mem::MemStatsType mem_stats;
        RegionNonmovableObjectAllocator allocator(&mem_stats, SpaceType::SPACE_TYPE_NON_MOVABLE_OBJECT);
        MT_AllocTest<MIN_MT_ALLOC_SIZE, MAX_MT_ALLOC_SIZE, THREADS_COUNT>(&allocator, MIN_ELEMENTS_COUNT,
                                                                          MAX_ELEMENTS_COUNT);
        // Region is allocated in allocator, so don't free it explicitly
        allocator.VisitAndRemoveAllPools([]([[maybe_unused]] void *mem, [[maybe_unused]] size_t size) {});
    }
}

TEST_F(RegionNonmovableLargeObjectAllocatorTest, AllocatorTest)
{
    mem::MemStatsType mem_stats;
    RegionNonmovableLargeObjectAllocator allocator(&mem_stats, SpaceType::SPACE_TYPE_NON_MOVABLE_OBJECT);
    size_t start_object_size = RegionNonmovableObjectAllocator::GetMaxSize() + 1;
    for (uint32_t i = start_object_size; i <= start_object_size + 200; i++) {
        ASSERT_TRUE(allocator.Alloc(i) != nullptr);
    }
    ASSERT_TRUE(allocator.Alloc(RegionNonmovableLargeObjectAllocator::GetMaxSize() - 1) != nullptr);
    ASSERT_TRUE(allocator.Alloc(RegionNonmovableLargeObjectAllocator::GetMaxSize()) != nullptr);
}

TEST_F(RegionNonmovableLargeObjectAllocatorTest, MTAllocatorTest)
{
#if defined(PANDA_TARGET_ARM64) || defined(PANDA_TARGET_32)
    // We have an issue with QEMU during MT tests. Issue 2852
    static constexpr size_t THREADS_COUNT = 1;
#else
    static constexpr size_t THREADS_COUNT = 10;
#endif
    static constexpr size_t MIN_MT_ALLOC_SIZE = RegionNonmovableObjectAllocator::GetMaxSize() + 1;
    static constexpr size_t MAX_MT_ALLOC_SIZE = RegionNonmovableLargeObjectAllocator::GetMaxSize();
    static constexpr size_t MIN_ELEMENTS_COUNT = 10;
    static constexpr size_t MAX_ELEMENTS_COUNT = 20;
    static constexpr size_t MT_TEST_RUN_COUNT = 20;
    for (size_t i = 0; i < MT_TEST_RUN_COUNT; i++) {
        mem::MemStatsType mem_stats;
        RegionNonmovableLargeObjectAllocator allocator(&mem_stats, SpaceType::SPACE_TYPE_NON_MOVABLE_OBJECT);
        MT_AllocTest<MIN_MT_ALLOC_SIZE, MAX_MT_ALLOC_SIZE, THREADS_COUNT>(&allocator, MIN_ELEMENTS_COUNT,
                                                                          MAX_ELEMENTS_COUNT);
        // Region is allocated in allocator, so don't free it explicitly
        allocator.VisitAndRemoveAllPools([]([[maybe_unused]] void *mem, [[maybe_unused]] size_t size) {});
    }
}

}  // namespace panda::mem
