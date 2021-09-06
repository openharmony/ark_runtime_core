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

#include <cstdlib>
#include <memory>
#include <vector>

#include "gtest/gtest.h"
#include "bitmap_test_base.h"
#include "runtime/mem/gc/bitmap.h"

namespace panda::mem {

TEST_F(BitmapTest, ClearRange)
{
    auto heap_begin = HEAP_STARTING_ADDRESS;
    constexpr size_t HEAP_CAPACITY = 16_MB;
    auto bm_ptr =
        std::make_unique<BitmapWordType[]>((HEAP_CAPACITY >> Bitmap::LOG_BITSPERWORD) / DEFAULT_ALIGNMENT_IN_BYTES);
    MemBitmap<DEFAULT_ALIGNMENT_IN_BYTES> bm(ToVoidPtr(heap_begin), HEAP_CAPACITY, bm_ptr.get());

    using mem_range = std::pair<object_pointer_type, object_pointer_type>;
    constexpr mem_range FIRST_RANGE {0, 10_KB + DEFAULT_ALIGNMENT_IN_BYTES};
    constexpr mem_range SECOND_RANGE {DEFAULT_ALIGNMENT_IN_BYTES, DEFAULT_ALIGNMENT_IN_BYTES};
    constexpr mem_range THIRD_RANGE {DEFAULT_ALIGNMENT_IN_BYTES, 2 * DEFAULT_ALIGNMENT_IN_BYTES};
    constexpr mem_range FOURTH_RANGE {DEFAULT_ALIGNMENT_IN_BYTES, 5 * DEFAULT_ALIGNMENT_IN_BYTES};
    constexpr mem_range FIFTH_RANGE {1_KB + DEFAULT_ALIGNMENT_IN_BYTES, 2_KB + 5 * DEFAULT_ALIGNMENT_IN_BYTES};
    constexpr mem_range SIXTH_RANGE {0, HEAP_CAPACITY};

    std::vector<mem_range> ranges {FIRST_RANGE, SECOND_RANGE, THIRD_RANGE, FOURTH_RANGE, FIFTH_RANGE, SIXTH_RANGE};

    for (const auto &range : ranges) {
        bm.IterateOverChunks([&bm](void *mem) { bm.Set(mem); });
        bm.ClearRange(ToVoidPtr(heap_begin + range.first), ToVoidPtr(heap_begin + range.second));

        auto test_true_fn = [&bm](void *mem) { EXPECT_TRUE(bm.Test(mem)) << "address: " << mem << std::endl; };
        auto test_false_fn = [&bm](void *mem) { EXPECT_FALSE(bm.Test(mem)) << "address: " << mem << std::endl; };
        bm.IterateOverChunkInRange(ToVoidPtr(heap_begin), ToVoidPtr(heap_begin + range.first), test_true_fn);
        bm.IterateOverChunkInRange(ToVoidPtr(heap_begin + range.first), ToVoidPtr(heap_begin + range.second),
                                   test_false_fn);
        // for SIXTH_RANGE, range.second is not in the heap, so we skip this test
        if (range.second < bm.MemSizeInBytes()) {
            bm.IterateOverChunkInRange(ToVoidPtr(heap_begin + range.second),
                                       ToVoidPtr(heap_begin + bm.MemSizeInBytes()), test_true_fn);
        }
    }
}

}  // namespace panda::mem
