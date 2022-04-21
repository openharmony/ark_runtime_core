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

#include <cstdlib>
#include <ctime>
#include <climits>
#include <random>
#include "gtest/gtest.h"

#include "mem/mem.h"
#include "mem/mem_range.h"

namespace panda::test::mem_range {

constexpr uintptr_t MAX_PTR = std::numeric_limits<uintptr_t>::max();

constexpr uint64_t NUM_RANDOM_TESTS = 100;
constexpr uint64_t NUM_ITER_PER_TEST = 1000;
constexpr uintptr_t RANDOM_AREA_SIZE = 100000;

std::default_random_engine g_generator;
std::uniform_int_distribution<uintptr_t> g_distribution(0, MAX_PTR);

// function to generate random uintptr_t
static uintptr_t random_uintptr()
{
    return g_distribution(g_generator);
}

// function to generate MemRange randomly from given range
static panda::mem::MemRange randomMemRange(uintptr_t min_start, uintptr_t max_end)
{
    ASSERT(max_end > min_start);

    uintptr_t rand_1, rand_2;
    rand_1 = min_start + random_uintptr() % (max_end - min_start + 1);
    rand_2 = min_start + random_uintptr() % (max_end - min_start + 1);

    if (rand_1 < rand_2) {
        return panda::mem::MemRange(rand_1, rand_2);
    } else if (rand_1 > rand_2) {
        return panda::mem::MemRange(rand_2, rand_1);
    } else {
        if (rand_1 > 0) {
            return panda::mem::MemRange(rand_1 - 1, rand_1);
        } else {
            return panda::mem::MemRange(rand_1, rand_1 + 1);
        }
    }
}

// test constructor and simple methods
TEST(MemRangeTest, BasicTest)
{
    constexpr uintptr_t START = 10, END = 10000;
    constexpr uintptr_t LOWER_THAN_START = 0, HIGHER_THAN_END = 50000;

    auto mem_range = panda::mem::MemRange(START, END);

    // test correct start and end addresses
    ASSERT_EQ(START, mem_range.GetStartAddress());
    ASSERT_EQ(END, mem_range.GetEndAddress());

    // test inner addresses
    ASSERT_TRUE(mem_range.IsAddressInRange(START));
    ASSERT_TRUE(mem_range.IsAddressInRange(END));
    ASSERT_TRUE(mem_range.IsAddressInRange((START + END) / 2));

    // test outer addresses
    ASSERT_FALSE(mem_range.IsAddressInRange(LOWER_THAN_START));
    ASSERT_FALSE(mem_range.IsAddressInRange(START - 1));
    ASSERT_FALSE(mem_range.IsAddressInRange(END + 1));
    ASSERT_FALSE(mem_range.IsAddressInRange(HIGHER_THAN_END));
}

// test constructor with incorrect args
TEST(MemRangeTest, AssertTest)
{
    constexpr uintptr_t MIN = 10000, MAX = 50000;

    ASSERT_DEBUG_DEATH(panda::mem::MemRange(MIN, MIN), "");
    ASSERT_DEBUG_DEATH(panda::mem::MemRange(MAX, MIN), "");
}

// test IsIntersect method
TEST(MemRangeTest, IntersectTest)
{
    constexpr uintptr_t START_1 = 10, END_1 = 100;
    constexpr uintptr_t START_2 = 101, END_2 = 200;
    constexpr uintptr_t START_3 = 50, END_3 = 500;
    constexpr uintptr_t START_4 = 500, END_4 = 600;
    constexpr uintptr_t START_5 = 10, END_5 = 100;

    auto mem_range_1 = panda::mem::MemRange(START_1, END_1);
    auto mem_range_2 = panda::mem::MemRange(START_2, END_2);
    auto mem_range_3 = panda::mem::MemRange(START_3, END_3);
    auto mem_range_4 = panda::mem::MemRange(START_4, END_4);
    auto mem_range_5 = panda::mem::MemRange(START_5, END_5);

    // ranges are not intersecting
    ASSERT_FALSE(mem_range_1.IsIntersect(mem_range_2));
    ASSERT_FALSE(mem_range_2.IsIntersect(mem_range_1));

    // ranges are partly intersecting
    ASSERT_TRUE(mem_range_1.IsIntersect(mem_range_3));
    ASSERT_TRUE(mem_range_3.IsIntersect(mem_range_1));

    // ranges are nested
    ASSERT_TRUE(mem_range_2.IsIntersect(mem_range_3));
    ASSERT_TRUE(mem_range_3.IsIntersect(mem_range_2));

    // ranges have common bound
    ASSERT_TRUE(mem_range_3.IsIntersect(mem_range_4));
    ASSERT_TRUE(mem_range_4.IsIntersect(mem_range_3));

    // ranges are equal
    ASSERT_TRUE(mem_range_1.IsIntersect(mem_range_5));

    // test self
    ASSERT_TRUE(mem_range_1.IsIntersect(mem_range_1));
}

// function to conduct num_iter random tests with addresses in given bounds
static void randomTestInBounds(uintptr_t from, uintptr_t to, uint64_t num_iter = NUM_ITER_PER_TEST)
{
    ASSERT(from < to);

    panda::mem::MemRange mem_range_1(0, 1), mem_range_2(0, 1);
    // check intersection via cycle
    for (uint64_t iter = 0; iter < num_iter; iter++) {
        mem_range_1 = randomMemRange(from, to);
        mem_range_2 = randomMemRange(from, to);

        if (mem_range_1.GetStartAddress() < mem_range_2.GetStartAddress()) {
            for (uintptr_t i = mem_range_1.GetStartAddress(); i < MAX_PTR; i++) {
                if (i == mem_range_2.GetStartAddress()) {
                    ASSERT_TRUE(mem_range_1.IsIntersect(mem_range_2));
                    ASSERT_TRUE(mem_range_2.IsIntersect(mem_range_1));
                    break;
                }
                if (i == mem_range_1.GetEndAddress()) {
                    ASSERT_FALSE(mem_range_1.IsIntersect(mem_range_2));
                    ASSERT_FALSE(mem_range_2.IsIntersect(mem_range_1));
                    break;
                }
            }
        } else if (mem_range_1.GetStartAddress() > mem_range_2.GetStartAddress()) {
            for (uintptr_t i = mem_range_2.GetStartAddress(); i < MAX_PTR; i++) {
                if (i == mem_range_1.GetStartAddress()) {
                    ASSERT_TRUE(mem_range_1.IsIntersect(mem_range_2));
                    ASSERT_TRUE(mem_range_2.IsIntersect(mem_range_1));
                    break;
                }
                if (i == mem_range_2.GetEndAddress()) {
                    ASSERT_FALSE(mem_range_1.IsIntersect(mem_range_2));
                    ASSERT_FALSE(mem_range_2.IsIntersect(mem_range_1));
                    break;
                }
            }
        } else {
            // case with equal start addresses
            ASSERT_TRUE(mem_range_1.IsIntersect(mem_range_2));
            ASSERT_TRUE(mem_range_2.IsIntersect(mem_range_1));
        }
    }
}

// set of random tests with different address ranges
// no bug detected during a lot of tries with different parameters
TEST(MemRangeTest, RandomIntersectTest)
{
    unsigned int seed_;
#ifdef PANDA_NIGHTLY_TEST_ON
    seed_ = std::time(NULL);
#else
    seed_ = 0xDEADBEEF;
#endif
    srand(seed_);
    g_generator.seed(seed_);

    // random tests in specified ranges
    randomTestInBounds(0, RANDOM_AREA_SIZE);
    randomTestInBounds(MAX_PTR - RANDOM_AREA_SIZE, MAX_PTR);

    // tests in random ranges
    uintptr_t position;
    for (uint64_t i = 0; i < NUM_RANDOM_TESTS; i++) {
        position = random_uintptr();
        if (position > RANDOM_AREA_SIZE) {
            randomTestInBounds(position - RANDOM_AREA_SIZE, position);
        } else {
            randomTestInBounds(position, position + RANDOM_AREA_SIZE);
        }
    }
}

}  // namespace panda::test::mem_range
