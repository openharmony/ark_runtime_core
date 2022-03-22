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
// These includes to avoid linker error:

#include "runtime/include/mem/allocator.h"
#include "runtime/include/mem/allocator-inl.h"

#include "mem/mem_pool.h"
#include "mem/mem_config.h"
#include "runtime/include/runtime.h"
#include "runtime/include/panda_vm.h"
#include "runtime/mem/bump-allocator-inl.h"
#include "runtime/mem/freelist_allocator-inl.h"
#include "runtime/mem/internal_allocator-inl.h"
#include "runtime/mem/runslots_allocator-inl.h"
#include "runtime/mem/pygote_space_allocator-inl.h"
#include "runtime/mem/tlab.h"

namespace panda::mem {

Allocator::~Allocator() = default;

ObjectAllocatorBase::ObjectAllocatorBase(MemStatsType *mem_stats, GCCollectMode gc_collect_mode,
                                         bool create_pygote_space_allocator)
    : Allocator(mem_stats, AllocatorPurpose::ALLOCATOR_PURPOSE_OBJECT, gc_collect_mode)
{
    if (create_pygote_space_allocator) {
        pygote_space_allocator_ = new (std::nothrow) PygoteAllocator(mem_stats);
        pygote_alloc_enabled_ = true;
    }
}

ObjectAllocatorBase::~ObjectAllocatorBase()
{
    // NOLINTNEXTLINE(readability-delete-null-pointer)
    if (pygote_space_allocator_ != nullptr) {
        delete pygote_space_allocator_;
    }
}

template <MTModeT MTMode>
ObjectAllocatorNoGen<MTMode>::ObjectAllocatorNoGen(MemStatsType *mem_stats, bool create_pygote_space_allocator)
    : ObjectAllocatorBase(mem_stats, GCCollectMode::GC_ALL, create_pygote_space_allocator)
{
    object_allocator_ = new (std::nothrow) ObjectAllocator(mem_stats);
    large_object_allocator_ = new (std::nothrow) LargeObjectAllocator(mem_stats);
    humongous_object_allocator_ = new (std::nothrow) HumongousObjectAllocator(mem_stats);
}

template <MTModeT MTMode>
ObjectAllocatorNoGen<MTMode>::~ObjectAllocatorNoGen()
{
    delete object_allocator_;
    delete large_object_allocator_;
    delete humongous_object_allocator_;
}

template <MTModeT MTMode>
void *ObjectAllocatorNoGen<MTMode>::Allocate(size_t size, Alignment align,
                                             [[maybe_unused]] panda::ManagedThread *thread)
{
    void *mem = nullptr;
    size_t aligned_size;
    aligned_size = AlignUp(size, GetAlignmentInBytes(align));
    if (aligned_size <= ObjectAllocator::GetMaxSize()) {
        size_t pool_size = std::max(PANDA_DEFAULT_POOL_SIZE, ObjectAllocator::GetMinPoolSize());
        mem = AllocateSafe(size, align, object_allocator_, pool_size, SpaceType::SPACE_TYPE_OBJECT);
    } else if (aligned_size <= LargeObjectAllocator::GetMaxSize()) {
        size_t pool_size = std::max(PANDA_DEFAULT_POOL_SIZE, LargeObjectAllocator::GetMinPoolSize());
        mem = AllocateSafe(size, align, large_object_allocator_, pool_size, SpaceType::SPACE_TYPE_OBJECT);
    } else {
        size_t pool_size = std::max(PANDA_DEFAULT_POOL_SIZE, HumongousObjectAllocator::GetMinPoolSize(size));
        mem = AllocateSafe(size, align, humongous_object_allocator_, pool_size, SpaceType::SPACE_TYPE_HUMONGOUS_OBJECT);
    }
    return mem;
}

template <MTModeT MTMode>
void *ObjectAllocatorNoGen<MTMode>::AllocateNonMovable(size_t size, Alignment align, panda::ManagedThread *thread)
{
    // Before pygote fork, allocate small non-movable objects in pygote space
    if (UNLIKELY(IsPygoteAllocEnabled() && pygote_space_allocator_->CanAllocNonMovable(size, align))) {
        return pygote_space_allocator_->Alloc(size, align);
    }
    // Without generations - no compaction now, so all allocations are non-movable
    return Allocate(size, align, thread);
}

template <MTModeT MTMode>
void ObjectAllocatorNoGen<MTMode>::VisitAndRemoveAllPools(const MemVisitor &mem_visitor)
{
    if (pygote_space_allocator_ != nullptr) {
        pygote_space_allocator_->VisitAndRemoveAllPools(mem_visitor);
    }
    object_allocator_->VisitAndRemoveAllPools(mem_visitor);
    large_object_allocator_->VisitAndRemoveAllPools(mem_visitor);
    humongous_object_allocator_->VisitAndRemoveAllPools(mem_visitor);
}

template <MTModeT MTMode>
void ObjectAllocatorNoGen<MTMode>::VisitAndRemoveFreePools(const MemVisitor &mem_visitor)
{
    if (pygote_space_allocator_ != nullptr) {
        pygote_space_allocator_->VisitAndRemoveFreePools(mem_visitor);
    }
    object_allocator_->VisitAndRemoveFreePools(mem_visitor);
    large_object_allocator_->VisitAndRemoveFreePools(mem_visitor);
    humongous_object_allocator_->VisitAndRemoveFreePools(mem_visitor);
}

template <MTModeT MTMode>
void ObjectAllocatorNoGen<MTMode>::IterateOverObjects(const ObjectVisitor &object_visitor)
{
    if (pygote_space_allocator_ != nullptr) {
        pygote_space_allocator_->IterateOverObjects(object_visitor);
    }
    object_allocator_->IterateOverObjects(object_visitor);
    large_object_allocator_->IterateOverObjects(object_visitor);
    humongous_object_allocator_->IterateOverObjects(object_visitor);
}

template <MTModeT MTMode>
void ObjectAllocatorNoGen<MTMode>::IterateRegularSizeObjects(const ObjectVisitor &object_visitor)
{
    object_allocator_->IterateOverObjects(object_visitor);
}

template <MTModeT MTMode>
void ObjectAllocatorNoGen<MTMode>::IterateNonRegularSizeObjects(const ObjectVisitor &object_visitor)
{
    if (pygote_space_allocator_ != nullptr) {
        pygote_space_allocator_->IterateOverObjects(object_visitor);
    }
    large_object_allocator_->IterateOverObjects(object_visitor);
    humongous_object_allocator_->IterateOverObjects(object_visitor);
}

template <MTModeT MTMode>
void ObjectAllocatorNoGen<MTMode>::FreeObjectsMovedToPygoteSpace()
{
    // Clear allocator because we have move all objects in it to pygote space
    object_allocator_->VisitAndRemoveAllPools(
        [](void *mem, size_t size) { PoolManager::GetMmapMemPool()->FreePool(mem, size); });
    delete object_allocator_;
    object_allocator_ = new (std::nothrow) ObjectAllocator(mem_stats_);
}

template <MTModeT MTMode>
void ObjectAllocatorNoGen<MTMode>::Collect(const GCObjectVisitor &gc_object_visitor,
                                           [[maybe_unused]] GCCollectMode collect_mode)
{
    if (pygote_space_allocator_ != nullptr) {
        pygote_space_allocator_->Collect(gc_object_visitor);
    }
    object_allocator_->Collect(gc_object_visitor);
    large_object_allocator_->Collect(gc_object_visitor);
    humongous_object_allocator_->Collect(gc_object_visitor);
}

// If there is a common base class for these allocators, we could split this func and return the pointer to the
// allocator containing the object
template <MTModeT MTMode>
bool ObjectAllocatorNoGen<MTMode>::ContainObject(const ObjectHeader *obj) const
{
    if (object_allocator_->ContainObject(obj)) {
        return true;
    }
    if (large_object_allocator_->ContainObject(obj)) {
        return true;
    }
    if (humongous_object_allocator_->ContainObject(obj)) {
        return true;
    }

    return false;
}

template <MTModeT MTMode>
bool ObjectAllocatorNoGen<MTMode>::IsLive(const ObjectHeader *obj)
{
    if (pygote_space_allocator_ != nullptr && pygote_space_allocator_->ContainObject(obj)) {
        return pygote_space_allocator_->IsLive(obj);
    }
    if (object_allocator_->ContainObject(obj)) {
        return object_allocator_->IsLive(obj);
    }
    if (large_object_allocator_->ContainObject(obj)) {
        return large_object_allocator_->IsLive(obj);
    }
    if (humongous_object_allocator_->ContainObject(obj)) {
        return humongous_object_allocator_->IsLive(obj);
    }
    return false;
}

template <MTModeT MTMode>
void *ObjectAllocatorGen<MTMode>::Allocate(size_t size, Alignment align, [[maybe_unused]] panda::ManagedThread *thread)
{
    void *mem = nullptr;
    size_t aligned_size;
    aligned_size = AlignUp(size, GetAlignmentInBytes(align));
    if (LIKELY(aligned_size <= YOUNG_ALLOC_MAX_SIZE)) {
        mem = young_gen_allocator_->Alloc(size, align);
    } else {
        mem = AllocateTenured(size);
    }
    return mem;
}

template <MTModeT MTMode>
void *ObjectAllocatorGen<MTMode>::AllocateNonMovable(size_t size, Alignment align,
                                                     [[maybe_unused]] panda::ManagedThread *thread)
{
    // Before pygote fork, allocate small non-movable objects in pygote space
    if (UNLIKELY(IsPygoteAllocEnabled() && pygote_space_allocator_->CanAllocNonMovable(size, align))) {
        return pygote_space_allocator_->Alloc(size, align);
    }
    void *mem = nullptr;
    size_t aligned_size;
    aligned_size = AlignUp(size, GetAlignmentInBytes(align));
    if (aligned_size <= ObjectAllocator::GetMaxSize()) {
        size_t pool_size = std::max(PANDA_DEFAULT_POOL_SIZE, ObjectAllocator::GetMinPoolSize());
        mem = AllocateSafe(size, align, non_movable_object_allocator_, pool_size,
                           SpaceType::SPACE_TYPE_NON_MOVABLE_OBJECT);
    } else if (aligned_size <= LargeObjectAllocator::GetMaxSize()) {
        size_t pool_size = std::max(PANDA_DEFAULT_POOL_SIZE, LargeObjectAllocator::GetMinPoolSize());
        mem = AllocateSafe(size, align, large_non_movable_object_allocator_, pool_size,
                           SpaceType::SPACE_TYPE_NON_MOVABLE_OBJECT);
    } else {
        // We don't need special allocator for these Humongous objects are non-movable
        size_t pool_size = std::max(PANDA_DEFAULT_POOL_SIZE, HumongousObjectAllocator::GetMinPoolSize(size));
        mem = AllocateSafe(size, align, humongous_object_allocator_, pool_size, SpaceType::SPACE_TYPE_HUMONGOUS_OBJECT);
    }
    return mem;
}

template <MTModeT MTMode>
void *ObjectAllocatorGen<MTMode>::AllocateTenured(size_t size)
{
    void *mem = nullptr;
    Alignment align = DEFAULT_ALIGNMENT;
    size_t aligned_size = AlignUp(size, GetAlignmentInBytes(align));
    if (aligned_size <= ObjectAllocator::GetMaxSize()) {
        size_t pool_size = std::max(PANDA_DEFAULT_POOL_SIZE, ObjectAllocator::GetMinPoolSize());
        mem = AllocateSafe(size, align, object_allocator_, pool_size, SpaceType::SPACE_TYPE_OBJECT);
    } else if (aligned_size <= LargeObjectAllocator::GetMaxSize()) {
        size_t pool_size = std::max(PANDA_DEFAULT_POOL_SIZE, LargeObjectAllocator::GetMinPoolSize());
        mem = AllocateSafe(size, align, large_object_allocator_, pool_size, SpaceType::SPACE_TYPE_OBJECT);
    } else {
        size_t pool_size = std::max(PANDA_DEFAULT_POOL_SIZE, HumongousObjectAllocator::GetMinPoolSize(size));
        mem = AllocateSafe(size, align, humongous_object_allocator_, pool_size, SpaceType::SPACE_TYPE_HUMONGOUS_OBJECT);
    }
    return mem;
}

template <MTModeT MTMode>
void ObjectAllocatorGen<MTMode>::VisitAndRemoveAllPools(const MemVisitor &mem_visitor)
{
    if (pygote_space_allocator_ != nullptr) {
        pygote_space_allocator_->VisitAndRemoveAllPools(mem_visitor);
    }
    object_allocator_->VisitAndRemoveAllPools(mem_visitor);
    large_object_allocator_->VisitAndRemoveAllPools(mem_visitor);
    humongous_object_allocator_->VisitAndRemoveAllPools(mem_visitor);
    non_movable_object_allocator_->VisitAndRemoveAllPools(mem_visitor);
    large_non_movable_object_allocator_->VisitAndRemoveAllPools(mem_visitor);
}

template <MTModeT MTMode>
void ObjectAllocatorGen<MTMode>::VisitAndRemoveFreePools(const MemVisitor &mem_visitor)
{
    if (pygote_space_allocator_ != nullptr) {
        pygote_space_allocator_->VisitAndRemoveFreePools(mem_visitor);
    }
    object_allocator_->VisitAndRemoveFreePools(mem_visitor);
    large_object_allocator_->VisitAndRemoveFreePools(mem_visitor);
    humongous_object_allocator_->VisitAndRemoveFreePools(mem_visitor);
    non_movable_object_allocator_->VisitAndRemoveFreePools(mem_visitor);
    large_non_movable_object_allocator_->VisitAndRemoveFreePools(mem_visitor);
}

template <MTModeT MTMode>
void ObjectAllocatorGen<MTMode>::IterateOverYoungObjects(const ObjectVisitor &object_visitor)
{
    young_gen_allocator_->IterateOverObjects(object_visitor);
}

template <MTModeT MTMode>
void ObjectAllocatorGen<MTMode>::IterateOverTenuredObjects(const ObjectVisitor &object_visitor)
{
    if (pygote_space_allocator_ != nullptr) {
        pygote_space_allocator_->IterateOverObjects(object_visitor);
    }
    object_allocator_->IterateOverObjects(object_visitor);
    large_object_allocator_->IterateOverObjects(object_visitor);
    humongous_object_allocator_->IterateOverObjects(object_visitor);
    non_movable_object_allocator_->IterateOverObjects(object_visitor);
    large_non_movable_object_allocator_->IterateOverObjects(object_visitor);
}

template <MTModeT MTMode>
void ObjectAllocatorGen<MTMode>::IterateOverObjects(const ObjectVisitor &object_visitor)
{
    if (pygote_space_allocator_ != nullptr) {
        pygote_space_allocator_->IterateOverObjects(object_visitor);
    }
    young_gen_allocator_->IterateOverObjects(object_visitor);
    object_allocator_->IterateOverObjects(object_visitor);
    large_object_allocator_->IterateOverObjects(object_visitor);
    humongous_object_allocator_->IterateOverObjects(object_visitor);
    non_movable_object_allocator_->IterateOverObjects(object_visitor);
    large_non_movable_object_allocator_->IterateOverObjects(object_visitor);
}

template <MTModeT MTMode>
void ObjectAllocatorGen<MTMode>::IterateRegularSizeObjects(const ObjectVisitor &object_visitor)
{
    object_allocator_->IterateOverObjects(object_visitor);
}

template <MTModeT MTMode>
void ObjectAllocatorGen<MTMode>::IterateNonRegularSizeObjects(const ObjectVisitor &object_visitor)
{
    if (pygote_space_allocator_ != nullptr) {
        pygote_space_allocator_->IterateOverObjects(object_visitor);
    }
    large_object_allocator_->IterateOverObjects(object_visitor);
    humongous_object_allocator_->IterateOverObjects(object_visitor);
    non_movable_object_allocator_->IterateOverObjects(object_visitor);
    large_non_movable_object_allocator_->IterateOverObjects(object_visitor);
}

template <MTModeT MTMode>
void ObjectAllocatorGen<MTMode>::FreeObjectsMovedToPygoteSpace()
{
    // Clear allocator because we have move all objects in it to pygote space
    object_allocator_->VisitAndRemoveAllPools(
        [](void *mem, size_t size) { PoolManager::GetMmapMemPool()->FreePool(mem, size); });
    delete object_allocator_;
    object_allocator_ = new (std::nothrow) ObjectAllocator(mem_stats_);
}

template <MTModeT MTMode>
void ObjectAllocatorGen<MTMode>::Collect(const GCObjectVisitor &gc_object_visitor, GCCollectMode collect_mode)
{
    switch (collect_mode) {
        case GCCollectMode::GC_MINOR:
            break;
        case GCCollectMode::GC_ALL:
        case GCCollectMode::GC_MAJOR:
            if (pygote_space_allocator_ != nullptr) {
                pygote_space_allocator_->Collect(gc_object_visitor);
            }
            object_allocator_->Collect(gc_object_visitor);
            large_object_allocator_->Collect(gc_object_visitor);
            humongous_object_allocator_->Collect(gc_object_visitor);
            non_movable_object_allocator_->Collect(gc_object_visitor);
            large_non_movable_object_allocator_->Collect(gc_object_visitor);
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
size_t ObjectAllocatorNoGen<MTMode>::GetRegularObjectMaxSize()
{
    return ObjectAllocator::GetMaxSize();
}

template <MTModeT MTMode>
size_t ObjectAllocatorNoGen<MTMode>::GetLargeObjectMaxSize()
{
    return LargeObjectAllocator::GetMaxSize();
}

template <MTModeT MTMode>
TLAB *ObjectAllocatorNoGen<MTMode>::CreateNewTLAB([[maybe_unused]] panda::ManagedThread *thread)
{
    LOG(FATAL, ALLOC) << "Unimplemented";
    return nullptr;
}

template <MTModeT MTMode>
size_t ObjectAllocatorNoGen<MTMode>::GetTLABMaxAllocSize()
{
    return 0;
}

template <MTModeT MTMode>
ObjectAllocatorGen<MTMode>::ObjectAllocatorGen(MemStatsType *mem_stats, bool create_pygote_space_allocator)
    : ObjectAllocatorGenBase(mem_stats, GCCollectMode::GC_ALL, create_pygote_space_allocator)
{
    auto young_space_size = Runtime::GetOptions().GetYoungSpaceSize();
    auto young_shared_space_size = Runtime::GetOptions().GetYoungSharedSpaceSize();
    auto tlabs_count_in_young_gen = (young_space_size - young_shared_space_size) / YOUNG_TLAB_SIZE;
    ASSERT(((young_space_size - young_shared_space_size) % YOUNG_TLAB_SIZE) == 0);
    ASSERT(YOUNG_ALLOC_MAX_SIZE <= YOUNG_TLAB_SIZE);
    ASSERT(tlabs_count_in_young_gen * YOUNG_TLAB_SIZE <= young_space_size);

    // Because we construct BumpPointer Allocator after calling AllocArena method
    auto young_pool = PoolManager::GetMmapMemPool()->AllocPool(
        young_space_size, SpaceType::SPACE_TYPE_OBJECT, YoungGenAllocator::GetAllocatorType(), &young_gen_allocator_);
    young_gen_allocator_ = new (std::nothrow)
        YoungGenAllocator(std::move(young_pool), SpaceType::SPACE_TYPE_OBJECT, mem_stats, tlabs_count_in_young_gen);
    object_allocator_ = new (std::nothrow) ObjectAllocator(mem_stats);
    large_object_allocator_ = new (std::nothrow) LargeObjectAllocator(mem_stats);
    humongous_object_allocator_ = new (std::nothrow) HumongousObjectAllocator(mem_stats);
    non_movable_object_allocator_ =
        new (std::nothrow) ObjectAllocator(mem_stats, SpaceType::SPACE_TYPE_NON_MOVABLE_OBJECT);
    large_non_movable_object_allocator_ =
        new (std::nothrow) LargeObjectAllocator(mem_stats, SpaceType::SPACE_TYPE_NON_MOVABLE_OBJECT);
    mem_stats_ = mem_stats;
}

template <MTModeT MTMode>
ObjectAllocatorGen<MTMode>::~ObjectAllocatorGen()
{
    // Need to free the pool space when the allocator destroyed
    young_gen_allocator_->VisitAndRemoveAllPools(
        [](void *mem, [[maybe_unused]] size_t size) { PoolManager::GetMmapMemPool()->FreePool(mem, size); });
    delete young_gen_allocator_;
    delete object_allocator_;
    delete large_object_allocator_;
    delete humongous_object_allocator_;
    delete non_movable_object_allocator_;
    delete large_non_movable_object_allocator_;
}
template <MTModeT MTMode>
size_t ObjectAllocatorGen<MTMode>::GetRegularObjectMaxSize()
{
    return ObjectAllocator::GetMaxSize();
}

template <MTModeT MTMode>
size_t ObjectAllocatorGen<MTMode>::GetLargeObjectMaxSize()
{
    return LargeObjectAllocator::GetMaxSize();
}

template <MTModeT MTMode>
bool ObjectAllocatorGen<MTMode>::IsAddressInYoungSpace(uintptr_t address)
{
    return young_gen_allocator_->GetMemRange().IsAddressInRange(address);
}

template <MTModeT MTMode>
bool ObjectAllocatorGen<MTMode>::IsObjectInNonMovableSpace(const ObjectHeader *obj)
{
    return non_movable_object_allocator_->ContainObject(obj);
}

template <MTModeT MTMode>
bool ObjectAllocatorGen<MTMode>::HasYoungSpace()
{
    return young_gen_allocator_ != nullptr;
}

template <MTModeT MTMode>
MemRange ObjectAllocatorGen<MTMode>::GetYoungSpaceMemRange()
{
    return young_gen_allocator_->GetMemRange();
}

template <MTModeT MTMode>
void ObjectAllocatorGen<MTMode>::ResetYoungAllocator()
{
    MemStatsType *mem_stats = mem_stats_;
    Thread::GetCurrent()->GetVM()->GetThreadManager()->EnumerateThreads(
        [&mem_stats](ManagedThread *thread) {
            if (!PANDA_TRACK_TLAB_ALLOCATIONS && (thread->GetTLAB()->GetOccupiedSize() != 0)) {
                mem_stats->RecordAllocateObject(thread->GetTLAB()->GetOccupiedSize(), SpaceType::SPACE_TYPE_OBJECT);
            }
            thread->ClearTLAB();
            return true;
        },
        static_cast<unsigned int>(EnumerationFlag::ALL));
    young_gen_allocator_->Reset();
}

template <MTModeT MTMode>
TLAB *ObjectAllocatorGen<MTMode>::CreateNewTLAB([[maybe_unused]] panda::ManagedThread *thread)
{
    return young_gen_allocator_->CreateNewTLAB(YOUNG_TLAB_SIZE);
}

template <MTModeT MTMode>
size_t ObjectAllocatorGen<MTMode>::GetTLABMaxAllocSize()
{
    return YOUNG_ALLOC_MAX_SIZE;
}

template <MTModeT MTMode>
void ObjectAllocatorGen<MTMode>::IterateOverObjectsInRange(MemRange mem_range, const ObjectVisitor &object_visitor)
{
    // We need ensure that the mem range related to a card must be located in one allocator
    auto space_type = PoolManager::GetMmapMemPool()->GetSpaceTypeForAddr(ToVoidPtr(mem_range.GetStartAddress()));
    auto alloc_info = PoolManager::GetMmapMemPool()->GetAllocatorInfoForAddr(ToVoidPtr(mem_range.GetStartAddress()));
    auto *allocator = alloc_info.GetAllocatorHeaderAddr();
    switch (space_type) {
        case SpaceType::SPACE_TYPE_OBJECT:
            if (allocator == object_allocator_) {
                object_allocator_->IterateOverObjectsInRange(object_visitor, ToVoidPtr(mem_range.GetStartAddress()),
                                                             ToVoidPtr(mem_range.GetEndAddress()));
            } else if (allocator == pygote_space_allocator_) {
                pygote_space_allocator_->IterateOverObjectsInRange(
                    object_visitor, ToVoidPtr(mem_range.GetStartAddress()), ToVoidPtr(mem_range.GetEndAddress()));
            } else if (allocator == &young_gen_allocator_) {
                young_gen_allocator_->IterateOverObjectsInRange(object_visitor, ToVoidPtr(mem_range.GetStartAddress()),
                                                                ToVoidPtr(mem_range.GetEndAddress()));
            } else if (allocator == large_object_allocator_) {
                large_object_allocator_->IterateOverObjectsInRange(
                    object_visitor, ToVoidPtr(mem_range.GetStartAddress()), ToVoidPtr(mem_range.GetEndAddress()));
            } else {
                // If we reach this line, we may have an issue with multiVM CardTable iteration
                UNREACHABLE();
            }
            break;
        case SpaceType::SPACE_TYPE_HUMONGOUS_OBJECT:
            if (allocator == humongous_object_allocator_) {
                humongous_object_allocator_->IterateOverObjectsInRange(
                    object_visitor, ToVoidPtr(mem_range.GetStartAddress()), ToVoidPtr(mem_range.GetEndAddress()));
            } else {
                // If we reach this line, we may have an issue with multiVM CardTable iteration
                UNREACHABLE();
            }
            break;
        case SpaceType::SPACE_TYPE_NON_MOVABLE_OBJECT:
            if (allocator == non_movable_object_allocator_) {
                non_movable_object_allocator_->IterateOverObjectsInRange(
                    object_visitor, ToVoidPtr(mem_range.GetStartAddress()), ToVoidPtr(mem_range.GetEndAddress()));
            } else if (allocator == large_non_movable_object_allocator_) {
                large_non_movable_object_allocator_->IterateOverObjectsInRange(
                    object_visitor, ToVoidPtr(mem_range.GetStartAddress()), ToVoidPtr(mem_range.GetEndAddress()));
            } else {
                // If we reach this line, we may have an issue with multiVM CardTable iteration
                UNREACHABLE();
            }
            break;
        default:
            // If we reach this line, we may have an issue with multiVM CardTable iteration
            UNREACHABLE();
            break;
    }
}

template <MTModeT MTMode>
bool ObjectAllocatorGen<MTMode>::ContainObject(const ObjectHeader *obj) const
{
    if (pygote_space_allocator_ != nullptr && pygote_space_allocator_->ContainObject(obj)) {
        return true;
    }
    if (young_gen_allocator_->ContainObject(obj)) {
        return true;
    }
    if (object_allocator_->ContainObject(obj)) {
        return true;
    }
    if (large_object_allocator_->ContainObject(obj)) {
        return true;
    }
    if (humongous_object_allocator_->ContainObject(obj)) {
        return true;
    }
    if (non_movable_object_allocator_->ContainObject(obj)) {
        return true;
    }
    if (large_non_movable_object_allocator_->ContainObject(obj)) {
        return true;
    }

    return false;
}

template <MTModeT MTMode>
bool ObjectAllocatorGen<MTMode>::IsLive(const ObjectHeader *obj)
{
    if (pygote_space_allocator_ != nullptr && pygote_space_allocator_->ContainObject(obj)) {
        return pygote_space_allocator_->IsLive(obj);
    }
    if (young_gen_allocator_->ContainObject(obj)) {
        return young_gen_allocator_->IsLive(obj);
    }
    if (object_allocator_->ContainObject(obj)) {
        return object_allocator_->IsLive(obj);
    }
    if (large_object_allocator_->ContainObject(obj)) {
        return large_object_allocator_->IsLive(obj);
    }
    if (humongous_object_allocator_->ContainObject(obj)) {
        return humongous_object_allocator_->IsLive(obj);
    }
    if (non_movable_object_allocator_->ContainObject(obj)) {
        return non_movable_object_allocator_->IsLive(obj);
    }
    if (large_non_movable_object_allocator_->ContainObject(obj)) {
        return large_non_movable_object_allocator_->IsLive(obj);
    }

    return false;
}

template class ObjectAllocatorGen<MT_MODE_SINGLE>;
template class ObjectAllocatorGen<MT_MODE_MULTI>;
template class ObjectAllocatorNoGen<MT_MODE_SINGLE>;
template class ObjectAllocatorNoGen<MT_MODE_MULTI>;

}  // namespace panda::mem
