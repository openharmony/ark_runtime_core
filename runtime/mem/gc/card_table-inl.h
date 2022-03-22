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

#ifndef PANDA_RUNTIME_MEM_GC_CARD_TABLE_INL_H_
#define PANDA_RUNTIME_MEM_GC_CARD_TABLE_INL_H_

#include "runtime/mem/gc/card_table.h"
#include "runtime/include/mem/panda_containers.h"

#include <atomic>

namespace panda::mem {

inline void CardTable::FillRanges(PandaVector<MemRange> *ranges, const Card *start_card, const Card *end_card)
{
    constexpr size_t MIN_RANGE = 32;
    constexpr size_t MAX_CARDS_COUNT = 1000;  // How many cards we can process at once
    static std::array<char, MAX_CARDS_COUNT> zero_array {};

    if (static_cast<size_t>(end_card - start_card) < MIN_RANGE) {
        for (auto card_ptr = start_card; card_ptr <= end_card; card_ptr++) {
            if (card_ptr->IsMarked()) {
                ranges->emplace_back(min_address_ + (card_ptr - cards_) * CARD_SIZE,
                                     min_address_ + (card_ptr - cards_ + 1) * CARD_SIZE - 1);
            }
        }
    } else {
        size_t diff = end_card - start_card + 1;
        size_t split_size = std::min(diff / 2, MAX_CARDS_COUNT);  // divide 2 to get smaller split_size
        if (memcmp(start_card, &zero_array, split_size) != 0) {
            FillRanges(ranges, start_card, ToNativePtr<Card>(ToUintPtr(start_card) + split_size - 1));
        }
        // NOLINTNEXTLINE(bugprone-branch-clone)
        if (diff - split_size > MAX_CARDS_COUNT) {
            FillRanges(ranges, ToNativePtr<Card>(ToUintPtr(start_card) + split_size), end_card);
        } else if (memcmp(ToNativePtr<Card>(ToUintPtr(start_card) + split_size), &zero_array, diff - split_size) != 0) {
            FillRanges(ranges, ToNativePtr<Card>(ToUintPtr(start_card) + split_size), end_card);
        }
    }
}

// Make sure we can treat size_t as lockfree atomic
static_assert(std::atomic_size_t::is_always_lock_free);
static_assert(sizeof(std::atomic_size_t) == sizeof(size_t));

template <typename CardVisitor>
void CardTable::VisitMarked(CardVisitor card_visitor, uint32_t processed_flag)
{
    bool visit_marked = processed_flag & CardTableProcessedFlag::VISIT_MARKED;
    bool visit_processed = processed_flag & CardTableProcessedFlag::VISIT_PROCESSED;
    bool set_processed = processed_flag & CardTableProcessedFlag::SET_PROCESSED;
    static_assert(sizeof(std::atomic_size_t) % sizeof(Card) == 0);
    constexpr size_t chunk_card_num = sizeof(std::atomic_size_t) / sizeof(Card);
    auto *card = cards_;
    auto *card_end = cards_ + (cards_count_ / chunk_card_num) * chunk_card_num;
    while (card < card_end) {
        // NB! In general wide load/short store on overlapping memory of different address are allowed to be reordered
        // This optimization currently is allowed since additional VisitMarked is called after concurrent mark with
        // global Mutator lock held, so all previous java thread's writes should be visible by GC thread
        if (LIKELY((reinterpret_cast<std::atomic_size_t *>(card))->load(std::memory_order_relaxed) == 0)) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            card += chunk_card_num;
            continue;
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        auto *chunk_end = card + chunk_card_num;
        while (card < chunk_end) {
            if (!(visit_marked && card->IsMarked()) && !(visit_processed && card->IsProcessed())) {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                ++card;
                continue;
            }

            if (set_processed) {
                card->SetProcessed();
            }
            card_visitor(GetMemoryRange(card));
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            ++card;
        }
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    for (; card < cards_ + cards_count_; ++card) {
        if ((visit_marked && card->IsMarked()) || (visit_processed && card->IsProcessed())) {
            if (set_processed) {
                card->SetProcessed();
            }
            card_visitor(GetMemoryRange(card));
        }
    }
}

template <typename CardVisitor>
void CardTable::VisitMarkedCompact(CardVisitor card_visitor)
{
    constexpr size_t MAX_CARDS_COUNT = 1000;
    size_t cur_pos = 0;
    size_t end_pos = 0;
    PandaVector<MemRange> mem_ranges;

    ASSERT(cards_count_ > 0);
    auto max_pool_address = PoolManager::GetMmapMemPool()->GetMaxObjectAddress();
    while (cur_pos < cards_count_) {
        end_pos = std::min(cur_pos + MAX_CARDS_COUNT - 1, cards_count_ - 1);
        FillRanges(&mem_ranges, &cards_[cur_pos], &cards_[end_pos]);
        cur_pos = end_pos + 1;
        if (GetCardStartAddress(&cards_[cur_pos]) > max_pool_address) {
            break;
        }
    }
    for (const auto &mem_range : mem_ranges) {
        card_visitor(mem_range);
    }
}

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_GC_CARD_TABLE_INL_H_
