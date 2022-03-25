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

#ifndef PANDA_RUNTIME_GLOBAL_HANDLE_STORAGE_H_
#define PANDA_RUNTIME_GLOBAL_HANDLE_STORAGE_H_

#include "runtime/include/coretypes/tagged_value.h"
#include "runtime/include/mem/allocator.h"
#include "runtime/include/mem/panda_containers.h"

namespace panda {
using InternalAllocatorPtr = mem::AllocatorPtr<mem::AllocatorPurpose::ALLOCATOR_PURPOSE_INTERNAL>;

template <class T>
class HandleScope;

// storage is the storage structure of the object pointer
template <typename T>
class GlobalHandleStorage {
public:
    static const int32_t GLOBAL_BLOCK_SIZE = 256;

    explicit GlobalHandleStorage(InternalAllocatorPtr allocator) : allocator_(allocator)
    {
        ASSERT(allocator_ != nullptr);
        globalNodes_ = allocator_->New<PandaVector<std::array<Node, GLOBAL_BLOCK_SIZE> *>>(allocator_->Adapter());
    };
    ~GlobalHandleStorage();

    NO_COPY_SEMANTIC(GlobalHandleStorage);
    NO_MOVE_SEMANTIC(GlobalHandleStorage);

    class Node {
    public:
        void PushNodeToFreeList();

        T GetObject() const
        {
            return obj_;
        }

        Node *GetNext() const
        {
            return next_;
        }

        void SetNext(Node *node)
        {
            next_ = node;
        }

        void SetObject(T obj)
        {
            obj_ = obj;
        }

        uintptr_t GetObjectAddress() const
        {
            return reinterpret_cast<uintptr_t>(&obj_);
        }

    private:
        T obj_;
        Node *next_;
    };

    inline uintptr_t NewGlobalHandle(T value);

    inline void DisposeGlobalHandle(uintptr_t addr);

    inline PandaVector<std::array<Node, GLOBAL_BLOCK_SIZE> *> *GetNodes() const
    {
        return globalNodes_;
    }

    inline int32_t GetCount() const
    {
        return count_;
    }

private:
    // TaggedType has been specialized for js, Other types are empty implementation
    inline void UpdateHeapObject() {}

    // TaggedType has been specialized for js, Other types are empty implementation
    inline void VisitGCRoots([[maybe_unused]] const ObjectVisitor &cb) {}

    // TaggedType has been specialized for js, Other types are empty implementation
    inline void DealUpdateObject([[maybe_unused]] std::array<Node, GLOBAL_BLOCK_SIZE> *block,
                                 [[maybe_unused]] size_t index)
    {
    }
    // TaggedType has been specialized for js, Other types are empty implementation
    inline void DealVisitGCRoots([[maybe_unused]] std::array<Node, GLOBAL_BLOCK_SIZE> *block,
                                 [[maybe_unused]] size_t index, [[maybe_unused]] const ObjectVisitor &cb)
    {
    }

    PandaVector<std::array<Node, GLOBAL_BLOCK_SIZE> *> *globalNodes_ {nullptr};
    InternalAllocatorPtr allocator_;
    int32_t count_ {GLOBAL_BLOCK_SIZE};
    Node *freeList_ {nullptr};

    friend class ManagedThread;
};

}  // namespace panda

#endif  // PANDA_RUNTIME_GLOBAL_HANDLE_STORAGE_H_
