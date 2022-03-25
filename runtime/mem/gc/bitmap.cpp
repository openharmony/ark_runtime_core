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

#include "runtime/mem/gc/bitmap.h"

#include <atomic>

namespace panda::mem {

void Bitmap::ClearBitsInRange(size_t begin, size_t end)
{
    CheckBitRange(begin, end);
    if (GetWordIdx(end) == GetWordIdx(begin)) {  // [begin, end] in the same word
        ClearRangeWithinWord(begin, end);
        return;
    }

    auto begin_roundup = RoundUp(begin, BITSPERWORD);
    auto fn_rounddown = [](BitmapWordType val) -> BitmapWordType {
        constexpr BitmapWordType MASK = ~((static_cast<BitmapWordType>(1) << LOG_BITSPERWORD) - 1);
        return val & MASK;
    };
    auto end_rounddown = fn_rounddown(end);
    ClearRangeWithinWord(begin, begin_roundup);
    ClearWords(GetWordIdx(begin_roundup), GetWordIdx(end_rounddown));
    ClearRangeWithinWord(end_rounddown, end);
}

bool Bitmap::AtomicTestAndSetBit(size_t bit_offset)
{
    CheckBitOffset(bit_offset);
    auto word_idx = GetWordIdx(bit_offset);
    auto *word_addr = reinterpret_cast<std::atomic<BitmapWordType> *>(&bitmap_[word_idx]);
    auto mask = GetBitMask(bit_offset);
    BitmapWordType old_word;
    do {
        old_word = word_addr->load(std::memory_order_seq_cst);
        if ((old_word & mask) != 0) {
            return true;
        }
    } while (!word_addr->compare_exchange_weak(old_word, old_word | mask, std::memory_order_seq_cst));
    return false;
}

bool Bitmap::AtomicTestAndClearBit(size_t bit_offset)
{
    CheckBitOffset(bit_offset);
    auto word_idx = GetWordIdx(bit_offset);
    auto *word_addr = reinterpret_cast<std::atomic<BitmapWordType> *>(&bitmap_[word_idx]);
    auto mask = GetBitMask(bit_offset);
    BitmapWordType old_word;
    do {
        old_word = word_addr->load(std::memory_order_seq_cst);
        if ((old_word & mask) == 0) {
            return false;
        }
    } while (!word_addr->compare_exchange_weak(old_word, old_word & (~mask), std::memory_order_seq_cst));
    return true;
}

bool Bitmap::AtomicTestBit(size_t bit_offset)
{
    CheckBitOffset(bit_offset);
    auto word_idx = GetWordIdx(bit_offset);
    auto *word_addr = reinterpret_cast<std::atomic<BitmapWordType> *>(&bitmap_[word_idx]);
    auto mask = GetBitMask(bit_offset);
    BitmapWordType word = word_addr->load(std::memory_order_seq_cst);
    return (word & mask) != 0;
}

}  // namespace panda::mem
