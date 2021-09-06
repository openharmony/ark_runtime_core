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

#ifndef PANDA_RUNTIME_MEM_INTERNAL_ALLOCATOR_H_
#define PANDA_RUNTIME_MEM_INTERNAL_ALLOCATOR_H_

#include <vector>

#include "libpandabase/concepts.h"
#include "libpandabase/mem/mmap_mem_pool-inl.h"
#include "libpandabase/mem/pool_manager.h"
#include "libpandabase/os/mutex.h"
#include "runtime/mem/freelist_allocator.h"
#include "runtime/mem/humongous_obj_allocator.h"
#include "runtime/mem/runslots_allocator.h"

#ifdef TRACK_INTERNAL_ALLOCATIONS
#include "libpandabase/mem/alloc_tracker.h"
#endif  // TRACK_INTERNAL_ALLOCATIONS

namespace panda::mem {

namespace test {
class InternalAllocatorTest;
}  // namespace test

template <typename AllocConfigT>
class MallocProxyAllocator;
class Allocator;

enum class AllocScope {
    GLOBAL,  // The allocation will be in global storage
    LOCAL    // The allocation will be in thread-local storage
};

enum class InternalAllocatorConfig {
    PANDA_ALLOCATORS,  // Use panda allocators as internal allocator
    MALLOC_ALLOCATOR   // Use malloc allocator as internal allocator
};

class RawMemoryConfig;
class EmptyMemoryConfig;

template <InternalAllocatorConfig Config = InternalAllocatorConfig::PANDA_ALLOCATORS>
class InternalAllocator {
public:
    explicit InternalAllocator(MemStatsType *mem_stats);

    NO_COPY_SEMANTIC(InternalAllocator);
    NO_MOVE_SEMANTIC(InternalAllocator);

    template <AllocScope AllocScopeT = AllocScope::GLOBAL>
    [[nodiscard]] void *Alloc(size_t size, Alignment align = DEFAULT_ALIGNMENT);

    [[nodiscard]] void *AllocLocal(size_t size, Alignment align = DEFAULT_ALIGNMENT)
    {
        return Alloc<AllocScope::LOCAL>(size, align);
    }

    template <class T>
    [[nodiscard]] T *AllocArray(size_t size);

    template <typename T, typename... Args>
    [[nodiscard]] std::enable_if_t<!std::is_array_v<T>, T *> New(Args &&... args);

    template <typename T>
    [[nodiscard]] std::enable_if_t<is_unbounded_array_v<T>, std::remove_extent_t<T> *> New(size_t size);

    template <typename T>
    void DeleteArray(T *data);

    void Free(void *ptr);

#ifdef TRACK_INTERNAL_ALLOCATIONS
    void Dump()
    {
        tracker_->Dump();
    }
#endif  // TRACK_INTERNAL_ALLOCATIONS

    template <class T>
    void Delete(T *ptr);

    ~InternalAllocator();

    /**
     * \brief Iterates over memory pools used by this allocator.
     * @tparam MemVisitor
     * @param mem_visitor - function pointer or functor
     */
    template <typename MemVisitor>
    void VisitAndRemoveAllPools(MemVisitor mem_visitor);

    /**
     * \brief Visit memory pools that can be returned to the system in this allocator
     * and remove them from the allocator structure.
     * @tparam MemVisitor
     * @param mem_visitor - function pointer or functor
     */
    template <typename MemVisitor>
    void VisitAndRemoveFreePools(MemVisitor mem_visitor);

#ifdef NDEBUG
    using InternalAllocConfigT = EmptyMemoryConfig;
#else
    using InternalAllocConfigT = RawMemoryConfig;
#endif
    using LocalSmallObjectAllocator = RunSlotsAllocator<InternalAllocConfigT, RunSlotsAllocatorLockConfig::DummyLock>;

    /**
     * \brief Create and set up local internal allocator instance for fast small objects allocation
     * @param allocator - a pointer to the allocator which will be used for local allocator instance storage
     * @return - a pointer to the local internal allocator instance
     */
    static InternalAllocator::LocalSmallObjectAllocator *SetUpLocalInternalAllocator(Allocator *allocator);

    /**
     * \brief Delete local internal allocator instance and return all pools to the system
     * @param allocator - a pointer to the allocator which was used for local allocator instance storage
     * @param local_allocator - a pointer to the local internal allocator instance
     */
    static void FinalizeLocalInternalAllocator(LocalSmallObjectAllocator *local_allocator, Allocator *allocator);

    /**
     * \brief Return free memory pools to the system in local internal allocator
     * and remove them from the allocator structure.
     * @param local_allocator - a pointer to a local internal allocator instance
     */
    static void RemoveFreePoolsForLocalInternalAllocator(LocalSmallObjectAllocator *local_allocator);

    static void InitInternalAllocatorFromRuntime(Allocator *allocator);

    static Allocator *GetInternalAllocatorFromRuntime();

    static void ClearInternalAllocatorFromRuntime();

private:
#ifdef TRACK_INTERNAL_ALLOCATIONS
    os::memory::Mutex lock_;
    MemStatsType *mem_stats_;
    AllocTracker *tracker_ = nullptr;
#endif  // TRACK_INTERNAL_ALLOCATIONS

    using RunSlotsAllocatorT = RunSlotsAllocator<InternalAllocConfigT>;
    using FreeListAllocatorT = FreeListAllocator<InternalAllocConfigT>;
    using HumongousObjAllocatorT = HumongousObjAllocator<InternalAllocConfigT>;
    using MallocProxyAllocatorT = MallocProxyAllocator<InternalAllocConfigT>;
    template <AllocScope AllocScopeT>
    void *AllocViaPandaAllocators(size_t size, Alignment align);
    void FreeViaPandaAllocators(void *ptr);
    RunSlotsAllocatorT *runslots_allocator_ {nullptr};
    FreeListAllocatorT *freelist_allocator_ {nullptr};
    HumongousObjAllocatorT *humongous_allocator_ {nullptr};
    MallocProxyAllocatorT *malloc_allocator_ {nullptr};
    static Allocator *allocator_from_runtime;

    friend class test::InternalAllocatorTest;
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_INTERNAL_ALLOCATOR_H_
