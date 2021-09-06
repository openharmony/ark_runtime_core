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

#ifndef PANDA_RUNTIME_MEM_REGION_ALLOCATOR_INL_H_
#define PANDA_RUNTIME_MEM_REGION_ALLOCATOR_INL_H_

#include "libpandabase/mem/mem.h"
#include "libpandabase/utils/logger.h"
#include "runtime/include/runtime.h"
#include "runtime/include/thread.h"
#include "runtime/mem/region_allocator.h"
#include "runtime/mem/region_space-inl.h"
#include "runtime/mem/runslots_allocator-inl.h"
#include "runtime/mem/freelist_allocator-inl.h"
#include "runtime/mem/alloc_config.h"

namespace panda::mem {

template <typename LockConfigT>
RegionAllocatorBase<LockConfigT>::RegionAllocatorBase(MemStatsType *mem_stats, SpaceType space_type,
                                                      AllocatorType allocator_type, size_t init_space_size, bool extend,
                                                      size_t region_size)
    : mem_stats_(mem_stats),
      space_type_(space_type),
      region_pool_(region_size, extend, InternalAllocatorPtr(InternalAllocator<>::GetInternalAllocatorFromRuntime())),
      region_space_(space_type, allocator_type, &region_pool_),
      init_block_(0, nullptr)
{
    ASSERT(space_type_ == SpaceType::SPACE_TYPE_OBJECT || space_type_ == SpaceType::SPACE_TYPE_NON_MOVABLE_OBJECT);
    init_block_ = NULLPOOL;
    if (init_space_size > 0) {
        ASSERT(init_space_size % region_size == 0);
        init_block_ = PoolManager::GetMmapMemPool()->AllocPool(init_space_size, space_type,
                                                               panda::AllocatorType::REGION_ALLOCATOR, this);
        ASSERT(init_block_.GetMem() != nullptr);
        ASSERT(init_block_.GetSize() >= init_space_size);
        if (init_block_.GetMem() != nullptr) {
            region_pool_.InitRegionBlock(ToUintPtr(init_block_.GetMem()),
                                         ToUintPtr(init_block_.GetMem()) + init_space_size);
        }
    }
}

template <typename LockConfigT>
RegionAllocatorBase<LockConfigT>::RegionAllocatorBase(MemStatsType *mem_stats, SpaceType space_type,
                                                      AllocatorType allocator_type, RegionPool *shared_region_pool)
    : mem_stats_(mem_stats),
      space_type_(space_type),
      region_pool_(0, false, nullptr),  // unused
      region_space_(space_type, allocator_type, shared_region_pool),
      init_block_(0, nullptr)  // unused
{
    ASSERT(space_type_ == SpaceType::SPACE_TYPE_OBJECT || space_type_ == SpaceType::SPACE_TYPE_NON_MOVABLE_OBJECT);
}

template <typename AllocConfigT, typename LockConfigT>
RegionAllocator<AllocConfigT, LockConfigT>::RegionAllocator(MemStatsType *mem_stats, SpaceType space_type,
                                                            size_t init_space_size, bool extend)
    : RegionAllocatorBase<LockConfigT>(mem_stats, space_type, AllocatorType::REGION_ALLOCATOR, init_space_size, extend,
                                       REGION_SIZE),
      full_region_(nullptr, 0, 0),
      eden_current_region_(&full_region_),
      old_current_region_(&full_region_)
{
}

template <typename AllocConfigT, typename LockConfigT>
RegionAllocator<AllocConfigT, LockConfigT>::RegionAllocator(MemStatsType *mem_stats, SpaceType space_type,
                                                            RegionPool *shared_region_pool)
    : RegionAllocatorBase<LockConfigT>(mem_stats, space_type, AllocatorType::REGION_ALLOCATOR, shared_region_pool),
      full_region_(nullptr, 0, 0),
      eden_current_region_(&full_region_),
      old_current_region_(&full_region_)
{
}

template <typename AllocConfigT, typename LockConfigT>
template <RegionFlag region_type>
void *RegionAllocator<AllocConfigT, LockConfigT>::AllocRegular(size_t align_size)
{
    static constexpr bool is_atomic = std::is_same_v<LockConfigT, RegionAllocatorLockConfig::CommonLock>;
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE)
    void *mem = GetCurrentRegion<is_atomic, region_type>()->template Alloc<is_atomic>(align_size);
    if (mem == nullptr) {
        os::memory::LockHolder lock(this->region_lock_);
        // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE)
        mem = GetCurrentRegion<is_atomic, region_type>()->template Alloc<is_atomic>(align_size);
        if (mem == nullptr) {
            Region *region = this->AllocRegion(REGION_SIZE);
            if (LIKELY(region != nullptr)) {
                region->CreateRemSet();
                region->CreateMarkBitmap();
                region->AddFlag(region_type);
                // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE)
                mem = region->template Alloc<false>(align_size);
                SetCurrentRegion<is_atomic, region_type>(region);
            }
        }
    }
    return mem;
}

