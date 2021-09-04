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

#include <memory>
#include <vector>

#include "gtest/gtest.h"
#include "bitmap_test_base.h"
#include "runtime/mem/gc/bitmap.h"

namespace panda::mem {

TEST_F(BitmapTest, Init)
{
    const size_t sz = 1_MB;
    auto bm_ptr = std::make_unique<BitmapWordType[]>(sz >> MemBitmap<>::LOG_BITSPERWORD);
    MemBitmap<> bm(ToVoidPtr(HEAP_STARTING_ADDRESS), sz, bm_ptr.get());
    EXPECT_EQ(bm.Size(), sz);
}

TEST_F(BitmapTest, ScanRange)
{
    auto heap_begin = HEAP_STARTING_ADDRESS;
    size_t heap_capacity = 16_MB;
    auto bm_ptr =
        std::make_unique<BitmapWordType[]>((heap_capacity >> Bitmap::LOG_BITSPERWORD) / DEFAULT_ALIGNMENT_IN_BYTES);

    MemBitmap<DEFAULT_ALIGNMENT_IN_BYTES> bm(ToVoidPtr(heap_begin), heap_capacity, bm_ptr.get());

    constexpr size_t BIT_SET_RANGE_END = Bitmap::BITSPERWORD * 3;

    for (size_t j = 0; j < BIT_SET_RANGE_END; ++j) {
        auto *obj = ToVoidPtr(heap_begin + j * DEFAULT_ALIGNMENT_IN_BYTES);
        if (ToUintPtr(obj) & BitmapVerify::ADDRESS_MASK_TO_SET) {
            bm.Set(obj);
        }
    }

    constexpr size_t BIT_VERIFY_RANGE_END = Bitmap::BITSPERWORD * 2;

    for (size_t i = 0; i < Bitmap::BITSPERWORD; ++i) {
        auto *start = ToVoidPtr(heap_begin + i * DEFAULT_ALIGNMENT_IN_BYTES);
        for (size_t j = 0; j < BIT_VERIFY_RANGE_END; ++j) {
            auto *end = ToVoidPtr(heap_begin + (i + j) * DEFAULT_ALIGNMENT_IN_BYTES);
            BitmapVerify(&bm, start, end);
        }
    }
}

TEST_F(BitmapTest, VisitorPageAlignment)
{
    RunTestCount<4_KB>();
}

TEST_F(BitmapTest, OrderPageAlignment)
{
    RunTestOrder<4_KB>();
}

}  // namespace panda::mem
