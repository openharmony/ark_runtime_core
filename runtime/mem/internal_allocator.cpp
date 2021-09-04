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

#include "include/runtime.h"
#include "runtime/mem/internal_allocator-inl.h"
#include "runtime/include/thread.h"

namespace panda::mem {

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_INTERNAL_ALLOCATOR(level) LOG(level, ALLOC) << "InternalAllocator: "

#if defined(TRACK_INTERNAL_ALLOCATIONS)
static AllocTracker *CreateAllocTracker()
{
    static constexpr int SIMPLE_ALLOC_TRACKER = 1;
    static constexpr int DETAIL_ALLOC_TRACKER = 2;

    if constexpr (TRACK_INTERNAL_ALLOCATIONS == SIMPLE_ALLOC_TRACKER) {
        return new SimpleAllocTracker();
    } else if (TRACK_INTERNAL_ALLOCATIONS == DETAIL_ALLOC_TRACKER) {
        return new DetailAllocTracker();
    } else {
        UNREACHABLE();
    }
}
#endif  // TRACK_INTERNAL_ALLOCATIONS

template <InternalAllocatorConfig Config>
Allocator *InternalAllocator<Config>::allocator_from_runtime = nullptr;

template <InternalAllocatorConfig Config>
InternalAllocator<Config>::InternalAllocator(MemStatsType *mem_stats)
{
    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if constexpr (Config == InternalAllocatorConfig::PANDA_ALLOCATORS) {
        runslots_allocator_ = new RunSlotsAllocatorT(mem_stats, SpaceType::SPACE_TYPE_INTERNAL);
        freelist_allocator_ = new FreeListAllocatorT(mem_stats, SpaceType::SPACE_TYPE_INTERNAL);
        humongous_allocator_ = new HumongousObjAllocatorT(mem_stats, SpaceType::SPACE_TYPE_INTERNAL);
    } else {  // NOLINT(readability-misleading-indentation
        malloc_allocator_ = new MallocProxyAllocatorT(mem_stats, SpaceType::SPACE_TYPE_INTERNAL);
    }

#if defined(TRACK_INTERNAL_ALLOCATIONS)
    mem_stats_ = mem_stats;
    tracker_ = CreateAllocTracker();
#endif  // TRACK_INTERNAL_ALLOCATIONS
    LOG_INTERNAL_ALLOCATOR(INFO) << "Initializing InternalAllocator finished";
}

template <InternalAllocatorConfig Config>
template <AllocScope AllocScopeT>
[[nodiscard]] void *InternalAllocator<Config>::Alloc(size_t size, Alignment align)
{
#ifdef TRACK_INTERNAL_ALLOCATIONS
    os::memory::LockHolder lock(lock_);
#endif  // TRACK_INTERNAL_ALLOCATIONS
    void *res = nullptr;
    LOG_INTERNAL_ALLOCATOR(INFO) << "Try to allocate " << size << " bytes";
    if (UNLIKELY(size == 0)) {
        LOG_INTERNAL_ALLOCATOR(DEBUG) << "Failed to allocate - size of object is zero";
        return nullptr;
    }
    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if constexpr (Config == InternalAllocatorConfig::PANDA_ALLOCATORS) {
        res = AllocViaPandaAllocators<AllocScopeT>(size, align);
    } else {  // NOLINT(readability-misleading-indentation
        res = malloc_allocator_->Alloc(size, align);
    }
    if (res == nullptr) {
        return nullptr;
    }
    LOG_INTERNAL_ALLOCATOR(INFO) << "Allocate " << size << " bytes at address " << std::hex << res;
#ifdef TRACK_INTERNAL_ALLOCATIONS
    tracker_->TrackAlloc(res, AlignUp(size, align), SpaceType::SPACE_TYPE_INTERNAL);
#endif  // TRACK_INTERNAL_ALLOCATIONS
    return res;
}

template <InternalAllocatorConfig Config>
void InternalAllocator<Config>::Free(void *ptr)
{
#ifdef TRACK_INTERNAL_ALLOCATIONS
    os::memory::LockHolder lock(lock_);
#endif  // TRACK_INTERNAL_ALLOCATIONS
    if (ptr == nullptr) {
        return;
    }
    LOG_INTERNAL_ALLOCATOR(INFO) << "Try to free via InternalAllocator at address " << std::hex << ptr;
    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if constexpr (Config == InternalAllocatorConfig::PANDA_ALLOCATORS) {
        FreeViaPandaAllocators(ptr);
    } else {  // NOLINT(readability-misleading-indentation
        malloc_allocator_->Free(ptr);
    }
#ifdef TRACK_INTERNAL_ALLOCATIONS
    tracker_->TrackFree(ptr);
#endif  // TRACK_INTERNAL_ALLOCATIONS
}

template <InternalAllocatorConfig Config>
InternalAllocator<Config>::~InternalAllocator()
{
#ifdef TRACK_INTERNAL_ALLOCATIONS
    if (mem_stats_->GetFootprint(SpaceType::SPACE_TYPE_INTERNAL) != 0) {
        // Memory leaks are detected.
        LOG(ERROR, RUNTIME) << "Memory leaks detected.";
        tracker_->DumpMemLeaks(std::cerr);
    }
    tracker_->Dump();
    delete tracker_;
#endif  // TRACK_INTERNAL_ALLOCATIONS
    LOG_INTERNAL_ALLOCATOR(DEBUG) << "Destroying InternalAllocator";
    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if constexpr (Config == InternalAllocatorConfig::PANDA_ALLOCATORS) {
        delete runslots_allocator_;
        delete freelist_allocator_;
        delete humongous_allocator_;
    } else {  // NOLINT(readability-misleading-indentation
        delete malloc_allocator_;
    }
    LOG_INTERNAL_ALLOCATOR(INFO) << "Destroying InternalAllocator finished";
}

template <class AllocatorT>
void *AllocInRunSlots(AllocatorT *runslots_allocator, size_t size, Alignment align, size_t pool_size)
{
    void *res = runslots_allocator->Alloc(size, align);
    if (res == nullptr) {
        // Get rid of extra pool adding to the allocator
        static os::memory::Mutex pool_lock;
        os::memory::LockHolder lock(pool_lock);
        while (true) {
            res = runslots_allocator->Alloc(size, align);
            if (res != nullptr) {
                break;
            }
            LOG_INTERNAL_ALLOCATOR(DEBUG) << "RunSlotsAllocator didn't allocate memory, try to add new pool";
            auto pool = PoolManager::GetMmapMemPool()->AllocPool(pool_size, SpaceType::SPACE_TYPE_INTERNAL,
                                                                 AllocatorType::RUNSLOTS_ALLOCATOR, runslots_allocator);
            if (UNLIKELY(pool.GetMem() == nullptr)) {
                return nullptr;
            }
            runslots_allocator->AddMemoryPool(pool.GetMem(), pool.GetSize());
            LOG_INTERNAL_ALLOCATOR(DEBUG) << "RunSlotsAllocator try to allocate memory again after pool adding";
        }
    }
    return res;
}

template <InternalAllocatorConfig Config>
template <AllocScope AllocScopeT>
void *InternalAllocator<Config>::AllocViaPandaAllocators(size_t size, Alignment align)
{
    void *res = nullptr;
    size_t aligned_size = AlignUp(size, GetAlignmentInBytes(align));
    static_assert(RunSlotsAllocatorT::GetMaxSize() == LocalSmallObjectAllocator::GetMaxSize());
    if (LIKELY(aligned_size <= RunSlotsAllocatorT::GetMaxSize())) {
        // NOLINTNEXTLINE(readability-braces-around-statements)
        if constexpr (AllocScopeT == AllocScope::GLOBAL) {
            LOG_INTERNAL_ALLOCATOR(DEBUG) << "Try to use RunSlotsAllocator";
            res = AllocInRunSlots(runslots_allocator_, size, align, RunSlotsAllocatorT::GetMinPoolSize());
            if (res == nullptr) {
                return nullptr;
            }
        } else {  // NOLINT(readability-misleading-indentation)
            static_assert(AllocScopeT == AllocScope::LOCAL);
            LOG_INTERNAL_ALLOCATOR(DEBUG) << "Try to use thread-local RunSlotsAllocator";
            ASSERT(panda::ManagedThread::GetCurrent()->GetLocalInternalAllocator() != nullptr);
            res = AllocInRunSlots(panda::ManagedThread::GetCurrent()->GetLocalInternalAllocator(), size, align,
                                  LocalSmallObjectAllocator::GetMinPoolSize());
            if (res == nullptr) {
                return nullptr;
            }
        }
    } else if (aligned_size <= FreeListAllocatorT::GetMaxSize()) {
        LOG_INTERNAL_ALLOCATOR(DEBUG) << "Try to use FreeListAllocator";
        res = freelist_allocator_->Alloc(size, align);
        if (res == nullptr) {
            // Get rid of extra pool adding to the allocator
            static os::memory::Mutex pool_lock;
            os::memory::LockHolder lock(pool_lock);
            while (true) {
                res = freelist_allocator_->Alloc(size, align);
                if (res != nullptr) {
                    break;
                }
                LOG_INTERNAL_ALLOCATOR(DEBUG) << "FreeListAllocator didn't allocate memory, try to add new pool";
                size_t pool_size = FreeListAllocatorT::GetMinPoolSize();
                auto pool = PoolManager::GetMmapMemPool()->AllocPool(
                    pool_size, SpaceType::SPACE_TYPE_INTERNAL, AllocatorType::FREELIST_ALLOCATOR, freelist_allocator_);
                if (UNLIKELY(pool.GetMem() == nullptr)) {
                    return nullptr;
                }
                freelist_allocator_->AddMemoryPool(pool.GetMem(), pool.GetSize());
            }
        }
    } else {
        LOG_INTERNAL_ALLOCATOR(DEBUG) << "Try to use HumongousObjAllocator";
        res = humongous_allocator_->Alloc(size, align);
        if (res == nullptr) {
            // Get rid of extra pool adding to the allocator
            static os::memory::Mutex pool_lock;
            os::memory::LockHolder lock(pool_lock);
            while (true) {
                res = humongous_allocator_->Alloc(size, align);
                if (res != nullptr) {
                    break;
                }
                LOG_INTERNAL_ALLOCATOR(DEBUG) << "HumongousObjAllocator didn't allocate memory, try to add new pool";
                size_t pool_size = HumongousObjAllocatorT::GetMinPoolSize(size);
                auto pool =
                    PoolManager::GetMmapMemPool()->AllocPool(pool_size, SpaceType::SPACE_TYPE_INTERNAL,
                                                             AllocatorType::HUMONGOUS_ALLOCATOR, humongous_allocator_);
                if (UNLIKELY(pool.GetMem() == nullptr)) {
                    return nullptr;
                }
                humongous_allocator_->AddMemoryPool(pool.GetMem(), pool.GetSize());
            }
        }
    }
    return res;
}

template <InternalAllocatorConfig Config>
void InternalAllocator<Config>::FreeViaPandaAllocators(void *ptr)
{
    AllocatorType alloc_type = PoolManager::GetMmapMemPool()->GetAllocatorInfoForAddr(ptr).GetType();
    switch (alloc_type) {
        case AllocatorType::RUNSLOTS_ALLOCATOR:
            if (PoolManager::GetMmapMemPool()->GetAllocatorInfoForAddr(ptr).GetAllocatorHeaderAddr() ==
                runslots_allocator_) {
                LOG_INTERNAL_ALLOCATOR(DEBUG) << "free via RunSlotsAllocator";
                runslots_allocator_->Free(ptr);
            } else {
                LOG_INTERNAL_ALLOCATOR(DEBUG) << "free via thread-local RunSlotsAllocator";
                // It is a thread-local internal allocator instance
                LocalSmallObjectAllocator *local_allocator =
                    panda::ManagedThread::GetCurrent()->GetLocalInternalAllocator();
                ASSERT(PoolManager::GetMmapMemPool()->GetAllocatorInfoForAddr(ptr).GetAllocatorHeaderAddr() ==
                       local_allocator);
                local_allocator->Free(ptr);
            }
            break;
        case AllocatorType::FREELIST_ALLOCATOR:
            LOG_INTERNAL_ALLOCATOR(DEBUG) << "free via FreeListAllocator";
            ASSERT(PoolManager::GetMmapMemPool()->GetAllocatorInfoForAddr(ptr).GetAllocatorHeaderAddr() ==
                   freelist_allocator_);
            freelist_allocator_->Free(ptr);
            break;
        case AllocatorType::HUMONGOUS_ALLOCATOR:
            LOG_INTERNAL_ALLOCATOR(DEBUG) << "free via HumongousObjAllocator";
            ASSERT(PoolManager::GetMmapMemPool()->GetAllocatorInfoForAddr(ptr).GetAllocatorHeaderAddr() ==
                   humongous_allocator_);
            humongous_allocator_->Free(ptr);
            break;
        default:
            UNREACHABLE();
            break;
    }
}

/* static */
template <InternalAllocatorConfig Config>
typename InternalAllocator<Config>::LocalSmallObjectAllocator *InternalAllocator<Config>::SetUpLocalInternalAllocator(
    Allocator *allocator)
{
    (void)allocator;
    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if constexpr (Config == InternalAllocatorConfig::PANDA_ALLOCATORS) {
        auto local_allocator = allocator->New<LocalSmallObjectAllocator>(allocator->GetMemStats());
        LOG_INTERNAL_ALLOCATOR(DEBUG) << "Set up local internal allocator at addr " << local_allocator
                                      << " for the thread " << panda::Thread::GetCurrent();
        return local_allocator;
    }
    return nullptr;
}

/* static */
template <InternalAllocatorConfig Config>
void InternalAllocator<Config>::FinalizeLocalInternalAllocator(
    InternalAllocator::LocalSmallObjectAllocator *local_allocator, Allocator *allocator)
{
    (void)local_allocator;
    (void)allocator;
    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if constexpr (Config == InternalAllocatorConfig::PANDA_ALLOCATORS) {
        local_allocator->VisitAndRemoveAllPools(
            [](void *mem, [[maybe_unused]] size_t size) { PoolManager::GetMmapMemPool()->FreePool(mem, size); });
        allocator->Delete(local_allocator);
    }
}

/* static */
template <InternalAllocatorConfig Config>
void InternalAllocator<Config>::RemoveFreePoolsForLocalInternalAllocator(LocalSmallObjectAllocator *local_allocator)
{
    (void)local_allocator;
    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if constexpr (Config == InternalAllocatorConfig::PANDA_ALLOCATORS) {
        local_allocator->VisitAndRemoveFreePools(
            [](void *mem, [[maybe_unused]] size_t size) { PoolManager::GetMmapMemPool()->FreePool(mem, size); });
    }
}

template <InternalAllocatorConfig Config>
void InternalAllocator<Config>::InitInternalAllocatorFromRuntime(Allocator *allocator)
{
    ASSERT(allocator_from_runtime == nullptr);
    allocator_from_runtime = allocator;
}

template <InternalAllocatorConfig Config>
Allocator *InternalAllocator<Config>::GetInternalAllocatorFromRuntime()
{
    return allocator_from_runtime;
}

template <InternalAllocatorConfig Config>
void InternalAllocator<Config>::ClearInternalAllocatorFromRuntime()
{
    allocator_from_runtime = nullptr;
}

template class InternalAllocator<InternalAllocatorConfig::PANDA_ALLOCATORS>;
template class InternalAllocator<InternalAllocatorConfig::MALLOC_ALLOCATOR>;
template void *InternalAllocator<InternalAllocatorConfig::PANDA_ALLOCATORS>::Alloc<AllocScope::GLOBAL>(size_t,
                                                                                                       Alignment);
template void *InternalAllocator<InternalAllocatorConfig::PANDA_ALLOCATORS>::Alloc<AllocScope::LOCAL>(size_t,
                                                                                                      Alignment);
template void *InternalAllocator<InternalAllocatorConfig::MALLOC_ALLOCATOR>::Alloc<AllocScope::GLOBAL>(size_t,
                                                                                                       Alignment);
template void *InternalAllocator<InternalAllocatorConfig::MALLOC_ALLOCATOR>::Alloc<AllocScope::LOCAL>(size_t,
                                                                                                      Alignment);

#undef LOG_INTERNAL_ALLOCATOR

}  // namespace panda::mem