template <typename AllocConfigT, typename LockConfigT>
template <RegionFlag region_type>
void *RegionAllocator<AllocConfigT, LockConfigT>::Alloc(size_t size, Alignment align)
{
    ASSERT(GetAlignmentInBytes(align) % GetAlignmentInBytes(DEFAULT_ALIGNMENT) == 0);
    size_t align_size = AlignUp(size, GetAlignmentInBytes(align));
    void *mem = nullptr;
    // for movable & regular size object, allocate it from a region
    // for nonmovable or large size object, allocate a seprate large region for it
    if (this->GetSpaceType() != SpaceType::SPACE_TYPE_NON_MOVABLE_OBJECT &&
        LIKELY(align_size <= GetMaxRegularObjectSize())) {
        mem = AllocRegular<region_type>(align_size);
    } else {
        os::memory::LockHolder lock(this->region_lock_);
        Region *region = this->AllocRegion(Region::RegionSize(align_size, REGION_SIZE));
        if (LIKELY(region != nullptr)) {
            region->CreateRemSet();
            region->CreateMarkBitmap();
            region->AddFlag(region_type);
            region->AddFlag(RegionFlag::IS_LARGE_OBJECT);
            mem = region->Alloc<false>(align_size);
        }
    }
    if (mem != nullptr) {
        AllocConfigT::OnAlloc(align_size, this->space_type_, this->mem_stats_);
        AllocConfigT::MemoryInit(mem, size);
    }
    return mem;
}

template <typename AllocConfigT, typename LockConfigT>
TLAB *RegionAllocator<AllocConfigT, LockConfigT>::CreateNewTLAB(panda::ManagedThread *thread, size_t size)
{
    ASSERT(size <= GetMaxRegularObjectSize());

    // Firstly revoke current tlab
    RevokeTLAB(thread);

    Region *region = nullptr;
    size_t aligned_size = AlignUp(size, GetAlignmentInBytes(DEFAULT_ALIGNMENT));

    {
        os::memory::LockHolder lock(this->region_lock_);
        // first search in partial tlab map
        if (USE_PARTIAL_TLAB) {
            auto largest_tlab = retained_tlabs_.begin();
            if (largest_tlab != retained_tlabs_.end() && largest_tlab->first <= aligned_size) {
                region = largest_tlab->second;
                retained_tlabs_.erase(largest_tlab);
            }
        }

        // allocate a free region if none partial tlab has enough space
        if (region == nullptr) {
            region = this->AllocRegion(REGION_SIZE);
            if (LIKELY(region != nullptr)) {
                region->CreateRemSet();
                region->CreateMarkBitmap();
                region->AddFlag(RegionFlag::IS_EDEN);
            }
        }
    }

    auto tlab = thread->GetTLAB();
    ASSERT(tlab != nullptr);
    if (region != nullptr) {
        auto top = region->Top();
        auto end = region->End();
        region->SetTLAB(tlab);
        // left space for tlab
        tlab->Fill(ToVoidPtr(top), end - top);
    }

    return tlab;
}

template <typename AllocConfigT, typename LockConfigT>
void RegionAllocator<AllocConfigT, LockConfigT>::RevokeTLAB(panda::ManagedThread *thread)
{
    auto tlab = thread->GetTLAB();
    ASSERT(tlab != nullptr);
    if (tlab->IsEmpty()) {
        return;
    }

    // Return unused bytes to region
    Region *r = Region::AddrToRegion(tlab->GetStartAddr());
    r->SetTop(ToUintPtr(tlab->GetCurPos()));
    r->SetTLAB(nullptr);
    tlab->Reset();

    // if remaining size is greater than retire threshold, we store it for later reuse.
    auto remaining_size = r->End() - r->Top();
    if (USE_PARTIAL_TLAB && remaining_size > TLAB_RETIRE_THRESHOLD) {
        os::memory::LockHolder lock(this->region_lock_);
        retained_tlabs_.insert(std::make_pair(remaining_size, r));
    }
}

