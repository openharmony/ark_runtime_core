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

#include "runtime/mem/refstorage/reference_storage.h"

#include "libpandabase/mem/mem.h"
#include "libpandabase/utils/dfx.h"
#include "runtime/include/thread.h"
#include "runtime/mem/object_helpers.h"
#include "runtime/mem/refstorage/global_object_storage.h"
#include "runtime/mem/gc/gc_root.h"

namespace panda::mem {

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define ASSERT_THREAD_STATE() \
    ASSERT(MTManagedThread::GetCurrent() == nullptr || !MTManagedThread::GetCurrent()->IsInNativeCode())

ReferenceStorage::ReferenceStorage(GlobalObjectStorage *global_storage, mem::InternalAllocatorPtr allocator,
                                   bool ref_check_validate)
    : global_storage_(global_storage), internal_allocator_(allocator), ref_check_validate_(ref_check_validate)
{
}

ReferenceStorage::~ReferenceStorage()
{
    if (frame_allocator_ != nullptr) {
        internal_allocator_->Delete(frame_allocator_);
    }
    if (local_storage_ != nullptr) {
        internal_allocator_->Delete(local_storage_);
    }
}

bool ReferenceStorage::Init()
{
    if (local_storage_ != nullptr || frame_allocator_ != nullptr || blocks_count_ != 0) {
        return false;
    }
    local_storage_ = internal_allocator_->New<PandaVector<RefBlock *>>(internal_allocator_->Adapter());
    if (local_storage_ == nullptr) {
        return false;
    }
    frame_allocator_ = internal_allocator_->New<StorageFrameAllocator>();
    if (frame_allocator_ == nullptr) {
        return false;
    }
    // main frame should always be created
    auto *first_block = CreateBlock();
    if (first_block == nullptr) {
        return false;
    }
    first_block->Reset();
    blocks_count_ = 1;
    local_storage_->push_back(first_block);
    return true;
}

bool ReferenceStorage::IsValidRef(const Reference *ref)
{
    ASSERT(ref != nullptr);
    auto type = Reference::GetType(ref);

    bool res = false;
    if (type == mem::Reference::ObjectType::STACK) {
        res = true;
    } else if (type == mem::Reference::ObjectType::GLOBAL || type == mem::Reference::ObjectType::WEAK) {
        // global-storage should accept ref with type
        res = global_storage_->IsValidGlobalRef(ref);
    } else {
        auto ref_without_type = Reference::GetRefWithoutType(ref);
        // Issue 3645
        res = frame_allocator_->Contains(reinterpret_cast<void *>(ref_without_type));
    }
    return res;
}

Reference::ObjectType ReferenceStorage::GetObjectType(const Reference *ref)
{
    return ref->GetType();
}

// NOLINTNEXTLINE(readability-function-size)
Reference *ReferenceStorage::NewRef(const ObjectHeader *object, Reference::ObjectType type)
{
    ASSERT(type != Reference::ObjectType::STACK);
    ASSERT_THREAD_STATE();
    if (object == nullptr) {
        return nullptr;
    }

    Reference *ref = nullptr;
    if (type == Reference::ObjectType::GLOBAL || type == Reference::ObjectType::WEAK) {
        ref = static_cast<Reference *>(global_storage_->Add(object, type));
    } else {
        auto *last_block = local_storage_->back();
        ASSERT(last_block != nullptr);

        RefBlock *cur_block = nullptr;
        if (last_block->IsFull()) {
            cur_block = CreateBlock();
            if (cur_block == nullptr) {
                LOG(ERROR, RUNTIME) << "Can't allocate local ref for object: " << object
                                    << ", cls: " << object->ClassAddr<panda::Class>()->GetName()
                                    << " with type: " << static_cast<int>(type);
                DumpLocalRef();
                return nullptr;
            }
            cur_block->Reset(last_block);
            (*local_storage_)[local_storage_->size() - 1] = cur_block;
        } else {
            cur_block = last_block;
        }
        ref = cur_block->AddRef(object, type);
    }
    LOG(DEBUG, RUNTIME) << "Add reference to object: " << std::hex << object << " type: " << static_cast<int>(type)
                        << " ref: " << ref;
    return ref;
}

void ReferenceStorage::RemoveRef(const Reference *ref)
{
    ASSERT_THREAD_STATE();
    if (ref == nullptr) {
        return;
    }

    if (ref_check_validate_) {
        if (UNLIKELY(!IsValidRef(ref))) {
            // Undefined behavior, we just print warning here.
            LOG(WARNING, RUNTIME) << "Try to remove not existed ref: " << std::hex << ref;
            return;
        }
    }
    Reference::ObjectType object_type = ref->GetType();
    if (object_type == Reference::ObjectType::GLOBAL || object_type == Reference::ObjectType::WEAK) {
        global_storage_->Remove(ref);
    } else if (object_type == Reference::ObjectType::LOCAL) {
        auto addr = ToUintPtr(ref);
        auto block_addr = (addr >> BLOCK_ALIGNMENT) << BLOCK_ALIGNMENT;
        auto *block = reinterpret_cast<RefBlock *>(block_addr);

        block->Remove(ref);
        LOG(DEBUG, RUNTIME) << "Remove local reference: " << std::hex << ref;
    } else if (object_type == Reference::ObjectType::STACK) {
        LOG(ERROR, RUNTIME) << "Cannot remove stack type: " << ref;
    } else {
        LOG(FATAL, RUNTIME) << "Unknown reference type: " << ref;
    }
}

ObjectHeader *ReferenceStorage::GetObject(const Reference *ref)
{
    if (UNLIKELY(ref == nullptr)) {
        return nullptr;
    }

    if (ref_check_validate_) {
        if (UNLIKELY(!IsValidRef(ref))) {
            // Undefined behavior, we just print warning here.
            LOG(WARNING, RUNTIME) << "Try to GetObject from a not existed ref: " << std::hex << ref;
            return nullptr;
        }
    }
    Reference::ObjectType object_type = ref->GetType();
    switch (object_type) {
        case Reference::ObjectType::GLOBAL:
        case Reference::ObjectType::WEAK: {
            ObjectHeader *obj = global_storage_->Get(ref);
#ifndef NDEBUG
            // only weakly reachable objects can be null in storage
            if (object_type == mem::Reference::ObjectType::GLOBAL) {
                ASSERT(obj != nullptr);
            }
#endif
            return obj;
        }
        case Reference::ObjectType::STACK: {
            // In current scheme object passed in 64-bit argument
            // But compiler may store 32-bit and trash in hi-part
            // That's why need cut object pointer
            return reinterpret_cast<ObjectHeader *>(
                (*reinterpret_cast<object_pointer_type *>(Reference::GetRefWithoutType(ref))));
        }
        case Reference::ObjectType::LOCAL: {
            ObjectHeader *obj = FindLocalObject(ref);
            ASSERT(obj != nullptr);
#ifndef NDEBUG
            /*
             * classes are not movable objects, so they can be read from storage in native code, but general objects are
             * not
             */
            auto base_cls = obj->ClassAddr<BaseClass>();
            if (!base_cls->IsDynamicClass()) {
                auto cls = static_cast<Class *>(base_cls);
                if (!cls->IsClassClass()) {
                    ASSERT_THREAD_STATE();
                }
            }
#endif
            return obj;
        }
        default: {
            LOG(FATAL, RUNTIME) << "Unknown reference: " << ref << " type: " << static_cast<int>(object_type);
        }
    }
    return nullptr;
}

bool ReferenceStorage::PushLocalFrame(uint32_t capacity)
{
    ASSERT_THREAD_STATE();
    size_t need_blocks = (capacity + RefBlock::REFS_IN_BLOCK - 1) / RefBlock::REFS_IN_BLOCK;
    size_t blocks_free = MAX_STORAGE_BLOCK_COUNT - blocks_count_;
    if (need_blocks > blocks_free) {
        LOG(ERROR, RUNTIME) << "Free size of local reference storage is less than capacity: " << capacity
                            << " blocks_count_: " << blocks_count_ << " need_blocks: " << need_blocks
                            << " blocks_free: " << blocks_free;
        return false;
    }
    auto *new_block = CreateBlock();
    if (new_block == nullptr) {
        LOG(FATAL, RUNTIME) << "Can't allocate new frame";
        UNREACHABLE();
    }
    new_block->Reset();
    local_storage_->push_back(new_block);
    return true;
}

Reference *ReferenceStorage::PopLocalFrame(Reference *result)
{
    ASSERT_THREAD_STATE();

    ObjectHeader *obj;
    if (result != nullptr) {
        obj = GetObject(result);
    } else {
        obj = nullptr;
    }

    if (cached_block_ != nullptr) {
        RemoveBlock(cached_block_);
        cached_block_ = nullptr;
    }

    auto *last_block = local_storage_->back();
    auto is_first = local_storage_->size() == 1;
    while (last_block != nullptr) {
        auto *prev = last_block->GetPrev();
        if (prev == nullptr && is_first) {
            // it's the first block, which we don't delete
            break;
        }
        // cache the last block for ping-pong effect
        if (prev == nullptr) {
            if (cached_block_ == nullptr) {
                cached_block_ = last_block;
                break;
            }
        }
        RemoveBlock(last_block);
        last_block = prev;
    }

    Reference *res = nullptr;
    if (obj == nullptr) {
        local_storage_->pop_back();
        res = nullptr;
    } else {
        Reference::ObjectType type = result->GetType();
        local_storage_->pop_back();
        res = NewRef(obj, type);
    }
    return res;
}

bool ReferenceStorage::EnsureLocalCapacity(size_t capacity)
{
    size_t need_blocks = (capacity + RefBlock::REFS_IN_BLOCK - 1) / RefBlock::REFS_IN_BLOCK;
    size_t blocks_freed = MAX_STORAGE_BLOCK_COUNT - blocks_count_;
    if (need_blocks > blocks_freed) {
        LOG(ERROR, RUNTIME) << "Can't store size: " << capacity << " in local references";
        return false;
    }
    return true;
}

ObjectHeader *ReferenceStorage::FindLocalObject(const Reference *ref)
{
    ref = Reference::GetRefWithoutType(ref);
    ObjectPointer<ObjectHeader> obj_pointer = *(reinterpret_cast<const ObjectPointer<ObjectHeader> *>(ref));
    return obj_pointer;
}

PandaVector<ObjectHeader *> ReferenceStorage::GetAllObjects()
{
    auto objects = global_storage_->GetAllObjects();
    for (const auto &current_frame : *local_storage_) {
        auto last_block = current_frame;
        const PandaVector<mem::Reference *> &refs = last_block->GetAllReferencesInFrame();
        for (const auto &ref : refs) {
            ObjectHeader *obj = FindLocalObject(ref);
            objects.push_back(reinterpret_cast<ObjectHeader *>(obj));
        }
    }
    return objects;
}

void ReferenceStorage::VisitObjects(const GCRootVisitor &gc_root_visitor, mem::RootType rootType)
{
    for (const auto &frame : *local_storage_) {
        frame->VisitObjects(gc_root_visitor, rootType);
    }
}

void ReferenceStorage::UpdateMovedRefs()
{
    for (const auto &frame : *local_storage_) {
        frame->UpdateMovedRefs();
    }
}

void ReferenceStorage::DumpLocalRefClasses()
{
    PandaMap<PandaString, int> classes_info;

    for (const auto &frame : *local_storage_) {
        auto last_block = frame;
        auto refs = last_block->GetAllReferencesInFrame();
        for (const auto &ref : refs) {
            ObjectHeader *obj = FindLocalObject(ref);
            PandaString cls_name = ConvertToString(obj->ClassAddr<panda::Class>()->GetName());
            classes_info[cls_name]++;
        }
    }
    using InfoPair = std::pair<PandaString, int>;
    PandaVector<InfoPair> info_vec(classes_info.begin(), classes_info.end());
    size_t size = std::min(MAX_DUMP_LOCAL_NUMS, info_vec.size());
    std::partial_sort(info_vec.begin(), info_vec.begin() + size, info_vec.end(),
                      [](const InfoPair &lhs, const InfoPair &rhs) { return lhs.second < rhs.second; });
    LOG(ERROR, RUNTIME) << "The top " << size << " classes of local references are:";
    for (size_t i = 0; i < size; i++) {
        LOG(ERROR, RUNTIME) << "\t" << info_vec[i].first << ": " << info_vec[i].second;
    }
}

void ReferenceStorage::DumpLocalRef()
{
    if (DfxController::IsInitialized() && DfxController::GetOptionValue(DfxOptionHandler::REFERENCE_DUMP) != 1) {
        return;
    }
    LOG(ERROR, RUNTIME) << "--- local reference storage dump ---";
    LOG(ERROR, RUNTIME) << "Local reference storage addr: " << &local_storage_;
    LOG(ERROR, RUNTIME) << "Dump the last several local references info(max " << MAX_DUMP_LOCAL_NUMS << "):";
    size_t n_dump = 0;

    for (auto it = local_storage_->rbegin(); it != local_storage_->rend(); ++it) {
        auto *frame = *it;
        auto refs = frame->GetAllReferencesInFrame();
        for (const auto &ref : refs) {
            ObjectHeader *res = FindLocalObject(ref);
            PandaString cls_name = ConvertToString(res->ClassAddr<panda::Class>()->GetName());
            LOG(ERROR, RUNTIME) << "\t local reference: " << std::hex << ref << ", object: " << res
                                << ", cls: " << cls_name;
            n_dump++;
            if (n_dump == MAX_DUMP_LOCAL_NUMS) {
                DumpLocalRefClasses();
                LOG(ERROR, RUNTIME) << "---";
                LOG(ERROR, RUNTIME) << "JNI ERROR (app bug): local reference storage overflow ";
                return;
            }
        }
    }
}

RefBlock *ReferenceStorage::CreateBlock()
{
    if (blocks_count_ == MAX_STORAGE_BLOCK_COUNT) {
        return nullptr;
    }

    RefBlock *new_block = nullptr;
    if (cached_block_ != nullptr) {
        new_block = cached_block_;
        cached_block_ = nullptr;
    } else {
        new_block = static_cast<RefBlock *>(frame_allocator_->Alloc(BLOCK_SIZE));
        blocks_count_++;
    }
    return new_block;
}

void ReferenceStorage::RemoveBlock(RefBlock *block)
{
    frame_allocator_->Free(block);
    blocks_count_--;
}

void ReferenceStorage::RemoveAllLocalRefs()
{
    ASSERT_THREAD_STATE();

    for (const auto &frame : *local_storage_) {
        auto last_block = frame;
        auto refs = last_block->GetAllReferencesInFrame();
        for (const auto &ref : refs) {
            last_block->Remove(ref);
        }
    }
}

size_t ReferenceStorage::GetGlobalObjectStorageSize()
{
    return global_storage_->GetSize();
}

size_t ReferenceStorage::GetLocalObjectStorageSize()
{
    size_t size = 0;
    for (const auto &block : *local_storage_) {
        auto *current_block = block;
        size += current_block->GetAllReferencesInFrame().size();
    }
    return size;
}

void ReferenceStorage::SetRefCheckValidate(bool ref_check_validate)
{
    ref_check_validate_ = ref_check_validate;
}
}  // namespace panda::mem
