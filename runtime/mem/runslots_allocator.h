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

#ifndef PANDA_RUNTIME_MEM_RUNSLOTS_ALLOCATOR_H_
#define PANDA_RUNTIME_MEM_RUNSLOTS_ALLOCATOR_H_

#include <algorithm>
#include <array>
#include <cstddef>

#include "libpandabase/macros.h"
#include "libpandabase/mem/mem.h"
#include "libpandabase/mem/space.h"
#include "libpandabase/utils/logger.h"
#include "runtime/mem/runslots.h"
#include "runtime/mem/gc/bitmap.h"
#include "runtime/mem/lock_config_helper.h"

namespace panda::mem {

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_RUNSLOTS_ALLOCATOR(level) LOG(level, ALLOC) << "RunSlotsAllocator: "

class RunSlotsAllocatorLockConfig {
public:
    class CommonLock {
    public:
        using PoolLock = os::memory::RWLock;
        using ListLock = os::memory::Mutex;
        using RunSlotsLock = RunSlotsLockConfig::CommonLock;
    };

    class DummyLock {
    public:
        using PoolLock = os::memory::DummyLock;
        using ListLock = os::memory::DummyLock;
        using RunSlotsLock = RunSlotsLockConfig::DummyLock;
    };

    template <MTModeT MTMode>
    using ParameterizedLock = typename LockConfigHelper<RunSlotsAllocatorLockConfig, MTMode>::Value;
};

template <typename T, typename AllocConfigT, typename LockConfigT>
class RunSlotsAllocatorAdapter;
enum class InternalAllocatorConfig;
template <InternalAllocatorConfig Config>
class InternalAllocator;

/**
 * RunSlotsAllocator is an allocator based on RunSlots instance.
 * It gets a big pool of memory from OS and uses it for creating RunSlots with different slot sizes.
 */
template <typename AllocConfigT, typename LockConfigT = RunSlotsAllocatorLockConfig::CommonLock>
class RunSlotsAllocator {
public:
    explicit RunSlotsAllocator(MemStatsType *mem_stats, SpaceType type_allocation = SpaceType::SPACE_TYPE_OBJECT);
    ~RunSlotsAllocator();
    NO_COPY_SEMANTIC(RunSlotsAllocator);
    NO_MOVE_SEMANTIC(RunSlotsAllocator);

    template <typename T, typename... Args>
    [[nodiscard]] T *New(Args &&... args)
    {
        auto p = reinterpret_cast<void *>(Alloc(sizeof(T)));
        new (p) T(std::forward<Args>(args)...);
        return reinterpret_cast<T *>(p);
    }

    template <typename T>
    [[nodiscard]] T *AllocArray(size_t arr_length);

    template <bool disable_use_free_runslots = false>
    [[nodiscard]] void *Alloc(size_t size, Alignment align = DEFAULT_ALIGNMENT);

    void Free(void *mem);

    void Collect(const GCObjectVisitor &death_checker_fn);

    bool AddMemoryPool(void *mem, size_t size);

    /**
     * \brief Iterates over all objects allocated by this allocator.
     * @tparam MemVisitor
     * @param mem_visitor - function pointer or functor
     */
    template <typename ObjectVisitor>
    void IterateOverObjects(const ObjectVisitor &object_visitor);

    /**
     * \brief Iterates over all memory pools used by this allocator
     * and remove them from the allocator structure.
     * NOTE: This method can't be used to clear all internal allocator
     * information and reuse the allocator somewhere else.
     * @tparam MemVisitor
     * @param mem_visitor - function pointer or functor
     */
    template <typename MemVisitor>
    void VisitAndRemoveAllPools(const MemVisitor &mem_visitor);

    /**
     * \brief Visit memory pools that can be returned to the system in this allocator
     * and remove them from the allocator structure.
     * @tparam MemVisitor
     * @param mem_visitor - function pointer or functor
     */
    template <typename MemVisitor>
    void VisitAndRemoveFreePools(const MemVisitor &mem_visitor);

    /**
     * \brief Iterates over objects in the range inclusively.
     * @tparam MemVisitor
     * @param mem_visitor - function pointer or functor
     * @param left_border - a pointer to the first byte of the range
     * @param right_border - a pointer to the last byte of the range
     */
    template <typename MemVisitor>
    void IterateOverObjectsInRange(const MemVisitor &mem_visitor, void *left_border, void *right_border);

    RunSlotsAllocatorAdapter<void, AllocConfigT, LockConfigT> Adapter();

    /**
     * \brief returns maximum size which can be allocated by this allocator
     * @return
     */
    static constexpr size_t GetMaxSize()
    {
        return RunSlotsType::MaxSlotSize();
    }

