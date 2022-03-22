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

#include "mem/pool_manager.h"
#include "utils/arena_containers.h"
#include "utils/small_vector.h"
#include <gtest/gtest.h>

namespace panda::test {

class SmallVectorTest : public ::testing::Test {
public:
    SmallVectorTest()
    {
        panda::mem::MemConfig::Initialize(0, 64_MB, 256_MB, 32_MB);
        PoolManager::Initialize();
        allocator_ = new ArenaAllocator(SpaceType::SPACE_TYPE_COMPILER);
    }

    virtual ~SmallVectorTest()
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

template <typename Vector>
void TestVectorGrow(Vector &vector)
{
    std::array values = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    ASSERT_EQ(vector.size(), 0);
    ASSERT_EQ(vector.capacity(), 4U);

    vector.push_back(values[0]);
    ASSERT_EQ(vector.size(), 1);
    ASSERT_EQ(vector.capacity(), 4U);
    ASSERT_TRUE(vector.IsStatic());

    vector.push_back(values[1]);
    vector.push_back(values[2U]);
    vector.push_back(values[3U]);
    ASSERT_EQ(vector.size(), 4U);
    ASSERT_EQ(vector.capacity(), 4U);
    ASSERT_TRUE(vector.IsStatic());

    vector.push_back(values[4U]);
    ASSERT_EQ(vector.size(), 5U);
    ASSERT_GE(vector.capacity(), 5U);
    ASSERT_FALSE(vector.IsStatic());

    ASSERT_TRUE(std::equal(values.begin(), values.begin() + 5U, vector.begin()));

    std::copy(values.begin() + 5U, values.end(), std::back_inserter(vector));
    ASSERT_EQ(vector.size(), 10U);
    ASSERT_FALSE(vector.IsStatic());
    for (size_t i = 0; i < values.size(); i++) {
        ASSERT_EQ(vector[i], values[i]);
    }
}

TEST_F(SmallVectorTest, Growing)
{
    {
        SmallVector<int, 4> vector;
        TestVectorGrow(vector);
    }
    {
        SmallVector<int, 4, ArenaAllocator> vector(GetAllocator());
        TestVectorGrow(vector);
    }
}

template <typename Vector>
void TestVectorIteration(Vector &vector)
{
    std::array values = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    ASSERT_EQ(vector.size(), 0);

    std::copy(values.begin(), values.begin() + 4U, std::back_inserter(vector));
    ASSERT_TRUE(vector.IsStatic());
    ASSERT_EQ(vector.size(), 4U);
    ASSERT_TRUE(std::equal(vector.begin(), vector.end(), values.begin()));

    {
        auto it = std::find(vector.begin(), vector.end(), 30U);
        ASSERT_NE(it, vector.end());
        ASSERT_EQ(*it, 30U);
        ASSERT_EQ(std::distance(vector.begin(), it), 2U);

        it = std::find(vector.begin(), vector.end(), 50U);
        ASSERT_EQ(it, vector.end());
    }

    {
        auto it = std::find(vector.rbegin(), vector.rend(), 30U);
        ASSERT_NE(it, vector.rend());
        ASSERT_EQ(*it, 30U);
        ASSERT_EQ(std::distance(vector.rbegin(), it), 1);

        it = std::find(vector.rbegin(), vector.rend(), 50U);
        ASSERT_EQ(it, vector.rend());
    }

    {
        const auto const_vector = vector;
        ASSERT_TRUE(std::equal(const_vector.begin(), const_vector.end(), values.begin()));
    }

    std::copy(values.begin() + 4U, values.end(), std::back_inserter(vector));
    ASSERT_EQ(vector.size(), 10U);
    ASSERT_FALSE(vector.IsStatic());
    ASSERT_TRUE(std::equal(vector.begin(), vector.end(), values.begin()));

    {
        auto it = std::find(vector.crbegin(), vector.crend(), 30U);
        ASSERT_NE(it, vector.crend());
        ASSERT_EQ(*it, 30U);
        ASSERT_EQ(std::distance(vector.crbegin(), it), 7U);

        it = std::find(vector.crbegin(), vector.crend(), 190U);
        ASSERT_EQ(it, vector.crend());
    }

    {
        auto it = vector.begin();
        ASSERT_EQ(*(it + 3U), vector[3U]);
        std::advance(it, 8U);
        ASSERT_EQ(*it, vector[8U]);
        it -= 3U;
        ASSERT_EQ(*it, vector[5U]);
        ASSERT_EQ(*(it - 3U), vector[2U]);
        it++;
        ASSERT_EQ(*(it - 3U), vector[3U]);
        --it;
        ASSERT_EQ(*(it - 3U), vector[2U]);
        it--;
        ASSERT_EQ(*(it - 3U), vector[1]);
    }
}

TEST_F(SmallVectorTest, Iteration)
{
    {
        SmallVector<int, 4> vector;
        TestVectorIteration(vector);
    }
    {
        SmallVector<int, 4, ArenaAllocator> vector(GetAllocator());
        TestVectorIteration(vector);
    }
}

struct Item {
    Item()
    {
        constructed++;
    }
    Item(int aa, double bb) : a(aa), b(bb)
    {
        constructed++;
    }
    virtual ~Item()
    {
        destroyed++;
    }
    DEFAULT_COPY_SEMANTIC(Item);
    DEFAULT_MOVE_SEMANTIC(Item);

