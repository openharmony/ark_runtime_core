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

#include "utils/bit_memory_region-inl.h"

#include <gtest/gtest.h>
#include <array>

namespace panda::test {

static void CompareData(const uint8_t *data, size_t offset, size_t length, uint32_t value, uint8_t fill_value)
{
    for (size_t i = 0; i < length; i++) {
        uint8_t expected = (offset <= i && i < offset + length) ? value >> (i - offset) : fill_value;
        uint8_t actual = data[i / BITS_PER_BYTE] >> (i % BITS_PER_BYTE);
        ASSERT_EQ(expected & 1, actual & 1);
    }
}

TEST(BitMemoryRegion, TestBitAccess)
{
    std::array<uint8_t, 16> data;
    static constexpr std::array<uint8_t, 2> fill_data = {0x00, 0xff};
    static constexpr std::array<bool, 2> value_data = {false, true};
    static constexpr size_t MAX_BITS_COUNT = (data.size() - sizeof(uint32_t)) * BITS_PER_BYTE;

    for (size_t offset = 0; offset < MAX_BITS_COUNT; offset++) {
        uint32_t mask = 0;
        for (auto fill_value : fill_data) {
            for (auto value : value_data) {
                std::fill(data.begin(), data.end(), fill_value);
                BitMemoryRegion region1(data.data(), offset, 1);
                region1.Write(value, 0);
                ASSERT_EQ(region1.Read(0), value);
                CompareData(data.data(), offset, 1, value, fill_value);
                std::fill(data.begin(), data.end(), fill_value);
                BitMemoryRegion region2(data.data(), data.size() * BITS_PER_BYTE);
                region2.Write(value, offset);
                ASSERT_EQ(region2.Read(offset), value);
                CompareData(data.data(), offset, 1, value, fill_value);
            }
        }
    }
}

TEST(BitMemoryRegion, TestBitsAccess)
{
    std::array<uint8_t, 16> data;
    static constexpr std::array<uint8_t, 2> fill_data = {0x00, 0xff};
    static constexpr size_t MAX_BITS_COUNT = (data.size() - sizeof(uint32_t)) * BITS_PER_BYTE;

    for (size_t offset = 0; offset < MAX_BITS_COUNT; offset++) {
        uint32_t mask = 0;
        for (size_t length = 0; length < BITS_PER_UINT32; length++) {
            const uint32_t value = 0xBADDCAFE & mask;
            for (auto fill_value : fill_data) {
                std::fill(data.begin(), data.end(), fill_value);
                BitMemoryRegion region1(data.data(), offset, length);
                region1.Write(value, 0, length);
                ASSERT_EQ(region1.Read(0, length), value);
                CompareData(data.data(), offset, length, value, fill_value);
                std::fill(data.begin(), data.end(), fill_value);
                BitMemoryRegion region2(data.data(), data.size() * BITS_PER_BYTE);
                region2.Write(value, offset, length);
                ASSERT_EQ(region2.Read(offset, length), value);
                CompareData(data.data(), offset, length, value, fill_value);
            }
            mask = (mask << 1) | 1;
        }
    }
}

TEST(BitMemoryRegion, Dumping)
{
    std::array<uint64_t, 4> data {};
    std::stringstream ss;
    auto clear = [&]() {
        data.fill(0);
        ss.str(std::string());
    };

    {
        clear();
        BitMemoryRegion region(data.data(), 0, data.size() * BITS_PER_UINT64);
        ss << region;
        ASSERT_EQ(ss.str(), "0x0");
    }

    {
        clear();
        data[0] = 0x5;
        BitMemoryRegion region(data.data(), 0, 130);
        ss << region;
        ASSERT_EQ(ss.str(), "0x5");
    }

    {
        clear();
        data[0] = 0x1;
        data[1] = 0x2;
        BitMemoryRegion region(data.data(), 1, 65);
        ss << region;
        ASSERT_EQ(ss.str(), "0x10000000000000000");
    }

    {
        clear();
        data[0] = 0x1;
        data[1] = 0x500;
        BitMemoryRegion region(data.data(), 0, 129);
        ss << region;
        ASSERT_EQ(ss.str(), "0x5000000000000000001");
    }

    {
        clear();
        data[0] = 0x1234560000000000;
        data[1] = 0x4321;
        BitMemoryRegion region(data.data(), 40, 40);
        ss << region;
        ASSERT_EQ(ss.str(), "0x4321123456");
    }

    {
        clear();
        data[0] = 0x123456789abcdef0;
        BitMemoryRegion region(data.data(), 2, 20);
        ss << region;
        ASSERT_EQ(ss.str(), "0xf37bc");
    }

    {
        clear();
        data[0] = 0x123456789abcdef0;
        data[1] = 0xfedcba9876543210;
        BitMemoryRegion region(data.data(), 16, 96);
        ss << region;
        ASSERT_EQ(ss.str(), "0xba9876543210123456789abc");
    }

    {
        clear();
        data[0] = 0x1111111111111111;
        data[1] = 0x2222222222222222;
        data[2] = 0x4444444444444444;
        BitMemoryRegion region(data.data(), 31, 120);
        ss << region;
        ASSERT_EQ(ss.str(), "0x888888444444444444444422222222");
    }
}

}  // namespace panda::test
