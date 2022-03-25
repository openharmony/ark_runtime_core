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

#include "serializer/serializer.h"
#include <gtest/gtest.h>

namespace panda {

class SerializatorTest : public testing::Test {
protected:
    void SetUp()
    {
        buffer.resize(0);
    }
    std::vector<uint8_t> buffer;
};

template <typename T>
void SerializerTypeToBuffer(const T &type, std::vector<uint8_t> *buffer, size_t ret_val)
{
    auto ret = serializer::TypeToBuffer(type, *buffer);
    ASSERT_TRUE(ret);
    ASSERT_EQ(ret.Value(), ret_val);
}

template <typename T>
void SerializerBufferToType(const std::vector<uint8_t> &buffer, T &type, size_t ret_val)
{
    auto ret = serializer::BufferToType(buffer.data(), buffer.size(), type);
    ASSERT_TRUE(ret);
    ASSERT_EQ(ret.Value(), ret_val);
}

template <typename T>
void DoTest(T value, int ret_val)
{
    T a = value;
    T b;
    std::vector<uint8_t> buffer;
    SerializerTypeToBuffer(a, &buffer, ret_val);
    buffer.resize(4U * buffer.size());
    SerializerBufferToType(buffer, b, ret_val);
    ASSERT_EQ(a, value);
    ASSERT_EQ(b, value);
    ASSERT_EQ(a, b);
}

template <typename T>
void TestPod(T value)
{
    static_assert(std::is_pod<T>::value, "Type is not supported");

    DoTest(value, sizeof(value));
}

struct PodStruct {
    uint8_t a;
    int16_t b;
    uint32_t c;
    int64_t d;
    float e;
    long double f;
};

bool operator==(const PodStruct &lhs, const PodStruct &rhs)
{
    return lhs.a == rhs.a && lhs.b == rhs.b && lhs.c == rhs.c && lhs.d == rhs.d && lhs.e == rhs.e && lhs.f == rhs.f;
}

TEST_F(SerializatorTest, TestPodTypes)
{
    TestPod<uint8_t>(0xac);
    TestPod<uint16_t>(0xc0de);
    TestPod<uint32_t>(0x123f567f);
    TestPod<uint64_t>(0xff12345789103c4b);

    TestPod<int8_t>(0x1c);
    TestPod<int16_t>(0x1ebd);
    TestPod<int32_t>(0xfe52567f);
    TestPod<int64_t>(0xff1234fdec57891b);

    TestPod<float>(0.234664);
    TestPod<double>(22345.3453453);
    TestPod<long double>(99453.64345);

    TestPod<PodStruct>({0xff, -23458, 10345893, -98343451, -3.54634, 1.44e6});
}

TEST_F(SerializatorTest, TestString)
{
    DoTest<std::string>({}, 4);
    DoTest<std::string>("", 4);
    DoTest<std::string>("Hello World!", 4 + 12);
    DoTest<std::string>("1", 4 + 1);
    DoTest<std::string>({}, 4);
}

TEST_F(SerializatorTest, TestVectorPod)
{
    DoTest<std::vector<uint8_t>>({1, 2, 3, 4}, 4 + 1 * 4);
    DoTest<std::vector<uint16_t>>({143, 452, 334}, 4 + 2 * 3);
    DoTest<std::vector<uint32_t>>({15434, 4564562, 33453, 43456, 346346}, 4 + 5 * 4);
    DoTest<std::vector<uint64_t>>({14345665644345, 34645345465}, 4 + 8 * 2);
    DoTest<std::vector<char>>({}, 4 + 1 * 0);
}

TEST_F(SerializatorTest, TestUnorderedMap1)
{
    using Map = std::unordered_map<uint32_t, uint16_t>;
    DoTest<Map>(
        {
            {12343526, 23424},
            {3, 234356},
            {45764746, 4},
        },
        4 + 3 * (4 + 2));
}

TEST_F(SerializatorTest, TestUnorderedMap2)
{
    using Map = std::unordered_map<std::string, std::string>;
    DoTest<Map>(
        {
            {"one", {}},
            {"two", "123"},
            {"three", ""},
            {"", {}},
        },
        4 + 4 + 3 + 4 + 0 + 4 + 3 + 4 + 3 + 4 + 5 + 4 + 0 + 4 + 0 + 4 + 0);
}

TEST_F(SerializatorTest, TestUnorderedMap3)
{
    using Map = std::unordered_map<std::string, std::vector<uint32_t>>;
    DoTest<Map>(
        {
            {"one", {}},
            {"two", {1, 2, 3, 4}},
            {"three", {9, 34, 45335}},
            {"", {}},
        },
        4 + 4 + 3 + 4 + 4 * 0 + 4 + 3 + 4 + 4 * 4 + 4 + 5 + 4 + 4 * 3 + 4 + 0 + 4 + 4 * 0);
}

struct TestStruct {
    uint8_t a;
    uint16_t b;
    uint32_t c;
    uint64_t d;
    std::string e;
    std::vector<int> f;
};

bool operator==(const TestStruct &lhs, const TestStruct &rhs)
{
    return lhs.a == rhs.a && lhs.b == rhs.b && lhs.c == rhs.c && lhs.d == rhs.d && lhs.e == rhs.e && lhs.f == rhs.f;
}

TEST_F(SerializatorTest, TestStruct)
{
    TestStruct test_struct {1, 2, 3, 4, "Liza", {8, 9, 5}};
    unsigned test_ret = 1 + 2 + 4 + 8 + 4 + 4 + 4 + sizeof(int) * 3;

    TestStruct a = test_struct;
    TestStruct b;
    ASSERT_EQ(serializer::StructToBuffer<6>(a, buffer), true);
    buffer.resize(4 * buffer.size());
    auto ret = serializer::RawBufferToStruct<6>(buffer.data(), buffer.size(), b);
    ASSERT_TRUE(ret.HasValue());
    ASSERT_EQ(ret.Value(), test_ret);
    ASSERT_EQ(a, test_struct);
    ASSERT_EQ(b, test_struct);
    ASSERT_EQ(a, b);
}

}  // namespace panda
