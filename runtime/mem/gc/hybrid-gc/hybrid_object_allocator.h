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

#ifndef PANDA_RUNTIME_MEM_GC_HYBRID_GC_HYBRID_OBJECT_ALLOCATOR_H_
#define PANDA_RUNTIME_MEM_GC_HYBRID_GC_HYBRID_OBJECT_ALLOCATOR_H_

#include "runtime/include/mem/allocator.h"
#include "runtime/mem/region_allocator.h"

namespace panda::mem {

class HybridObjectAllocator final : public ObjectAllocatorBase {
public:
    using ObjectAllocator = RegionAllocator<ObjectAllocConfig>;
    using LargeObjectAllocator = FreeListAllocator<ObjectAllocConfig>;          // Allocator used for large objects
    using HumongousObjectAllocator = HumongousObjAllocator<ObjectAllocConfig>;  // Allocator used for humongous objects
    NO_MOVE_SEMANTIC(HybridObjectAllocator);
    NO_COPY_SEMANTIC(HybridObjectAllocator);
    explicit HybridObjectAllocator(mem::MemStatsType *mem_stats, bool create_pygote_space_allocator);

    ~HybridObjectAllocator() final;

    [[nodiscard]] void *Allocate(size_t size, Alignment align, [[maybe_unused]] panda::ManagedThread *thread) final;

    [[nodiscard]] void *AllocateInLargeAllocator(size_t size, Alignment align, BaseClass *cls) final;

    [[nodiscard]] void *AllocateNonMovable([[maybe_unused]] size_t size, [[maybe_unused]] Alignment align,
                                           [[maybe_unused]] panda::ManagedThread *thread) final
    {
        return nullptr;
    }

    void IterateOverObjects([[maybe_unused]] const ObjectVisitor &object_visitor) final {}

    void VisitAndRemoveAllPools([[maybe_unused]] const MemVisitor &mem_visitor) final {}

    void VisitAndRemoveFreePools([[maybe_unused]] const MemVisitor &mem_visitor) final {}

    void Collect([[maybe_unused]] const GCObjectVisitor &gc_object_visitor,
                 [[maybe_unused]] GCCollectMode collect_mode) final
    {
    }

    void IterateOverObjectsInRange([[maybe_unused]] MemRange mem_range,
                                   [[maybe_unused]] const ObjectVisitor &object_visitor) final
    {
    }

    size_t GetRegularObjectMaxSize() final
    {
        return 0;
    }

    size_t GetLargeObjectMaxSize() final
    {
        return 0;
    }

    bool IsAddressInYoungSpace([[maybe_unused]] uintptr_t address) final
    {
        return false;
    }

    bool IsObjectInNonMovableSpace([[maybe_unused]] const ObjectHeader *obj) final
    {
        return false;
    }

    bool HasYoungSpace() final
    {
        return false;
    }

    MemRange GetYoungSpaceMemRange() final
    {
        UNREACHABLE();
    }

    void ResetYoungAllocator() final {}

    TLAB *CreateNewTLAB(ManagedThread *thread) final;

    size_t GetTLABMaxAllocSize() final;

    bool IsTLABSupported() final
    {
        return true;
    }

    bool ContainObject(const ObjectHeader *obj) const final;

    bool IsLive(const ObjectHeader *obj) final;

    size_t VerifyAllocatorStatus() final;

    ObjectAllocator *GetRegularObjectAllocator()
    {
        return object_allocator_;
    }

    LargeObjectAllocator *GetLargeObjectAllocator()
    {
        return large_object_allocator_;
    }

    HumongousObjectAllocator *GetHumongousObjectAllocator()
    {
        return humongous_object_allocator_;
    }

    constexpr static size_t GetLargeThreshold()
    {
        return LARGE_OBJECT_THRESHHOLD;
    }
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE)
    [[nodiscard]] void *AllocateLocal(size_t /* size */, Alignment /* align */,
                                      panda::ManagedThread * /* thread */) final
    {
        LOG(FATAL, ALLOC) << "HybridObjectAllocator: AllocateLocal not supported";
        return nullptr;
    }

private:
    ObjectAllocator *object_allocator_ = nullptr;
    LargeObjectAllocator *large_object_allocator_ = nullptr;
    HumongousObjectAllocator *humongous_object_allocator_ = nullptr;
    size_t static constexpr LARGE_OBJECT_THRESHHOLD = 12_KB;
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_GC_HYBRID_GC_HYBRID_OBJECT_ALLOCATOR_H_
