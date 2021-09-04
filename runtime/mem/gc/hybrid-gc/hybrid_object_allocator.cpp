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

#include <mem/gc/hybrid-gc/hybrid_object_allocator.h>

#include "libpandabase/mem/mem.h"
#include "runtime/include/class.h"
#include "runtime/mem/freelist_allocator-inl.h"
#include "runtime/mem/humongous_obj_allocator-inl.h"
#include "runtime/mem/region_allocator-inl.h"

namespace panda::mem {
HybridObjectAllocator::HybridObjectAllocator(mem::MemStatsType *mem_stats, bool create_pygote_space_allocator)
    : ObjectAllocatorBase(mem_stats, GCCollectMode::GC_ALL, create_pygote_space_allocator)
{
    object_allocator_ = new (std::nothrow) ObjectAllocator(mem_stats);
    large_object_allocator_ = new (std::nothrow) LargeObjectAllocator(mem_stats);
    humongous_object_allocator_ = new (std::nothrow) HumongousObjectAllocator(mem_stats);
}

void *HybridObjectAllocator::Allocate(size_t size, Alignment align, [[maybe_unused]] panda::ManagedThread *thread)
{
    size_t aligned_size = AlignUp(size, GetAlignmentInBytes(align));
    return object_allocator_->Alloc(aligned_size, align);
}

void *HybridObjectAllocator::AllocateInLargeAllocator(size_t size, Alignment align, BaseClass *base_cls)
{
    if (base_cls->IsDynamicClass()) {
        return nullptr;
    }
    auto cls = static_cast<Class *>(base_cls);
    size_t aligned_size = AlignUp(size, GetAlignmentInBytes(align));
    void *mem = nullptr;
    if ((aligned_size >= GetLargeThreshold()) &&
        (cls->IsStringClass() || (cls->IsArrayClass() && cls->GetComponentType()->IsPrimitive()))) {
        if (aligned_size <= LargeObjectAllocator::GetMaxSize()) {
            mem = large_object_allocator_->Alloc(size, align);
            if (UNLIKELY(mem == nullptr)) {
                size_t pool_size = std::max(PANDA_DEFAULT_POOL_SIZE, LargeObjectAllocator::GetMinPoolSize());
                auto pool = PoolManager::GetMmapMemPool()->AllocPool(pool_size, SpaceType::SPACE_TYPE_OBJECT,
                                                                     LargeObjectAllocator::GetAllocatorType(),
                                                                     large_object_allocator_);
                bool added_memory_pool = large_object_allocator_->AddMemoryPool(pool.GetMem(), pool.GetSize());
                LOG_IF(!added_memory_pool, FATAL, ALLOC)
                    << "HybridObjectAllocator: couldn't add memory pool to large object allocator";
                mem = large_object_allocator_->Alloc(size, align);
            }
        } else {
            mem = humongous_object_allocator_->Alloc(size, align);
            if (UNLIKELY(mem == nullptr)) {
                size_t pool_size;
                pool_size = std::max(PANDA_DEFAULT_POOL_SIZE, HumongousObjectAllocator::GetMinPoolSize(size));
                auto pool = PoolManager::GetMmapMemPool()->AllocPool(pool_size, SpaceType::SPACE_TYPE_HUMONGOUS_OBJECT,
                                                                     HumongousObjectAllocator::GetAllocatorType(),
                                                                     humongous_object_allocator_);
                bool added_memory_pool = humongous_object_allocator_->AddMemoryPool(pool.GetMem(), pool_size);
                LOG_IF(!added_memory_pool, FATAL, ALLOC)
                    << "HybridObjectAllocator: couldn't add memory pool to humongous object allocator";
                mem = humongous_object_allocator_->Alloc(size, align);
            }
        }
    }
    return mem;
}

bool HybridObjectAllocator::ContainObject(const ObjectHeader *obj) const
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

bool HybridObjectAllocator::IsLive(const ObjectHeader *obj)
{
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

size_t HybridObjectAllocator::VerifyAllocatorStatus()
{
    return 0;
}

TLAB *HybridObjectAllocator::CreateNewTLAB(ManagedThread *thread)
{
    return object_allocator_->CreateNewTLAB(thread);
}

size_t HybridObjectAllocator::GetTLABMaxAllocSize()
{
    return ObjectAllocator::GetMaxRegularObjectSize();
}

HybridObjectAllocator::~HybridObjectAllocator()
{
    delete object_allocator_;
    delete large_object_allocator_;
    delete humongous_object_allocator_;
}

}  // namespace panda::mem
