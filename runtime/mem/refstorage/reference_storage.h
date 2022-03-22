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

#ifndef PANDA_RUNTIME_MEM_REFSTORAGE_REFERENCE_STORAGE_H_
#define PANDA_RUNTIME_MEM_REFSTORAGE_REFERENCE_STORAGE_H_

#include "runtime/include/object_header.h"
#include "runtime/mem/frame_allocator.h"
#include "runtime/mem/refstorage/reference.h"
#include "runtime/mem/internal_allocator.h"
#include "ref_block.h"

namespace panda {
class ObjectHeader;
namespace mem {
class Reference;
namespace test {
class ReferenceStorageTest;
}  // namespace test
}  // namespace mem
}  // namespace panda

namespace panda::mem {

/**
 * Storage stores all References for proper interaction with GC.
 */
class ReferenceStorage {
public:
    static_assert(offsetof(RefBlock, refs_) == 0);

    explicit ReferenceStorage(GlobalObjectStorage *global_storage, mem::InternalAllocatorPtr allocator,
                              bool ref_check_validate);

    ~ReferenceStorage();

    bool Init();

    static Reference::ObjectType GetObjectType(const Reference *ref);

    [[nodiscard]] static Reference *NewStackRef(const ObjectHeader *const *object_ptr)
    {
        ASSERT(object_ptr != nullptr);
        if (*object_ptr == nullptr) {
            return nullptr;
        }
        return Reference::Create(ToUintPtr(object_ptr), Reference::ObjectType::STACK);
    }

    [[nodiscard]] Reference *NewRef(const ObjectHeader *object, Reference::ObjectType object_type);

    void RemoveRef(const Reference *ref);

    [[nodiscard]] ObjectHeader *GetObject(const Reference *ref);

    /**
     * Creates a new frame with at least given number of local references which can be created in this frame.
     *
     * @param capacity minimum number of local references in the frame.
     * @return true if frame was successful allocated, false otherwise.
     */
    bool PushLocalFrame(uint32_t capacity);

    /**
     * Pops the last frame, frees all local references in this frame and moves given reference to the previous frame and
     * return it's new reference. Should be NULL if you don't want to return any value to previous frame.
     *
     * @param result reference which you want to return in the previous frame.
     * @return new reference in the previous frame for given reference.
     */
    Reference *PopLocalFrame(Reference *result);

    /**
     * Ensure that capacity in current frame can contain at least `size` references.
     * @param capacity minimum number of references for this frame
     * @return true if current frame can store at least `size` references, false otherwise.
     */
    bool EnsureLocalCapacity(size_t capacity);

    /**
     * Get all objects in global & local storage. Use for debugging only
     */
    PandaVector<ObjectHeader *> GetAllObjects();

    void VisitObjects(const GCRootVisitor &gc_root_visitor, mem::RootType rootType);

    /**
     * Update pointers to moved Objects in local storage
     */
    void UpdateMovedRefs();

    /**
     * Dump the last several local references info(max MAX_DUMP_LOCAL_NUMS).
     */
    void DumpLocalRef();

    /**
     * Dump the top MAX_DUMP_LOCAL_NUMS(if exists) classes of local references.
     */
    void DumpLocalRefClasses();
    bool IsValidRef(const Reference *ref);
    void SetRefCheckValidate(bool ref_check_validate);

private:
    NO_COPY_SEMANTIC(ReferenceStorage);
    NO_MOVE_SEMANTIC(ReferenceStorage);

    ObjectHeader *FindLocalObject(const Reference *ref);

    RefBlock *CreateBlock();

    void RemoveBlock(RefBlock *block);

    static constexpr size_t MAX_DUMP_LOCAL_NUMS = 10;

    static constexpr Alignment BLOCK_ALIGNMENT = Alignment::LOG_ALIGN_8;
    static constexpr size_t BLOCK_SIZE = sizeof(RefBlock);

    static_assert(GetAlignmentInBytes(BLOCK_ALIGNMENT) >= BLOCK_SIZE);
    static_assert(GetAlignmentInBytes(static_cast<Alignment>(BLOCK_ALIGNMENT - 1)) <= BLOCK_SIZE);

    static constexpr size_t MAX_STORAGE_SIZE = 128_MB;
    static constexpr size_t MAX_STORAGE_BLOCK_COUNT = MAX_STORAGE_SIZE / BLOCK_SIZE;

    using StorageFrameAllocator = mem::FrameAllocator<BLOCK_ALIGNMENT, false>;

    GlobalObjectStorage *global_storage_;
    mem::InternalAllocatorPtr internal_allocator_;
    PandaVector<RefBlock *> *local_storage_ {nullptr};
    StorageFrameAllocator *frame_allocator_ {nullptr};
    size_t blocks_count_ {0};
    RefBlock *cached_block_ {nullptr};

    bool ref_check_validate_;
    // private methods for test purpose only
    size_t GetGlobalObjectStorageSize();

    size_t GetLocalObjectStorageSize();

    void RemoveAllLocalRefs();

    friend class panda::mem::test::ReferenceStorageTest;
};

/**
 * Handle the reference of object that might be moved by gc, note that
 * it should be only used in Managed code(with ScopedObjectFix).
 */
class ReferenceHandle {
public:
    ~ReferenceHandle() = default;

    template <typename T>
    ReferenceHandle(const ReferenceHandle &rhs, T *object, Reference::ObjectType type = Reference::ObjectType::LOCAL)
        : rs_(rhs.rs_), ref_(rs_->NewRef(reinterpret_cast<ObjectHeader *>(object), type))
    {
        ASSERT(ref_ != nullptr);
    }

    template <typename T>
    ReferenceHandle(ReferenceStorage *rs, T *object, Reference::ObjectType type = Reference::ObjectType::LOCAL)
        : rs_(rs), ref_(rs_->NewRef(reinterpret_cast<ObjectHeader *>(object), type))
    {
        ASSERT(ref_ != nullptr);
    }

    template <typename T>
    T *GetObject() const
    {
        return reinterpret_cast<T *>(rs_->GetObject(ref_));
    }

    template <typename T>
    Reference *NewRef(T *object, bool release_old = true, Reference::ObjectType type = Reference::ObjectType::LOCAL)
    {
        if (release_old && ref_ != nullptr) {
            rs_->RemoveRef(ref_);
        }
        ref_ = rs_->NewRef(reinterpret_cast<ObjectHeader *>(object), type);
        return ref_;
    }

    /**
     * Remove a reference explicitly,
     * suggest not doing this unless the ReferenceStorage will be out of capacity,
     * and the reference is created in caller scope and not used by any other place
     */
    void RemoveRef()
    {
        rs_->RemoveRef(ref_);
        ref_ = nullptr;
    }

private:
    ReferenceStorage *rs_;
    Reference *ref_;
    NO_COPY_SEMANTIC(ReferenceHandle);
    NO_MOVE_SEMANTIC(ReferenceHandle);
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_REFSTORAGE_REFERENCE_STORAGE_H_
