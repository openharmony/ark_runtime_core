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

#include "utils/bit_table.h"
#include "mem/pool_manager.h"

#include <gtest/gtest.h>

namespace panda::test {

class BitTableTest : public ::testing::Test {
public:
    BitTableTest()
    {
        panda::mem::MemConfig::Initialize(0, 64_MB, 256_MB, 32_MB);
        PoolManager::Initialize();
        allocator_ = new ArenaAllocator(SpaceType::SPACE_TYPE_COMPILER);
    }

    virtual ~BitTableTest()
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

template <typename C, size_t N>
typename C::Entry CreateEntry(std::array<uint32_t, N> data)
{
    typename C::Entry entry;
    static_assert(N == C::NUM_COLUMNS);
    for (size_t i = 0; i < N; i++) {
        entry[i] = data[i];
    }
    return entry;
}

TEST_F(BitTableTest, EmptyTable)
{
    ArenaVector<uint8_t> data(GetAllocator()->Adapter());
    data.reserve(1_KB);

    BitTableBuilder<BitTableDefault<1>> builder(GetAllocator());
    using Builder = decltype(builder);
    BitMemoryStreamOut out(&data);
    builder.Encode(out);

    BitMemoryStreamIn in(data.data(), 0, data.size() * BITS_PER_BYTE);
    BitTable<BitTableDefault<1>> table;
    table.Decode(&in);

    ASSERT_EQ(table.GetRowsCount(), 0);
    ASSERT_EQ(table.begin(), table.end());
}

TEST_F(BitTableTest, SingleNoValue)
{
    ArenaVector<uint8_t> data(GetAllocator()->Adapter());
    data.reserve(1_KB);

    BitTableBuilder<BitTableDefault<1>> builder(GetAllocator());
    using Builder = decltype(builder);
    builder.Emplace(Builder::Entry({Builder::NO_VALUE}));
    BitMemoryStreamOut out(&data);
    builder.Encode(out);

    BitMemoryStreamIn in(data.data(), 0, data.size() * BITS_PER_BYTE);
    BitTable<BitTableDefault<1>> table;
    table.Decode(&in);

    ASSERT_EQ(table.GetRowsCount(), 1);
    ASSERT_FALSE(table.GetRow(0).Has(0));
    ASSERT_EQ(table.GetRow(0).Get(0), Builder::NO_VALUE);
    ASSERT_TRUE(
        std::all_of(table.begin(), table.end(), [](const auto &row) { return row.Get(0) == Builder::NO_VALUE; }));
}

TEST_F(BitTableTest, SingleColumn)
{
    ArenaVector<uint8_t> data(GetAllocator()->Adapter());
    data.reserve(1_KB);

    BitTableBuilder<BitTableDefault<1>> builder(GetAllocator());
    using Builder = decltype(builder);
    builder.Emplace(Builder::Entry({9}));
    builder.Emplace(Builder::Entry({Builder::NO_VALUE}));
    builder.Emplace(Builder::Entry({19}));
    builder.Emplace(Builder::Entry({29}));

    BitMemoryStreamOut out(&data);
    builder.Encode(out);

    BitMemoryStreamIn in(data.data(), 0, data.size() * BITS_PER_BYTE);
    BitTable<BitTableDefault<1>> table;
    table.Decode(&in);

    ASSERT_EQ(table.GetRowsCount(), 4);
    ASSERT_EQ(table.GetRow(0).Get(0), 9);
    ASSERT_FALSE(table.GetRow(1).Has(0));
    ASSERT_EQ(table.GetRow(2).Get(0), 19);
    ASSERT_EQ(table.GetRow(3).Get(0), 29);
}

TEST_F(BitTableTest, MultipleColumns)
{
    ArenaVector<uint8_t> data(GetAllocator()->Adapter());
    data.reserve(1_KB);

    std::array<std::array<uint32_t, 10>, 5> values = {
        {{0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U},
         {10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U},
         {0_KB, 1_KB + 1, 1_KB + 2, 1_KB + 3, 1_KB + 4, 1_KB + 5, 1_KB + 6, 1_KB + 7, 1_KB + 8, 1_KB + 9},
         {0_MB, 0_MB + 1, 1_MB + 2, 1_MB + 3, 1_MB + 4, 1_MB + 5, 1_MB + 6, 1_MB + 7, 1_MB + 8, 1_MB + 9},
         {0_GB, 0_GB + 1, 0_GB + 2, 1_GB + 3, 1_GB + 4, 1_GB + 5, 1_GB + 6, 1_GB + 7, 1_GB + 8, 1_GB + 9}}};

    BitTableBuilder<BitTableDefault<10>> builder(GetAllocator());
    builder.Emplace(CreateEntry<decltype(builder)>(values[0]));
    builder.Emplace(CreateEntry<decltype(builder)>(values[1]));
    builder.Emplace(CreateEntry<decltype(builder)>(values[2]));
    builder.Emplace(CreateEntry<decltype(builder)>(values[3]));
    builder.Emplace(CreateEntry<decltype(builder)>(values[4]));

    BitMemoryStreamOut out(&data);
    builder.Encode(out);

    BitMemoryStreamIn in(data.data(), 0, data.size() * BITS_PER_BYTE);
    BitTable<BitTableDefault<10>> table;
    table.Decode(&in);

    ASSERT_EQ(table.GetRowsCount(), 5);

    size_t row_index = 0;
    for (auto row : table) {
        for (size_t i = 0; i < row.ColumnsCount(); i++) {
            ASSERT_EQ(row.Get(i), values[row_index][i]);
        }
        row_index++;
    }
}

class TestAccessor : public BitTableRow<2U, TestAccessor> {
public:
    using Base = BitTableRow<2U, TestAccessor>;
    using Base::Base;

