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

#include "mem/arena.h"
#include "mem/arena_allocator.h"
#include "mem/arena_allocator_stl_adapter.h"
#include "mem/pool_manager.h"
#include "mem/mem.h"
#include "mem/mem_config.h"

#include "utils/arena_containers.h"

#include "gtest/gtest.h"
#include "utils/logger.h"

#include <string>
#include <array>
#include <limits>
#include <cstdint>
#include <ctime>

namespace panda {

class ArenaAllocatorTest : public testing::Test {
public:
    ArenaAllocatorTest()
    {
#ifdef PANDA_NIGHTLY_TEST_ON
        seed_ = std::time(NULL);
#else
        seed_ = 123456U;
#endif
    }

    ~ArenaAllocatorTest() {}

protected:
    static constexpr size_t MIN_LOG_ALIGN_SIZE_T = static_cast<size_t>(LOG_ALIGN_MIN);
    static constexpr size_t MAX_LOG_ALIGN_SIZE_T = static_cast<size_t>(LOG_ALIGN_MAX);
    static constexpr size_t ARRAY_SIZE = 1024;

    unsigned int seed_;

    template <class T>
    static constexpr T MAX_VALUE()
    {
        return std::numeric_limits<T>::max();
    }

    static bool IsAligned(const void *ptr, size_t alignment)
    {
        return reinterpret_cast<uintptr_t>(ptr) % alignment == 0;
    }

    void SetUp() override
    {
        panda::mem::MemConfig::Initialize(0, 128_MB, 0, 0);
        PoolManager::Initialize();
    }

    template <class T>
    void AllocateWithAlignment() const
    {
        ArenaAllocator aa(SpaceType::SPACE_TYPE_INTERNAL);

        for (Alignment align = LOG_ALIGN_MIN; align <= LOG_ALIGN_MAX;
             align = static_cast<Alignment>(static_cast<size_t>(align) + 1)) {
            std::array<T *, ARRAY_SIZE> arr;

            size_t mask = GetAlignmentInBytes(align) - 1;

            // Allocations
            srand(seed_);
            for (size_t i = 0; i < ARRAY_SIZE; ++i) {
                arr[i] = static_cast<T *>(aa.Alloc(sizeof(T), align));
                *arr[i] = rand() % MAX_VALUE<T>();
            }

            // Allocations checking
            srand(seed_);
            for (size_t i = 0; i < ARRAY_SIZE; ++i) {
                ASSERT_NE(arr[i], nullptr) << "value of i: " << i << ", align: " << align;
                ASSERT_EQ(reinterpret_cast<size_t>(arr[i]) & mask, 0U) << "value of i: " << i << ", align: " << align;
                ASSERT_EQ(*arr[i], rand() % MAX_VALUE<T>()) << "value of i: " << i << ", align: " << align;
            }
        }
    }

    template <class T>
    void AllocateWithDiffAlignment() const
    {
        ArenaAllocator aa(SpaceType::SPACE_TYPE_INTERNAL);

        std::array<T *, ARRAY_SIZE> arr;

        // Allocations
        srand(seed_);
        for (size_t i = 0; i < ARRAY_SIZE; ++i) {
            auto random_value = rand();
            size_t rand_align = MIN_LOG_ALIGN_SIZE_T + random_value % (MAX_LOG_ALIGN_SIZE_T - MIN_LOG_ALIGN_SIZE_T);
            arr[i] = static_cast<T *>(aa.Alloc(sizeof(T), static_cast<Alignment>(rand_align)));
            *arr[i] = random_value % MAX_VALUE<T>();
        }

        // Allocations checking
        srand(seed_);
        for (size_t i = 0; i < ARRAY_SIZE; ++i) {
            auto random_value = rand();
            size_t align = MIN_LOG_ALIGN_SIZE_T + random_value % (MAX_LOG_ALIGN_SIZE_T - MIN_LOG_ALIGN_SIZE_T);
            size_t mask = GetAlignmentInBytes(static_cast<Alignment>(align)) - 1;

            ASSERT_NE(arr[i], nullptr);
            ASSERT_EQ(reinterpret_cast<size_t>(arr[i]) & mask, 0U) << "value of i: " << i << ", align: " << align;
            ASSERT_EQ(*arr[i], random_value % MAX_VALUE<T>()) << "value of i: " << i;
        }
    }

