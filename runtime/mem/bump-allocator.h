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

#ifndef PANDA_RUNTIME_MEM_BUMP_ALLOCATOR_H_
#define PANDA_RUNTIME_MEM_BUMP_ALLOCATOR_H_

#include <functional>
#include <memory>

#include "mem/mem_pool.h"
#include "libpandabase/macros.h"
#include "libpandabase/mem/arena.h"
#include "libpandabase/mem/mem.h"
#include "libpandabase/mem/mem_range.h"
#include "runtime/mem/tlab.h"
#include "runtime/mem/lock_config_helper.h"

namespace panda {
class ObjectHeader;
}  // namespace panda

namespace panda::mem {

class BumpPointerAllocatorLockConfig {
public:
    using CommonLock = os::memory::Mutex;
    using DummyLock = os::memory::DummyLock;

    template <MTModeT MTMode>
    using ParameterizedLock = typename LockConfigHelper<BumpPointerAllocatorLockConfig, MTMode>::Value;
};

// This allocator can allocate memory as a BumpPointerAllocator
// and also can allocate big pieces of memory for the TLABs.
//
// Structure:
//
//  |------------------------------------------------------------------------------------------------------------|
//  |                                                 Memory Pool                                                |
//  |------------------------------------------------------------------------------------------------------------|
//  |     allocated objects     |         unused memory        |                 memory for TLABs                |
//  |---------------------------|------------------------------|-------------------------------------------------|
//  |xxxxxxxxxx|xxxxxx|xxxxxxxxx|                              |               ||               ||               |
//  |xxxxxxxxxx|xxxxxx|xxxxxxxxx|                              |               ||               ||               |
//  |xxxxxxxxxx|xxxxxx|xxxxxxxxx|           free memory        |     TLAB 3    ||     TLAB 2    ||     TLAB 1    |
//  |xxxxxxxxxx|xxxxxx|xxxxxxxxx|                              |               ||               ||               |
//  |xxxxxxxxxx|xxxxxx|xxxxxxxxx|                              |               ||               ||               |
//  |------------------------------------------------------------------------------------------------------------|
//

template <typename AllocConfigT, typename LockConfigT = BumpPointerAllocatorLockConfig::CommonLock,
          bool UseTlabs = false>
class BumpPointerAllocator {
public:
    DEFAULT_NOEXCEPT_MOVE_SEMANTIC(BumpPointerAllocator);
    NO_COPY_SEMANTIC(BumpPointerAllocator);
    ~BumpPointerAllocator();

    BumpPointerAllocator() = delete;

    TLAB *CreateNewTLAB(size_t size);

    /**
     * Construct BumpPointer allocator with provided pool
     * @param pool - pool
     */
    explicit BumpPointerAllocator(Pool pool, SpaceType type_allocation, MemStatsType *mem_stats,
                                  size_t tlabs_max_count = 0);

    [[nodiscard]] void *Alloc(size_t size, Alignment alignment = panda::DEFAULT_ALIGNMENT);

    void VisitAndRemoveAllPools(const MemVisitor &mem_visitor);

    void VisitAndRemoveFreePools(const MemVisitor &mem_visitor);

    /**
     * \brief Iterates over all objects allocated by this allocator
     * @param object_visitor
     */
    void IterateOverObjects(const std::function<void(ObjectHeader *object_header)> &object_visitor);

    /**
     * \brief Iterates over objects in the range inclusively.
     * @tparam MemVisitor
     * @param mem_visitor - function pointer or functor
     * @param left_border - a pointer to the first byte of the range
     * @param right_border - a pointer to the last byte of the range
     */
    template <typename MemVisitor>
    void IterateOverObjectsInRange(const MemVisitor &mem_visitor, void *left_border, void *right_border);

    /**
     * Resets to the "all clear" state
     */
    void Reset();

    /**
     * \brief Add an extra memory pool to the allocator.
     * The memory pool must be located just after the current memory given to this allocator.
     * @param mem - pointer to the extra memory pool.
     * @param size - a size of the extra memory pool.
     */
    void ExpandMemory(void *mem, size_t size);

    /**
     * Get MemRange used by allocator
     * @return MemRange for allocator
     */
    MemRange GetMemRange();

    // BumpPointer allocator can't be used for simple collection.
    // Only for CollectAndMove.
    void Collect(GCObjectVisitor death_checker_fn) = delete;

    /**
     * Collects dead objects and move alive with provided visitor
     * @param death_checker - functor for check if object alive
     * @param object_move_visitor - object visitor
     */
    template <typename ObjectMoveVisitorT>
    void CollectAndMove(const GCObjectVisitor &death_checker, const ObjectMoveVisitorT &object_move_visitor);

    static constexpr AllocatorType GetAllocatorType()
    {
        return AllocatorType::BUMP_ALLOCATOR;
    }

    bool ContainObject(const ObjectHeader *obj);

    bool IsLive(const ObjectHeader *obj);

private:
    class TLABsManager {
    public:
        explicit TLABsManager(size_t tlabs_max_count) : tlabs_max_count_(tlabs_max_count), tlabs_(tlabs_max_count) {}
        ~TLABsManager() = default;
        DEFAULT_MOVE_SEMANTIC(TLABsManager);
        DEFAULT_COPY_SEMANTIC(TLABsManager);

        void Reset()
        {
            for (size_t i = 0; i < cur_tlab_num_; i++) {
                tlabs_[i].Fill(nullptr, 0);
            }
            cur_tlab_num_ = 0;
            tlabs_occupied_size_ = 0;
        }

        TLAB *GetUnusedTLABInstance()
        {
            if (cur_tlab_num_ < tlabs_max_count_) {
                return &tlabs_[cur_tlab_num_++];
            }
            return nullptr;
        }

        template <class Visitor>
        void IterateOverTLABs(const Visitor &visitor)
        {
            for (size_t i = 0; i < cur_tlab_num_; i++) {
                if (!visitor(&tlabs_[i])) {
                    return;
                }
            }
        }

        size_t GetTLABsOccupiedSize()
        {
            return tlabs_occupied_size_;
        }

        void IncreaseTLABsOccupiedSize(size_t size)
        {
            tlabs_occupied_size_ += size;
        }

    private:
        size_t cur_tlab_num_ {0};
        size_t tlabs_max_count_;
        std::vector<TLAB> tlabs_;
        size_t tlabs_occupied_size_ {0};
    };

    // Mutex, which allows only one thread to Alloc/Free/Collect/Iterate inside this allocator
    LockConfigT allocator_lock_;
    Arena arena_;
    TLABsManager tlab_manager_;
    SpaceType type_allocation_;
    MemStatsType *mem_stats_;
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_BUMP_ALLOCATOR_H_
