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

#ifndef PANDA_RUNTIME_MEM_REFSTORAGE_REF_BLOCK_H_
#define PANDA_RUNTIME_MEM_REFSTORAGE_REF_BLOCK_H_

#include <cstddef>
#include <limits>

#include "include/object_header.h"
#include <libpandabase/mem/object_pointer.h>
#include "runtime/mem/refstorage/reference.h"
#include "runtime/mem/object_helpers.h"
#include "runtime/mem/gc/gc_root.h"

namespace panda::mem::test {
class ReferenceStorageTest;
}  // namespace panda::mem::test

namespace panda::mem {

class RefBlock {
    using SlotBitMap = uint64_t;

    class Iterator {
        RefBlock *ref_block_;

    public:
        explicit Iterator(RefBlock *ref_block) : ref_block_(ref_block) {}

        ~Iterator() = default;

        DEFAULT_COPY_SEMANTIC(Iterator);
        DEFAULT_MOVE_SEMANTIC(Iterator);

        RefBlock *operator*() const
        {
            return ref_block_;
        }

        Iterator &operator++()
        {
            ref_block_ = ref_block_->GetPrev();
            return *this;
        }

        const Iterator operator++(int)  // NOLINT(readability-const-return-type)
        {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        friend bool operator==(const Iterator &a, const Iterator &b)
        {
            return a.ref_block_ == b.ref_block_;
        }

        friend bool operator!=(const Iterator &a, const Iterator &b)
        {
            return a.ref_block_ != b.ref_block_;
        }
    };

    static constexpr unsigned int REFS_IN_BLOCK = 60;
    static constexpr SlotBitMap START_VALUE = std::numeric_limits<SlotBitMap>::max();
    static constexpr SlotBitMap FINAL_VALUE =
        std::numeric_limits<SlotBitMap>::max() & (~((static_cast<SlotBitMap>(1U) << REFS_IN_BLOCK) - 1U));

    // should be first field in the class
    ObjectPointer<ObjectHeader> refs_[REFS_IN_BLOCK];  // NOLINT(modernize-avoid-c-arrays)

    // slots in the array. slot is free - bit=1, slot is busy - bit=0
    SlotBitMap slots_ {START_VALUE};

    RefBlock *prev_block_ {nullptr};

public:
    explicit RefBlock(RefBlock *prev);

    RefBlock() : RefBlock(nullptr) {}

    void Reset()
    {
        Reset(nullptr);
    }

    void Reset(RefBlock *prev);

    bool IsFull() const
    {
        return slots_ == FINAL_VALUE;
    }

    bool IsEmpty() const
    {
        return slots_ == START_VALUE;
    }

    Reference *AddRef(const ObjectHeader *object, Reference::ObjectType type);

    void Remove(const Reference *ref);

    RefBlock *GetPrev() const
    {
        return prev_block_;
    }

    void VisitObjects(const GCRootVisitor &gc_root_visitor, mem::RootType rootType);

    void UpdateMovedRefs();

    // used only for dumping and tests
    PandaVector<Reference *> GetAllReferencesInFrame();

    Iterator begin()  // NOLINT(readability-identifier-naming)
    {
        return Iterator(this);
    }

    Iterator end() const  // NOLINT(readability-identifier-naming)
    {
        return Iterator(nullptr);
    }

    ~RefBlock() = default;

    DEFAULT_COPY_SEMANTIC(RefBlock);
    DEFAULT_MOVE_SEMANTIC(RefBlock);

private:
    uint8_t GetFreeIndex() const;

    void Set(uint8_t index, const ObjectHeader *object);

    bool IsFreeIndex(uint8_t index) const;

    bool IsBusyIndex(uint8_t index) const;

    void PrintBlock();

    // test purpose only, don't use in production

    void DumpFrame()
    {
        std::cout << "Dump chain start --------------";
        for (auto block : *this) {
            std::cout << std::endl << "block: " << std::hex << &block << std::endl;
            block->PrintBlock();
        }
        std::cout << std::endl << "Dump chain finish --------------" << std::endl;
    }

    void DumpBlock()
    {
        std::cout << "Dump block start --------------" << std::endl;
        std::cout << std::endl << "block: " << std::hex << this << std::endl;
        PrintBlock();
        std::cout << std::endl << "Dump block finish --------------" << std::endl;
    }
    friend class panda::mem::test::ReferenceStorageTest;
    friend class panda::mem::ReferenceStorage;
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_REFSTORAGE_REF_BLOCK_H_
