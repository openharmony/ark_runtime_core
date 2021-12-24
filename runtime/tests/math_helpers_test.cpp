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

#include <gtest/gtest.h>

#include "runtime/interpreter/math_helpers.h"

namespace panda::interpreter::math_helpers::test {

template <class T>
void TestBitShl()
{
    std::ostringstream ss;
    ss << "Test bit_shl with sizeof(T) = ";
    ss << sizeof(T);

    using unsigned_type = std::make_unsigned_t<T>;

    {
        T v = 1;
        size_t shift = 5;
        T res = bit_shl<T>()(v, shift);
        EXPECT_EQ(res, v << shift) << ss.str();
    }

    {
        T v = 1;
        size_t shift = std::numeric_limits<unsigned_type>::digits - 1;
        T res = bit_shl<T>()(v, shift);
        EXPECT_EQ(res, static_cast<T>(v << shift)) << ss.str();
    }

    {
        T v = 1;
        size_t shift = std::numeric_limits<unsigned_type>::digits;
        T res = bit_shl<T>()(v, shift);
        EXPECT_EQ(res, v) << ss.str();
    }

    {
        T v = 1;
        size_t shift = std::numeric_limits<unsigned_type>::digits + 2U;
        T res = bit_shl<T>()(v, shift);
        EXPECT_EQ(res, v << 2U) << ss.str();
    }
}

template <class T>
void TestBitShr()
{
    std::ostringstream ss;
    ss << "Test bit_shr with sizeof(T) = ";
    ss << sizeof(T);

    using unsigned_type = std::make_unsigned_t<T>;

    {
        T v = 64;
        T shift = 5;
        T res = bit_shr<T>()(v, shift);
        EXPECT_EQ(res, v >> shift) << ss.str();
    }

    {
        T v = std::numeric_limits<T>::min();
        T shift = std::numeric_limits<unsigned_type>::digits - 1;
        T res = bit_shr<T>()(v, shift);
        EXPECT_EQ(res, 1) << ss.str();
    }

    {
        T v = 1;
        T shift = std::numeric_limits<unsigned_type>::digits;
        T res = bit_shr<T>()(v, shift);
        EXPECT_EQ(res, v) << ss.str();
    }

    {
        T v = 20;
        T shift = std::numeric_limits<unsigned_type>::digits + 2U;
        T res = bit_shr<T>()(v, shift);
        EXPECT_EQ(res, v >> 2U) << ss.str();
    }
}

template <class T>
void TestBitAshr()
{
    std::ostringstream ss;
    ss << "Test bit_ashr with sizeof(T) = ";
    ss << sizeof(T);

    using unsigned_type = std::make_unsigned_t<T>;

    {
        T v = 64;
        T shift = 5;
        T res = bit_ashr<T>()(v, shift);
        EXPECT_EQ(res, v >> shift) << ss.str();
    }

    {
        T v = std::numeric_limits<T>::min();
        T shift = std::numeric_limits<unsigned_type>::digits - 1;
        T res = bit_ashr<T>()(v, shift);
        EXPECT_EQ(res, -1) << ss.str();
    }

    {
        T v = 1;
        T shift = std::numeric_limits<unsigned_type>::digits;
        T res = bit_ashr<T>()(v, shift);
        EXPECT_EQ(res, v) << ss.str();
    }

    {
        T v = 20;
        T shift = std::numeric_limits<unsigned_type>::digits + 2U;
        T res = bit_ashr<T>()(v, shift);
        EXPECT_EQ(res, v >> 2U) << ss.str();
    }
}

template <class T>
T GetNaN();

template <>
float GetNaN()
{
    return nanf("");
}

template <>
double GetNaN()
{
    return nan("");
}

template <class T>
void TestFcmpl()
{
    std::ostringstream ss;
    ss << "Test fcmpl with sizeof(T) = ";
    ss << sizeof(T);

    {
        T v1 = 1.0;
        T v2 = GetNaN<T>();
        EXPECT_EQ(fcmpl<T>()(v1, v2), -1);
    }

    {
        T v1 = GetNaN<T>();
        T v2 = 1.0;
        EXPECT_EQ(fcmpl<T>()(v1, v2), -1);
    }

    {
        T v1 = GetNaN<T>();
        T v2 = GetNaN<T>();
        EXPECT_EQ(fcmpl<T>()(v1, v2), -1);
    }

    {
        T v1 = 1.0;
        T v2 = 2.0;
        EXPECT_EQ(fcmpl<T>()(v1, v2), -1);
    }

    {
        T v1 = 1.0;
        T v2 = v1;
        EXPECT_EQ(fcmpl<T>()(v1, v2), 0);
    }

    {
        T v1 = 2.0;
        T v2 = 1.0;
        EXPECT_EQ(fcmpl<T>()(v1, v2), 1);
    }
}

template <class T>
void TestFcmpg()
{
    std::ostringstream ss;
    ss << "Test fcmpg with sizeof(T) = ";
    ss << sizeof(T);

    {
        T v1 = 1.0;
        T v2 = GetNaN<T>();
        EXPECT_EQ(fcmpg<T>()(v1, v2), 1);
    }

    {
        T v1 = GetNaN<T>();
        T v2 = 1.0;
        EXPECT_EQ(fcmpg<T>()(v1, v2), 1);
    }

    {
        T v1 = GetNaN<T>();
        T v2 = GetNaN<T>();
        EXPECT_EQ(fcmpg<T>()(v1, v2), 1);
    }

    {
        T v1 = 1.0;
        T v2 = 2.0;
        EXPECT_EQ(fcmpg<T>()(v1, v2), -1);
    }

    {
        T v1 = 1.0;
        T v2 = v1;
        EXPECT_EQ(fcmpg<T>()(v1, v2), 0);
    }

    {
        T v1 = 2.0;
        T v2 = 1.0;
        EXPECT_EQ(fcmpg<T>()(v1, v2), 1);
    }
}

TEST(MathHelpers, BitShl)
{
    TestBitShl<int8_t>();
    TestBitShl<int16_t>();
    TestBitShl<int32_t>();
    TestBitShl<int64_t>();
}

TEST(MathHelpers, BitShr)
{
    TestBitShr<int8_t>();
    TestBitShr<int16_t>();
    TestBitShr<int32_t>();
    TestBitShr<int64_t>();
}

TEST(MathHelpers, BitAshr)
{
    TestBitAshr<int8_t>();
    TestBitAshr<int16_t>();
    TestBitAshr<int32_t>();
    TestBitAshr<int64_t>();
}

TEST(MathHelpers, Fcmpl)
{
    TestFcmpl<float>();
    TestFcmpl<double>();
}

TEST(MathHelpers, Fcmpg)
{
    TestFcmpg<float>();
    TestFcmpg<double>();
}

}  // namespace panda::interpreter::math_helpers::test
