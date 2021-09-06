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

#include "runtime/mem/runslots.h"

#include <cstring>

#include "runtime/include/object_header.h"

namespace panda::mem {

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_RUNSLOTS(level) LOG(level, ALLOC) << "RunSlots: "

template <typename LockTypeT>
void RunSlots<LockTypeT>::Initialize(size_t slot_size, uintptr_t pool_pointer, bool initialize_lock)
{
    ASAN_UNPOISON_MEMORY_REGION(this, RUNSLOTS_SIZE);
    LOG_RUNSLOTS(INFO) << "Initializing RunSlots:";
    ASSERT_PRINT((slot_size >= SlotToSize(SlotsSizes::SLOT_MIN_SIZE_BYTES)), "Size of slot in RunSlots is too small");
    ASSERT_PRINT((slot_size <= SlotToSize(SlotsSizes::SLOT_MAX_SIZE_BYTES)), "Size of slot in RunSlots is too big");
    ASSERT(pool_pointer != 0);
    pool_pointer_ = pool_pointer;
    ASSERT_PRINT(!(ToUintPtr(this) & RUNSLOTS_ALIGNMENT_MASK), "RunSlots object must have alignment");
    slot_size_ = slot_size;
    size_t first_slot_offset = ComputeFirstSlotOffset(slot_size);
    first_uninitialized_slot_offset_ = first_slot_offset;
    ASSERT(first_uninitialized_slot_offset_ != 0);
    next_free_ = nullptr;
    used_slots_ = 0;
    next_runslot_ = nullptr;
    prev_runslot_ = nullptr;
    if (initialize_lock) {
        new (&lock_) LockTypeT();
    }
    (void)memset_s(bitmap_.data(), BITMAP_ARRAY_SIZE, 0x0, BITMAP_ARRAY_SIZE);
    LOG_RUNSLOTS(DEBUG) << "- Memory started from = 0x" << std::hex << ToUintPtr(this);
    LOG_RUNSLOTS(DEBUG) << "- Pool size = " << RUNSLOTS_SIZE << " bytes";
    LOG_RUNSLOTS(DEBUG) << "- Slots size = " << slot_size_ << " bytes";
    LOG_RUNSLOTS(DEBUG) << "- First free slot = " << std::hex << static_cast<void *>(next_free_);
    LOG_RUNSLOTS(DEBUG) << "- First uninitialized slot offset = " << std::hex
                        << static_cast<void *>(ToVoidPtr(first_uninitialized_slot_offset_));
    LOG_RUNSLOTS(DEBUG) << "- Pool pointer = " << std::hex << static_cast<void *>(ToVoidPtr(pool_pointer_));
    LOG_RUNSLOTS(DEBUG) << "Successfully finished RunSlots init";
    ASAN_POISON_MEMORY_REGION(this, RUNSLOTS_SIZE);
}

template <typename LockTypeT>
FreeSlot *RunSlots<LockTypeT>::PopFreeSlot()
{
    ASAN_UNPOISON_MEMORY_REGION(this, GetHeaderSize());
    FreeSlot *free_slot = nullptr;
    if (next_free_ == nullptr) {
        void *uninitialized_slot = PopUninitializedSlot();
        if (uninitialized_slot == nullptr) {
            LOG_RUNSLOTS(DEBUG) << "Failed to get free slot - there are no free slots in RunSlots";
            ASAN_POISON_MEMORY_REGION(this, GetHeaderSize());
            return nullptr;
        }
        free_slot = static_cast<FreeSlot *>(uninitialized_slot);
    } else {
        free_slot = next_free_;
        ASAN_UNPOISON_MEMORY_REGION(free_slot, sizeof(FreeSlot));
        next_free_ = next_free_->GetNext();
        ASAN_POISON_MEMORY_REGION(free_slot, sizeof(FreeSlot));
    }
    MarkAsOccupied(free_slot);
    used_slots_++;
    LOG_RUNSLOTS(DEBUG) << "Successfully get free slot " << std::hex << static_cast<void *>(free_slot)
                        << ". Used slots in this RunSlots = " << std::dec << used_slots_;
    ASAN_POISON_MEMORY_REGION(this, GetHeaderSize());
    return free_slot;
}

template <typename LockTypeT>
void RunSlots<LockTypeT>::PushFreeSlot(FreeSlot *mem_slot)
{
    ASAN_UNPOISON_MEMORY_REGION(this, GetHeaderSize());
    LOG_RUNSLOTS(DEBUG) << "Free slot in RunSlots at addr " << std::hex << static_cast<void *>(mem_slot);
    // We need to poison/unpoison mem_slot here cause we could allocate an object with size less than FreeSlot size
    ASAN_UNPOISON_MEMORY_REGION(mem_slot, sizeof(FreeSlot));
    mem_slot->SetNext(next_free_);
    ASAN_POISON_MEMORY_REGION(mem_slot, sizeof(FreeSlot));
    next_free_ = mem_slot;
    MarkAsFree(mem_slot);
    used_slots_--;
    LOG_RUNSLOTS(DEBUG) << "Used slots in RunSlots = " << used_slots_;
    ASAN_POISON_MEMORY_REGION(this, GetHeaderSize());
}

template <typename LockTypeT>
size_t RunSlots<LockTypeT>::ComputeFirstSlotOffset(size_t slot_size)
{
    size_t slots_for_header = (GetHeaderSize() / slot_size);
    if ((GetHeaderSize() % slot_size) > 0) {
        slots_for_header++;
    }
    return slots_for_header * slot_size;
}

template <typename LockTypeT>
void *RunSlots<LockTypeT>::PopUninitializedSlot()
{
    if (first_uninitialized_slot_offset_ != 0) {
        ASSERT(RUNSLOTS_SIZE > first_uninitialized_slot_offset_);
        void *uninitialized_slot = ToVoidPtr(ToUintPtr(this) + first_uninitialized_slot_offset_);
        first_uninitialized_slot_offset_ += slot_size_;
        if (first_uninitialized_slot_offset_ >= RUNSLOTS_SIZE) {
            ASSERT(first_uninitialized_slot_offset_ == RUNSLOTS_SIZE);
            first_uninitialized_slot_offset_ = 0;
        }
        return uninitialized_slot;
    }
    return nullptr;
}

template <typename LockTypeT>
void RunSlots<LockTypeT>::MarkAsOccupied(const FreeSlot *slot_mem)
{
    uintptr_t bit_index =
        (ToUintPtr(slot_mem) & (RUNSLOTS_SIZE - 1U)) >> SlotToSize(SlotsSizes::SLOT_MIN_SIZE_BYTES_POWER_OF_TWO);
    uintptr_t array_index = bit_index >> BITS_IN_BYTE_POWER_OF_TWO;
    uintptr_t bit_in_array_element = bit_index & ((1U << BITS_IN_BYTE_POWER_OF_TWO) - 1U);
    ASSERT(!(bitmap_[array_index] & (1U << bit_in_array_element)));
    bitmap_[array_index] |= 1U << bit_in_array_element;
}

template <typename LockTypeT>
void RunSlots<LockTypeT>::MarkAsFree(const FreeSlot *slot_mem)
{
    uintptr_t bit_index =
        (ToUintPtr(slot_mem) & (RUNSLOTS_SIZE - 1U)) >> SlotToSize(SlotsSizes::SLOT_MIN_SIZE_BYTES_POWER_OF_TWO);
    uintptr_t array_index = bit_index >> BITS_IN_BYTE_POWER_OF_TWO;
    uintptr_t bit_in_array_element = bit_index & ((1U << BITS_IN_BYTE_POWER_OF_TWO) - 1U);
    ASSERT(bitmap_[array_index] & (1U << bit_in_array_element));
    bitmap_[array_index] ^= 1U << bit_in_array_element;
}

template <typename LockTypeT>
FreeSlot *RunSlots<LockTypeT>::BitMapToSlot(size_t array_index, size_t bit)
{
    return static_cast<FreeSlot *>(
        ToVoidPtr(ToUintPtr(this) + (((array_index << BITS_IN_BYTE_POWER_OF_TWO) + bit)
                                     << SlotToSize(SlotsSizes::SLOT_MIN_SIZE_BYTES_POWER_OF_TWO))));
}

template <typename LockTypeT>
size_t RunSlots<LockTypeT>::RunVerifier::operator()(RunSlots *run)
{
    // 1. should verify whether run's bracket size is the same as recorded in RunSlotsAllocator, but RunSlotsAllocator
    // does not record this
    // 2. should verify thread local run's ownership, but thread local run not implemented yet

    // check alloc'ed size
    auto size_check_func = [this, &run](const ObjectHeader *obj) {
        auto size_power_of_two = ConvertToPowerOfTwoUnsafe(obj->ObjectSize());
        if ((1U << size_power_of_two) != run->GetSlotsSize()) {
            ++(this->fail_cnt_);
        }
    };
    run->IterateOverOccupiedSlots(size_check_func);

    return fail_cnt_;
}

template <typename LockTypeT>
bool RunSlots<LockTypeT>::IsLive(const ObjectHeader *obj) const
{
    ASAN_UNPOISON_MEMORY_REGION(this, GetHeaderSize());
    uintptr_t mem_tail_by_runslots = ToUintPtr(obj) & (RUNSLOTS_SIZE - 1U);
    if ((mem_tail_by_runslots & (static_cast<uintptr_t>(slot_size_) - 1)) != 0) {
        ASAN_POISON_MEMORY_REGION(this, GetHeaderSize());
        return false;
    }
    uintptr_t bit_index = mem_tail_by_runslots >> SlotToSize(SlotsSizes::SLOT_MIN_SIZE_BYTES_POWER_OF_TWO);
    uintptr_t array_index = bit_index >> BITS_IN_BYTE_POWER_OF_TWO;
    uintptr_t bit_in_array_element = bit_index & ((1U << BITS_IN_BYTE_POWER_OF_TWO) - 1U);
    auto live_word = bitmap_[array_index] & (1U << bit_in_array_element);
    ASAN_POISON_MEMORY_REGION(this, GetHeaderSize());
    return live_word != 0;
}

template class RunSlots<RunSlotsLockConfig::CommonLock>;
template class RunSlots<RunSlotsLockConfig::DummyLock>;
}  // namespace panda::mem
