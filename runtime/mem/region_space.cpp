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

#include "runtime/mem/region_space-inl.h"
#include "runtime/mem/rem_set-inl.h"

namespace panda::mem {

InternalAllocatorPtr Region::GetInternalAllocator()
{
    return space_->GetPool()->GetInternalAllocator();
}

void Region::CreateRemSet()
{
    ASSERT(rem_set_ == nullptr);
    rem_set_ = GetInternalAllocator()->New<RemSetT>(this);
}

MarkBitmap *Region::CreateMarkBitmap()
{
    if (mark_bitmap_ == nullptr) {
        auto allocator = GetInternalAllocator();
        auto bitmap_data = allocator->Alloc(MarkBitmap::GetBitMapSizeInByte(Size()));
        ASSERT(bitmap_data != nullptr);
        mark_bitmap_ = allocator->New<MarkBitmap>(this, Size(), bitmap_data);
        ASSERT(mark_bitmap_ != nullptr);
    }
    mark_bitmap_->ClearAllBits();
    return mark_bitmap_;
}

void Region::SetMarkBit(ObjectHeader *object)
{
    ASSERT(IsInRange(object));
    mark_bitmap_->Set(object);
}

uint32_t Region::CalcLiveBytes() const
{
    ASSERT(live_bitmap_ != nullptr);
    uint32_t live_bytes = 0;
    live_bitmap_->IterateOverMarkedChunks(
        [&live_bytes](const void *object) { live_bytes += GetAlignedObjectSize(GetObjectSize(object)); });
    return live_bytes;
}

void Region::Destroy()
{
    auto allocator = GetInternalAllocator();
    if (rem_set_ != nullptr) {
        allocator->Delete(rem_set_);
        rem_set_ = nullptr;
    }
    if (live_bitmap_ != nullptr) {
        allocator->Delete(live_bitmap_->GetBitMap().data());
        allocator->Delete(live_bitmap_);
        live_bitmap_ = nullptr;
    }
    if (mark_bitmap_ != nullptr) {
        allocator->Delete(mark_bitmap_->GetBitMap().data());
        allocator->Delete(mark_bitmap_);
        mark_bitmap_ = nullptr;
    }
}

void RegionBlock::Init(uintptr_t regions_begin, uintptr_t regions_end)
{
    os::memory::LockHolder lock(lock_);
    ASSERT(occupied_.Empty());
    ASSERT(region_size_ > 0);
    ASSERT(Region::IsAlignment(regions_begin, region_size_));
    ASSERT((regions_end - regions_begin) % region_size_ == 0);
    size_t num_regions = (regions_end - regions_begin) / region_size_;
    if (num_regions > 0) {
        size_t size = num_regions * sizeof(Region *);
        auto data = reinterpret_cast<Region **>(allocator_->Alloc(size));
        (void)memset_s(data, size, 0, size);
        occupied_ = Span<Region *>(data, num_regions);
        regions_begin_ = regions_begin;
        regions_end_ = regions_end;
    }
}

Region *RegionBlock::AllocRegion()
{
    os::memory::LockHolder lock(lock_);
    for (size_t i = 0; i < occupied_.Size(); ++i) {
        if (occupied_[i] == nullptr) {
            auto *region = RegionAt(i);
            occupied_[i] = region;
            num_used_regions_++;
            return region;
        }
    }
    return nullptr;
}

Region *RegionBlock::AllocLargeRegion(size_t large_region_size)
{
    os::memory::LockHolder lock(lock_);
    ASSERT(region_size_ > 0);
    size_t alloc_region_num = large_region_size / region_size_;
    size_t left = 0;
    while (left + alloc_region_num <= occupied_.Size()) {
        bool found = true;
        size_t right = left;
        while (right < left + alloc_region_num) {
            if (occupied_[right] != nullptr) {
                found = false;
                break;
            }
            ++right;
        }
        if (found) {
            // mark those regions as 'used'
            auto *region = RegionAt(left);
            for (size_t i = 0; i < alloc_region_num; i++) {
                occupied_[left + i] = region;
            }
            num_used_regions_ += alloc_region_num;
            return region;
        }
        // next round
        left = right + 1;
    }
    return nullptr;
}

void RegionBlock::FreeRegion(Region *region, bool release_pages)
{
    os::memory::LockHolder lock(lock_);
    ASSERT(region_size_ > 0);
    size_t region_idx = RegionIndex(region);
    size_t region_num = region->Size() / region_size_;
    ASSERT(region_idx + region_num <= occupied_.Size());
    for (size_t i = 0; i < region_num; i++) {
        ASSERT(occupied_[region_idx + i] == region);
        occupied_[region_idx + i] = nullptr;
    }
    num_used_regions_ -= region_num;
    if (release_pages) {
        os::mem::ReleasePages(ToUintPtr(region), region->End());
    }
}

Region *RegionPool::NewRegion(RegionSpace *space, SpaceType space_type, AllocatorType allocator_type,
                              size_t region_size)
{
    ASSERT(region_size_ > 0);
    // check that the input region_size is aligned
    ASSERT(region_size % region_size_ == 0);

    // 1.get region from pre-allocated region block(e.g. a big mmaped continuous space)
    void *region = nullptr;
    if (block_.GetFreeRegionsNum() > 0) {
        region = (region_size <= region_size_) ? block_.AllocRegion() : block_.AllocLargeRegion(region_size);
    }

    // 2.mmap region directly, this is more flexible for memory usage
    if (region == nullptr && extend_) {
        region = PoolManager::GetMmapMemPool()->AllocPool(region_size, space_type, allocator_type, this).GetMem();
    }

    if (UNLIKELY(region == nullptr)) {
        return nullptr;
    }

    ASSERT(Region::IsAlignment(ToUintPtr(region), region_size_));

    // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
    return new (region) Region(space, ToUintPtr(region) + Region::HeadSize(), ToUintPtr(region) + region_size);
}

void RegionPool::FreeRegion(Region *region, bool release_pages)
{
    if (block_.IsAddrInRange(region)) {
        block_.FreeRegion(region, release_pages);
    } else {
        PoolManager::GetMmapMemPool()->FreePool(region, region->Size());
    }
}

Region *RegionSpace::NewRegion(size_t region_size)
{
    auto *region = region_pool_->NewRegion(this, space_type_, allocator_type_, region_size);
    if (UNLIKELY(region == nullptr)) {
        return nullptr;
    }
    regions_.push_back(region->AsListNode());
    return region;
}

void RegionSpace::FreeRegion(Region *region)
{
    ASSERT(region->GetSpace() == this);
    regions_.erase(region->AsListNode());
    DestroyRegion(region);
}

void RegionSpace::FreeAllRegions()
{
    // delete all regions
    IterateRegions([this](Region *region) { FreeRegion(region); });
}

}  // namespace panda::mem
