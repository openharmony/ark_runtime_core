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

#include "utils/bit_vector.h"
#include "mem/pool_manager.h"
#include "utils/arena_containers.h"

#include <gtest/gtest.h>

namespace panda::test {

class BitVectorTest : public ::testing::Test {
public:
    BitVectorTest()
    {
        panda::mem::MemConfig::Initialize(0, 64_MB, 256_MB, 32_MB);
        PoolManager::Initialize();
        allocator_ = new ArenaAllocator(SpaceType::SPACE_TYPE_COMPILER);
    }

    virtual ~BitVectorTest()
    {
        delete allocator_;
        PoolManager::Finalize();
        panda::mem::MemConfig::Finalize();
    }

    ArenaAllocator *GetAllocator() const
    {
        return allocator_;
    }

private:
    ArenaAllocator *allocator_ {nullptr};
};

TEST_F(BitVectorTest, Basics)
{
    BitVector<> vector;
    const BitVector<> &cvector = vector;
    ASSERT_EQ(vector.capacity(), 0);

    // Index iterators for empty vector
    for (uint32_t i : cvector.GetSetBitsIndices()) {
        ADD_FAILURE();
    }
    for (uint32_t i : cvector.GetZeroBitsIndices()) {
        ADD_FAILURE();
    }

    vector.push_back(true);
    vector.push_back(false);
    ASSERT_NE(vector.capacity(), 0);

    // Check `GetDataSpan`
    ASSERT_NE(vector.GetDataSpan().size(), 0);
    uint32_t value = 1;
    ASSERT_EQ(std::memcmp(vector.GetDataSpan().data(), &value, 1), 0);

    // Constant operator[]
    ASSERT_EQ(cvector[0], vector[0]);

    // Constant versions of begin and end
    ASSERT_EQ(cvector.begin(), vector.begin());
    ASSERT_EQ(cvector.end(), vector.end());

    vector.resize(20);
    std::fill(vector.begin(), vector.end(), false);
    ASSERT_EQ(vector.PopCount(), 0);
    std::fill(vector.begin() + 2, vector.begin() + 15, true);
    ASSERT_EQ(vector.PopCount(), 13);
    for (size_t i = 0; i < 15; i++) {
        if (i > 2) {
            ASSERT_EQ(vector.PopCount(i), i - 2);
        } else {
            ASSERT_EQ(vector.PopCount(i), 0);
        }
    }
    ASSERT_EQ(vector.GetHighestBitSet(), 14);
    ASSERT_EQ(vector[0], false);
    ASSERT_EQ(vector[1], false);
    ASSERT_EQ(vector[2], true);

    ASSERT_EQ(vector, vector.GetFixed());
    ASSERT_FALSE(vector.GetContainerDataSpan().empty());
}

TEST_F(BitVectorTest, Comparison)
{
    std::vector<bool> values = {false, true, false, true, false, true, false, true, false, true};
    BitVector<> vec1;
    std::copy(values.begin(), values.end(), std::back_inserter(vec1));
    BitVector<ArenaAllocator> vec2(GetAllocator());
    std::copy(values.begin(), values.end(), std::back_inserter(vec2));
    ASSERT_EQ(vec1, vec2);
    vec2[0] = true;
    ASSERT_NE(vec1, vec2);
}

template <typename T>
void TestIteration(T &vector, size_t bits)
{
    ASSERT_FALSE(vector.empty());
    ASSERT_EQ(vector.size(), bits);

    std::fill(vector.begin(), vector.end(), true);
    for (uint32_t i : vector.GetZeroBitsIndices()) {
        ADD_FAILURE();
    }
    int index = 0;
    for (uint32_t i : vector.GetSetBitsIndices()) {
        ASSERT_EQ(i, index++);
    }

    std::fill(vector.begin(), vector.end(), false);
    for (uint32_t i : vector.GetSetBitsIndices()) {
        ADD_FAILURE();
    }
    index = 0;
    for (uint32_t i : vector.GetZeroBitsIndices()) {
        ASSERT_EQ(i, index++);
    }

    index = 0;
    for (auto v : vector) {
        v = (index++ % 2U) != 0;
    }
    index = 0;
    for (auto v : vector) {
        ASSERT_EQ(v, index++ % 2U);
    }
    index = vector.size() - 1;
    for (auto it = vector.end() - 1;; --it) {
        ASSERT_EQ(*it, index-- % 2U);
        if (it == vector.begin()) {
            break;
        }
    }
    index = 1;
    for (uint32_t i : vector.GetSetBitsIndices()) {
        ASSERT_EQ(i, index);
        index += 2U;
    }
    index = 0;
    for (uint32_t i : vector.GetZeroBitsIndices()) {
        ASSERT_EQ(i, index);
        index += 2U;
    }

    auto it = vector.begin();
    ASSERT_EQ(*it, false);
    ++it;
    ASSERT_EQ(*it, true);
    auto it1 = it++;
    ASSERT_EQ(*it, false);
    ASSERT_EQ(*it1, true);
    ASSERT_TRUE(it1 < it);
    it += 3U;
    ASSERT_EQ(*it, true);
    it -= 5U;
    ASSERT_EQ(*it, false);
    ASSERT_EQ(it, vector.begin());

    it = it + 6U;
    ASSERT_EQ(*it, false);
    ASSERT_EQ(std::distance(vector.begin(), it), 6U);
    ASSERT_EQ(it[1], true);
    it = it - 3U;
    ASSERT_EQ(*it, true);
    ASSERT_EQ(std::distance(vector.begin(), it), 3U);
    --it;
    ASSERT_EQ(*it, false);
    it1 = it--;
    ASSERT_EQ(*it, true);
    ASSERT_EQ(*it1, false);
    ASSERT_TRUE(it1 > it);
    it = vector.begin() + 100U;
    ASSERT_EQ(std::distance(vector.begin(), it), 100U);
    ASSERT_TRUE(it + 2U > it);
    ASSERT_TRUE(it + 2U >= it);
    ASSERT_TRUE(it + 0 >= it);
    ASSERT_TRUE(it - 2U < it);
    ASSERT_TRUE(it - 2U <= it);

    auto cit = vector.cbegin();
    ASSERT_EQ(cit, vector.begin());
    ASSERT_EQ(++cit, ++vector.begin());
    ASSERT_EQ(vector.cend(), vector.end());
}

TEST_F(BitVectorTest, Iteration)
{
    std::array<uint32_t, 10U> data {};
    size_t bits_num = data.size() * BitsNumInValue(data[0]);

    BitVector<> vec1;
    vec1.resize(bits_num);
    TestIteration(vec1, bits_num);

    BitVector<ArenaAllocator> vec2(GetAllocator());
    vec2.resize(bits_num);
    TestIteration(vec2, bits_num);

    BitVector<ArenaAllocator> vec3(bits_num, GetAllocator());
    TestIteration(vec3, bits_num);

    BitVectorSpan vec4(Span<uint32_t>(data.data(), data.size()));
    TestIteration(vec4, bits_num);

    data.fill(0);
    BitVectorSpan vec5(data.data(), bits_num);
    TestIteration(vec5, bits_num);
}

template <typename T>
void TestModification(T &vector)
{
    std::vector<bool> values = {false, true, false, true, false, true, false, true, false, true};
    ASSERT_TRUE(vector.empty());
    ASSERT_EQ(vector.size(), 0);
    ASSERT_EQ(vector.PopCount(), 0);
    ASSERT_EQ(vector.GetHighestBitSet(), -1);

    vector.push_back(true);
    ASSERT_FALSE(vector.empty());
    ASSERT_EQ(vector.size(), 1);
    ASSERT_EQ(vector.PopCount(), 1);
    ASSERT_EQ(vector.GetHighestBitSet(), 0);

    std::copy(values.begin(), values.end(), std::back_inserter(vector));
    ASSERT_EQ(vector.size(), 11U);
    ASSERT_EQ(vector[1], false);
    ASSERT_EQ(vector.PopCount(), 6U);
    ASSERT_EQ(vector.GetHighestBitSet(), 10U);

    vector[1] = true;
    ASSERT_EQ(vector[1], true);

    uint32_t value = 0b10101010111;
    ASSERT_EQ(std::memcmp(vector.data(), &value, vector.GetSizeInBytes()), 0);

    vector.resize(3U);
    ASSERT_EQ(vector.size(), 3U);
    ASSERT_EQ(vector.PopCount(), 3U);

    vector.resize(10U);
    ASSERT_EQ(vector.PopCount(), 3U);

    vector.clear();
    ASSERT_TRUE(vector.empty());
    ASSERT_EQ(vector.size(), 0);

    // Push 1000 values with `true` in odd and `false` in even indexes
    for (int i = 0; i < 100; i++) {
        std::copy(values.begin(), values.end(), std::back_inserter(vector));
    }
    ASSERT_EQ(vector.size(), 1000U);
    ASSERT_EQ(vector.PopCount(), 500U);
    for (size_t i = 0; i < 1000U; i++) {
        vector.push_back(false);
    }
    ASSERT_EQ(vector.size(), 2000U);
    ASSERT_EQ(vector.PopCount(), 500U);
    ASSERT_EQ(vector.GetHighestBitSet(), 999U);

    vector.ClearBit(3000U);
    ASSERT_EQ(vector.size(), 3001U);
    ASSERT_EQ(vector.PopCount(), 500U);
    ASSERT_EQ(vector.GetHighestBitSet(), 999U);

    vector.SetBit(4000U);
    ASSERT_EQ(vector.size(), 4001U);
    ASSERT_EQ(vector.PopCount(), 501U);
    ASSERT_EQ(vector.GetHighestBitSet(), 4000U);
}

TEST_F(BitVectorTest, Modification)
{
    BitVector<> vec1;
    TestModification(vec1);
    BitVector<ArenaAllocator> vec2(GetAllocator());
    TestModification(vec2);
}

TEST_F(BitVectorTest, SetClearBit)
{
    BitVector<> vector;

    vector.SetBit(55);
    ASSERT_EQ(vector.size(), 56);

    vector.SetBit(45);
    ASSERT_EQ(vector.size(), 56);
    ASSERT_EQ(vector.PopCount(), 2);

    vector.ClearBit(105);
    ASSERT_EQ(vector.size(), 106);
    ASSERT_EQ(vector.PopCount(), 2);
    ASSERT_EQ(vector.GetHighestBitSet(), 55);

    vector.ClearBit(45);
    ASSERT_EQ(vector.size(), 106);
    ASSERT_EQ(vector.PopCount(), 1);
}

}  // namespace panda::test
