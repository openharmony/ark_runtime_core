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
#include "global_object_storage.h"

#include <libpandabase/os/mutex.h>
#include "runtime/include/runtime.h"
#include "runtime/include/mem/panda_containers.h"
#include "runtime/include/object_header.h"
#include "runtime/mem/object_helpers.h"
#include "runtime/mem/gc/gc.h"
#include "runtime/mem/gc/gc_root.h"
#include "runtime/mem/gc/gc_phase.h"
#include "runtime/include/class.h"
#include "reference.h"
#include "utils/logger.h"

namespace panda::mem {

GlobalObjectStorage::GlobalObjectStorage(mem::InternalAllocatorPtr allocator, size_t max_size, bool enable_size_check)
    : allocator_(allocator)
{
    global_storage_ = allocator->New<ArrayStorage>(allocator, max_size, enable_size_check);
    weak_storage_ = allocator->New<ArrayStorage>(allocator, max_size, enable_size_check);
}

GlobalObjectStorage::~GlobalObjectStorage()
{
    allocator_->Delete(global_storage_);
    allocator_->Delete(weak_storage_);
}

bool GlobalObjectStorage::IsValidGlobalRef(const Reference *ref) const
{
    ASSERT(ref != nullptr);
    Reference::ObjectType type = Reference::GetType(ref);
    AssertType(type);
    if (type == Reference::ObjectType::GLOBAL) {
        if (!global_storage_->IsValidGlobalRef(ref)) {
            return false;
        }
    } else {
        if (!weak_storage_->IsValidGlobalRef(ref)) {
            return false;
        }
    }
    return true;
}

Reference *GlobalObjectStorage::Add(const ObjectHeader *object, Reference::ObjectType type) const
{
    AssertType(type);
    if (object == nullptr) {
        return nullptr;
    }
    Reference *ref = nullptr;
    if (type == Reference::ObjectType::GLOBAL) {
        ref = global_storage_->Add(object);
    } else {
        ref = weak_storage_->Add(object);
    }
    if (ref != nullptr) {
        ref = Reference::SetType(ref, type);
    }
    return ref;
}

ObjectHeader *GlobalObjectStorage::Get(const Reference *reference) const
{
    if (reference == nullptr) {
        return nullptr;
    }
    auto type = reference->GetType();
    reference = Reference::GetRefWithoutType(reference);
    AssertType(type);
    ObjectHeader *result = nullptr;
    if (type == Reference::ObjectType::GLOBAL) {
        result = global_storage_->Get(reference);
    } else {
        result = weak_storage_->Get(reference);
    }
    return result;
}

void GlobalObjectStorage::Remove(const Reference *reference) const
{
    if (reference == nullptr) {
        return;
    }
    auto type = reference->GetType();
    AssertType(type);
    reference = Reference::GetRefWithoutType(reference);
    if (type == Reference::ObjectType::GLOBAL) {
        global_storage_->Remove(reference);
    } else {
        weak_storage_->Remove(reference);
    }
}

PandaVector<ObjectHeader *> GlobalObjectStorage::GetAllObjects()
{
    auto objects = PandaVector<ObjectHeader *>(allocator_->Adapter());

    auto global_objects = global_storage_->GetAllObjects();
    objects.insert(objects.end(), global_objects.begin(), global_objects.end());

    auto weak_objects = weak_storage_->GetAllObjects();
    objects.insert(objects.end(), weak_objects.begin(), weak_objects.end());

    return objects;
}

void GlobalObjectStorage::VisitObjects(const GCRootVisitor &gc_root_visitor, mem::RootType rootType) const
{
    global_storage_->VisitObjects(gc_root_visitor, rootType);
}

void GlobalObjectStorage::UpdateMovedRefs()
{
    LOG(DEBUG, GC) << "=== GlobalStorage Update moved. BEGIN ===";
    global_storage_->UpdateMovedRefs();
    weak_storage_->UpdateMovedRefs();
    LOG(DEBUG, GC) << "=== GlobalStorage Update moved. END ===";
}

void GlobalObjectStorage::ClearUnmarkedWeakRefs(const GC *gc)
{
    weak_storage_->ClearUnmarkedWeakRefs(gc);
}

size_t GlobalObjectStorage::GetSize()
{
    return global_storage_->GetSizeWithLock() + weak_storage_->GetSizeWithLock();
}

void GlobalObjectStorage::Dump()
{
    global_storage_->DumpWithLock();
}
}  // namespace panda::mem
