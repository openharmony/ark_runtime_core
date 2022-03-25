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

#ifndef PANDA_RUNTIME_MEM_REGION_SPACE_INL_H_
#define PANDA_RUNTIME_MEM_REGION_SPACE_INL_H_

#include "runtime/mem/region_space.h"

namespace panda::mem {

class RegionAllocCheck {
public:
    explicit RegionAllocCheck(Region *region) : region_(region)
    {
        ASSERT(region_->SetAllocating(true));
    }
    ~RegionAllocCheck()
    {
        ASSERT(region_->SetAllocating(false));
    }
    NO_COPY_SEMANTIC(RegionAllocCheck);
    NO_MOVE_SEMANTIC(RegionAllocCheck);

private:
    Region *region_ FIELD_UNUSED;
};

class RegionIterateCheck {
public:
    explicit RegionIterateCheck(Region *region) : region_(region)
    {
        ASSERT(region_->SetIterating(true));
    }
    ~RegionIterateCheck()
    {
        ASSERT(region_->SetIterating(false));
    }
    NO_COPY_SEMANTIC(RegionIterateCheck);
    NO_MOVE_SEMANTIC(RegionIterateCheck);

private:
    Region *region_ FIELD_UNUSED;
};

template <bool atomic>
void *Region::Alloc(size_t size, Alignment align)
{
    RegionAllocCheck alloc(this);
    uintptr_t old_top;
    uintptr_t new_top;
    size_t aligned_size = AlignUp(size, GetAlignmentInBytes(align));
    if (atomic) {
        auto atomic_top = reinterpret_cast<std::atomic<uintptr_t> *>(&top_);
        old_top = atomic_top->load(std::memory_order_relaxed);
        do {
            new_top = old_top + aligned_size;
            if (UNLIKELY(new_top > end_)) {
                return nullptr;
            }
        } while (!atomic_top->compare_exchange_weak(old_top, new_top, std::memory_order_relaxed));
        return ToVoidPtr(old_top);
    }
    new_top = top_ + aligned_size;
    if (UNLIKELY(new_top > end_)) {
        return nullptr;
    }
    old_top = top_;
    top_ = new_top;
    return ToVoidPtr(old_top);
}

template <typename ObjectVisitor>
void Region::IterateOverObjects(const ObjectVisitor &visitor)
{
    // currently just for gc stw phase, so check it is not in allocating state
    RegionIterateCheck iterate(this);
    auto cur_ptr = Begin();
    auto end_ptr = Top();
    while (cur_ptr < end_ptr) {
        auto object_header = reinterpret_cast<ObjectHeader *>(cur_ptr);
        size_t object_size = GetObjectSize(object_header);
        visitor(object_header);
        cur_ptr = AlignUp(cur_ptr + object_size, DEFAULT_ALIGNMENT_IN_BYTES);
    }
    if (tlab_ != nullptr) {
        tlab_->IterateOverObjects(visitor);
    }
}

template <typename RegionVisitor>
void RegionSpace::IterateRegions(RegionVisitor visitor)
{
    auto it = regions_.begin();
    while (it != regions_.end()) {
        auto *region = Region::AsRegion(&(*it));
        ++it;  // increase before visitor which may remove region
        visitor(region);
    }
}

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_REGION_SPACE_INL_H_