    static_assert(Base::NUM_COLUMNS == 2U);

    uint32_t GetField0() const
    {
        return Base::Get(0);
    }
    uint32_t GetField1() const
    {
        return Base::Get(1);
    }
    const char *GetName(size_t index) const
    {
        ASSERT(index < Base::ColumnsCount());
        static constexpr const char *names[] = {"field0", "field1"};
        return names[index];
    }

    enum { FIELD0, FIELD1 };
};

TEST_F(BitTableTest, CustomAccessor)
{
    ArenaVector<uint8_t> data(GetAllocator()->Adapter());
    data.reserve(1_KB);

    using Builder = BitTableBuilder<TestAccessor>;

    Builder builder(GetAllocator());
    {
        Builder::Entry entry;
        entry[TestAccessor::FIELD0] = 1;
        entry[TestAccessor::FIELD1] = 2;
        builder.Emplace(entry);
    }
    {
        Builder::Entry entry;
        entry[TestAccessor::FIELD0] = 3;
        entry[TestAccessor::FIELD1] = 4;
        builder.Emplace(entry);
    }

    BitMemoryStreamOut out(&data);
    builder.Encode(out);

    BitMemoryStreamIn in(data.data(), 0, data.size() * BITS_PER_BYTE);
    BitTable<TestAccessor> table;
    table.Decode(&in);

    ASSERT_EQ(table.GetRowsCount(), 2);
    ASSERT_EQ(table.GetRow(0).GetField0(), 1);
    ASSERT_EQ(table.GetRow(0).GetField1(), 2);
    ASSERT_EQ(table.GetRow(1).GetField0(), 3);
    ASSERT_EQ(table.GetRow(1).GetField1(), 4);

    int idx = 1;
    for (auto &row : table) {
        for (size_t i = 0; i < row.ColumnsCount(); i++) {
            ASSERT_EQ(row.Get(i), idx++);
        }
    }
}

TEST_F(BitTableTest, Ranges)
{
    ArenaVector<uint8_t> data(GetAllocator()->Adapter());
    data.reserve(1_KB);

    std::array<std::array<uint32_t, 2>, 10> values = {
        {{0U, 10U}, {1U, 11U}, {2U, 12U}, {3U, 13U}, {4U, 14U}, {5U, 15U}, {6U, 16U}, {7U, 17U}, {8U, 18U}, {9U, 19U}}};

    BitTableBuilder<TestAccessor> builder(GetAllocator());
    for (auto &v : values) {
        builder.Emplace(CreateEntry<decltype(builder)>(v));
    }

    BitMemoryStreamOut out(&data);
    builder.Encode(out);

    BitMemoryStreamIn in(data.data(), 0, data.size() * BITS_PER_BYTE);
    BitTable<TestAccessor> table;
    table.Decode(&in);

    ASSERT_EQ(table.GetRowsCount(), 10);
    ASSERT_EQ(table.GetColumnsCount(), 2);

    {
        auto range = table.GetRange(0, 6);
        auto it = range.begin();
        ASSERT_EQ(range[0].GetField0(), values[0][0]);
        ASSERT_EQ(range[0].GetField1(), values[0][1]);
        ASSERT_EQ(range[1].GetField0(), values[1][0]);
        ASSERT_EQ(range[1].GetField1(), values[1][1]);
        ASSERT_EQ(range[2].GetField0(), values[2][0]);
        ASSERT_EQ(range[2].GetField1(), values[2][1]);
        ASSERT_EQ(range[3].GetField0(), values[3][0]);
        ASSERT_EQ(range[3].GetField1(), values[3][1]);
        ASSERT_EQ(range[4].GetField0(), values[4][0]);
        ASSERT_EQ(range[4].GetField1(), values[4][1]);
        ASSERT_EQ(range[5].GetField0(), values[5][0]);
        ASSERT_EQ(range[5].GetField1(), values[5][1]);

        size_t i = 0;
        for (auto &v : table.GetRange(0, 6)) {
            ASSERT_EQ(v.GetField0(), values[i][0]);
            ASSERT_EQ(v.GetField1(), values[i][1]);
            i++;
        }
        ASSERT_EQ(i, 6);

        i = 0;
        for (auto &v : table) {
            ASSERT_EQ(v.GetField0(), values[i][0]);
            ASSERT_EQ(v.GetField1(), values[i][1]);
            i++;
        }
        ASSERT_EQ(i, 10);
    }

    {
        auto range = table.GetRangeReversed(4, 10);
        auto it = range.begin();
        ASSERT_EQ(range[0].GetField0(), values[9][0]);
        ASSERT_EQ(range[0].GetField1(), values[9][1]);
        ASSERT_EQ(range[1].GetField0(), values[8][0]);
        ASSERT_EQ(range[1].GetField1(), values[8][1]);
        ASSERT_EQ(range[2].GetField0(), values[7][0]);
        ASSERT_EQ(range[2].GetField1(), values[7][1]);
        ASSERT_EQ(range[3].GetField0(), values[6][0]);
        ASSERT_EQ(range[3].GetField1(), values[6][1]);
        ASSERT_EQ(range[4].GetField0(), values[5][0]);
        ASSERT_EQ(range[4].GetField1(), values[5][1]);

        int i = 10;
        for (auto &v : table.GetRangeReversed(4, 10)) {
            ASSERT_EQ(v.GetField0(), values[i - 1][0]);
            ASSERT_EQ(v.GetField1(), values[i - 1][1]);
            i--;
        }
        ASSERT_EQ(i, 4);

        i = 10;
        for (auto &v : table.GetRangeReversed()) {
            ASSERT_EQ(v.GetField0(), values[i - 1][0]);
            ASSERT_EQ(v.GetField1(), values[i - 1][1]);
            i--;
        }
        ASSERT_EQ(i, 0);
    }

    {
        auto range = table.GetRange(0, 0);
        ASSERT_EQ(range.begin(), range.end());

        size_t i = 0;
        for (auto &v : range) {
            i++;
        }
        ASSERT_EQ(i, 0);
    }

    {
        auto range = table.GetRangeReversed(0, 0);
        ASSERT_EQ(range.begin(), range.end());

        size_t i = 0;
        for (auto &v : range) {
            i++;
        }
        ASSERT_EQ(i, 0);
    }
}

TEST_F(BitTableTest, Deduplication)
{
    ArenaVector<uint8_t> data(GetAllocator()->Adapter());
    data.reserve(1_KB);

    using Builder = BitTableBuilder<TestAccessor>;
    using Entry = Builder::Entry;

    Builder builder(GetAllocator());

    std::array<Entry, 3> values = {Entry {1}, Entry {2}, Entry {3}};

    ASSERT_EQ(0U, builder.Add(values[0]));
    ASSERT_EQ(1U, builder.Add(values[1]));
    ASSERT_EQ(0U, builder.Add(values[0]));
    ASSERT_EQ(2U, builder.Add(values[2]));
    ASSERT_EQ(1U, builder.Add(values[1]));
    ASSERT_EQ(2U, builder.Add(values[2]));

    ASSERT_EQ(3U, builder.AddArray(Span<Entry>(values.begin(), 2)));
    ASSERT_EQ(1U, builder.AddArray(Span<Entry>(values.begin() + 1, 1)));
    ASSERT_EQ(5U, builder.AddArray(Span<Entry>(values.begin() + 1, 2)));
    ASSERT_EQ(3U, builder.AddArray(Span<Entry>(values.begin(), 2)));
    ASSERT_EQ(5U, builder.AddArray(Span<Entry>(values.begin() + 1, 2)));
}

TEST_F(BitTableTest, Bitmap)
{
    uint64_t pattern = 0xBADDCAFEDEADF00DULL;

    BitmapTableBuilder builder(GetAllocator());

    ArenaVector<std::pair<uint32_t, uint64_t>> values(GetAllocator()->Adapter());
    for (size_t i = 0; i <= 64; i++) {
        uint64_t mask = (i == 64) ? std::numeric_limits<uint64_t>::max() : ((1ULL << i) - 1);
        uint64_t value = pattern & mask;
        BitVector<ArenaAllocator> vec(MinimumBitsToStore(value), GetAllocator());
        vec.Reset();
        for (size_t j = 0; j < i; j++) {
            if ((value & (1ULL << j)) != 0) {
                vec.SetBit(j);
            }
        }
        auto fixed_vec = vec.GetFixed();
        values.push_back({builder.Add(fixed_vec), value});
    }

    // Zero value also occupies row in the table
    ASSERT_EQ(Popcount(pattern), builder.GetRowsCount());

    ArenaVector<uint8_t> data(GetAllocator()->Adapter());
    data.reserve(10_KB);
    BitMemoryStreamOut out(&data);
    builder.Encode(out);

    BitMemoryStreamIn in(data.data(), 0, data.size() * BITS_PER_BYTE);
    BitTable<BitTableDefault<1>> table;
    table.Decode(&in);

    ASSERT_EQ(table.GetRowSizeInBits(), MinimumBitsToStore(pattern));

    for (auto &[index, value] : values) {
        if (index != BitTableDefault<1>::NO_VALUE) {
            ASSERT_EQ(table.GetBitMemoryRegion(index).Read<uint64_t>(0, table.GetRowSizeInBits()), value);
        }
    }
}

template <typename T>
void FillVector(T &vector, uint32_t value)
{
    auto sp = vector.GetDataSpan();
    std::fill(sp.begin(), sp.end(), value);
}

TEST_F(BitTableTest, BitmapDeduplication)
{
    BitmapTableBuilder builder(GetAllocator());
    std::array<uint32_t, 128> buff;
    std::array fixed_vectors = {ArenaBitVectorSpan(&buff[0], 23), ArenaBitVectorSpan(&buff[1], 48),
                                ArenaBitVectorSpan(&buff[3], 0), ArenaBitVectorSpan(&buff[4], 123),
                                ArenaBitVectorSpan(&buff[8], 48)};
    std::array vectors = {ArenaBitVector(GetAllocator()), ArenaBitVector(GetAllocator()),
                          ArenaBitVector(GetAllocator()), ArenaBitVector(GetAllocator()),
                          ArenaBitVector(GetAllocator())};
    FillVector(fixed_vectors[0], 0x23232323);
    FillVector(fixed_vectors[1], 0x48484848);
    FillVector(fixed_vectors[2], 0);
    FillVector(fixed_vectors[3], 0x23123123);
    FillVector(fixed_vectors[4], 0x48484848);
    ASSERT_EQ(fixed_vectors[1], fixed_vectors[4]);
    vectors[0].resize(1);
    vectors[1].resize(23);
    vectors[2].resize(123);
    vectors[3].resize(234);
    vectors[4].resize(0);
    FillVector(vectors[0], 0x1);
    FillVector(vectors[1], 0x11111111);
    FillVector(vectors[2], 0x23123123);
    FillVector(vectors[3], 0x34234234);
    ASSERT_EQ(builder.Add(fixed_vectors[0].GetFixed()), 0);
    ASSERT_EQ(builder.Add(fixed_vectors[1].GetFixed()), 1);
    ASSERT_EQ(builder.Add(fixed_vectors[2].GetFixed()), BitTableDefault<1>::NO_VALUE);
    ASSERT_EQ(builder.Add(fixed_vectors[3].GetFixed()), 2);
    ASSERT_EQ(builder.Add(fixed_vectors[4].GetFixed()), 1);
    ASSERT_EQ(builder.Add(vectors[0].GetFixed()), 3);
    ASSERT_EQ(builder.Add(vectors[1].GetFixed()), 4);
    ASSERT_EQ(builder.Add(vectors[2].GetFixed()), 2);
    ASSERT_EQ(builder.Add(vectors[3].GetFixed()), 5);
    ASSERT_EQ(builder.Add(vectors[4].GetFixed()), BitTableDefault<1>::NO_VALUE);

    ArenaVector<uint8_t> data(GetAllocator()->Adapter());
    data.reserve(10_KB);
    BitMemoryStreamOut out(&data);
    builder.Encode(out);

    BitMemoryStreamIn in(data.data(), 0, data.size() * BITS_PER_BYTE);
    BitTable<BitTableDefault<1>> table;
    table.Decode(&in);

    ASSERT_EQ(table.GetRowsCount(), 6);
    ASSERT_EQ(table.GetRowSizeInBits(), 234);
}

}  // namespace panda::test
