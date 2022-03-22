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

#ifndef PANDA_RUNTIME_MEM_REM_SET_INL_H_
#define PANDA_RUNTIME_MEM_REM_SET_INL_H_

#include "runtime/mem/rem_set.h"
#include "runtime/mem/region_space-inl.h"

namespace panda::mem {

template <typename LockConfigT>
RemSet<LockConfigT>::RemSet(Region *region) : region_(region)
{
    allocator_ = region->GetInternalAllocator();
}

template <typename LockConfigT>
RemSet<LockConfigT>::~RemSet()
{
    Clear();
}

template <typename LockConfigT>
void RemSet<LockConfigT>::AddRef(const void *from_field_addr)
{
    auto from_region = Region::AddrToRegion(from_field_addr);
    auto card_ptr = GetCardPtr(from_field_addr);
    auto list = GetCardList(from_region);
    os::memory::LockHolder lock(rem_set_lock_);
    if (list == nullptr) {
        list = allocator_->New<CardList>();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        regions_[from_region] = list;
    }
    if (find(list->begin(), list->end(), card_ptr) == list->end()) {
        list->push_back(card_ptr);
    }
}

template <typename LockConfigT>
void RemSet<LockConfigT>::Clear()
{
    os::memory::LockHolder lock(rem_set_lock_);
    for (auto region_iter : regions_) {
        auto list = region_iter.second;
        allocator_->Delete(list);
    }
    regions_.clear();
}

template <typename LockConfigT>
CardList *RemSet<LockConfigT>::GetCardList(Region *region)
{
    os::memory::LockHolder lock(rem_set_lock_);
    if (regions_.find(region) == regions_.end()) {
        return nullptr;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    return regions_[region];
}

template <typename LockConfigT>
CardPtr RemSet<LockConfigT>::GetCardPtr(const void *addr)
{
    return card_table_ != nullptr ? card_table_->GetCardPtr(ToUintPtr(addr)) : nullptr;
}

template <typename LockConfigT>
MemRange RemSet<LockConfigT>::GetMemoryRange(CardPtr card)
{
    return card_table_->GetMemoryRange(card);
}

/* static */
template <typename LockConfigT>
void RemSet<LockConfigT>::AddRefWithAddr(const void *obj_addr, const void *value_addr)
{
    auto from_region = Region::AddrToRegion(obj_addr);
    // The eden region must be included in the collection set in GC. So, we don't need add ref here.
    if (from_region->IsEden()) {
        return;
    }
    auto to_region = Region::AddrToRegion(value_addr);
    to_region->GetRemSet()->AddRef(obj_addr);
}

/* static */
template <typename LockConfigT>
void RemSet<LockConfigT>::TraverseObjectToAddRef(const void *addr)
{
    auto traverse_object_visitor = [](ObjectHeader *from_object, ObjectHeader *object_to_traverse) {
        AddRefWithAddr(from_object, object_to_traverse);
    };
    auto obj = static_cast<ObjectHeader *>(const_cast<void *>(addr));
    GCStaticObjectHelpers::TraverseAllObjects(obj, traverse_object_visitor);
}

template <typename LockConfigT>
template <typename ObjectVisitor>
inline void RemSet<LockConfigT>::VisitMarkedCards(const ObjectVisitor &object_visitor)
{
    os::memory::LockHolder lock(rem_set_lock_);
    for (auto region_iter : regions_) {
        auto *region = region_iter.first;
        auto *card_list = region_iter.second;
        for (auto card_ptr : *card_list) {
            // visit live objects in old region
            auto mem_range = GetMemoryRange(card_ptr);
            region->GetLiveBitmap()->IterateOverMarkedChunkInRange(
                ToVoidPtr(mem_range.GetStartAddress()), ToVoidPtr(mem_range.GetEndAddress()), object_visitor);
        }
    }
}

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_REM_SET_INL_H_
