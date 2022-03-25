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

#ifndef PANDA_RUNTIME_MEM_REFSTORAGE_GLOBAL_OBJECT_STORAGE_H_
#define PANDA_RUNTIME_MEM_REFSTORAGE_GLOBAL_OBJECT_STORAGE_H_

#include <libpandabase/os/mutex.h>

#include "runtime/include/runtime.h"
#include "runtime/include/mem/panda_containers.h"
#include "runtime/include/object_header.h"
#include "runtime/mem/object_helpers.h"
#include "runtime/mem/gc/gc.h"
#include "runtime/mem/gc/gc_root.h"
#include "runtime/mem/gc/gc_phase.h"
#include "runtime/include/class.h"
#include "runtime/include/panda_vm.h"
#include "reference.h"
#include "utils/logger.h"
#include "utils/dfx.h"

namespace panda::mem::test {
class ReferenceStorageTest;
}  // namespace panda::mem::test

namespace panda::mem {

/**
 * Storage for objects which need to handle by GC. GC will handle moving these objects and will not reclaim then until
 * user haven't called Remove method on this object.
 * References will be removed automatically after Remove method or after storage's destructor.
 */
class GlobalObjectStorage {
public:
    explicit GlobalObjectStorage(mem::InternalAllocatorPtr allocator, size_t max_size, bool enable_size_check);

    ~GlobalObjectStorage();

    /**
     * Check whether ref is a valid global reference or not.
     */
    bool IsValidGlobalRef(const Reference *ref) const;

    /**
     * Add object to the storage and return associated pointer with this object
     */
    Reference *Add(const ObjectHeader *object, Reference::ObjectType type) const;

    /**
     * Get stored object associated with given reference. Reference should be returned on Add method before.
     */
    ObjectHeader *Get(const Reference *reference) const;

    /**
     * Remove object from storage by given reference. Reference should be returned on Add method before.
     */
    void Remove(const Reference *reference) const;

    /**
     * Get all objects from storage. Used by debugging.
     */
    PandaVector<ObjectHeader *> GetAllObjects();

    void VisitObjects(const GCRootVisitor &gc_root_visitor, mem::RootType rootType) const;

    /**
     * Update pointers to moved Objects in global storage.
     */
    void UpdateMovedRefs();

    void ClearUnmarkedWeakRefs(const GC *gc);

    size_t GetSize();

    void Dump();

private:
    NO_COPY_SEMANTIC(GlobalObjectStorage);
    NO_MOVE_SEMANTIC(GlobalObjectStorage);

    class ArrayStorage;

    static constexpr size_t GLOBAL_REF_SIZE_WARNING_LINE = 20;

    mem::InternalAllocatorPtr allocator_;
    ArrayStorage *global_storage_;
    ArrayStorage *weak_storage_;

    static void AssertType([[maybe_unused]] Reference::ObjectType type)
    {
        ASSERT(type == Reference::ObjectType::GLOBAL || type == Reference::ObjectType::WEAK);
    }

    friend class ::panda::mem::test::ReferenceStorageTest;

    class ArrayStorage {
#ifndef NDEBUG
        // for better coverage of EnsureCapacity
        static constexpr size_t INITIAL_SIZE = 2;
#else
        static constexpr size_t INITIAL_SIZE = 128;
#endif  // NDEBUG
        static constexpr size_t FREE_INDEX_BIT = 0;
        static constexpr size_t BITS_FOR_TYPE = 2U;
        static constexpr size_t BITS_FOR_INDEX = 1U;
        static constexpr size_t ENSURE_CAPACITY_MULTIPLIER = 2;

