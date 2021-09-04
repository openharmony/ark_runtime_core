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

#include "ref_block.h"

namespace panda::mem {

RefBlock::RefBlock(RefBlock *prev)
{
    slots_ = START_VALUE;
    prev_block_ = prev;
}

void RefBlock::Reset(RefBlock *prev)
{
    slots_ = START_VALUE;
    prev_block_ = prev;
}

Reference *RefBlock::AddRef(const ObjectHeader *object, Reference::ObjectType type)
{
    ASSERT(!IsFull());
    uint8_t index = GetFreeIndex();
    Set(index, object);
    auto *ref = reinterpret_cast<Reference *>(&refs_[index]);
    ref = Reference::SetType(ref, type);
    return ref;
}

void RefBlock::Remove(const Reference *ref)
{
    ASSERT(!IsEmpty());
    ref = Reference::GetRefWithoutType(ref);

    auto ref_ptr = ToUintPtr(ref);
    auto block_ptr = ToUintPtr(this);
    auto index = (ref_ptr - block_ptr) / sizeof(ObjectPointer<ObjectHeader>);
    ASSERT(IsBusyIndex(index));
    slots_ |= static_cast<uint64_t>(1U) << index;
    ASAN_POISON_MEMORY_REGION(refs_[index], sizeof(refs_[index]));
}

void RefBlock::VisitObjects(const GCRootVisitor &gc_root_visitor, mem::RootType rootType)
{
    for (auto *block : *this) {
        if (block->IsEmpty()) {
            continue;
        }
        for (size_t index = 0; index < REFS_IN_BLOCK; index++) {
            if (block->IsBusyIndex(index)) {
                auto object_pointer = block->refs_[index];
                auto *obj = object_pointer.ReinterpretCast<ObjectHeader *>();
                ASSERT(obj->ClassAddr<Class>() != nullptr);
                LOG(DEBUG, GC) << " Found root from ref-storage: " << mem::GetDebugInfoAboutObject(obj);
                gc_root_visitor({rootType, obj});
            }
        }
    }
}

void RefBlock::UpdateMovedRefs()
{
    for (auto *block : *this) {
        if (block->IsEmpty()) {
            continue;
        }
        for (size_t index = 0; index < REFS_IN_BLOCK; index++) {
            if (block->IsBusyIndex(index)) {
                auto object_pointer = block->refs_[index];
                auto *object = object_pointer.ReinterpretCast<ObjectHeader *>();
                auto obj = reinterpret_cast<ObjectHeader *>(object);
                if (obj->IsForwarded()) {
                    LOG(DEBUG, GC) << " Update pointer for obj: " << mem::GetDebugInfoAboutObject(obj);
                    ObjectHeader *forward_address = GetForwardAddress(obj);
                    block->refs_[index] = reinterpret_cast<ObjectHeader *>(forward_address);
                }
            }
        }
    }
}

// used only for dumping and tests
PandaVector<Reference *> RefBlock::GetAllReferencesInFrame()
{
    PandaVector<Reference *> refs;
    for (auto *block : *this) {
        if (block->IsEmpty()) {
            continue;
        }
        for (size_t index = 0; index < REFS_IN_BLOCK; index++) {
            if (block->IsBusyIndex(index)) {
                auto *current_ref = reinterpret_cast<Reference *>(&block->refs_[index]);
                refs.push_back(current_ref);
            }
        }
    }
    return refs;
}

uint8_t RefBlock::GetFreeIndex() const
{
    ASSERT(!IsFull());
    auto res = Ffs(slots_) - 1;
#ifndef NDEBUG
    uint8_t index = 0;
    while (IsBusyIndex(index)) {
        index++;
    }
    ASSERT(index == res);
#endif
    return res;
}

void RefBlock::Set(uint8_t index, const ObjectHeader *object)
{
    ASSERT(IsFreeIndex(index));
    ASAN_UNPOISON_MEMORY_REGION(refs_[index], sizeof(refs_[index]));
    refs_[index] = object;
    slots_ &= ~(static_cast<uint64_t>(1U) << index);
}

bool RefBlock::IsFreeIndex(uint8_t index) const
{
    return !IsBusyIndex(index);
}

bool RefBlock::IsBusyIndex(uint8_t index) const
{
    return ((slots_ >> index) & 1U) == 0;
}

void RefBlock::PrintBlock()
{
    auto refs = GetAllReferencesInFrame();
    for (const auto &ref : refs) {
        std::cout << ref << " ";
    }
}

}  // namespace panda::mem