    bool operator==(const Item &rhs) const
    {
        return a == rhs.a && b == rhs.b;
    }
    static void Reset()
    {
        constructed = 0;
        destroyed = 0;
    }
    int a {101};
    double b {202};
    static inline size_t constructed = 0;
    static inline size_t destroyed = 0;
};

TEST_F(SmallVectorTest, Emplace)
{
    SmallVector<Item, 1> vector;
    vector.emplace_back(1, 1.1);
    ASSERT_EQ(vector.size(), 1);
    ASSERT_EQ(vector[0], Item(1, 1.1));
    ASSERT_TRUE(vector.IsStatic());
    vector.emplace_back(2, 2.2);
    ASSERT_FALSE(vector.IsStatic());
    ASSERT_EQ(vector[1], Item(2, 2.2));
    vector.push_back(Item(3, 3.3));
    ASSERT_EQ(vector[2], Item(3, 3.3));
}

TEST_F(SmallVectorTest, ResizeStatic)
{
    SmallVector<Item, 4> vector;

    vector.push_back(Item(1, 1.2));
    ASSERT_EQ(vector[0], Item(1, 1.2));
    Item::Reset();
    vector.resize(3);
    ASSERT_EQ(Item::constructed, 2);
    ASSERT_EQ(vector.size(), 3);
    ASSERT_TRUE(vector.IsStatic());
    ASSERT_EQ(vector[0], Item(1, 1.2));
    ASSERT_EQ(vector[1], Item());
    ASSERT_EQ(vector[2], Item());

    Item::Reset();
    vector.resize(1);
    ASSERT_EQ(vector.size(), 1);
    ASSERT_EQ(Item::destroyed, 2);

    Item::Reset();
    vector.clear();
    ASSERT_EQ(Item::destroyed, 1);
    ASSERT_EQ(vector.size(), 0);
}

TEST_F(SmallVectorTest, ResizeDynamic)
{
    std::array values = {Item(1, 1.2), Item(2, 2.3), Item(3, 3.4)};
    SmallVector<Item, 2> vector;

    Item::Reset();
    vector.resize(6);
    ASSERT_EQ(Item::constructed, 6);
    ASSERT_FALSE(vector.IsStatic());
    ASSERT_EQ(vector.size(), 6);
    ASSERT_TRUE(std::all_of(vector.begin(), vector.end(), [](const auto &v) { return v == Item(); }));

    Item::Reset();
    vector.resize(3);
    ASSERT_EQ(vector.size(), 3);
    ASSERT_EQ(Item::destroyed, 3);
    ASSERT_FALSE(vector.IsStatic());

    Item::Reset();
    vector.clear();
    ASSERT_EQ(Item::destroyed, 3);
    ASSERT_EQ(vector.size(), 0);
    ASSERT_FALSE(vector.IsStatic());
}

TEST_F(SmallVectorTest, ResizeStaticWithValue)
{
    SmallVector<Item, 4> vector;

    vector.push_back(Item(1, 1.2));
    ASSERT_EQ(vector[0], Item(1, 1.2));
    Item::Reset();
    vector.resize(3, Item(3, 3.3));
    ASSERT_EQ(vector.size(), 3);
    ASSERT_TRUE(vector.IsStatic());
    ASSERT_EQ(vector[0], Item(1, 1.2));
    ASSERT_EQ(vector[1], Item(3, 3.3));
    ASSERT_EQ(vector[2], Item(3, 3.3));

    Item item(3, 3.3);
    Item::Reset();
    vector.resize(1, item);
    ASSERT_EQ(vector.size(), 1);
    ASSERT_EQ(Item::destroyed, 2);

    Item::Reset();
    vector.clear();
    ASSERT_EQ(Item::destroyed, 1);
    ASSERT_EQ(vector.size(), 0);
}

TEST_F(SmallVectorTest, ResizeDynamicWithValue)
{
    std::array values = {Item(1, 1.2), Item(2, 2.3), Item(3, 3.4)};
    SmallVector<Item, 2> vector;

    Item::Reset();
    vector.resize(6, Item(3, 3.3));
    ASSERT_FALSE(vector.IsStatic());
    ASSERT_EQ(vector.size(), 6);
    ASSERT_TRUE(std::all_of(vector.begin(), vector.end(), [](const auto &v) { return v == Item(3, 3.3); }));

    Item item(3, 3.3);
    Item::Reset();
    vector.resize(3, item);
    ASSERT_EQ(vector.size(), 3);
    ASSERT_EQ(Item::destroyed, 3);
    ASSERT_FALSE(vector.IsStatic());

    Item::Reset();
    vector.clear();
    ASSERT_EQ(Item::destroyed, 3);
    ASSERT_EQ(vector.size(), 0);
    ASSERT_FALSE(vector.IsStatic());
}

TEST_F(SmallVectorTest, Constructing)
{
    std::array values = {0, 1, 2, 3, 4, 5, 6, 7};

    // Assign from static vector to dynamic
    {
        SmallVector<int, 2> vector1;
        SmallVector<int, 2> vector2;
        std::copy(values.begin(), values.end(), std::back_inserter(vector1));
        vector2.push_back(values[0]);
        vector2.push_back(values[1]);

        vector1 = vector2;
        ASSERT_EQ(vector1.size(), 2);
        ASSERT_TRUE(vector1.IsStatic());
        ASSERT_TRUE(std::equal(vector1.begin(), vector1.end(), vector2.begin()));
        vector1.push_back(values[2]);
        ASSERT_FALSE(vector1.IsStatic());
    }
    // Assign from dynamic vector to static
    {
        SmallVector<int, 2> vector1;
        SmallVector<int, 2> vector2;
        std::copy(values.begin(), values.end(), std::back_inserter(vector2));
        vector1.push_back(values[0]);
        vector1.push_back(values[1]);

        vector1 = vector2;
        ASSERT_EQ(vector1.size(), values.size());
        ASSERT_FALSE(vector1.IsStatic());
        ASSERT_TRUE(std::equal(vector1.begin(), vector1.end(), vector2.begin()));
    }

    // Move assign from static vector to dynamic
    {
        SmallVector<int, 2> vector1;
        SmallVector<int, 2> vector2;
        std::copy(values.begin(), values.end(), std::back_inserter(vector1));
        vector2.push_back(values[0]);
        vector2.push_back(values[1]);

        vector1 = std::move(vector2);
        ASSERT_EQ(vector1.size(), 2);
        ASSERT_EQ(vector2.size(), 0);
        ASSERT_TRUE(vector2.IsStatic());
        ASSERT_TRUE(vector1.IsStatic());
        ASSERT_TRUE(std::equal(vector1.begin(), vector1.end(), values.begin()));
    }
    // Move assign from dynamic vector to static
    {
        SmallVector<int, 2> vector1;
        SmallVector<int, 2> vector2;
        std::copy(values.begin(), values.end(), std::back_inserter(vector2));
        vector1.push_back(values[0]);
        vector1.push_back(values[1]);

        vector1 = std::move(vector2);
        ASSERT_EQ(vector1.size(), values.size());
        ASSERT_EQ(vector2.size(), 0);
        ASSERT_TRUE(vector2.IsStatic());
        ASSERT_FALSE(vector1.IsStatic());
        ASSERT_TRUE(std::equal(vector1.begin(), vector1.end(), values.begin()));
    }

    // Copy constructor from dynamic
    {
        SmallVector<int, 2> vector1;
        std::copy(values.begin(), values.end(), std::back_inserter(vector1));
        ASSERT_FALSE(vector1.IsStatic());
        ASSERT_EQ(vector1.size(), values.size());
        SmallVector<int, 2> vector2(vector1);
        ASSERT_EQ(vector1.size(), values.size());
        ASSERT_EQ(vector2.size(), values.size());
        ASSERT_TRUE(std::equal(vector2.begin(), vector2.end(), vector1.begin()));
    }
    // Copy constructor from static
    {
        SmallVector<int, 2> vector1;
        std::copy(values.begin(), values.begin() + 2, std::back_inserter(vector1));
        ASSERT_TRUE(vector1.IsStatic());
        SmallVector<int, 2> vector2(vector1);
        ASSERT_EQ(vector1.size(), 2);
        ASSERT_EQ(vector2.size(), 2);
        ASSERT_TRUE(std::equal(vector2.begin(), vector2.end(), vector1.begin()));
    }

    // Move constructor from dynamic
    {
        SmallVector<int, 2> vector1;
        std::copy(values.begin(), values.end(), std::back_inserter(vector1));
        ASSERT_FALSE(vector1.IsStatic());
        ASSERT_EQ(vector1.size(), values.size());
        SmallVector<int, 2> vector2(std::move(vector1));
        ASSERT_EQ(vector1.size(), 0);
        ASSERT_EQ(vector2.size(), values.size());
        ASSERT_TRUE(std::equal(vector2.begin(), vector2.end(), values.begin()));
    }
    // Move constructor from static
    {
        SmallVector<int, 2> vector1;
        std::copy(values.begin(), values.begin() + 2, std::back_inserter(vector1));
        ASSERT_TRUE(vector1.IsStatic());
        SmallVector<int, 2> vector2(std::move(vector1));
        ASSERT_EQ(vector1.size(), 0);
        ASSERT_EQ(vector2.size(), 2);
        ASSERT_TRUE(std::equal(vector2.begin(), vector2.end(), values.begin()));
    }
}

}  // namespace panda::test
