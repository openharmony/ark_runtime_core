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

#ifndef PANDA_RUNTIME_MEM_TLAB_H_
#define PANDA_RUNTIME_MEM_TLAB_H_

#include "libpandabase/utils/logger.h"
#include "libpandabase/mem/mem.h"
#include "libpandabase/mem/pool_map.h"
#include "libpandabase/mem/mem_range.h"

namespace panda::mem {

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_TLAB_ALLOCATOR(level) LOG(level, ALLOC) << "TLAB: "

static constexpr size_t PANDA_TLAB_SIZE = 4_KB;
static constexpr size_t PANDA_TLAB_MAX_ALLOC_SIZE = PANDA_TLAB_SIZE;

#ifdef NDEBUG
static constexpr bool PANDA_TRACK_TLAB_ALLOCATIONS = false;
#else
static constexpr bool PANDA_TRACK_TLAB_ALLOCATIONS = true;
#endif
// Current TLAB structure looks like that:
//
// |--------------------------|
// |........TLAB class........|
// |--------------------------|
// |.........end addr.........|------------|
// |.......free pointer.......|--------|   |
// |........start addr........|----|   |   |
// |--------------------------|    |   |   |
//                                 |   |   |
//                                 |   |   |
// |--------------------------|    |   |   |
// |..Memory for allocations..|    |   |   |
// |--------------------------|    |   |   |
// |xxxxxxxxxxxxxxxxxxxxxxxxxx|<---|   |   |
// |xxxxxxxxxxxxxxxxxxxxxxxxxx|        |   |
// |xxxxxxxxxxxxxxxxxxxxxxxxxx|        |   |
// |xxxxxxxxxxxxxxxxxxxxxxxxxx|        |   |
// |xxxxxxxxxxxxxxxxxxxxxxxxxx|        |   |
// |xxxxxxxxxxxxxxxxxxxxxxxxxx|<-------|   |
// |..........................|            |
// |..........................|            |
// |..........................|            |
// |..........................|            |
// |..........................|<-----------|
// |--------------------------|
//
// Each TLAB is connected with certain thread:
// (NOTE: In current implementation, we can reach max one TLAB from a thread metadata)
// it is better to remove these fields.
// |------------------------|              |---------------|
// | Thread Metainformation | ---------->  | Current  TLAB |----
// |------------------------|              |---------------|    |
//                                                              |
//                                                              |
//                                         |---------------|    |
//                                         | Previous TLAB |<---|
//                                         |---------------|

// How to use TLAB from the compiler if we want to allocate an object for class 'cls' with size 'allocation_size':
// NOTE: If we have PANDA_TRACK_TLAB_ALLOCATIONS option on, JIT should always call Runtime at AllocateObject calls.
// Pseudocode:
// IF  allocation_size > TLAB::GetMaxSize() || IsFinalizable(cls)
//     call HeapManager::AllocateObject(obj_class, allocation_size)
// ELSE
//     // We should use TLS for this purpose.
//     // Read current TLAB pointer from TLS:
//     load TLS.TLAB -> cur_TLAB
//     // Read uintptr_t value from TLAB structure:
//     load (AddressOf(cur_TLAB) + TLAB::TLABFreePointerOffset) -> free_pointer
//     // Read uintptr_t value from TLAB structure:
//     load (AddressOf(cur_TLAB) + TLAB::TLABEndAddrOffset) -> end_pointer
//     // Align the size of an object to DEFAULT_ALIGNMENT_IN_BYTES
//     // One can use GetAlignedObjectSize() method for that.
//     align (allocation_size, DEFAULT_ALIGNMENT_IN_BYTES) -> allocation_size
//     IF  free_pointer + allocation_size > end_pointer
//         // Goto slow path
//         call HeapManager::AllocateObject(obj_class, allocation_size)
//     // Calculate new_free_pointer:
//     new_free_pointer = AddressOf(free_pointer) + allocation_size
//     // Store new_free_pointer to (cur_TLAB + TLAB::TLABFreePointerOffset):
//     store (AddressOf(cur_TLAB) + TLAB::TLABFreePointerOffset) <- new_free_pointer
//     return free_pointer
//
// After that the Compiler should initialize class word inside new object and
// set correct GC bits in the mark word:
//     ObjectHeader obj_header
//     obj_header.SetClass(cls)
//     GetGC()->InitGCBitsForAllocationInTLAB(&obj_header)
//     free_pointer <- obj_header
//
// Runtime should provide these parameters:
// HeapManager::GetTLABMaxAllocSize() - max size that can be allocated via TLAB. (depends on the allocator used by GC)
// HeapManager::UseTLABForAllocations() - do we need to use TLABs for allocations. (it is a runtime option)
// GC::InitGCBitsForAllocationInTLAB() - method for initialize GC bits inside the object header
//                                       during allocations through TLAB
// TLAB::TLABFreePointerOffset() - an offset of a free pointer field inside TLAB.
// TLAB::TLABEndAddrOffset() - an offset of an end buffer pointer field inside TLAB.
//

class TLAB {
public:
    /**
     * \brief Construct TLAB with the buffer at \param address with \param size
     * @param address - a pointer into the memory where TLAB memory will be created
     * @param size - a size of the allocated memory for the TLAB
     */
    explicit TLAB(void *address = nullptr, size_t size = 0);
    ~TLAB();

