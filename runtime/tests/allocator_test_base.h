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

#ifndef PANDA_RUNTIME_TESTS_ALLOCATOR_TEST_BASE_H_
#define PANDA_RUNTIME_TESTS_ALLOCATOR_TEST_BASE_H_

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <tuple>
#include <unordered_set>

#include "libpandabase/mem/mem.h"
#include "libpandabase/os/thread.h"
#include "runtime/mem/bump-allocator.h"
#include "runtime/mem/mem_stats_additional_info.h"
#include "runtime/mem/mem_stats_default.h"
#include "runtime/include/object_header.h"

namespace panda::mem {

template <class Allocator>
class AllocatorTest : public testing::Test {
public:
    explicit AllocatorTest()
    {
#ifdef PANDA_NIGHTLY_TEST_ON
        seed_ = std::time(NULL);
#else
        seed_ = 0xDEADBEEF;
#endif
        srand(seed_);
        InitByteArray();
    }

    ~AllocatorTest() {}

protected:
    static constexpr size_t BYTE_ARRAY_SIZE = 1000;

    unsigned int seed_;
    std::array<uint8_t, BYTE_ARRAY_SIZE> byte_array_;

    /**
     * Byte array initialization of random bytes
     */
    void InitByteArray()
    {
        for (size_t i = 0; i < BYTE_ARRAY_SIZE; ++i) {
            int random_max_limit = 255;
            byte_array_[i] = RandFromRange(0, random_max_limit);
        }
    }

    /**
     * \brief Add pool to allocator (maybe empty for some allocators)
     * @param allocator - allocator for pool memory adding
     */
    virtual void AddMemoryPoolToAllocator([[maybe_unused]] Allocator &allocator) = 0;

    /**
     * \brief Add pool to allocator and protect (maybe empty for some allocators)
     * @param allocator - allocator for pool memory addition and protection
     */
    virtual void AddMemoryPoolToAllocatorProtected([[maybe_unused]] Allocator &allocator) = 0;

    /**
     * \brief Check to allocated by this allocator
     * @param allocator - allocator
     * @param mem - allocated memory
     */
    virtual bool AllocatedByThisAllocator([[maybe_unused]] Allocator &allocator, [[maybe_unused]] void *mem) = 0;

    /**
     * \brief Generate random value from [min_value, max_value]
     * @param min_value - minimum size_t value in range
     * @param max_value - maximum size_t value in range
     * @return random size_t value [min_value, max_value]
     */
    size_t RandFromRange(size_t min_value, size_t max_value)
    {
        // rand() is not thread-safe method.
        // So do it under the lock
        static os::memory::Mutex rand_lock;
        os::memory::LockHolder lock(rand_lock);
        return min_value + rand() % (max_value - min_value + 1);
    }

    /**
     * \brief Write value in memory for death test
     * @param mem - memory for writing
     *
     * Write value in memory for address sanitizer test
     */
    void DeathWriteUint64(void *mem)
    {
        *(static_cast<uint64_t *>(mem)) = 0xDEADBEEF;
    }

    /**
     * \brief Set random bytes in memory from byte array
     * @param mem - memory for random bytes from byte array writing
     * @param size - size memory in bytes
     * @return start index in byte_array
     */
    size_t SetBytesFromByteArray(void *mem, size_t size)
    {
        size_t start_index = RandFromRange(0, BYTE_ARRAY_SIZE - 1);
        size_t copied = 0;
        size_t first_copy_size = std::min(size, BYTE_ARRAY_SIZE - start_index);
        // Set head of memory
        if (memcpy_s(mem, size, &byte_array_[start_index], first_copy_size) != EOK) {
            LOG(FATAL, RUNTIME) << __func__ << " memcpy_s failed";
            UNREACHABLE();
        }
        size -= first_copy_size;
        copied += first_copy_size;
        // Set middle part of memory
        while (size > BYTE_ARRAY_SIZE) {
            if (memcpy_s(ToVoidPtr(ToUintPtr(mem) + copied), size, byte_array_.data(), BYTE_ARRAY_SIZE) != EOK) {
                LOG(FATAL, RUNTIME) << __func__ << " memcpy_s failed";
                UNREACHABLE();
            }
            size -= BYTE_ARRAY_SIZE;
            copied += BYTE_ARRAY_SIZE;
        }
        // Set tail of memory
        (void)memcpy_s(ToVoidPtr(ToUintPtr(mem) + copied), size, byte_array_.data(), size);

        return start_index;
    }

    /**
     * \brief Compare bytes in memory with byte array
     * @param mem - memory for random bytes from byte array writing
     * @param size - size memory in bytes
     * @param start_index_in_byte_array - start index in byte array for comaration with memory
     * @return boolean value: true if bytes are equal and false if not equal
     */
    bool CompareBytesWithByteArray(void *mem, size_t size, size_t start_index_in_byte_array)
    {
        size_t compared = 0;
        size_t first_compare_size = std::min(size, BYTE_ARRAY_SIZE - start_index_in_byte_array);
        // Compare head of memory
        if (memcmp(mem, &byte_array_[start_index_in_byte_array], first_compare_size) != 0) {
            return false;
        }
        compared += first_compare_size;
        size -= first_compare_size;
        // Compare middle part of memory
        while (size >= BYTE_ARRAY_SIZE) {
            if (memcmp(ToVoidPtr(ToUintPtr(mem) + compared), byte_array_.data(), BYTE_ARRAY_SIZE) != 0) {
                return false;
            }
            size -= BYTE_ARRAY_SIZE;
            compared += BYTE_ARRAY_SIZE;
        }
        // Compare tail of memory
        if (memcmp(ToVoidPtr(ToUintPtr(mem) + compared), byte_array_.data(), size) != 0) {
            return false;
        }

        return true;
    }

    /**
     * \brief Allocate with one alignment
     * @tparam MIN_ALLOC_SIZE - minimum possible size for one allocation
     * @tparam MAX_ALLOC_SIZE - maximum possible size for one allocation
     * @tparam ALIGNMENT - enum Alignment value for allocations
     * @param pools_count - count of pools needed by allocation
     *
     * Allocate all possible sizes from [MIN_ALLOC_SIZE, MAX_ALLOC_SIZE] with ALIGNMENT alignment
     */
    template <size_t MIN_ALLOC_SIZE, size_t MAX_ALLOC_SIZE, Alignment ALIGNMENT>
    void OneAlignedAllocFreeTest(size_t pools_count = 1);

    /**
     * \brief Allocate with all alignment
     * @tparam MIN_ALLOC_SIZE - minimum possible size for one allocation
     * @tparam MAX_ALLOC_SIZE - maximum possible size for one allocation
     * @tparam LOG_ALIGN_MIN_VALUE - minimum possible alignment for one allocation
     * @tparam LOG_ALIGN_MAX_VALUE - maximum possible alignment for one allocation
     * @param pools_count - count of pools needed by allocation
     *
     * Allocate all possible sizes from [MIN_ALLOC_SIZE, MAX_ALLOC_SIZE] with all possible alignment from
     * [LOG_ALIGN_MIN, LOG_ALIGN_MAX]
     */
    template <size_t MIN_ALLOC_SIZE, size_t MAX_ALLOC_SIZE, Alignment LOG_ALIGN_MIN_VALUE = LOG_ALIGN_MIN,
              Alignment LOG_ALIGN_MAX_VALUE = LOG_ALIGN_MAX>
    void AlignedAllocFreeTest(size_t pools_count = 1);

    /**
     * \brief Simple test for allocate and free
     * @param alloc_size - size in bytes for each allocation
     * @param elements_count - count of elements for allocation
     * @param pools_count - count of pools needed by allocation
     *
     * Allocate elements with random values setting, check and free memory
     */
    void AllocateAndFree(size_t alloc_size, size_t elements_count, size_t pools_count = 1);

    /**
     * \brief Simple test for checking iteration over free pools method.
     * @tparam pools_count - count of pools needed by allocation, must be bigger than 3
     * @param alloc_size - size in bytes for each allocation
     *
     * Allocate and use memory pools; free all elements from first, last
     * and one in the middle; call iteration over free pools
     * and allocate smth again.
     */
    template <size_t POOLS_COUNT = 5>
    void VisitAndRemoveFreePools(size_t alloc_size);

