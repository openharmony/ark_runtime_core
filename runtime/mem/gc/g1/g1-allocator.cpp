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

#include "runtime/include/mem/allocator-inl.h"
#include "runtime/mem/gc/g1/g1-allocator.h"
#include "runtime/mem/freelist_allocator-inl.h"
#include "runtime/mem/humongous_obj_allocator-inl.h"
#include "runtime/mem/pygote_space_allocator-inl.h"

namespace panda::mem {

template <MTModeT MTMode>
ObjectAllocatorG1<MTMode>::ObjectAllocatorG1(MemStatsType *mem_stats, bool create_pygote_space_allocator)
    : ObjectAllocatorGenBase(mem_stats, GCCollectMode::GC_ALL, create_pygote_space_allocator)
{
    object_allocator_ = MakePandaUnique<ObjectAllocator>(mem_stats);
    nonmovable_allocator_ = MakePandaUnique<NonMovableAllocator>(mem_stats, SpaceType::SPACE_TYPE_NON_MOVABLE_OBJECT);
    humongous_object_allocator_ = MakePandaUnique<HumongousObjectAllocator>(mem_stats);
    mem_stats_ = mem_stats;
}

template <MTModeT MTMode>
size_t ObjectAllocatorG1<MTMode>::GetRegularObjectMaxSize()
{
    return ObjectAllocator::GetMaxRegularObjectSize();
}

template <MTModeT MTMode>
size_t ObjectAllocatorG1<MTMode>::GetLargeObjectMaxSize()
{
    return ObjectAllocator::GetMaxRegularObjectSize();
}

template <MTModeT MTMode>
bool ObjectAllocatorG1<MTMode>::IsAddressInYoungSpace(uintptr_t address)
{
    (void)address;
    return false;
}

template <MTModeT MTMode>
bool ObjectAllocatorG1<MTMode>::HasYoungSpace()
{
    return true;
}

template <MTModeT MTMode>
MemRange ObjectAllocatorG1<MTMode>::GetYoungSpaceMemRange()
{
    return MemRange(0, 1);
}

template <MTModeT MTMode>
TLAB *ObjectAllocatorG1<MTMode>::CreateNewTLAB([[maybe_unused]] panda::ManagedThread *thread)
{
    return object_allocator_->CreateNewTLAB(thread, TLAB_SIZE);
}

template <MTModeT MTMode>
size_t ObjectAllocatorG1<MTMode>::GetTLABMaxAllocSize()
{
    return PANDA_TLAB_MAX_ALLOC_SIZE;
}

template <MTModeT MTMode>
void ObjectAllocatorG1<MTMode>::IterateOverObjectsInRange(MemRange mem_range, const ObjectVisitor &object_visitor)
{
    if (pygote_space_allocator_ != nullptr) {
        pygote_space_allocator_->IterateOverObjectsInRange(object_visitor, ToVoidPtr(mem_range.GetStartAddress()),
                                                           ToVoidPtr(mem_range.GetEndAddress()));
    }
    humongous_object_allocator_->IterateOverObjectsInRange(object_visitor, ToVoidPtr(mem_range.GetStartAddress()),
                                                           ToVoidPtr(mem_range.GetEndAddress()));
}

// ObjectAllocatorGen and ObjectAllocatorNoGen should have inheritance relationship
template <MTModeT MTMode>
bool ObjectAllocatorG1<MTMode>::ContainObject(const ObjectHeader *obj) const
{
    if (pygote_space_allocator_ != nullptr && pygote_space_allocator_->ContainObject(obj)) {
        return true;
    }
    if (object_allocator_->ContainObject(obj)) {
        return true;
    }
    if (humongous_object_allocator_->ContainObject(obj)) {
        return true;
    }

    return false;
}

template <MTModeT MTMode>
bool ObjectAllocatorG1<MTMode>::IsLive(const ObjectHeader *obj)
{
    if (pygote_space_allocator_ != nullptr && pygote_space_allocator_->ContainObject(obj)) {
        return pygote_space_allocator_->IsLive(obj);
    }
    if (object_allocator_->ContainObject(obj)) {
        return object_allocator_->IsLive(obj);
    }
    if (humongous_object_allocator_->ContainObject(obj)) {
        return humongous_object_allocator_->IsLive(obj);
    }

    return false;
}

template <MTModeT MTMode>
void *ObjectAllocatorG1<MTMode>::Allocate(size_t size, Alignment align, [[maybe_unused]] panda::ManagedThread *thread)
{
    void *mem = nullptr;
    size_t aligned_size = AlignUp(size, GetAlignmentInBytes(align));
    if (LIKELY(aligned_size <= REGION_SIZE)) {
        mem = object_allocator_->Alloc(size, align);
    } else {
        mem = AllocateTenured(size);
    }
    return mem;
}

template <MTModeT MTMode>
void *ObjectAllocatorG1<MTMode>::AllocateNonMovable(size_t size, Alignment align,
                                                    [[maybe_unused]] panda::ManagedThread *thread)
{
    // before pygote fork, allocate small non-movable objects in pygote space
    if (UNLIKELY(IsPygoteAllocEnabled() && pygote_space_allocator_->CanAllocNonMovable(size, align))) {
        return pygote_space_allocator_->Alloc(size, align);
    }
    void *mem = nullptr;
    size_t aligned_size = AlignUp(size, GetAlignmentInBytes(align));
    if (aligned_size <= ObjectAllocator::GetMaxRegularObjectSize()) {
        mem = nonmovable_allocator_->Alloc(aligned_size, align);
    } else {
        // We don't need special allocator for this
        // Humongous objects are non-movable
        size_t pool_size = std::max(PANDA_DEFAULT_POOL_SIZE, HumongousObjectAllocator::GetMinPoolSize(size));
        mem = AllocateSafe(size, align, humongous_object_allocator_.get(), pool_size,
                           SpaceType::SPACE_TYPE_HUMONGOUS_OBJECT);
    }
    return mem;
}

template <MTModeT MTMode>
void *ObjectAllocatorG1<MTMode>::AllocateTenured(size_t size)
{
    void *mem = nullptr;
    Alignment align = DEFAULT_ALIGNMENT;
    size_t aligned_size = AlignUp(size, GetAlignmentInBytes(align));
    if (aligned_size <= ObjectAllocator::GetMaxRegularObjectSize()) {
        mem = object_allocator_->Alloc<RegionFlag::IS_OLD>(size, align);
    } else {
        size_t pool_size = std::max(PANDA_DEFAULT_POOL_SIZE, HumongousObjectAllocator::GetMinPoolSize(size));
        mem = AllocateSafe(size, align, humongous_object_allocator_.get(), pool_size,
                           SpaceType::SPACE_TYPE_HUMONGOUS_OBJECT);
    }
    return mem;
}

template <MTModeT MTMode>
void ObjectAllocatorG1<MTMode>::VisitAndRemoveAllPools(const MemVisitor &mem_visitor)
{
    if (pygote_space_allocator_ != nullptr) {
        pygote_space_allocator_->VisitAndRemoveAllPools(mem_visitor);
    }
    object_allocator_->VisitAndRemoveAllPools(mem_visitor);
    humongous_object_allocator_->VisitAndRemoveAllPools(mem_visitor);
}

template <MTModeT MTMode>
void ObjectAllocatorG1<MTMode>::VisitAndRemoveFreePools(const MemVisitor &mem_visitor)
{
    if (pygote_space_allocator_ != nullptr) {
        pygote_space_allocator_->VisitAndRemoveFreePools(mem_visitor);
    }
    humongous_object_allocator_->VisitAndRemoveFreePools(mem_visitor);
}

template <MTModeT MTMode>
void ObjectAllocatorG1<MTMode>::IterateOverYoungObjects(const ObjectVisitor &object_visitor)
{
    // Use CompactAllSpecificRegions method to compact young regions.
    (void)object_visitor;
    UNREACHABLE();
}

template <MTModeT MTMode>
void ObjectAllocatorG1<MTMode>::IterateOverTenuredObjects(const ObjectVisitor &object_visitor)
{
    if (pygote_space_allocator_ != nullptr) {
        pygote_space_allocator_->IterateOverObjects(object_visitor);
    }
    object_allocator_->IterateOverObjects(object_visitor);
    humongous_object_allocator_->IterateOverObjects(object_visitor);
}

template <MTModeT MTMode>
void ObjectAllocatorG1<MTMode>::IterateOverObjects(const ObjectVisitor &object_visitor)
{
    if (pygote_space_allocator_ != nullptr) {
        pygote_space_allocator_->IterateOverObjects(object_visitor);
    }
    object_allocator_->IterateOverObjects(object_visitor);
    humongous_object_allocator_->IterateOverObjects(object_visitor);
}

template <MTModeT MTMode>
void ObjectAllocatorG1<MTMode>::IterateRegularSizeObjects(const ObjectVisitor &object_visitor)
{
    object_allocator_->IterateOverObjects(object_visitor);
}

template <MTModeT MTMode>
void ObjectAllocatorG1<MTMode>::IterateNonRegularSizeObjects(const ObjectVisitor &object_visitor)
{
    if (pygote_space_allocator_ != nullptr) {
        pygote_space_allocator_->IterateOverObjects(object_visitor);
    }
    humongous_object_allocator_->IterateOverObjects(object_visitor);
}

template <MTModeT MTMode>
void ObjectAllocatorG1<MTMode>::FreeObjectsMovedToPygoteSpace()
{
    // Clear allocator because we have moved all objects in it to pygote space
    object_allocator_.reset(new (std::nothrow) ObjectAllocator(mem_stats_));
}

template <MTModeT MTMode>
void ObjectAllocatorG1<MTMode>::Collect(const GCObjectVisitor &gc_object_visitor, GCCollectMode collect_mode)
{
    switch (collect_mode) {
        case GCCollectMode::GC_MINOR:
            break;
        case GCCollectMode::GC_ALL:
        case GCCollectMode::GC_MAJOR:
            if (pygote_space_allocator_ != nullptr) {
                pygote_space_allocator_->Collect(gc_object_visitor);
            }
            humongous_object_allocator_->Collect(gc_object_visitor);
            break;
        case GCCollectMode::GC_FULL:
            UNREACHABLE();
            break;
        case GC_NONE:
            UNREACHABLE();
            break;
        default:
            UNREACHABLE();
    }
}

template <MTModeT MTMode>
void ObjectAllocatorG1<MTMode>::ResetYoungAllocator()
{
    object_allocator_->ResetAllSpecificRegions<RegionFlag::IS_EDEN>();
}

template <MTModeT MTMode>
bool ObjectAllocatorG1<MTMode>::IsObjectInNonMovableSpace(const ObjectHeader *obj)
{
    return nonmovable_allocator_->ContainObject(obj);
}

template class ObjectAllocatorG1<MT_MODE_SINGLE>;
template class ObjectAllocatorG1<MT_MODE_MULTI>;

}  // namespace panda::mem