        /**
        There are 2 cases:
        1) When index is busy - then we store jobject in storage_ and 0 in the lowest bit (cause of alignment).
        Reference* contains it's index shifted by 2 with reference-type in lowest bits which we return to user and
        doesn't stores inside storage explicity.

        2) When index if free - storage[index] stores next free index (shifted by 1) with lowest bit equals to 1

        |-----------------------------------------------------|------------------|------------------|
        |      Case          |         Highest bits           | [1] lowest bit   | [0] lowest bit   |
        --------------------------------------------------------------------------------------------|
        | busy-index         |                                |                  |                  |
        | Reference* (index) |            index               |   0/1 (ref-type) |   0/1 (ref-type) |
        | storage[index]     |                           xxx                     |        0         |
        ---------------------|--------------------------------|------------------|-------------------
        | free-index         |                                                   |                  |
        | storage[index]     |                        xxx                        |        1         |
        ---------------------------------------------------------------------------------------------
        */

        PandaVector<uintptr_t> storage_ GUARDED_BY(mutex_) {};
        /**
         * Index of first available block in list
         */
        uintptr_t first_available_block_;
        /**
         * How many blocks are available in current storage (can be increased if size less than max size)
         */
        size_t blocks_available_;

        bool enable_size_check_;
        size_t max_size_;

        mutable os::memory::RWLock mutex_;
        mem::InternalAllocatorPtr allocator_;

    public:
        explicit ArrayStorage(mem::InternalAllocatorPtr allocator, size_t max_size, bool enable_size_check)
            : enable_size_check_(enable_size_check), max_size_(max_size), allocator_(allocator)
        {
            ASSERT(max_size < (std::numeric_limits<uintptr_t>::max() >> (BITS_FOR_TYPE)));

            blocks_available_ = INITIAL_SIZE;
            first_available_block_ = 0;

            storage_.resize(INITIAL_SIZE);
            for (size_t i = 0; i < storage_.size() - 1; i++) {
                storage_[i] = EncodeNextIndex(i + 1);
            }
            storage_[storage_.size() - 1] = 0;
        }

        ~ArrayStorage() = default;

        NO_COPY_SEMANTIC(ArrayStorage);
        NO_MOVE_SEMANTIC(ArrayStorage);

        Reference *Add(const ObjectHeader *object)
        {
            ASSERT(object != nullptr);
            os::memory::WriteLockHolder lk(mutex_);

            if (blocks_available_ == 0) {
                if (storage_.size() * ENSURE_CAPACITY_MULTIPLIER <= max_size_) {
                    EnsureCapacity();
                } else {
                    LOG(ERROR, RUNTIME) << "Global reference storage is full";
                    Dump();
                    return nullptr;
                }
            }
            ASSERT(blocks_available_ != 0);
            auto next_block = DecodeIndex(storage_[first_available_block_]);
            auto current_index = first_available_block_;
            AssertIndex(current_index);

            auto addr = reinterpret_cast<uintptr_t>(object);
            [[maybe_unused]] uintptr_t last_bit = BitField<uintptr_t, FREE_INDEX_BIT>::Get(addr);
            ASSERT(last_bit == 0);  // every object should be alignmented

            storage_[current_index] = addr;
            auto ref = IndexToReference(current_index);
            first_available_block_ = next_block;
            blocks_available_--;

            CheckAlmostOverflow();
            return ref;
        }

        void EnsureCapacity() REQUIRES(mutex_)
        {
            auto prev_length = storage_.size();
            size_t new_length = storage_.size() * ENSURE_CAPACITY_MULTIPLIER;
            blocks_available_ = first_available_block_ = prev_length;
            storage_.resize(new_length);
            for (size_t i = prev_length; i < new_length - 1; i++) {
                storage_[i] = EncodeNextIndex(i + 1);
            }
            storage_[storage_.size() - 1] = 0;
            LOG(DEBUG, RUNTIME) << "Increase global storage from: " << prev_length << " to: " << new_length;
        }

        void CheckAlmostOverflow() REQUIRES_SHARED(mutex_)
        {
            size_t now_size = GetSize();
            if (enable_size_check_ && now_size >= max_size_ - GLOBAL_REF_SIZE_WARNING_LINE) {
                LOG(INFO, RUNTIME) << "Global reference storage almost overflow. now size: " << now_size
                                   << ", max size: " << max_size_;
                Dump();
            }
        }