template <typename AllocConfigT, typename LockConfigT>
PandaVector<Region *> RegionAllocator<AllocConfigT, LockConfigT>::GetTopGarbageRegions(size_t region_count)
{
    PandaPriorityQueue<std::pair<uint32_t, Region *>> queue;
    this->GetSpace()->IterateRegions([&](Region *region) {
        auto garbage_bytes = region->GetGarbageBytes();
        queue.push(std::pair<uint32_t, Region *>(garbage_bytes, region));
    });
    PandaVector<Region *> regions;
    for (size_t i = 0; i < region_count; i++) {
        auto *region = queue.top().second;
        regions.push_back(region);
        queue.pop();
    }
    return regions;
}

template <typename AllocConfigT, typename LockConfigT>
template <RegionFlag regions_type>
PandaVector<Region *> RegionAllocator<AllocConfigT, LockConfigT>::GetAllSpecificRegions()
{
    PandaVector<Region *> vector;
    this->GetSpace()->IterateRegions([&](Region *region) {
        if (region->HasFlag(regions_type)) {
            vector.push_back(region);
        }
    });
    return vector;
}

template <typename AllocConfigT, typename LockConfigT>
template <RegionFlag regions_type_from, RegionFlag regions_type_to, bool use_marked_bitmap>
void RegionAllocator<AllocConfigT, LockConfigT>::CompactAllSpecificRegions(const GCObjectVisitor &death_checker)
{
    if constexpr (regions_type_from == regions_type_to) {
        // There is an issue with IterateRegions during creating a new one.
        ASSERT(regions_type_from != regions_type_to);
        SetCurrentRegion<false, regions_type_to>(&full_region_);
    }
    auto visitor = [&](ObjectHeader *object) {
        if (death_checker(object) == ObjectStatus::ALIVE_OBJECT) {
            size_t object_size = GetObjectSize(object);
            void *dst = this->Alloc<regions_type_to>(object_size);
            ASSERT(dst != nullptr);
            (void)memcpy_s(dst, object_size, object, object_size);
        }
    };
    this->GetSpace()->IterateRegions([&](Region *region) {
        if (!region->HasFlag(regions_type_from)) {
            return;
        }
        if constexpr (use_marked_bitmap) {
            region->GetMarkBitmap()->IterateOverMarkedChunks(
                [&](void *object_addr) { visitor(static_cast<ObjectHeader *>(object_addr)); });
        } else {
            region->IterateOverObjects(visitor);
        }
    });
}

template <typename AllocConfigT, typename LockConfigT>
template <RegionFlag regions_type_from, RegionFlag regions_type_to, bool use_marked_bitmap>
void RegionAllocator<AllocConfigT, LockConfigT>::CompactSeveralSpecificRegions(const PandaVector<Region *> &regions,
                                                                               const GCObjectVisitor &death_checker)
{
    if constexpr (regions_type_from == regions_type_to) {
        auto cur_region = std::find(regions.begin(), regions.end(), GetCurrentRegion<false, regions_type_to>());
        if (cur_region != regions.end()) {
            SetCurrentRegion<false, regions_type_to>(&full_region_);
        }
    }
    auto visitor = [&](ObjectHeader *object) {
        if (death_checker(object) == ObjectStatus::ALIVE_OBJECT) {
            size_t object_size = GetObjectSize(object);
            void *dst = this->Alloc<regions_type_to>(object_size);
            ASSERT(dst != nullptr);
            (void)memcpy_s(dst, object_size, object, object_size);
        }
    };
    for (auto i : regions) {
        ASSERT(i->HasFlag(regions_type_from));
        if constexpr (use_marked_bitmap) {
            i->GetMarkBitmap()->IterateOverMarkedChunks(
                [&](void *object_addr) { visitor(static_cast<ObjectHeader *>(object_addr)); });
        } else {
            i->IterateOverObjects(visitor);
        }
    }
}

template <typename AllocConfigT, typename LockConfigT>
template <RegionFlag regions_type>
void RegionAllocator<AllocConfigT, LockConfigT>::ResetAllSpecificRegions()
{
    SetCurrentRegion<false, regions_type>(&full_region_);
    this->GetSpace()->IterateRegions([&](Region *region) {
        if (!region->HasFlag(regions_type)) {
            return;
        }
        this->GetSpace()->FreeRegion(region);
    });
}