    /**
     * \brief Allocate with different sizes and free in random order
     * @tparam MIN_ALLOC_SIZE - minimum possible size for one allocation
     * @tparam MAX_ALLOC_SIZE - maximum possible size for one allocation
     * @param elements_count - count of elements for allocation
     * @param pools_count - count of pools needed by allocation
     *
     * Allocate elements with random size and random values setting in random order, check and free memory in random
     * order too
     */
    template <size_t MIN_ALLOC_SIZE, size_t MAX_ALLOC_SIZE>
    void AllocateFreeDifferentSizesTest(size_t elements_count = MAX_ALLOC_SIZE - MIN_ALLOC_SIZE + 1,
                                        size_t pools_count = 1);

    /**
     * \brief Try to allocate too big object, must not allocate memory
     * @tparam MAX_ALLOC_SIZE - maximum possible size for allocation by this allocator
     */
    template <size_t MAX_ALLOC_SIZE>
    void AllocateTooBigObjectTest();

    /**
     * \brief Try to allocate too many objects, must not allocate all objects
     * @param alloc_size - size in bytes for one allocation
     * @param elements_count - count of elements for allocation
     *
     * Allocate too many elements, so must not allocate all objects
     */
    void AllocateTooMuchTest(size_t alloc_size, size_t elements_count);

    /**
     * \brief Use allocator in std::vector
     * @param elements_count - count of elements for allocation
     *
     * Check working of adapter of this allocator on example std::vector
     */
    void AllocateVectorTest(size_t elements_count = 32);

    /**
     * \brief Allocate and reuse
     * @tparam element_type - type of elements for allocations
     * @param alignment_mask - mask for alignment of two addresses
     * @param elements_count - count of elements for allocation
     *
     * Allocate and free memory and later reuse. Checking for two start addresses
     */
    template <class element_type = uint64_t>
    void AllocateReuseTest(size_t alignment_mask, size_t elements_count = 100);

    /**
     * \brief Allocate and free objects, collect via allocator method
     * @tparam MIN_ALLOC_SIZE - minimum possible size for one allocation
     * @tparam MAX_ALLOC_SIZE - maximum possible size for one allocation
     * @tparam LOG_ALIGN_MIN_VALUE - minimum possible alignment for one allocation
     * @tparam LOG_ALIGN_MAX_VALUE - maximum possible alignment for one allocation
     * @tparam ELEMENTS_COUNT_FOR_NOT_POOL_ALLOCATOR - 0 if allocator use pools, count of elements for allocation if
     * don't use pools
     * @param free_granularity - granularity for objects free before collection
     * @param pools_count - count of pools needed by allocation
     *
     * Allocate objects, free part of objects and collect via allocator method with free calls during the collection.
     * Check of collection.
     */
    template <size_t MIN_ALLOC_SIZE, size_t MAX_ALLOC_SIZE, Alignment LOG_ALIGN_MIN_VALUE = LOG_ALIGN_MIN,
              Alignment LOG_ALIGN_MAX_VALUE = LOG_ALIGN_MAX, size_t ELEMENTS_COUNT_FOR_NOT_POOL_ALLOCATOR = 0>
    void ObjectCollectionTest(size_t free_granularity = 4, size_t pools_count = 2);

    /**
     * \brief Allocate and free objects, collect via allocator method
     * @tparam MIN_ALLOC_SIZE - minimum possible size for one allocation
     * @tparam MAX_ALLOC_SIZE - maximum possible size for one allocation
     * @tparam LOG_ALIGN_MIN_VALUE - minimum possible alignment for one allocation
     * @tparam LOG_ALIGN_MAX_VALUE - maximum possible alignment for one allocation
     * @tparam ELEMENTS_COUNT_FOR_NOT_POOL_ALLOCATOR - 0 if allocator use pools, count of elements for allocation if
     * don't use pools
     * @param free_granularity - granularity for objects free before collection
     * @param pools_count - count of pools needed by allocation
     *
     * Allocate objects, free part of objects and iterate via allocator method.
     * Check the iterated elements and free later.
     */
    template <size_t MIN_ALLOC_SIZE, size_t MAX_ALLOC_SIZE, Alignment LOG_ALIGN_MIN_VALUE = LOG_ALIGN_MIN,
              Alignment LOG_ALIGN_MAX_VALUE = LOG_ALIGN_MAX, size_t ELEMENTS_COUNT_FOR_NOT_POOL_ALLOCATOR = 0>
    void ObjectIteratorTest(size_t free_granularity = 4, size_t pools_count = 2);

    /**
     * \brief Allocate and free objects, iterate via allocator method iterating in range
     * @tparam MIN_ALLOC_SIZE - minimum possible size for one allocation
     * @tparam MAX_ALLOC_SIZE - maximum possible size for one allocation
     * @tparam LOG_ALIGN_MIN_VALUE - minimum possible alignment for one allocation
     * @tparam LOG_ALIGN_MAX_VALUE - maximum possible alignment for one allocation
     * @tparam ELEMENTS_COUNT_FOR_NOT_POOL_ALLOCATOR - 0 if allocator use pools, count of elements for allocation if
     * don't use pools
     * @param range_iteration_size - size of a iteration range during test. Must be a power of two
     * @param free_granularity - granularity for objects free before collection
     * @param pools_count - count of pools needed by allocation
     *
     * Allocate objects, free part of objects and iterate via allocator method iterating in range. Check of iteration
     * and free later.
     */
    template <size_t MIN_ALLOC_SIZE, size_t MAX_ALLOC_SIZE, Alignment LOG_ALIGN_MIN_VALUE = LOG_ALIGN_MIN,
              Alignment LOG_ALIGN_MAX_VALUE = LOG_ALIGN_MAX, size_t ELEMENTS_COUNT_FOR_NOT_POOL_ALLOCATOR = 0>
    void ObjectIteratorInRangeTest(size_t range_iteration_size, size_t free_granularity = 4, size_t pools_count = 2);

    /**
     * \brief Address sanitizer test for allocator
     * @tparam elements_count - count of elements for allocation
     * @param free_granularity - granularity for freed elements
     * @param pools_count - count of pools needed by allocation
     *
     * Test for address sanitizer. Free some elements and try to write value in freed elements.
     */
    template <size_t ELEMENTS_COUNT = 100>
    void AsanTest(size_t free_granularity = 3, size_t pools_count = 1);

    /**
     * \brief Test to allocated by this allocator
     *
     * Test for allocator function which check memory on allocaion by this allocator
     */
    void AllocatedByThisAllocatorTest();

    /**
     * \brief Test to allocated by this allocator
     *
     * Test for allocator function which check memory on allocaion by this allocator
     */
    void AllocatedByThisAllocatorTest(Allocator &allocator);

    /**
     * \brief Simultaneously allocate/free objects in different threads
     * @tparam allocator - target allocator for test
     * @tparam MIN_ALLOC_SIZE - minimum possible size for one allocation
     * @tparam MAX_ALLOC_SIZE - maximum possible size for one allocation
     * @tparam THREADS_COUNT - the number of threads used in this test
     * @param min_elements_count - minimum elements which will be allocated during test for each thread
     * @param max_elements_count - maximum elements which will be allocated during test for each thread
     */
    template <size_t MIN_ALLOC_SIZE, size_t MAX_ALLOC_SIZE, size_t THREADS_COUNT>
    void MT_AllocTest(Allocator *allocator, size_t min_elements_count, size_t max_elements_count);

    /**
     * \brief Simultaneously allocate/free objects in different threads
     * @tparam MIN_ALLOC_SIZE - minimum possible size for one allocation
     * @tparam MAX_ALLOC_SIZE - maximum possible size for one allocation
     * @tparam THREADS_COUNT - the number of threads used in this test
     * @param min_elements_count - minimum elements which will be allocated during test for each thread
     * @param max_elements_count - maximum elements which will be allocated during test for each thread
     * @param free_granularity - granularity for objects free before total free
     */
    template <size_t MIN_ALLOC_SIZE, size_t MAX_ALLOC_SIZE, size_t THREADS_COUNT>
    void MT_AllocFreeTest(size_t min_elements_count, size_t max_elements_count, size_t free_granularity = 4);