        ObjectHeader *Get(const Reference *ref) const
        {
            os::memory::ReadLockHolder lk(mutex_);
            auto index = ReferenceToIndex(ref);
            return reinterpret_cast<ObjectHeader *>(storage_[index]);
        }

        void Remove(const Reference *ref)
        {
            os::memory::WriteLockHolder lk(mutex_);
            auto index = ReferenceToIndex(ref);
            storage_[index] = EncodeNextIndex(first_available_block_);
            first_available_block_ = index;
            blocks_available_++;
        }

        void UpdateMovedRefs()
        {
            os::memory::WriteLockHolder lk(mutex_);
            // NOLINTNEXTLINE(modernize-loop-convert)
            for (size_t index = 0; index < storage_.size(); index++) {
                auto ref = storage_[index];
                if (IsBusy(ref)) {
                    auto obj = reinterpret_cast<ObjectHeader *>(ref);
                    if (obj != nullptr && obj->IsForwarded()) {
                        auto new_addr = reinterpret_cast<ObjectHeader *>(GetForwardAddress(obj));
                        storage_[index] = reinterpret_cast<uintptr_t>(new_addr);
                    }
                }
            }
        }

        void VisitObjects(const GCRootVisitor &gc_root_visitor, mem::RootType rootType)
        {
            os::memory::ReadLockHolder lk(mutex_);

            for (const auto &ref : storage_) {
                if (IsBusy(ref)) {
                    auto obj = reinterpret_cast<ObjectHeader *>(ref);
                    if (obj != nullptr) {
                        LOG(DEBUG, GC) << " Found root from global JNI: " << mem::GetDebugInfoAboutObject(obj);
                        gc_root_visitor({rootType, obj});
                    }
                }
            }
        }

        void ClearUnmarkedWeakRefs(const GC *gc)
        {
            ASSERT(IsMarking(gc->GetGCPhase()));
            os::memory::WriteLockHolder lk(mutex_);

            for (auto &ref : storage_) {
                if (IsBusy(ref)) {
                    auto obj = reinterpret_cast<ObjectHeader *>(ref);
                    uintptr_t obj_addr = ToUintPtr(obj);
                    if (gc->InGCSweepRange(obj_addr)) {
                        if (obj != nullptr && !gc->IsMarked(obj)) {
                            LOG(DEBUG, RUNTIME)
                                << "Clear not marked weak-reference: " << std::hex << ref << " object: " << obj;
                            ref = reinterpret_cast<uintptr_t>(nullptr);
                        }
                    }
                }
            }
        }

        PandaVector<ObjectHeader *> GetAllObjects()
        {
            auto objects = PandaVector<ObjectHeader *>(allocator_->Adapter());
            {
                os::memory::ReadLockHolder lk(mutex_);
                for (const auto &ref : storage_) {
                    // we don't return nulls on GetAllObjects
                    if (ref != 0 && IsBusy(ref)) {
                        auto obj = reinterpret_cast<ObjectHeader *>(ref);
                        objects.push_back(obj);
                    }
                }
            }
            return objects;
        }

        // NO_THREAD_SAFETY_ANALYSIS cause TSAN doesn't understand that we don't touch storage_ in ReferenceToIndex
        bool IsValidGlobalRef(const Reference *ref) NO_THREAD_SAFETY_ANALYSIS
        {
            ASSERT(ref != nullptr);
            uintptr_t index = ReferenceToIndex<false>(ref);
            if (index >= storage_.size()) {
                return false;
            }
            os::memory::ReadLockHolder lk(mutex_);
            if (IsFreeIndex(index)) {
                return false;
            }
            return index < storage_.size();
        }

        void DumpWithLock()
        {
            os::memory::ReadLockHolder lk(mutex_);
            Dump();
        }