    /**
     * \brief returns minimum pool size which can be added to this allocator
     * @return
     */
    static constexpr size_t GetMinPoolSize()
    {
        return MIN_POOL_SIZE;
    }

    static constexpr size_t PoolAlign()
    {
        return DEFAULT_ALIGNMENT_IN_BYTES;
    }

    size_t VerifyAllocator();

    bool ContainObject(const ObjectHeader *obj);

    bool IsLive(const ObjectHeader *obj);

    static constexpr AllocatorType GetAllocatorType()
    {
        return AllocatorType::RUNSLOTS_ALLOCATOR;
    }

private:
    using RunSlotsType = RunSlots<typename LockConfigT::RunSlotsLock>;

    static constexpr size_t MIN_POOL_SIZE = PANDA_DEFAULT_ALLOCATOR_POOL_SIZE;

    class RunSlotsList {
    public:
        RunSlotsList()
        {
            head_ = nullptr;
            tail_ = nullptr;
        }

        typename LockConfigT::ListLock *GetLock()
        {
            return &lock_;
        }

        RunSlotsType *GetHead()
        {
            return head_;
        }

        RunSlotsType *GetTail()
        {
            return tail_;
        }

        void PushToTail(RunSlotsType *runslots);

        RunSlotsType *PopFromHead();

        RunSlotsType *PopFromTail();

        void PopFromList(RunSlotsType *runslots);

        bool IsInThisList(RunSlotsType *runslots)
        {
            RunSlotsType *current = head_;
            while (current != nullptr) {
                if (current == runslots) {
                    return true;
                }
                current = current->GetNextRunSlots();
            }
            return false;
        }

        ~RunSlotsList() = default;

        NO_COPY_SEMANTIC(RunSlotsList);
        NO_MOVE_SEMANTIC(RunSlotsList);

    private:
        RunSlotsType *head_;
        RunSlotsType *tail_;
        typename LockConfigT::ListLock lock_;
    };

    /**
     * MemPoolManager class is used for manage memory which we get from OS.
     * Current implementation limits the amount of memory pools which can be managed by this class.
     */

    // MemPoolManager structure:
    //
    //           part_occupied_head_ - is a pointer to the first partially occupied pool in occupied list
    //            |
    //            |                occupied_tail_ - is a pointer to the last occupied pool
    //            |                 |
    //            |  part occupied  |
    //            v     pools       v
    // |x|x|x|x|x|x|x|x|x|x|x|x|x|x|x|
    //  ^                           ^
    //  |      occupied  pools      |
    //
    //                   free_tail_ - is a pointer to the last totally free pool.
    //                    |
    //                    |
    //                    v
    // |0|0|0|0|0|0|0|0|0|0|
    //  ^                 ^
    //  |   free  pools   |
    //

    class MemPoolManager {
    public:
        explicit MemPoolManager();

        RunSlotsType *GetNewRunSlots(size_t slots_size);

        bool AddNewMemoryPool(void *mem, size_t size);

        template <typename ObjectVisitor>
        void IterateOverObjects(const ObjectVisitor &object_visitor);

        template <typename MemVisitor>
        void VisitAllPools(const MemVisitor &mem_visitor);

        template <typename MemVisitor>
        void VisitAllPoolsWithOccupiedSize(const MemVisitor &mem_visitor);

        template <typename MemVisitor>
        void VisitAndRemoveFreePools(const MemVisitor &mem_visitor);

        void ReturnAndReleaseRunSlotsMemory(RunSlotsType *runslots);

        bool IsInMemPools(void *object);

        ~MemPoolManager() = default;

        NO_COPY_SEMANTIC(MemPoolManager);
        NO_MOVE_SEMANTIC(MemPoolManager);

    private:
        class PoolListElement {
        public:
            PoolListElement();

            void Initialize(void *pool_mem, uintptr_t unoccupied_mem, size_t size, PoolListElement *prev);

            static PoolListElement *Create(void *mem, size_t size, PoolListElement *prev)
            {
                LOG_RUNSLOTS_ALLOCATOR(DEBUG)
                    << "PoolMemory: Create new instance with size " << size << " bytes at addr " << std::hex << mem;
                ASSERT(mem != nullptr);
                ASSERT(sizeof(PoolListElement) <= RUNSLOTS_SIZE);
                ASAN_UNPOISON_MEMORY_REGION(mem, sizeof(PoolListElement));
                auto new_element = new (mem) PoolListElement();
                uintptr_t unoccupied_mem = AlignUp(ToUintPtr(mem) + sizeof(PoolListElement), RUNSLOTS_SIZE);
                ASSERT(unoccupied_mem < ToUintPtr(mem) + size);
                new_element->Initialize(mem, unoccupied_mem, size, prev);
                return new_element;
            }

