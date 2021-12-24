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

#include "runtime/mem/gc/card_table.h"

#include "trace/trace.h"
#include "libpandabase/mem/mem.h"
#include "libpandabase/utils/logger.h"

namespace panda::mem {

CardTable::CardTable(InternalAllocatorPtr internal_allocator, uintptr_t min_address, size_t size)
    : min_address_(min_address),
      cards_count_((size / CARD_SIZE) + (size % CARD_SIZE != 0 ? 1 : 0)),
      internal_allocator_(internal_allocator)
{
}

CardTable::~CardTable()
{
    ASSERT(cards_ != nullptr);
    internal_allocator_->Free(cards_);
}

void CardTable::Initialize()
{
    trace::ScopedTrace scoped_trace(__PRETTY_FUNCTION__);
    if (cards_ != nullptr) {
        LOG(FATAL, GC) << "try to initialize already initialized CardTable";
    }
    cards_ = static_cast<CardPtr>(internal_allocator_->Alloc(cards_count_));
    ClearCards(cards_, cards_count_);
    ASSERT(cards_ != nullptr);
}

void CardTable::ClearCards(CardPtr start, size_t card_count)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    CardPtr end = start + card_count;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    for (auto *cur_card = start; cur_card < end; ++cur_card) {
        cur_card->Clear();
    }
}

bool CardTable::IsMarked(uintptr_t addr) const
{
    CardPtr card = GetCardPtr(addr);
    return card->IsMarked();
}

void CardTable::MarkCard(uintptr_t addr)
{
    CardPtr card = GetCardPtr(addr);
    card->Mark();
}

bool CardTable::IsClear(uintptr_t addr) const
{
    CardPtr card = GetCardPtr(addr);
    return card->IsClear();
}

void CardTable::ClearCard(uintptr_t addr)
{
    CardPtr card = GetCardPtr(addr);
    card->Clear();
}

void CardTable::ClearAll()
{
    ClearCards(cards_, cards_count_);
}

void CardTable::ClearCardRange(uintptr_t begin_addr, uintptr_t end_addr)
{
    ASSERT((begin_addr - min_address_) % CARD_SIZE == 0);
    size_t cards_count = (end_addr - begin_addr) / CARD_SIZE;
    CardPtr start = GetCardPtr(begin_addr);
    ClearCards(start, cards_count);
}

uintptr_t CardTable::GetCardStartAddress(CardPtr card) const
{
    return min_address_ + (ToUintPtr(card) - ToUintPtr(cards_)) * CARD_SIZE;
}

uintptr_t CardTable::GetCardEndAddress(CardPtr card) const
{
    return min_address_ + (ToUintPtr(card + 1) - ToUintPtr(cards_)) * CARD_SIZE - 1;
}

MemRange CardTable::GetMemoryRange(CardPtr card) const
{
    return MemRange(GetCardStartAddress(card), GetCardEndAddress(card));
}

CardTable::Card::Card(uint8_t val)
{
    SetCard(val);
}

bool CardTable::Card::IsMarked() const
{
    return GetCard() == MARKED_VALUE;
}

void CardTable::Card::Mark()
{
    SetCard(MARKED_VALUE);
}

bool CardTable::Card::IsClear() const
{
    return GetCard() == CLEAR_VALUE;
}

void CardTable::Card::Clear()
{
    SetCard(CLEAR_VALUE);
}

bool CardTable::Card::IsProcessed() const
{
    return GetCard() == PROCESSED_VALUE;
}

void CardTable::Card::SetProcessed()
{
    SetCard(PROCESSED_VALUE);
}

uint8_t CardTable::Card::GetCard() const
{
    return value_.load(std::memory_order_relaxed);
}

void CardTable::Card::SetCard(uint8_t new_val)
{
    value_.store(new_val, std::memory_order_relaxed);
}

CardTable::CardPtr CardTable::GetCardPtr(uintptr_t addr) const
{
    ASSERT(addr >= min_address_);
    ASSERT(addr < min_address_ + cards_count_ * CARD_SIZE);
    auto card = static_cast<CardPtr>(ToVoidPtr(ToUintPtr(cards_) + ((addr - min_address_) >> LOG2_CARD_SIZE)));
    return card;
}
}  // namespace panda::mem