        void Dump() REQUIRES_SHARED(mutex_)
        {
            if (DfxController::IsInitialized() &&
                DfxController::GetOptionValue(DfxOptionHandler::REFERENCE_DUMP) != 1) {
                return;
            }
            static constexpr size_t DUMP_NUMS = 20;
            size_t num = 0;
            LOG(INFO, RUNTIME) << "Dump the last " << DUMP_NUMS << " global references info:";

            for (auto it = storage_.rbegin(); it != storage_.rend(); it++) {
                uintptr_t ref = *it;
                if (IsBusy(ref)) {
                    auto obj = reinterpret_cast<ObjectHeader *>(ref);
                    LOG(INFO, RUNTIME) << "\t Index: " << GetSize() - num << ", Global reference: " << std::hex << ref
                                       << ", Object: " << std::hex << obj
                                       << ", Class: " << obj->ClassAddr<panda::Class>()->GetName();
                    num++;
                    if (num == DUMP_NUMS || num > GetSize()) {
                        break;
                    }
                }
            }
        }

        size_t GetSize() const REQUIRES_SHARED(mutex_)
        {
            return storage_.size() - blocks_available_;
        }

        size_t GetSizeWithLock() const
        {
            os::memory::ReadLockHolder global_lock(mutex_);
            return GetSize();
        }

        bool IsFreeIndex(uintptr_t index) REQUIRES_SHARED(mutex_)
        {
            return IsFreeValue(storage_[index]);
        }

        bool IsFreeValue(uintptr_t value) const
        {
            uintptr_t last_bit = BitField<uintptr_t, FREE_INDEX_BIT>::Get(value);
            return last_bit == 1;
        }

        bool IsBusy(uintptr_t value) const
        {
            return !IsFreeValue(value);
        }

        static uintptr_t EncodeObjectIndex(uintptr_t index)
        {
            ASSERT(index < (std::numeric_limits<uintptr_t>::max() >> BITS_FOR_INDEX));
            return index << BITS_FOR_INDEX;
        }

        static uintptr_t EncodeNextIndex(uintptr_t index)
        {
            uintptr_t shifted_index = EncodeObjectIndex(index);
            BitField<uintptr_t, FREE_INDEX_BIT>::Set(1, &shifted_index);
            return shifted_index;
        }

        static uintptr_t DecodeIndex(uintptr_t index)
        {
            return index >> BITS_FOR_INDEX;
        }

        /**
         * We need to add 1 to not return nullptr (JNI-API would think that we couldn't add this object).
         * Shift by 2 is needed because every Reference stores type in lowest 2 bits.
         */
        Reference *IndexToReference(uintptr_t encoded_index) const REQUIRES_SHARED(mutex_)
        {
            AssertIndex(DecodeIndex(encoded_index));
            return reinterpret_cast<Reference *>((encoded_index + 1) << BITS_FOR_TYPE);
        }

        template <bool check_assert = true>
        uintptr_t ReferenceToIndex(const Reference *ref) const REQUIRES_SHARED(mutex_)
        {
            if (check_assert) {
                AssertIndex(ref);
            }
            return (reinterpret_cast<uintptr_t>(ref) >> BITS_FOR_TYPE) - 1;
        }

        void AssertIndex(const Reference *ref) const REQUIRES_SHARED(mutex_)
        {
            auto decoded_index = (reinterpret_cast<uintptr_t>(ref) >> BITS_FOR_TYPE) - 1;
            AssertIndex(DecodeIndex(decoded_index));
        }

        void AssertIndex([[maybe_unused]] uintptr_t index) const REQUIRES_SHARED(mutex_)
        {
            ASSERT(static_cast<uintptr_t>(index) < storage_.size());
        }

        // test usage only
        size_t GetVectorSize()
        {
            os::memory::ReadLockHolder lk(mutex_);
            return storage_.size();
        }

        friend class ::panda::mem::test::ReferenceStorageTest;
    };
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_REFSTORAGE_GLOBAL_OBJECT_STORAGE_H_