    void Destroy();

    /**
     * \brief Fill a TLAB with the buffer at \param address with \param size
     * @param address - a pointer into the memory where TLAB memory will be created
     * @param size - a size of the allocated memory for the TLAB
     */
    void Fill(void *address, size_t size);

    /**
     * \brief Set TLAB to be empty
     */
    void Reset()
    {
        Fill(nullptr, 0U);
    }

    bool IsEmpty()
    {
        return (memory_start_addr_ == nullptr) || (cur_free_position_ == nullptr) || (memory_end_addr_ == nullptr);
    }

    NO_MOVE_SEMANTIC(TLAB);
    NO_COPY_SEMANTIC(TLAB);

    /**
     * \brief returns maximum size which can be allocated by TLAB allocator
     * @return
     */
    static constexpr size_t GetMaxSize()
    {
        return PANDA_TLAB_MAX_ALLOC_SIZE;
    }

    /**
     * \brief returns default pool size which must be added to a TLAB
     * @return
     */
    static constexpr size_t GetDefaultPoolSize()
    {
        return PANDA_TLAB_SIZE;
    }

    /**
     * \brief Allocates memory with size \param size and aligned with DEFAULT_ALIGNMENT alignment
     * @param size - size of the allocated memory
     * @return pointer to the allocated memory on success, or nullptr on fail
     */
    void *Alloc(size_t size);

    /**
     * \brief Iterates over all objects in this TLAB
     * @param object_visitor
     */
    void IterateOverObjects(const std::function<void(ObjectHeader *object_header)> &object_visitor);

    /**
     * \brief Iterates over objects in the range inclusively.
     * @param mem_visitor - function pointer or functor
     * @param mem_range - memory range
     */
    void IterateOverObjectsInRange(const std::function<void(ObjectHeader *object_header)> &mem_visitor,
                                   const MemRange &mem_range);

    /**
     * Collects dead objects and move alive with provided visitor
     * @param death_checker - functor for check if object alive
     * @param object_move_visitor - object visitor
     */
    template <typename ObjectMoveVisitorT>
    void CollectAndMove(const GCObjectVisitor &death_checker, const ObjectMoveVisitorT &object_move_visitor)
    {
        LOG_TLAB_ALLOCATOR(DEBUG) << "CollectAndMove started";
        IterateOverObjects([&](ObjectHeader *object_header) {
            // We are interested only in moving alive objects, after that we cleanup this buffer
            if (death_checker(object_header) == ObjectStatus::ALIVE_OBJECT) {
                LOG_TLAB_ALLOCATOR(DEBUG) << "CollectAndMove found alive object with addr " << object_header;
                object_move_visitor(object_header);
            }
        });
        LOG_TLAB_ALLOCATOR(DEBUG) << "CollectAndMove finished";
    }

    bool ContainObject(const ObjectHeader *obj);

    bool IsLive(const ObjectHeader *obj);

    TLAB *GetNextTLAB()
    {
        return next_tlab_;
    }

    TLAB *GetPrevTLAB()
    {
        return prev_tlab_;
    }

    void SetNextTLAB(TLAB *tlab_pointer)
    {
        next_tlab_ = tlab_pointer;
    }

    void SetPrevTLAB(TLAB *tlab_pointer)
    {
        prev_tlab_ = tlab_pointer;
    }

    void *GetStartAddr()
    {
        return memory_start_addr_;
    }

    void *GetCurPos()
    {
        return cur_free_position_;
    }

    size_t GetOccupiedSize()
    {
        ASSERT(ToUintPtr(cur_free_position_) >= ToUintPtr(memory_start_addr_));
        return ToUintPtr(cur_free_position_) - ToUintPtr(memory_start_addr_);
    }

    MemRange GetMemRangeForOccupiedMemory() const
    {
        return MemRange(ToUintPtr(memory_start_addr_), ToUintPtr(cur_free_position_) - 1);
    }

    static constexpr size_t TLABStartAddrOffset()
    {
        return MEMBER_OFFSET(TLAB, memory_start_addr_);
    }

    static constexpr size_t TLABFreePointerOffset()
    {
        return MEMBER_OFFSET(TLAB, cur_free_position_);
    }

    static constexpr size_t TLABEndAddrOffset()
    {
        return MEMBER_OFFSET(TLAB, memory_end_addr_);
    }

    static constexpr AllocatorType GetAllocatorType()
    {
        return AllocatorType::TLAB_ALLOCATOR;
    }

private:
    size_t GetFreeSize()
    {
        ASSERT(ToUintPtr(cur_free_position_) >= ToUintPtr(memory_start_addr_));
        ASSERT(ToUintPtr(cur_free_position_) <= ToUintPtr(memory_end_addr_));
        return ToUintPtr(memory_end_addr_) - ToUintPtr(cur_free_position_);
    }

    size_t GetSize()
    {
        ASSERT(ToUintPtr(memory_end_addr_) >= ToUintPtr(memory_start_addr_));
        return ToUintPtr(memory_end_addr_) - ToUintPtr(memory_start_addr_);
    }

    TLAB *next_tlab_;
    TLAB *prev_tlab_;
    void *memory_start_addr_ {nullptr};
    void *memory_end_addr_ {nullptr};
    void *cur_free_position_ {nullptr};
};

#undef LOG_TLAB_ALLOCATOR

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_TLAB_H_
