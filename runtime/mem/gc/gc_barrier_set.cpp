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

#include "runtime/mem/gc/gc_barrier_set.h"

#include "libpandabase/mem/mem.h"
#include "runtime/mem/rem_set.h"

#include <atomic>

namespace panda::mem {

GCBarrierSet::~GCBarrierSet() = default;

void PreSATBBarrier(const bool *concurrent_marking_flag, objRefProcessFunc pre_store_func, void *pre_val)
{
    ASSERT(pre_store_func != nullptr);
    if (UNLIKELY(*concurrent_marking_flag)) {
        if (pre_val != nullptr) {
            LOG(DEBUG, GC) << "GC PreSATBBarrier pre val -> new val:" << std::hex << pre_val;
            pre_store_func(pre_val);
        }
    }
}

void PostIntergenerationalBarrier(const void *min_addr, uint8_t *card_table_addr, uint8_t card_bits,
                                  uint8_t dirty_card_value, const void *obj_field_addr)
{
    size_t card_index = (ToUintPtr(obj_field_addr) - *static_cast<const uintptr_t *>(min_addr)) >> card_bits;
    auto *card_addr = static_cast<std::atomic_uint8_t *>(ToVoidPtr(ToUintPtr(card_table_addr) + card_index));
    card_addr->store(dirty_card_value, std::memory_order_relaxed);
}

void PostIntergenerationalBarrierInRange(const void *min_addr, uint8_t *card_table_addr, uint8_t card_bits,
                                         uint8_t dirty_card_value, const void *obj_field_addr, size_t size)
{
    size_t card_first_index = (ToUintPtr(obj_field_addr) - *static_cast<const uintptr_t *>(min_addr)) >> card_bits;
    size_t card_last_index =
        (ToUintPtr(obj_field_addr) + size - *static_cast<const uintptr_t *>(min_addr)) >> card_bits;
    for (size_t card_index = card_first_index; card_index <= card_last_index; card_index++) {
        auto *card_addr = static_cast<std::atomic_uint8_t *>(ToVoidPtr(ToUintPtr(card_table_addr) + card_index));
        card_addr->store(dirty_card_value, std::memory_order_relaxed);
    }
}

void PostInterregionBarrier(const void *obj_addr, const void *ref, const size_t region_size_bits,
                            const std::function<void(const void *, const void *)> &update_func)
{
    if (ref != nullptr) {
        // If it is cross-region reference
        if ((ToObjPtrType(obj_addr) ^ ToObjPtrType(ref)) >> region_size_bits != 0) {
            update_func(obj_addr, ref);
        }
    }
}

BarrierOperand GCBarrierSet::GetBarrierOperand(BarrierPosition barrier_position, std::string_view name)
{
    if (barrier_position == BarrierPosition::BARRIER_POSITION_PRE) {
        if (UNLIKELY(pre_operands_.find(name.data()) == pre_operands_.end())) {
            LOG(FATAL, GC) << "Operand " << name << " not found for pre barrier";
        }
        return pre_operands_.at(name.data());
    }
    if (UNLIKELY(post_operands_.find(name.data()) == post_operands_.end())) {
        LOG(FATAL, GC) << "Operand " << name << " not found for post barrier";
    }
    return post_operands_.at(name.data());
}

void GCGenBarrierSet::PreBarrier([[maybe_unused]] const void *obj_field_addr, void *pre_val_addr)
{
    LOG(DEBUG, GC) << "GC PreBarrier: write to " << std::hex << obj_field_addr << " with pre-value " << pre_val_addr;
    PreSATBBarrier(concurrent_marking_flag_, pre_store_func_, pre_val_addr);
}

void GCGenBarrierSet::PostBarrier(const void *obj_addr, [[maybe_unused]] void *stored_val_addr)
{
    LOG(DEBUG, GC) << "GC PostBarrier: write to " << std::hex << obj_addr << " value " << stored_val_addr;
    PostIntergenerationalBarrier(min_addr_, card_table_addr_, card_bits_, dirty_card_value_, obj_addr);
}

void GCGenBarrierSet::PostBarrierArrayWrite(const void *obj_addr, [[maybe_unused]] size_t size)
{
    PostIntergenerationalBarrier(min_addr_, card_table_addr_, card_bits_, dirty_card_value_, obj_addr);
}

void GCGenBarrierSet::PostBarrierEveryObjectFieldWrite(const void *obj_addr, [[maybe_unused]] size_t size)
{
    // NOTE: We can improve an implementation here
    // because now we consider every field as an object reference field.
    // Maybe, it will be better to check it, but there can be possible performance degradation.
    PostIntergenerationalBarrier(min_addr_, card_table_addr_, card_bits_, dirty_card_value_, obj_addr);
}

void GCG1BarrierSet::PreBarrier([[maybe_unused]] const void *obj_field_addr, void *pre_val_addr)
{
    LOG(DEBUG, GC) << "GC PreBarrier: write to " << std::hex << obj_field_addr << " with pre-value " << pre_val_addr;
    PreSATBBarrier(concurrent_marking_flag_, pre_store_func_, pre_val_addr);
}

void GCG1BarrierSet::PostBarrier(const void *obj_addr, [[maybe_unused]] void *stored_val_addr)
{
    LOG(DEBUG, GC) << "GC PostBarrier: write to " << std::hex << obj_addr << " value " << stored_val_addr;
    PostInterregionBarrier(obj_addr, stored_val_addr, region_size_bits_count_, post_func_);
}

void GCG1BarrierSet::PostBarrierArrayWrite(const void *obj_addr, [[maybe_unused]] size_t size)
{
    PostIntergenerationalBarrier(min_addr_, card_table_addr_, card_bits_, dirty_card_value_, obj_addr);
}

void GCG1BarrierSet::PostBarrierEveryObjectFieldWrite(const void *obj_addr, [[maybe_unused]] size_t size)
{
    // NOTE: We can improve an implementation here
    // because now we consider every field as an object reference field.
    // Maybe, it will be better to check it, but there can be possible performance degradation.
    PostIntergenerationalBarrier(min_addr_, card_table_addr_, card_bits_, dirty_card_value_, obj_addr);
}

}  // namespace panda::mem