    void TearDown() override
    {
        PoolManager::Finalize();
        panda::mem::MemConfig::Finalize();
    }
};

class ComplexClass final {
public:
    ComplexClass() : value_(0), str_value_("0") {}
    explicit ComplexClass(size_t value) : value_(value), str_value_(std::to_string(value_)) {}
    ComplexClass(size_t value, const std::string str_value) : value_(value), str_value_(str_value) {}
    ComplexClass(const ComplexClass &other) = default;
    ComplexClass(ComplexClass &&other) noexcept = default;

    ComplexClass &operator=(const ComplexClass &other) = default;
    ComplexClass &operator=(ComplexClass &&other) = default;

    size_t getValue() const noexcept
    {
        return value_;
    }
    std::string getString() const noexcept
    {
        return str_value_;
    }

    void setValue(size_t value)
    {
        value_ = value;
        str_value_ = std::to_string(value);
    }

    ~ComplexClass() {}

private:
    size_t value_;
    std::string str_value_;
};

TEST_F(ArenaAllocatorTest, AllocateTest)
{
    ArenaAllocator aa(SpaceType::SPACE_TYPE_INTERNAL);
    void *addr = aa.Alloc(24);
    ASSERT_NE(addr, nullptr);
    ASSERT_TRUE(IsAligned(addr, GetAlignmentInBytes(DEFAULT_ARENA_ALIGNMENT)));
    addr = aa.Alloc(4);
    ASSERT_NE(addr, nullptr);
    ASSERT_TRUE(IsAligned(addr, GetAlignmentInBytes(DEFAULT_ARENA_ALIGNMENT)));

    void *tmp = aa.AllocArray<int>(1024);
    // Make sure that we force to use dynamic pool if STACK pool is enabled
    for (int i = 0; i < 5; ++i) {
        void *mem = nullptr;
        mem = aa.Alloc(DEFAULT_ARENA_SIZE / 2);
        ASSERT_NE(mem, nullptr);
        *(static_cast<char *>(mem)) = 33;  // Try to catch segfault in case something went wrong
    }
    ASSERT_NE(tmp = aa.Alloc(DEFAULT_ARENA_SIZE - AlignUp(sizeof(Arena), GetAlignmentInBytes(DEFAULT_ARENA_ALIGNMENT))),
              nullptr);
    size_t maxAlignDrift;
    maxAlignDrift = (DEFAULT_ALIGNMENT_IN_BYTES > alignof(Arena)) ? (DEFAULT_ALIGNMENT_IN_BYTES - alignof(Arena)) : 0;
    ASSERT_EQ(tmp = aa.Alloc(DEFAULT_ARENA_SIZE + maxAlignDrift + 1), nullptr);
}

TEST_F(ArenaAllocatorTest, AllocateVectorTest)
{
    constexpr size_t SIZE = 2048;
    constexpr size_t SMALL_MAGIC_CONSTANT = 3;

    ArenaAllocator aa(SpaceType::SPACE_TYPE_INTERNAL);
    ArenaVector<unsigned> vec(aa.Adapter());

    for (size_t i = 0; i < SIZE; ++i) {
        vec.push_back(i * SMALL_MAGIC_CONSTANT);
    }

    ASSERT_EQ(SIZE, vec.size());
    vec.shrink_to_fit();
    ASSERT_EQ(SIZE, vec.size());

    for (size_t i = 0; i < SIZE; ++i) {
        ASSERT_EQ(i * SMALL_MAGIC_CONSTANT, vec[i]) << "value of i: " << i;
    }
}

TEST_F(ArenaAllocatorTest, AllocateVectorWithComplexTypeTest)
{
    constexpr size_t SIZE = 512;
    constexpr size_t MAGIC_CONSTANT_1 = std::numeric_limits<size_t>::max() / (SIZE + 2);
    srand(seed_);
    size_t MAGIC_CONSTANT_2 = rand() % SIZE;

    ArenaAllocator aa(SpaceType::SPACE_TYPE_INTERNAL);
    ArenaVector<ComplexClass> vec(aa.Adapter());

    // Allocate SIZE objects
    for (size_t i = 0; i < SIZE; ++i) {
        vec.emplace_back(i * MAGIC_CONSTANT_1 + MAGIC_CONSTANT_2, std::to_string(i));
    }

    // Size checking
    ASSERT_EQ(SIZE, vec.size());

    // Allocations checking
    for (size_t i = 0; i < SIZE; ++i) {
        ASSERT_EQ(vec[i].getValue(), i * MAGIC_CONSTANT_1 + MAGIC_CONSTANT_2) << "value of i: " << i;
        ASSERT_EQ(vec[i].getString(), std::to_string(i)) << i;
    }
    ComplexClass *data_ptr = vec.data();
    for (size_t i = 0; i < SIZE; ++i) {
        ASSERT_EQ(data_ptr[i].getValue(), i * MAGIC_CONSTANT_1 + MAGIC_CONSTANT_2) << "value of i: " << i;
        ASSERT_EQ(data_ptr[i].getString(), std::to_string(i)) << "value of i: " << i;
    }

    // Resizing and new elements assignment
    constexpr size_t SIZE_2 = SIZE << 1;
    vec.assign(SIZE_2, ComplexClass(1, "1"));

    // Size checking
    ASSERT_EQ(SIZE_2, vec.size());
    vec.shrink_to_fit();
    ASSERT_EQ(SIZE_2, vec.size());

    // Allocations and assignment checking
    for (size_t i = 0; i < SIZE_2; ++i) {
        ASSERT_EQ(vec[i].getValue(), 1U) << "value of i: " << i;
        ASSERT_EQ(vec[i].getString(), "1") << "value of i: " << i;
    }

    // Increasing size
    constexpr size_t SIZE_4 = SIZE_2 << 1;
    vec.resize(SIZE_4, ComplexClass());

    // Size checking
    ASSERT_EQ(SIZE_4, vec.size());

    // Allocations checking
    for (size_t i = 0; i < SIZE_4 / 2; ++i) {
        ASSERT_EQ(vec[i].getValue(), 1U) << "value of i: " << i;
        ASSERT_EQ(vec[i].getString(), "1") << "value of i: " << i;
    }
    for (size_t i = SIZE_4 / 2; i < SIZE_4; ++i) {
        ASSERT_EQ(vec[i].getValue(), 0U) << "value of i: " << i;
        ASSERT_EQ(vec[i].getString(), "0") << "value of i: " << i;
    }

    // Decreasing size
    vec.resize(SIZE);

    // Size checking
    ASSERT_EQ(SIZE, vec.size());
    vec.shrink_to_fit();
    ASSERT_EQ(SIZE, vec.size());

    // Allocations checking
    for (size_t i = 0; i < SIZE; ++i) {
        ASSERT_EQ(vec[i].getValue(), 1U) << "value of i: " << i;
        ASSERT_EQ(vec[i].getString(), "1") << "value of i: " << i;
    }
}

TEST_F(ArenaAllocatorTest, AllocateDequeWithComplexTypeTest)
{
    constexpr size_t SIZE = 2048;
    constexpr size_t MAGIC_CONSTANT_1 = std::numeric_limits<size_t>::max() / (SIZE + 2);
    srand(seed_);
    size_t MAGIC_CONSTANT_2 = rand() % SIZE;

    size_t i;

    ArenaAllocator aa(SpaceType::SPACE_TYPE_INTERNAL);
    ArenaDeque<ComplexClass> deq(aa.Adapter());

    // SIZE objects allocating
    for (size_t j = 0; j < SIZE; ++j) {
        deq.emplace_back(j * MAGIC_CONSTANT_1 + MAGIC_CONSTANT_2, std::to_string(j));
    }

    // Size checking
    ASSERT_EQ(SIZE, deq.size());

    // Allocations checking
    i = 0;
    for (auto it = deq.cbegin(); it != deq.cend(); ++it, ++i) {
        ASSERT_EQ(it->getValue(), i * MAGIC_CONSTANT_1 + MAGIC_CONSTANT_2) << "value of i: " << i;
        ASSERT_EQ(it->getString(), std::to_string(i)) << "value of i: " << i;
    }

    // Resizing and new elements assignment
    constexpr size_t SIZE_2 = SIZE << 1;
    deq.assign(SIZE_2, ComplexClass(1, "1"));

    // Size checking
    ASSERT_EQ(SIZE_2, deq.size());
    deq.shrink_to_fit();
    ASSERT_EQ(SIZE_2, deq.size());

    // Allocations and assignment checking
    i = SIZE_2 - 1;
    for (auto it = deq.crbegin(); it != deq.crend(); ++it, --i) {
        ASSERT_EQ(it->getValue(), 1U) << "value of i: " << i;
        ASSERT_EQ(it->getString(), "1") << "value of i: " << i;
    }

    // Size increasing
    constexpr size_t SIZE_4 = SIZE_2 << 1;
    deq.resize(SIZE_4, ComplexClass());

    // Size checking
    ASSERT_EQ(SIZE_4, deq.size());

    // Allocations checking
    auto it = deq.cbegin();
    for (size_t j = 0; j < SIZE_4 / 2; ++j, ++it) {
        ASSERT_EQ(it->getValue(), 1U) << "value of i: " << j;
        ASSERT_EQ(it->getString(), "1") << "value of i: " << j;
    }
    for (size_t j = SIZE_4 / 2; j < SIZE_4; ++j, ++it) {
        ASSERT_EQ(it->getValue(), 0U) << "value of i: " << j;
        ASSERT_EQ(it->getString(), "0") << "value of i: " << j;
    }

    // Size decreasing
    deq.resize(SIZE);

    // Size checking
    ASSERT_EQ(SIZE, deq.size());
    deq.shrink_to_fit();
    ASSERT_EQ(SIZE, deq.size());

    // Allocations checking
    i = 0;
    for (auto t_it = deq.cbegin(); t_it != deq.cend(); ++t_it, ++i) {
        ASSERT_EQ(t_it->getValue(), 1U) << "value of i: " << i;
        ASSERT_EQ(t_it->getString(), "1") << "value of i: " << i;
    }
}

TEST_F(ArenaAllocatorTest, LongRandomTest)
{
    constexpr size_t SIZE = 3250000;
    constexpr size_t HALF_SIZE = SIZE >> 1;
    constexpr size_t DOUBLE_SIZE = SIZE << 1;
    constexpr uint32_t MAX_VAL = MAX_VALUE<uint32_t>();

    ArenaAllocator aa(SpaceType::SPACE_TYPE_INTERNAL);
    ArenaDeque<uint32_t> st(aa.Adapter());
    size_t i = 0;

    srand(seed_);

    // Allocations
    for (size_t j = 0; j < SIZE; ++j) {
        st.push_back(rand() % MAX_VAL);
    }

    // Size checking
    ASSERT_EQ(st.size(), SIZE);

    // Allocations checking
    srand(seed_);
    i = 0;
    for (auto t_it = st.cbegin(); t_it != st.cend(); ++t_it, ++i) {
        ASSERT_EQ(*t_it, rand() % MAX_VAL) << "value of i: " << i;
    }

    // Decreasing size
    st.resize(HALF_SIZE);

    // Size chcking
    ASSERT_EQ(st.size(), HALF_SIZE);

    // Allocations checking
    srand(seed_);
    i = 0;
    for (auto t_it = st.cbegin(); t_it != st.cend(); ++t_it, ++i) {
        ASSERT_EQ(*t_it, rand() % MAX_VAL) << "value of i: " << i;
    }

    // Increasing size
    st.resize(DOUBLE_SIZE, rand() % MAX_VAL);

    // Allocations checking
    srand(seed_);
    auto it = st.cbegin();
    for (i = 0; i < HALF_SIZE; ++it, ++i) {
        ASSERT_EQ(*it, rand() % MAX_VAL) << "value of i: " << i;
    }
    for (uint32_t value = rand() % MAX_VAL; it != st.cend(); ++it, ++i) {
        ASSERT_EQ(*it, value) << "value of i: " << i;
    }

    // Change values
    srand(seed_ >> 1);
    for (auto t_it = st.begin(); t_it != st.end(); ++t_it) {
        *t_it = rand() % MAX_VAL;
    }

    // Changes checking
    srand(seed_ >> 1);
    i = 0;
    for (auto t_it = st.cbegin(); t_it != st.cend(); ++t_it, ++i) {
        ASSERT_EQ(*t_it, rand() % MAX_VAL) << "value of i: " << i;
    }
}

TEST_F(ArenaAllocatorTest, LogAlignmentSmallSizesTest)
{
    constexpr size_t MAX_SMALL_SIZE = 32;

    for (size_t size = 1; size < MAX_SMALL_SIZE; ++size) {
        ArenaAllocator aa(SpaceType::SPACE_TYPE_INTERNAL);

        for (Alignment align = LOG_ALIGN_MIN; align <= LOG_ALIGN_MAX;
             align = static_cast<Alignment>(static_cast<size_t>(align) + 1)) {
            void *ptr = aa.Alloc(size, align);
            size_t mask = GetAlignmentInBytes(align) - 1;

            ASSERT_NE(ptr, nullptr);
            ASSERT_EQ(reinterpret_cast<size_t>(ptr) & mask, 0U)
                << "alignment: " << align << "addr: " << reinterpret_cast<size_t>(ptr);
        }
    }
}

TEST_F(ArenaAllocatorTest, LogAlignmentBigSizeTest)
{
    constexpr size_t SIZE = 0.3_KB;
    ArenaAllocator aa(SpaceType::SPACE_TYPE_INTERNAL);

    for (Alignment align = LOG_ALIGN_MIN; align <= LOG_ALIGN_MAX;
         align = static_cast<Alignment>(static_cast<size_t>(align) + 1)) {
        void *ptr = aa.Alloc(SIZE, align);
        size_t mask = GetAlignmentInBytes(align) - 1;

        ASSERT_NE(ptr, nullptr);
        ASSERT_EQ(reinterpret_cast<size_t>(ptr) & mask, 0U)
            << "alignment: " << align << "addr: " << reinterpret_cast<size_t>(ptr);
    }
}

TEST_F(ArenaAllocatorTest, ArrayUINT16AlignmentTest)
{
    AllocateWithAlignment<uint16_t>();
}

TEST_F(ArenaAllocatorTest, ArrayUINT32AlignmentTest)
{
    AllocateWithAlignment<uint32_t>();
}

TEST_F(ArenaAllocatorTest, ArrayUINT64AlignmentTest)
{
    AllocateWithAlignment<uint64_t>();
}

TEST_F(ArenaAllocatorTest, ArrayUINT16WithDiffAlignmentTest)
{
    AllocateWithDiffAlignment<uint16_t>();
}

TEST_F(ArenaAllocatorTest, ArrayUINT32WithDiffAlignmentTest)
{
    AllocateWithDiffAlignment<uint32_t>();
}

TEST_F(ArenaAllocatorTest, ArrayUINT64WithDiffAlignmentTest)
{
    AllocateWithDiffAlignment<uint64_t>();
}

TEST_F(ArenaAllocatorTest, FunctionNewTest)
{
    ArenaAllocator aa(SpaceType::SPACE_TYPE_INTERNAL);
    std::array<ComplexClass *, ARRAY_SIZE> arr;

    // Allocations
    srand(seed_);
    for (size_t i = 0; i < ARRAY_SIZE; ++i) {
        arr[i] = aa.New<ComplexClass>(rand() % MAX_VALUE<size_t>());
    }

    // Allocations checking
    srand(seed_);
    for (size_t i = 0; i < ARRAY_SIZE; ++i) {
        ASSERT_NE(arr[i], nullptr);
        size_t random_value = rand() % MAX_VALUE<size_t>();
        ASSERT_EQ(arr[i]->getValue(), random_value);
        ASSERT_EQ(arr[i]->getString(), std::to_string(random_value));
    }

    // Change values
    srand(seed_ >> 1);
    for (size_t i = 0; i < ARRAY_SIZE; ++i) {
        arr[i]->setValue(rand() % MAX_VALUE<size_t>());
    }

    // Changes checking
    srand(seed_ >> 1);
    for (size_t i = 0; i < ARRAY_SIZE; ++i) {
        size_t random_value = rand() % MAX_VALUE<size_t>();
        ASSERT_EQ(arr[i]->getValue(), random_value);
        ASSERT_EQ(arr[i]->getString(), std::to_string(random_value));
    }
}

TEST_F(ArenaAllocatorTest, ResizeTest)
{
    ArenaAllocator aa(SpaceType::SPACE_TYPE_INTERNAL);
    static constexpr size_t alloc_count = 1000;
    static constexpr size_t init_val = 0xdeadbeef;
    void *tmp;
    size_t *first_var = aa.New<size_t>(init_val);
    {
        size_t init_size = aa.GetAllocatedSize();
        for (size_t i = 0; i < alloc_count; i++) {
            tmp = aa.Alloc(sizeof(size_t));
        }
        EXPECT_DEATH(aa.Resize(aa.GetAllocatedSize() + 1), "");
        aa.Resize(init_size);
        ASSERT_EQ(aa.GetAllocatedSize(), init_size);
    }
    ASSERT_EQ(*first_var, init_val);
}

TEST_F(ArenaAllocatorTest, ResizeWrapperTest)
{
    static constexpr size_t VECTOR_SIZE = 1000;
    ArenaAllocator aa(SpaceType::SPACE_TYPE_INTERNAL);
    size_t old_size = aa.GetAllocatedSize();
    {
        ArenaResizeWrapper<false> wrapper(&aa);
        ArenaVector<size_t> vector(aa.Adapter());
        for (size_t i = 0; i < VECTOR_SIZE; i++) {
            vector.push_back(i);
        }
    }
    ASSERT(old_size == aa.GetAllocatedSize());
}

}  // namespace panda