            bool HasMemoryForRunSlots();

            bool IsInitialized()
            {
                return start_mem_ != 0;
            }

            RunSlotsType *GetMemoryForRunSlots(size_t slots_size);

            template <typename RunSlotsVisitor>
            void IterateOverRunSlots(const RunSlotsVisitor &runslots_visitor);

            bool HasUsedMemory();

            size_t GetOccupiedSize();

            bool IsInUsedMemory(void *object);

            void *GetPoolMemory()
            {
                return ToVoidPtr(pool_mem_);
            }

            size_t GetSize()
            {
                return size_;
            }

            PoolListElement *GetNext() const
            {
                return next_pool_;
            }

            PoolListElement *GetPrev() const
            {
                return prev_pool_;
            }

            void SetPrev(PoolListElement *prev)
            {
                prev_pool_ = prev;
            }

            void SetNext(PoolListElement *next)
            {
                next_pool_ = next;
            }

            void PopFromList();

            void AddFreedRunSlots(RunSlotsType *slots)
            {
                [[maybe_unused]] bool old_val = freed_runslots_bitmap_.AtomicTestAndSet(slots);
                ASSERT(!old_val);
                freeded_runslots_count_++;
                ASAN_POISON_MEMORY_REGION(slots, RUNSLOTS_SIZE);
            }

            bool IsInFreedRunSlots(void *addr)
            {
                void *align_addr = ToVoidPtr((ToUintPtr(addr) >> RUNSLOTS_ALIGNMENT) << RUNSLOTS_ALIGNMENT);
                return freed_runslots_bitmap_.TestIfAddrValid(align_addr);
            }

            size_t GetFreedRunSlotsCount()
            {
                return freeded_runslots_count_;
            }

            ~PoolListElement() = default;

            NO_COPY_SEMANTIC(PoolListElement);
            NO_MOVE_SEMANTIC(PoolListElement);

        private:
            using MemBitmapClass = MemBitmap<RUNSLOTS_SIZE, uintptr_t>;
            using BitMapStorageType = std::array<uint8_t, MemBitmapClass::GetBitMapSizeInByte(MIN_POOL_SIZE)>;

            uintptr_t GetFirstRunSlotsBlock(uintptr_t mem);

            RunSlotsType *GetFreedRunSlots(size_t slots_size);

            uintptr_t pool_mem_;
            uintptr_t start_mem_;
            std::atomic<uintptr_t> free_ptr_;
            size_t size_;
            PoolListElement *next_pool_;
            PoolListElement *prev_pool_;
            size_t freeded_runslots_count_;
            BitMapStorageType storage_for_bitmap_;
            MemBitmapClass freed_runslots_bitmap_ {nullptr, MIN_POOL_SIZE, storage_for_bitmap_.data()};
        };

        PoolListElement *free_tail_;
        PoolListElement *partially_occupied_head_;
        PoolListElement *occupied_tail_;
        typename LockConfigT::PoolLock lock_;
    };

    void ReleaseEmptyRunSlotsPagesUnsafe();

    template <bool LockRunSlots>
    void FreeUnsafe(void *mem);

    bool FreeUnsafeInternal(RunSlotsType *runslots, void *mem);

    void TrimUnsafe();

    // Return true if this object could be allocated by the RunSlots allocator.
    // Does not check any live objects bitmap inside.
    bool AllocatedByRunSlotsAllocator(void *object);

    bool AllocatedByRunSlotsAllocatorUnsafe(void *object);

    RunSlotsType *CreateNewRunSlotsFromMemory(size_t slots_size);

    // Add one to the array size to just use the size (power of two) for RunSlots list without any modifications
    static constexpr size_t SLOTS_SIZES_VARIANTS = RunSlotsType::SlotSizesVariants() + 1;

    std::array<RunSlotsList, SLOTS_SIZES_VARIANTS> runslots_;

    // Add totally free RunSlots in this list for possibility to reuse them with different element sizes.
    RunSlotsList free_runslots_;

    MemPoolManager memory_pool_;
    SpaceType type_allocation_;

    MemStatsType *mem_stats_;

    template <typename T>
    friend class PygoteSpaceAllocator;
    friend class RunSlotsAllocatorTest;
    template <InternalAllocatorConfig Config>
    friend class InternalAllocator;
};

template <typename AllocConfigT, typename LockConfigT>
template <typename T>
T *RunSlotsAllocator<AllocConfigT, LockConfigT>::AllocArray(size_t arr_length)
{
    return static_cast<T *>(Alloc(sizeof(T) * arr_length));
}

#undef LOG_RUNSLOTS_ALLOCATOR

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_RUNSLOTS_ALLOCATOR_H_