    /**
     * \brief Simultaneously allocate objects and iterate over objects (in range too) in different threads
     * @tparam MIN_ALLOC_SIZE - minimum possible size for one allocation
     * @tparam MAX_ALLOC_SIZE - maximum possible size for one allocation
     * @tparam THREADS_COUNT - the number of threads used in this test
     * @param min_elements_count - minimum elements which will be allocated during test for each thread
     * @param max_elements_count - maximum elements which will be allocated during test for each thread
     * @param range_iteration_size - size of a iteration range during test. Must be a power of two
     */
    template <size_t MIN_ALLOC_SIZE, size_t MAX_ALLOC_SIZE, size_t THREADS_COUNT>
    void MT_AllocIterateTest(size_t min_elements_count, size_t max_elements_count, size_t range_iteration_size);

    /**
     * \brief Simultaneously allocate and collect objects in different threads
     * @tparam MIN_ALLOC_SIZE - minimum possible size for one allocation
     * @tparam MAX_ALLOC_SIZE - maximum possible size for one allocation
     * @tparam THREADS_COUNT - the number of threads used in this test
     * @param min_elements_count - minimum elements which will be allocated during test for each thread
     * @param max_elements_count - maximum elements which will be allocated during test for each thread
     * @param max_thread_with_collect - maximum threads which will call collect simultaneously
     */
    template <size_t MIN_ALLOC_SIZE, size_t MAX_ALLOC_SIZE, size_t THREADS_COUNT>
    void MT_AllocCollectTest(size_t min_elements_count, size_t max_elements_count, size_t max_thread_with_collect = 1);

private:
    /**
     * \brief Allocate and free objects in allocator for future collecting/iterating checks
     * @tparam MIN_ALLOC_SIZE - minimum possible size for one allocation
     * @tparam MAX_ALLOC_SIZE - maximum possible size for one allocation
     * @tparam LOG_ALIGN_MIN_VALUE - minimum possible alignment for one allocation
     * @tparam LOG_ALIGN_MAX_VALUE - maximum possible alignment for one allocation
     * @tparam ELEMENTS_COUNT_FOR_NOT_POOL_ALLOCATOR - 0 if allocator use pools, count of elements for allocation if
     * don't use pools
     * @param free_granularity - granularity for objects free before collection
     * @param pools_count - count of pools needed by allocation
     *
     * Allocate objects and free part of objects.
     */
    template <size_t MIN_ALLOC_SIZE, size_t MAX_ALLOC_SIZE, Alignment LOG_ALIGN_MIN_VALUE,
              Alignment LOG_ALIGN_MAX_VALUE, size_t ELEMENTS_COUNT_FOR_NOT_POOL_ALLOCATOR>
    void ObjectIteratingSetUp(size_t free_granularity, size_t pools_count, Allocator &allocator, size_t &elements_count,
                              std::vector<void *> &allocated_elements, std::unordered_set<size_t> &used_indexes);

    /**
     * \brief Prepare Allocator for the MT work. Allocate and free everything except one element
     * It will generate a common allocator state before specific tests.
     */
    void MTTestPrologue(Allocator &allocator, size_t alloc_size);

    static void MT_AllocRun(AllocatorTest<Allocator> *allocator_test_instance, Allocator *allocator,
                            std::atomic<size_t> *num_finished, size_t min_alloc_size, size_t max_alloc_size,
                            size_t min_elements_count, size_t max_elements_count);

    static void MT_AllocFreeRun(AllocatorTest<Allocator> *allocator_test_instance, Allocator *allocator,
                                std::atomic<size_t> *num_finished, size_t free_granularity, size_t min_alloc_size,
                                size_t max_alloc_size, size_t min_elements_count, size_t max_elements_count);

    static void MT_AllocIterateRun(AllocatorTest<Allocator> *allocator_test_instance, Allocator *allocator,
                                   std::atomic<size_t> *num_finished, size_t range_iteration_size,
                                   size_t min_alloc_size, size_t max_alloc_size, size_t min_elements_count,
                                   size_t max_elements_count);

    static void MT_AllocCollectRun(AllocatorTest<Allocator> *allocator_test_instance, Allocator *allocator,
                                   std::atomic<size_t> *num_finished, size_t min_alloc_size, size_t max_alloc_size,
                                   size_t min_elements_count, size_t max_elements_count,
                                   uint32_t max_thread_with_collect, std::atomic<uint32_t> *thread_with_collect);

    static std::unordered_set<void *> objects_set_;

    static void VisitAndPutInSet(void *obj_mem)
    {
        objects_set_.insert(obj_mem);
    }

    static ObjectStatus ReturnDeadAndPutInSet(ObjectHeader *obj_mem)
    {
        objects_set_.insert(obj_mem);
        return ObjectStatus::DEAD_OBJECT;
    }

    static bool EraseFromSet(void *obj_mem)
    {
        auto it = objects_set_.find(obj_mem);
        if (it != objects_set_.end()) {
            objects_set_.erase(it);
            return true;
        }
        return false;
    }