template <typename AllocConfigT, typename LockConfigT>
template <RegionFlag regions_type>
void RegionAllocator<AllocConfigT, LockConfigT>::ResetSeveralSpecificRegions(const PandaVector<Region *> &regions)
{
    auto cur_region = std::find(regions.begin(), regions.end(), GetCurrentRegion<false, regions_type>());
    if (cur_region != regions.end()) {
        SetCurrentRegion<false, regions_type>(&full_region_);
    }
    for (auto i : regions) {
        ASSERT(i->HasFlag(regions_type));
        this->GetSpace()->FreeRegion(i);
    }
}

template <typename AllocConfigT, typename LockConfigT, typename ObjectAllocator>
RegionNonmovableAllocator<AllocConfigT, LockConfigT, ObjectAllocator>::RegionNonmovableAllocator(
    MemStatsType *mem_stats, SpaceType space_type, size_t init_space_size, bool extend)
    : RegionAllocatorBase<LockConfigT>(mem_stats, space_type, ObjectAllocator::GetAllocatorType(), init_space_size,
                                       extend, REGION_SIZE),
      object_allocator_(mem_stats)
{
}

template <typename AllocConfigT, typename LockConfigT, typename ObjectAllocator>
RegionNonmovableAllocator<AllocConfigT, LockConfigT, ObjectAllocator>::RegionNonmovableAllocator(
    MemStatsType *mem_stats, SpaceType space_type, RegionPool *shared_region_pool)
    : RegionAllocatorBase<LockConfigT>(mem_stats, space_type, ObjectAllocator::GetAllocatorType(), shared_region_pool),
      object_allocator_(mem_stats)
{
}

template <typename AllocConfigT, typename LockConfigT, typename ObjectAllocator>
void *RegionNonmovableAllocator<AllocConfigT, LockConfigT, ObjectAllocator>::Alloc(size_t size, Alignment align)
{
    ASSERT(GetAlignmentInBytes(align) % GetAlignmentInBytes(DEFAULT_ALIGNMENT) == 0);
    size_t align_size = AlignUp(size, GetAlignmentInBytes(align));
    ASSERT(align_size <= ObjectAllocator::GetMaxSize());

    void *mem = object_allocator_.Alloc(align_size);
    if (UNLIKELY(mem == nullptr)) {
        mem = NewRegionAndRetryAlloc(size, align);
        if (UNLIKELY(mem == nullptr)) {
            return nullptr;
        }
    }

    AllocConfigT::OnAlloc(align_size, this->space_type_, this->mem_stats_);
    AllocConfigT::MemoryInit(mem, size);
    return mem;
}

template <typename AllocConfigT, typename LockConfigT, typename ObjectAllocator>
void *RegionNonmovableAllocator<AllocConfigT, LockConfigT, ObjectAllocator>::NewRegionAndRetryAlloc(size_t object_size,
                                                                                                    Alignment align)
{
    os::memory::LockHolder lock(this->region_lock_);
    size_t pool_head_size = AlignUp(Region::HeadSize(), ObjectAllocator::PoolAlign());
    ASSERT(AlignUp(pool_head_size + object_size, REGION_SIZE) == REGION_SIZE);
    while (true) {
        Region *region = this->AllocRegion(REGION_SIZE);
        if (UNLIKELY(region == nullptr)) {
            return nullptr;
        }
        // no remset for nonmovable region
        region->CreateMarkBitmap();
        region->AddFlag(RegionFlag::IS_NONMOVABLE);
        uintptr_t aligned_pool = ToUintPtr(region) + pool_head_size;
        bool added_memory_pool = object_allocator_.AddMemoryPool(ToVoidPtr(aligned_pool), REGION_SIZE - pool_head_size);
        ASSERT(added_memory_pool);
        if (UNLIKELY(!added_memory_pool)) {
            LOG(FATAL, ALLOC) << "ObjectAllocator: couldn't add memory pool to allocator";
        }
        void *mem = object_allocator_.Alloc(object_size, align);
        if (LIKELY(mem != nullptr)) {
            return mem;
        }
    }
    return nullptr;
}

template <typename AllocConfigT, typename LockConfigT>
using RegionRunslotsAllocator = RegionNonmovableAllocator<AllocConfigT, LockConfigT, RunSlotsAllocator<AllocConfigT>>;

template <typename AllocConfigT, typename LockConfigT>
using RegionFreeListAllocator = RegionNonmovableAllocator<AllocConfigT, LockConfigT, FreeListAllocator<AllocConfigT>>;

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_REGION_ALLOCATOR_INL_H_
