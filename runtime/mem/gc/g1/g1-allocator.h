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

#ifndef PANDA_RUNTIME_MEM_GC_G1_G1_ALLOCATOR_H_
#define PANDA_RUNTIME_MEM_GC_G1_G1_ALLOCATOR_H_

#include "runtime/include/mem/allocator.h"
#include "runtime/mem/region_allocator.h"
#include "runtime/mem/region_allocator-inl.h"

namespace panda::mem {
class ObjectAllocConfigWithCrossingMap;
class ObjectAllocConfig;
class TLAB;

template <MTModeT MTMode = MT_MODE_MULTI>
class ObjectAllocatorG1 final : public ObjectAllocatorGenBase {
    static constexpr size_t REGION_SIZE = 1_MB;               // size of the region
    static constexpr size_t YOUNG_DEFAULT_REGIONS_COUNT = 2;  // default value for regions count in young space
    static constexpr size_t TLAB_SIZE = 4_KB;                 // TLAB size for young gen
    static constexpr size_t REGION_SHARED_SIZE = 512_KB;      // shared pool size for region
    static constexpr size_t TLABS_COUNT_IN_REGION = (REGION_SIZE - REGION_SHARED_SIZE) / TLAB_SIZE;

    using ObjectAllocator = RegionAllocator<ObjectAllocConfig>;
    using NonMovableAllocator = RegionNonmovableAllocator<ObjectAllocConfig, RegionAllocatorLockConfig::CommonLock,
                                                          FreeListAllocator<ObjectAllocConfig>>;
    using HumongousObjectAllocator =
        HumongousObjAllocator<ObjectAllocConfigWithCrossingMap>;  // Allocator used for humongous objects

public:
    NO_MOVE_SEMANTIC(ObjectAllocatorG1);
    NO_COPY_SEMANTIC(ObjectAllocatorG1);

    explicit ObjectAllocatorG1(MemStatsType *mem_stats, bool create_pygote_space_allocator);

    ~ObjectAllocatorG1() final = default;

    void *Allocate(size_t size, Alignment align, [[maybe_unused]] panda::ManagedThread *thread) final;

    void *AllocateNonMovable(size_t size, Alignment align, [[maybe_unused]] panda::ManagedThread *thread) final;

    void VisitAndRemoveAllPools(const MemVisitor &mem_visitor) final;

    void VisitAndRemoveFreePools(const MemVisitor &mem_visitor) final;

    void IterateOverYoungObjects(const ObjectVisitor &object_visitor) final;

    void IterateOverTenuredObjects(const ObjectVisitor &object_visitor) final;

    void IterateOverObjects(const ObjectVisitor &object_visitor) final;

    /**
     * \brief iterates all objects in object allocator
     */
    void IterateRegularSizeObjects(const ObjectVisitor &object_visitor) final;

    /**
     * \brief iterates objects in all allocators except object allocator
     */
    void IterateNonRegularSizeObjects(const ObjectVisitor &object_visitor) final;

    void FreeObjectsMovedToPygoteSpace() final;

    void Collect(const GCObjectVisitor &gc_object_visitor, GCCollectMode collect_mode) final;

    size_t GetRegularObjectMaxSize() final;

    size_t GetLargeObjectMaxSize() final;

    bool IsAddressInYoungSpace(uintptr_t address) final;

    bool HasYoungSpace() final;

    MemRange GetYoungSpaceMemRange() final;

    void ResetYoungAllocator() final;

    TLAB *CreateNewTLAB(panda::ManagedThread *thread) final;

    size_t GetTLABMaxAllocSize() final;

    bool IsTLABSupported() final
    {
        return false;
    }

    void IterateOverObjectsInRange(MemRange mem_range, const ObjectVisitor &object_visitor) final;

    bool ContainObject(const ObjectHeader *obj) const final;

    bool IsLive(const ObjectHeader *obj) final;

    size_t VerifyAllocatorStatus() final
    {
        LOG(FATAL, ALLOC) << "Not implemented";
        return 0;
    }

    [[nodiscard]] void *AllocateLocal([[maybe_unused]] size_t size, [[maybe_unused]] Alignment align,
                                      [[maybe_unused]] panda::ManagedThread *thread) final
    {
        LOG(FATAL, ALLOC) << "ObjectAllocatorGen: AllocateLocal not supported";
        return nullptr;
    }

    bool IsObjectInNonMovableSpace(const ObjectHeader *obj) final;

    static constexpr size_t GetRegionSize()
    {
        return REGION_SIZE;
    }

private:
    PandaUniquePtr<ObjectAllocator> object_allocator_ {nullptr};
    PandaUniquePtr<NonMovableAllocator> nonmovable_allocator_ {nullptr};
    PandaUniquePtr<HumongousObjectAllocator> humongous_object_allocator_ {nullptr};
    MemStatsType *mem_stats_ {nullptr};

    void *AllocateTenured(size_t size) final;

    friend class AllocTypeConfigG1;
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_GC_G1_G1_ALLOCATOR_H_