    static bool IsEmptySet() noexcept
    {
        return objects_set_.empty();
    }
};

template <class Allocator>
std::unordered_set<void *> AllocatorTest<Allocator>::objects_set_;

template <class Allocator>
template <size_t MIN_ALLOC_SIZE, size_t MAX_ALLOC_SIZE, Alignment ALIGNMENT>
inline void AllocatorTest<Allocator>::OneAlignedAllocFreeTest(size_t pools_count)
{
    static constexpr size_t ALLOCATIONS_COUNT = MAX_ALLOC_SIZE - MIN_ALLOC_SIZE + 1;

    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    Allocator allocator(mem_stats);
    for (size_t i = 0; i < pools_count; ++i) {
        AddMemoryPoolToAllocator(allocator);
    }
    std::array<std::pair<void *, size_t>, ALLOCATIONS_COUNT> allocated_elements;

    // Allocations
    for (size_t size = MIN_ALLOC_SIZE; size <= MAX_ALLOC_SIZE; ++size) {
        void *mem = allocator.Alloc(size, Alignment(ALIGNMENT));
        ASSERT_TRUE(mem != nullptr) << "Didn't allocate " << size << " bytes with  " << static_cast<size_t>(ALIGNMENT)
                                    << " log alignment, seed: " << seed_;
        ASSERT_EQ(reinterpret_cast<uintptr_t>(mem) & (GetAlignmentInBytes(Alignment(ALIGNMENT)) - 1), 0UL)
            << size << " bytes, " << static_cast<size_t>(ALIGNMENT) << " log alignment, seed: " << seed_;
        allocated_elements[size - MIN_ALLOC_SIZE] = {mem, SetBytesFromByteArray(mem, size)};
    }
    // Check and Free
    for (size_t size = MIN_ALLOC_SIZE; size <= MAX_ALLOC_SIZE; size++) {
        size_t k = size - MIN_ALLOC_SIZE;
        ASSERT_TRUE(CompareBytesWithByteArray(allocated_elements[k].first, size, allocated_elements[k].second))
            << "address: " << std::hex << allocated_elements[k].first << ", size: " << size
            << ", alignment: " << static_cast<size_t>(ALIGNMENT) << ", seed: " << seed_;
        allocator.Free(allocated_elements[k].first);
    }
    delete mem_stats;
}

template <class Allocator>
template <size_t MIN_ALLOC_SIZE, size_t MAX_ALLOC_SIZE, Alignment LOG_ALIGN_MIN_VALUE, Alignment LOG_ALIGN_MAX_VALUE>
inline void AllocatorTest<Allocator>::AlignedAllocFreeTest(size_t pools_count)
{
    static_assert(MIN_ALLOC_SIZE <= MAX_ALLOC_SIZE);
    static_assert(LOG_ALIGN_MIN_VALUE <= LOG_ALIGN_MAX_VALUE);
    static constexpr size_t ALLOCATIONS_COUNT =
        (MAX_ALLOC_SIZE - MIN_ALLOC_SIZE + 1) * (LOG_ALIGN_MAX_VALUE - LOG_ALIGN_MIN_VALUE + 1);

    std::array<std::pair<void *, size_t>, ALLOCATIONS_COUNT> allocated_elements;
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    Allocator allocator(mem_stats);
    for (size_t i = 0; i < pools_count; i++) {
        AddMemoryPoolToAllocator(allocator);
    }

    // Allocations with alignment
    size_t k = 0;
    for (size_t size = MIN_ALLOC_SIZE; size <= MAX_ALLOC_SIZE; ++size) {
        for (size_t align = LOG_ALIGN_MIN_VALUE; align <= LOG_ALIGN_MAX_VALUE; ++align, ++k) {
            void *mem = allocator.Alloc(size, Alignment(align));
            ASSERT_TRUE(mem != nullptr) << "Didn't allocate " << size << " bytes with  " << align
                                        << " log alignment, seed: " << seed_;
            ASSERT_EQ(reinterpret_cast<uintptr_t>(mem) & (GetAlignmentInBytes(Alignment(align)) - 1), 0UL)
                << size << " bytes, " << align << " log alignment, seed: " << seed_;
            allocated_elements[k] = {mem, SetBytesFromByteArray(mem, size)};
        }
    }
    // Check and free
    k = 0;
    for (size_t size = MIN_ALLOC_SIZE; size <= MAX_ALLOC_SIZE; ++size) {
        for (size_t align = LOG_ALIGN_MIN_VALUE; align <= LOG_ALIGN_MAX_VALUE; ++align, ++k) {
            ASSERT_TRUE(CompareBytesWithByteArray(allocated_elements[k].first, size, allocated_elements[k].second))
                << "address: " << std::hex << allocated_elements[k].first << ", size: " << size
                << ", alignment: " << align << ", seed: " << seed_;
            allocator.Free(allocated_elements[k].first);
        }
    }
    delete mem_stats;
}

template <class Allocator>
inline void AllocatorTest<Allocator>::AllocateAndFree(size_t alloc_size, size_t elements_count, size_t pools_count)
{
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    Allocator allocator(mem_stats);
    for (size_t i = 0; i < pools_count; i++) {
        AddMemoryPoolToAllocator(allocator);
    }
    std::vector<std::pair<void *, size_t>> allocated_elements(elements_count);

    // Allocations
    for (size_t i = 0; i < elements_count; ++i) {
        void *mem = allocator.Alloc(alloc_size);
        ASSERT_TRUE(mem != nullptr) << "Didn't allocate " << alloc_size << " bytes in " << i
                                    << " iteration, seed: " << seed_;
        size_t index = SetBytesFromByteArray(mem, alloc_size);
        allocated_elements[i] = {mem, index};
    }
    // Free
    for (auto &element : allocated_elements) {
        ASSERT_TRUE(CompareBytesWithByteArray(element.first, alloc_size, element.second))
            << "address: " << std::hex << element.first << ", size: " << alloc_size << ", seed: " << seed_;
        allocator.Free(element.first);
    }
    delete mem_stats;
}

template <class Allocator>
template <size_t POOLS_COUNT>
inline void AllocatorTest<Allocator>::VisitAndRemoveFreePools(size_t alloc_size)
{
    static constexpr size_t POOLS_TO_FREE = 3;
    static_assert(POOLS_COUNT > POOLS_TO_FREE);
    std::array<std::vector<void *>, POOLS_COUNT> allocated_elements;
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    Allocator allocator(mem_stats);

    for (size_t i = 0; i < POOLS_COUNT; i++) {
        AddMemoryPoolToAllocator(allocator);
        while (true) {
            void *mem = allocator.Alloc(alloc_size);
            if (mem == nullptr) {
                break;
            }
            allocated_elements[i].push_back(mem);
        }
    }
    std::array<size_t, POOLS_TO_FREE> freed_pools_indexes = {0, POOLS_COUNT / 2U, POOLS_COUNT - 1};
    // free all elements in pools
    for (auto i : freed_pools_indexes) {
        for (auto j : allocated_elements[i]) {
            allocator.Free(j);
        }
        allocated_elements[i].clear();
    }
    size_t freed_pools = 0;
    allocator.VisitAndRemoveFreePools([&](void *mem, size_t size) {
        (void)mem;
        (void)size;
        freed_pools++;
    });
    ASSERT_TRUE(freed_pools == POOLS_TO_FREE) << ", seed: " << seed_;
    ASSERT_TRUE(allocator.Alloc(alloc_size) == nullptr) << ", seed: " << seed_;
    // allocate again
    for (auto i : freed_pools_indexes) {
        AddMemoryPoolToAllocator(allocator);
        while (true) {
            void *mem = allocator.Alloc(alloc_size);
            if (mem == nullptr) {
                break;
            }
            allocated_elements[i].push_back(mem);
        }
    }
    // free everything:
    for (size_t i = 0; i < POOLS_COUNT; i++) {
        for (auto j : allocated_elements[i]) {
            allocator.Free(j);
        }
        allocated_elements[i].clear();
    }
    freed_pools = 0;
    allocator.VisitAndRemoveFreePools([&](void *mem, size_t size) {
        (void)mem;
        (void)size;
        freed_pools++;
    });
    delete mem_stats;
    ASSERT_TRUE(freed_pools == POOLS_COUNT) << ", seed: " << seed_;
}

template <class Allocator>
template <size_t MIN_ALLOC_SIZE, size_t MAX_ALLOC_SIZE>
inline void AllocatorTest<Allocator>::AllocateFreeDifferentSizesTest(size_t elements_count, size_t pools_count)
{
    std::unordered_set<size_t> used_indexes;
    // (memory, size, start_index_in_byte_array)
    std::vector<std::tuple<void *, size_t, size_t>> allocated_elements(elements_count);
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    Allocator allocator(mem_stats);
    for (size_t i = 0; i < pools_count; i++) {
        AddMemoryPoolToAllocator(allocator);
    }

    size_t full_size_allocated = 0;
    for (size_t i = 0; i < elements_count; ++i) {
        size_t size = RandFromRange(MIN_ALLOC_SIZE, MAX_ALLOC_SIZE);
        // Allocation
        void *mem = allocator.Alloc(size);
        ASSERT_TRUE(mem != nullptr) << "Didn't allocate " << size << " bytes, full allocated: " << full_size_allocated
                                    << ", seed: " << seed_;
        full_size_allocated += size;
        // Write random bytes
        allocated_elements[i] = {mem, size, SetBytesFromByteArray(mem, size)};
        used_indexes.insert(i);
    }
    // Compare and free
    while (!used_indexes.empty()) {
        size_t i = RandFromRange(0, elements_count - 1);
        auto it = used_indexes.find(i);
        if (it != used_indexes.end()) {
            used_indexes.erase(it);
        } else {
            i = *used_indexes.begin();
            used_indexes.erase(used_indexes.begin());
        }
        // Compare
        ASSERT_TRUE(CompareBytesWithByteArray(std::get<0>(allocated_elements[i]), std::get<1>(allocated_elements[i]),
                                              std::get<2U>(allocated_elements[i])))
            << "Address: " << std::hex << std::get<0>(allocated_elements[i])
            << ", size: " << std::get<1>(allocated_elements[i])
            << ", start index in byte array: " << std::get<2U>(allocated_elements[i]) << ", seed: " << seed_;
        allocator.Free(std::get<0>(allocated_elements[i]));
    }
    delete mem_stats;
}

template <class Allocator>
template <size_t MAX_ALLOC_SIZE>
inline void AllocatorTest<Allocator>::AllocateTooBigObjectTest()
{
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    Allocator allocator(mem_stats);
    AddMemoryPoolToAllocator(allocator);

    size_t size_obj = MAX_ALLOC_SIZE + 1 + static_cast<size_t>(rand());
    void *mem = allocator.Alloc(size_obj);
    ASSERT_TRUE(mem == nullptr) << "Allocate too big object with " << size_obj << " size at address " << std::hex
                                << mem;
    delete mem_stats;
}

template <class Allocator>
inline void AllocatorTest<Allocator>::AllocateTooMuchTest(size_t alloc_size, size_t elements_count)
{
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    Allocator allocator(mem_stats);
    AddMemoryPoolToAllocatorProtected(allocator);

    bool is_not_all = false;
    for (size_t i = 0; i < elements_count; i++) {
        void *mem = allocator.Alloc(alloc_size);
        if (mem == nullptr) {
            is_not_all = true;
            break;
        } else {
            SetBytesFromByteArray(mem, alloc_size);
        }
    }
    ASSERT_TRUE(is_not_all) << "elements count: " << elements_count << ", element size: " << alloc_size
                            << ", seed: " << seed_;
    delete mem_stats;
}

template <class Allocator>
inline void AllocatorTest<Allocator>::AllocateVectorTest(size_t elements_count)
{
    using element_type = size_t;
    static constexpr size_t MAGIC_CONST = 3;
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    Allocator allocator(mem_stats);
    AddMemoryPoolToAllocatorProtected(allocator);
    using adapter_type = typename decltype(allocator.Adapter())::template rebind<element_type>::other;
    std::vector<element_type, adapter_type> vec(allocator.Adapter());

    for (size_t i = 0; i < elements_count; i++) {
        vec.push_back(i * MAGIC_CONST);
    }
    for (size_t i = 0; i < elements_count; i++) {
        ASSERT_EQ(vec[i], i * MAGIC_CONST) << "iteration: " << i;
    }

    vec.clear();

    for (size_t i = 0; i < elements_count; i++) {
        vec.push_back(i * (MAGIC_CONST + 1));
    }
    for (size_t i = 0; i < elements_count; i++) {
        ASSERT_EQ(vec[i], i * (MAGIC_CONST + 1)) << "iteration: " << i;
    }
    delete mem_stats;
}

template <class Allocator>
template <class element_type>
inline void AllocatorTest<Allocator>::AllocateReuseTest(size_t alignmnent_mask, size_t elements_count)
{
    static constexpr size_t SIZE_1 = sizeof(element_type);
    static constexpr size_t SIZE_2 = SIZE_1 * 3;

    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    Allocator allocator(mem_stats);
    AddMemoryPoolToAllocator(allocator);
    std::vector<std::pair<void *, size_t>> allocated_elements(elements_count);

    // First allocations
    for (size_t i = 0; i < elements_count; ++i) {
        void *mem = allocator.Alloc(SIZE_1);
        ASSERT_TRUE(mem != nullptr) << "Didn't allocate " << SIZE_1 << " bytes in " << i << " iteration";
        size_t index = SetBytesFromByteArray(mem, SIZE_1);
        allocated_elements[i] = {mem, index};
    }
    uintptr_t first_allocated_mem = reinterpret_cast<uintptr_t>(allocated_elements[0].first);
    // Free
    for (size_t i = 0; i < elements_count; i++) {
        ASSERT_TRUE(CompareBytesWithByteArray(allocated_elements[i].first, SIZE_1, allocated_elements[i].second))
            << "address: " << std::hex << allocated_elements[i].first << ", size: " << SIZE_1 << ", seed: " << seed_;
        allocator.Free(allocated_elements[i].first);
    }
    // Second allocations
    for (size_t i = 0; i < elements_count; ++i) {
        void *mem = allocator.Alloc(SIZE_2);
        ASSERT_TRUE(mem != nullptr) << "Didn't allocate " << SIZE_2 << " bytes in " << i << " iteration";
        size_t index = SetBytesFromByteArray(mem, SIZE_2);
        allocated_elements[i] = {mem, index};
    }
    uintptr_t second_allocated_mem = reinterpret_cast<uintptr_t>(allocated_elements[0].first);
    // Free
    for (size_t i = 0; i < elements_count; i++) {
        ASSERT_TRUE(CompareBytesWithByteArray(allocated_elements[i].first, SIZE_2, allocated_elements[i].second))
            << "address: " << std::hex << allocated_elements[i].first << ", size: " << SIZE_2 << ", seed: " << seed_;
        allocator.Free(allocated_elements[i].first);
    }
    delete mem_stats;
    ASSERT_EQ(first_allocated_mem & ~alignmnent_mask, second_allocated_mem & ~alignmnent_mask)
        << "first address = " << std::hex << first_allocated_mem << ", second address = " << std::hex
        << second_allocated_mem << std::endl
        << "alignment mask: " << alignmnent_mask << ", seed: " << seed_;
}

template <class Allocator>
template <size_t MIN_ALLOC_SIZE, size_t MAX_ALLOC_SIZE, Alignment LOG_ALIGN_MIN_VALUE, Alignment LOG_ALIGN_MAX_VALUE,
          size_t ELEMENTS_COUNT_FOR_NOT_POOL_ALLOCATOR>
inline void AllocatorTest<Allocator>::ObjectIteratingSetUp(size_t free_granularity, size_t pools_count,
                                                           Allocator &allocator, size_t &elements_count,
                                                           std::vector<void *> &allocated_elements,
                                                           std::unordered_set<size_t> &used_indexes)
{
    AddMemoryPoolToAllocator(allocator);
    size_t allocated_pools = 1;
    auto doAllocations = [pools_count]([[maybe_unused]] size_t allocated_pools_count,
                                       [[maybe_unused]] size_t count) -> bool {
        if constexpr (ELEMENTS_COUNT_FOR_NOT_POOL_ALLOCATOR == 0) {
            return allocated_pools_count < pools_count;
        } else {
            (void)pools_count;
            return count < ELEMENTS_COUNT_FOR_NOT_POOL_ALLOCATOR;
        }
    };

    // Allocations
    while (doAllocations(allocated_pools, elements_count)) {
        size_t size = RandFromRange(MIN_ALLOC_SIZE, MAX_ALLOC_SIZE);
        size_t align = RandFromRange(LOG_ALIGN_MIN_VALUE, LOG_ALIGN_MAX_VALUE);
        void *mem = allocator.Alloc(size, Alignment(align));
        if constexpr (ELEMENTS_COUNT_FOR_NOT_POOL_ALLOCATOR == 0) {
            if (mem == nullptr) {
                AddMemoryPoolToAllocator(allocator);
                allocated_pools++;
                mem = allocator.Alloc(size);
            }
        }
        ASSERT_TRUE(mem != nullptr) << "Didn't allocate " << size << " bytes in " << elements_count
                                    << " iteration, seed : " << seed_;
        allocated_elements.push_back(mem);
        used_indexes.insert(elements_count++);
    }
    // Free some elements
    for (size_t i = 0; i < elements_count; i += free_granularity) {
        size_t index = RandFromRange(0, elements_count - 1);
        auto it = used_indexes.find(index);
        if (it == used_indexes.end()) {
            it = used_indexes.begin();
            index = *it;
        }
        allocator.Free(allocated_elements[index]);
        used_indexes.erase(it);
    }
}

template <class Allocator>
template <size_t MIN_ALLOC_SIZE, size_t MAX_ALLOC_SIZE, Alignment LOG_ALIGN_MIN_VALUE, Alignment LOG_ALIGN_MAX_VALUE,
          size_t ELEMENTS_COUNT_FOR_NOT_POOL_ALLOCATOR>
inline void AllocatorTest<Allocator>::ObjectCollectionTest(size_t free_granularity, size_t pools_count)
{
    size_t elements_count = 0;
    std::vector<void *> allocated_elements;
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    Allocator allocator(mem_stats);
    std::unordered_set<size_t> used_indexes;
    ObjectIteratingSetUp<MIN_ALLOC_SIZE, MAX_ALLOC_SIZE, LOG_ALIGN_MIN_VALUE, LOG_ALIGN_MAX_VALUE,
                         ELEMENTS_COUNT_FOR_NOT_POOL_ALLOCATOR>(free_granularity, pools_count, allocator,
                                                                elements_count, allocated_elements, used_indexes);

    // Collect all objects into unordered_set via allocator's method
    allocator.Collect(&AllocatorTest<Allocator>::ReturnDeadAndPutInSet);
    // Check in unordered_set
    for (size_t i = 0; i < elements_count; i++) {
        auto it = used_indexes.find(i);
        if (it != used_indexes.end()) {
            void *mem = allocated_elements[i];
            ASSERT_TRUE(EraseFromSet(mem))
                << "Object at address " << std::hex << mem << " isn't in collected objects, seed: " << seed_;
        }
    }

    delete mem_stats;
    ASSERT_TRUE(IsEmptySet()) << "seed: " << seed_;
}

template <class Allocator>
template <size_t MIN_ALLOC_SIZE, size_t MAX_ALLOC_SIZE, Alignment LOG_ALIGN_MIN_VALUE, Alignment LOG_ALIGN_MAX_VALUE,
          size_t ELEMENTS_COUNT_FOR_NOT_POOL_ALLOCATOR>
inline void AllocatorTest<Allocator>::ObjectIteratorTest(size_t free_granularity, size_t pools_count)
{
    size_t elements_count = 0;
    std::vector<void *> allocated_elements;
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    Allocator allocator(mem_stats);
    std::unordered_set<size_t> used_indexes;
    ObjectIteratingSetUp<MIN_ALLOC_SIZE, MAX_ALLOC_SIZE, LOG_ALIGN_MIN_VALUE, LOG_ALIGN_MAX_VALUE,
                         ELEMENTS_COUNT_FOR_NOT_POOL_ALLOCATOR>(free_granularity, pools_count, allocator,
                                                                elements_count, allocated_elements, used_indexes);

    // Collect all objects into unordered_set via allocator's method
    allocator.IterateOverObjects(&AllocatorTest<Allocator>::VisitAndPutInSet);
    // Free all and check in unordered_set
    for (size_t i = 0; i < elements_count; i++) {
        auto it = used_indexes.find(i);
        if (it != used_indexes.end()) {
            void *mem = allocated_elements[i];
            allocator.Free(mem);
            ASSERT_TRUE(EraseFromSet(mem))
                << "Object at address " << std::hex << mem << " isn't in collected objects, seed: " << seed_;
        }
    }

    delete mem_stats;
    ASSERT_TRUE(IsEmptySet()) << "seed: " << seed_;
}

template <class Allocator>
template <size_t MIN_ALLOC_SIZE, size_t MAX_ALLOC_SIZE, Alignment LOG_ALIGN_MIN_VALUE, Alignment LOG_ALIGN_MAX_VALUE,
          size_t ELEMENTS_COUNT_FOR_NOT_POOL_ALLOCATOR>
inline void AllocatorTest<Allocator>::ObjectIteratorInRangeTest(size_t range_iteration_size, size_t free_granularity,
                                                                size_t pools_count)
{
    ASSERT((range_iteration_size & (range_iteration_size - 1U)) == 0U);
    size_t elements_count = 0;
    std::vector<void *> allocated_elements;
    std::unordered_set<size_t> used_indexes;
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    Allocator allocator(mem_stats);
    ObjectIteratingSetUp<MIN_ALLOC_SIZE, MAX_ALLOC_SIZE, LOG_ALIGN_MIN_VALUE, LOG_ALIGN_MAX_VALUE,
                         ELEMENTS_COUNT_FOR_NOT_POOL_ALLOCATOR>(free_granularity, pools_count, allocator,
                                                                elements_count, allocated_elements, used_indexes);

    void *min_obj_pointer = *std::min_element(allocated_elements.begin(), allocated_elements.end());
    void *max_obj_pointer = *std::max_element(allocated_elements.begin(), allocated_elements.end());
    // Collect all objects into unordered_set via allocator's method
    uintptr_t cur_pointer = ToUintPtr(min_obj_pointer);
    cur_pointer = cur_pointer & (~(range_iteration_size - 1));
    while (cur_pointer <= ToUintPtr(max_obj_pointer)) {
        allocator.IterateOverObjectsInRange(&AllocatorTest<Allocator>::VisitAndPutInSet, ToVoidPtr(cur_pointer),
                                            ToVoidPtr(cur_pointer + range_iteration_size - 1U));
        cur_pointer = cur_pointer + range_iteration_size;
    }

    // Free all and check in unordered_set
    for (size_t i = 0; i < elements_count; i++) {
        auto it = used_indexes.find(i);
        if (it != used_indexes.end()) {
            void *mem = allocated_elements[i];
            allocator.Free(mem);
            ASSERT_TRUE(EraseFromSet(mem))
                << "Object at address " << std::hex << mem << " isn't in collected objects, seed: " << seed_;
        }
    }
    delete mem_stats;
    ASSERT_TRUE(IsEmptySet()) << "seed: " << seed_;
}

template <class Allocator>
template <size_t ELEMENTS_COUNT>
inline void AllocatorTest<Allocator>::AsanTest(size_t free_granularity, size_t pools_count)
{
    using element_type = uint64_t;
    static constexpr size_t ALLOC_SIZE = sizeof(element_type);
    static constexpr size_t ALLOCATIONS_COUNT = ELEMENTS_COUNT;

    if (free_granularity == 0) {
        free_granularity = 1;
    }

    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    Allocator allocator(mem_stats);
    for (size_t i = 0; i < pools_count; i++) {
        AddMemoryPoolToAllocatorProtected(allocator);
    }
    std::array<void *, ALLOCATIONS_COUNT> allocated_elements;
    // Allocations
    for (size_t i = 0; i < ALLOCATIONS_COUNT; ++i) {
        void *mem = allocator.Alloc(ALLOC_SIZE);
        ASSERT_TRUE(mem != nullptr) << "Didn't allocate " << ALLOC_SIZE << " bytes on " << i << " iteration";
        allocated_elements[i] = mem;
    }
    // Free some elements
    for (size_t i = 0; i < ALLOCATIONS_COUNT; i += free_granularity) {
        allocator.Free(allocated_elements[i]);
    }
    // Asan check
    for (size_t i = 0; i < ALLOCATIONS_COUNT; ++i) {
        if (i % free_granularity == 0) {
#ifdef PANDA_ASAN_ON
            EXPECT_DEATH(DeathWriteUint64(allocated_elements[i]), "")
                << "Write " << sizeof(element_type) << " bytes at address " << std::hex << allocated_elements[i];
#else
            continue;
#endif  // PANDA_ASAN_ON
        } else {
            allocator.Free(allocated_elements[i]);
        }
    }
    delete mem_stats;
}

template <class Allocator>
inline void AllocatorTest<Allocator>::AllocatedByThisAllocatorTest()
{
    mem::MemStatsType mem_stats;
    Allocator allocator(&mem_stats);
    AllocatedByThisAllocatorTest(allocator);
}

template <class Allocator>
inline void AllocatorTest<Allocator>::AllocatedByThisAllocatorTest(Allocator &allocator)
{
    static constexpr size_t ALLOC_SIZE = sizeof(uint64_t);
    AddMemoryPoolToAllocatorProtected(allocator);
    void *allocated_by_this = allocator.Alloc(ALLOC_SIZE);
    void *allocated_by_malloc = std::malloc(ALLOC_SIZE);
    uint8_t allocated_on_stack[ALLOC_SIZE];

    ASSERT_TRUE(AllocatedByThisAllocator(allocator, allocated_by_this)) << "address: " << std::hex << allocated_by_this;
    ASSERT_FALSE(AllocatedByThisAllocator(allocator, allocated_by_malloc))
        << "address: " << std::hex << allocated_by_malloc;
    ASSERT_FALSE(AllocatedByThisAllocator(allocator, static_cast<void *>(allocated_on_stack)))
        << "address on stack: " << std::hex << static_cast<void *>(allocated_on_stack);

    allocator.Free(allocated_by_this);
    std::free(allocated_by_malloc);

    ASSERT_FALSE(AllocatedByThisAllocator(allocator, allocated_by_malloc))
        << "after free, address: " << std::hex << allocated_by_malloc;
}

template <class Allocator>
void AllocatorTest<Allocator>::MT_AllocRun(AllocatorTest<Allocator> *allocator_test_instance, Allocator *allocator,
                                           std::atomic<size_t> *num_finished, size_t min_alloc_size,
                                           size_t max_alloc_size, size_t min_elements_count, size_t max_elements_count)
{
    size_t elements_count = allocator_test_instance->RandFromRange(min_elements_count, max_elements_count);
    std::unordered_set<size_t> used_indexes;
    // (memory, size, start_index_in_byte_array)
    std::vector<std::tuple<void *, size_t, size_t>> allocated_elements(elements_count);

    for (size_t i = 0; i < elements_count; ++i) {
        size_t size = allocator_test_instance->RandFromRange(min_alloc_size, max_alloc_size);
        // Allocation
        void *mem = allocator->Alloc(size);
        // Do while because other threads can use the whole pool before we try to allocate smth in it
        while (mem == nullptr) {
            allocator_test_instance->AddMemoryPoolToAllocator(*allocator);
            mem = allocator->Alloc(size);
        }
        ASSERT_TRUE(mem != nullptr);
        // Write random bytes
        allocated_elements[i] = {mem, size, allocator_test_instance->SetBytesFromByteArray(mem, size)};
        used_indexes.insert(i);
    }

    // Compare
    while (!used_indexes.empty()) {
        size_t i = allocator_test_instance->RandFromRange(0, elements_count - 1);
        auto it = used_indexes.find(i);
        if (it != used_indexes.end()) {
            used_indexes.erase(it);
        } else {
            i = *used_indexes.begin();
            used_indexes.erase(used_indexes.begin());
        }
        ASSERT_TRUE(allocator_test_instance->AllocatedByThisAllocator(*allocator, std::get<0>(allocated_elements[i])));
        ASSERT_TRUE(allocator_test_instance->CompareBytesWithByteArray(std::get<0>(allocated_elements[i]),
                                                                       std::get<1>(allocated_elements[i]),
                                                                       std::get<2U>(allocated_elements[i])))
            << "Address: " << std::hex << std::get<0>(allocated_elements[i])
            << ", size: " << std::get<1>(allocated_elements[i])
            << ", start index in byte array: " << std::get<2U>(allocated_elements[i])
            << ", seed: " << allocator_test_instance->seed_;
    }
    num_finished->fetch_add(1);
}

template <class Allocator>
void AllocatorTest<Allocator>::MT_AllocFreeRun(AllocatorTest<Allocator> *allocator_test_instance, Allocator *allocator,
                                               std::atomic<size_t> *num_finished, size_t free_granularity,
                                               size_t min_alloc_size, size_t max_alloc_size, size_t min_elements_count,
                                               size_t max_elements_count)
{
    size_t elements_count = allocator_test_instance->RandFromRange(min_elements_count, max_elements_count);
    std::unordered_set<size_t> used_indexes;
    // (memory, size, start_index_in_byte_array)
    std::vector<std::tuple<void *, size_t, size_t>> allocated_elements(elements_count);

    for (size_t i = 0; i < elements_count; ++i) {
        size_t size = allocator_test_instance->RandFromRange(min_alloc_size, max_alloc_size);
        // Allocation
        void *mem = allocator->Alloc(size);
        // Do while because other threads can use the whole pool before we try to allocate smth in it
        while (mem == nullptr) {
            allocator_test_instance->AddMemoryPoolToAllocator(*allocator);
            mem = allocator->Alloc(size);
        }
        ASSERT_TRUE(mem != nullptr);
        // Write random bytes
        allocated_elements[i] = {mem, size, allocator_test_instance->SetBytesFromByteArray(mem, size)};
        used_indexes.insert(i);
    }

    // Free some elements
    for (size_t i = 0; i < elements_count; i += free_granularity) {
        size_t index = allocator_test_instance->RandFromRange(0, elements_count - 1);
        auto it = used_indexes.find(index);
        if (it != used_indexes.end()) {
            used_indexes.erase(it);
        } else {
            index = *used_indexes.begin();
            used_indexes.erase(used_indexes.begin());
        }
        ASSERT_TRUE(
            allocator_test_instance->AllocatedByThisAllocator(*allocator, std::get<0>(allocated_elements[index])));
        // Compare
        ASSERT_TRUE(allocator_test_instance->CompareBytesWithByteArray(std::get<0>(allocated_elements[index]),
                                                                       std::get<1>(allocated_elements[index]),
                                                                       std::get<2U>(allocated_elements[index])))
            << "Address: " << std::hex << std::get<0>(allocated_elements[index])
            << ", size: " << std::get<1>(allocated_elements[index])
            << ", start index in byte array: " << std::get<2U>(allocated_elements[index])
            << ", seed: " << allocator_test_instance->seed_;
        allocator->Free(std::get<0>(allocated_elements[index]));
    }

    // Compare and free
    while (!used_indexes.empty()) {
        size_t i = allocator_test_instance->RandFromRange(0, elements_count - 1);
        auto it = used_indexes.find(i);
        if (it != used_indexes.end()) {
            used_indexes.erase(it);
        } else {
            i = *used_indexes.begin();
            used_indexes.erase(used_indexes.begin());
        }
        // Compare
        ASSERT_TRUE(allocator_test_instance->CompareBytesWithByteArray(std::get<0>(allocated_elements[i]),
                                                                       std::get<1>(allocated_elements[i]),
                                                                       std::get<2U>(allocated_elements[i])))
            << "Address: " << std::hex << std::get<0>(allocated_elements[i])
            << ", size: " << std::get<1>(allocated_elements[i])
            << ", start index in byte array: " << std::get<2U>(allocated_elements[i])
            << ", seed: " << allocator_test_instance->seed_;
        allocator->Free(std::get<0>(allocated_elements[i]));
    }
    num_finished->fetch_add(1);
}

template <class Allocator>
void AllocatorTest<Allocator>::MT_AllocIterateRun(AllocatorTest<Allocator> *allocator_test_instance,
                                                  Allocator *allocator, std::atomic<size_t> *num_finished,
                                                  size_t range_iteration_size, size_t min_alloc_size,
                                                  size_t max_alloc_size, size_t min_elements_count,
                                                  size_t max_elements_count)
{
    static constexpr size_t ITERATION_IN_RANGE_COUNT = 100;
    size_t elements_count = allocator_test_instance->RandFromRange(min_elements_count, max_elements_count);
    // (memory, size, start_index_in_byte_array)
    std::vector<std::tuple<void *, size_t, size_t>> allocated_elements(elements_count);

    // Iterate over all object
    allocator->IterateOverObjects([&](void *mem) { (void)mem; });

    // Allocate objects
    for (size_t i = 0; i < elements_count; ++i) {
        size_t size = allocator_test_instance->RandFromRange(min_alloc_size, max_alloc_size);
        // Allocation
        void *mem = allocator->Alloc(size);
        // Do while because other threads can use the whole pool before we try to allocate smth in it
        while (mem == nullptr) {
            allocator_test_instance->AddMemoryPoolToAllocator(*allocator);
            mem = allocator->Alloc(size);
        }
        ASSERT_TRUE(mem != nullptr);
        // Write random bytes
        allocated_elements[i] = {mem, size, allocator_test_instance->SetBytesFromByteArray(mem, size)};
    }

    // Iterate over all object
    allocator->IterateOverObjects([&](void *mem) { (void)mem; });

    size_t iterated_over_objects = 0;
    // Compare values inside the objects
    for (size_t i = 0; i < elements_count; ++i) {
        // do a lot of iterate over range calls to check possible races
        if (iterated_over_objects < ITERATION_IN_RANGE_COUNT) {
            void *left_border = ToVoidPtr(ToUintPtr(std::get<0>(allocated_elements[i])) & ~(range_iteration_size - 1U));
            void *right_border = ToVoidPtr(ToUintPtr(left_border) + range_iteration_size - 1U);
            allocator->IterateOverObjectsInRange([&](void *mem) { (void)mem; }, left_border, right_border);
            iterated_over_objects++;
        }
        ASSERT_TRUE(allocator_test_instance->AllocatedByThisAllocator(*allocator, std::get<0>(allocated_elements[i])));
        // Compare
        ASSERT_TRUE(allocator_test_instance->CompareBytesWithByteArray(std::get<0>(allocated_elements[i]),
                                                                       std::get<1>(allocated_elements[i]),
                                                                       std::get<2U>(allocated_elements[i])))
            << "Address: " << std::hex << std::get<0>(allocated_elements[i])
            << ", size: " << std::get<1>(allocated_elements[i])
            << ", start index in byte array: " << std::get<2U>(allocated_elements[i])
            << ", seed: " << allocator_test_instance->seed_;
    }
    num_finished->fetch_add(1);
}

template <class Allocator>
void AllocatorTest<Allocator>::MT_AllocCollectRun(AllocatorTest<Allocator> *allocator_test_instance,
                                                  Allocator *allocator, std::atomic<size_t> *num_finished,
                                                  size_t min_alloc_size, size_t max_alloc_size,
                                                  size_t min_elements_count, size_t max_elements_count,
                                                  uint32_t max_thread_with_collect,
                                                  std::atomic<uint32_t> *thread_with_collect)
{
    size_t elements_count = allocator_test_instance->RandFromRange(min_elements_count, max_elements_count);

    // Allocate objects
    for (size_t i = 0; i < elements_count; ++i) {
        size_t size = allocator_test_instance->RandFromRange(min_alloc_size, max_alloc_size);
        // Allocation
        void *mem = allocator->Alloc(size);
        // Do while because other threads can use the whole pool before we try to allocate smth in it
        while (mem == nullptr) {
            allocator_test_instance->AddMemoryPoolToAllocator(*allocator);
            mem = allocator->Alloc(size);
        }
        ASSERT_TRUE(mem != nullptr);
        auto object = static_cast<ObjectHeader *>(mem);
        object->SetMarkedForGC();
    }

    // Collect objects
    if (thread_with_collect->fetch_add(1U) < max_thread_with_collect) {
        allocator->Collect([&](ObjectHeader *object) {
            ObjectStatus object_status =
                object->IsMarkedForGC() ? ObjectStatus::DEAD_OBJECT : ObjectStatus::ALIVE_OBJECT;
            return object_status;
        });
    }
    num_finished->fetch_add(1);
}

template <class Allocator>
void AllocatorTest<Allocator>::MTTestPrologue(Allocator &allocator, size_t alloc_size)
{
    // Allocator preparing:
    std::vector<void *> allocated_elements;
    AddMemoryPoolToAllocator(allocator);
    // Allocate objects
    while (true) {
        // Allocation
        void *mem = allocator.Alloc(alloc_size);
        if (mem == nullptr) {
            break;
        }
        allocated_elements.push_back(mem);
    }
    // Free everything except one element:
    for (size_t i = 1; i < allocated_elements.size(); ++i) {
        allocator.Free(allocated_elements[i]);
    }

    allocator.VisitAndRemoveFreePools([&](void *mem, size_t size) {
        (void)mem;
        (void)size;
    });
}

template <class Allocator>
template <size_t MIN_ALLOC_SIZE, size_t MAX_ALLOC_SIZE, size_t THREADS_COUNT>
inline void AllocatorTest<Allocator>::MT_AllocTest(Allocator *allocator, size_t min_elements_count,
                                                   size_t max_elements_count)
{
#if defined(PANDA_TARGET_ARM64) || defined(PANDA_TARGET_32)
    // We have an issue with QEMU during MT tests. Issue 2852
    static_assert(THREADS_COUNT == 1);
#endif
    std::atomic<size_t> num_finished = 0;
    for (size_t i = 0; i < THREADS_COUNT; i++) {
        auto tid = os::thread::ThreadStart(&MT_AllocRun, this, allocator, &num_finished, MIN_ALLOC_SIZE, MAX_ALLOC_SIZE,
                                           min_elements_count, max_elements_count);
        os::thread::ThreadDetach(tid);
    }

    while (true) {
        if (num_finished.load() == THREADS_COUNT) {
            break;
        }
        os::thread::Yield();
    }
}

template <class Allocator>
template <size_t MIN_ALLOC_SIZE, size_t MAX_ALLOC_SIZE, size_t THREADS_COUNT>
inline void AllocatorTest<Allocator>::MT_AllocFreeTest(size_t min_elements_count, size_t max_elements_count,
                                                       size_t free_granularity)
{
#if defined(PANDA_TARGET_ARM64) || defined(PANDA_TARGET_32)
    // We have an issue with QEMU during MT tests. Issue 2852
    static_assert(THREADS_COUNT == 1);
#endif
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    Allocator allocator(mem_stats);
    std::atomic<size_t> num_finished = 0;

    // Prepare an allocator
    MTTestPrologue(allocator, RandFromRange(MIN_ALLOC_SIZE, MAX_ALLOC_SIZE));

    for (size_t i = 0; i < THREADS_COUNT; i++) {
        (void)free_granularity;
        auto tid = os::thread::ThreadStart(&MT_AllocFreeRun, this, &allocator, &num_finished, free_granularity,
                                           MIN_ALLOC_SIZE, MAX_ALLOC_SIZE, min_elements_count, max_elements_count);
        os::thread::ThreadDetach(tid);
    }

    while (true) {
        if (num_finished.load() == THREADS_COUNT) {
            break;
        }
        os::thread::Yield();
    }
    delete mem_stats;
}

template <class Allocator>
template <size_t MIN_ALLOC_SIZE, size_t MAX_ALLOC_SIZE, size_t THREADS_COUNT>
inline void AllocatorTest<Allocator>::MT_AllocIterateTest(size_t min_elements_count, size_t max_elements_count,
                                                          size_t range_iteration_size)
{
#if defined(PANDA_TARGET_ARM64) || defined(PANDA_TARGET_32)
    // We have an issue with QEMU during MT tests. Issue 2852
    static_assert(THREADS_COUNT == 1);
#endif
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    Allocator allocator(mem_stats);
    std::atomic<size_t> num_finished = 0;

    // Prepare an allocator
    MTTestPrologue(allocator, RandFromRange(MIN_ALLOC_SIZE, MAX_ALLOC_SIZE));

    for (size_t i = 0; i < THREADS_COUNT; i++) {
        (void)range_iteration_size;
        auto tid = os::thread::ThreadStart(&MT_AllocIterateRun, this, &allocator, &num_finished, range_iteration_size,
                                           MIN_ALLOC_SIZE, MAX_ALLOC_SIZE, min_elements_count, max_elements_count);
        os::thread::ThreadDetach(tid);
    }

    while (true) {
        if (num_finished.load() == THREADS_COUNT) {
            break;
        }
        os::thread::Yield();
    }

    // Delete all objects in allocator
    allocator.Collect([&](ObjectHeader *object) {
        (void)object;
        return ObjectStatus::DEAD_OBJECT;
    });
    delete mem_stats;
}

template <class Allocator>
template <size_t MIN_ALLOC_SIZE, size_t MAX_ALLOC_SIZE, size_t THREADS_COUNT>
inline void AllocatorTest<Allocator>::MT_AllocCollectTest(size_t min_elements_count, size_t max_elements_count,
                                                          size_t max_thread_with_collect)
{
#if defined(PANDA_TARGET_ARM64) || defined(PANDA_TARGET_32)
    // We have an issue with QEMU during MT tests. Issue 2852
    static_assert(THREADS_COUNT == 1);
#endif
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    Allocator allocator(mem_stats);
    std::atomic<size_t> num_finished = 0;
    std::atomic<uint32_t> thread_with_collect {0U};

    // Prepare an allocator
    MTTestPrologue(allocator, RandFromRange(MIN_ALLOC_SIZE, MAX_ALLOC_SIZE));

    for (size_t i = 0; i < THREADS_COUNT; i++) {
        auto tid = os::thread::ThreadStart(&MT_AllocCollectRun, this, &allocator, &num_finished, MIN_ALLOC_SIZE,
                                           MAX_ALLOC_SIZE, min_elements_count, max_elements_count,
                                           max_thread_with_collect, &thread_with_collect);
        os::thread::ThreadDetach(tid);
    }

    while (true) {
        if (num_finished.load() == THREADS_COUNT) {
            break;
        }
        os::thread::Yield();
    }

    // Delete all objects in allocator
    allocator.Collect([&](ObjectHeader *object) {
        (void)object;
        return ObjectStatus::DEAD_OBJECT;
    });
    delete mem_stats;
}

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_TESTS_ALLOCATOR_TEST_BASE_H_
