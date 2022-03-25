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

#ifndef PANDA_RUNTIME_HANDLE_STORAGE_H_
#define PANDA_RUNTIME_HANDLE_STORAGE_H_

#include "runtime/include/coretypes/tagged_value.h"
#include "runtime/include/mem/allocator.h"
#include "runtime/include/mem/panda_containers.h"

namespace panda {
class LocalScope;
class EscapeLocalScope;

template <class T>
class HandleScope;

template <class T>
class EscapeHandleScope;

using InternalAllocatorPtr = mem::AllocatorPtr<mem::AllocatorPurpose::ALLOCATOR_PURPOSE_INTERNAL>;

// storage is the storage structure of the object pointer
template <typename T>
class HandleStorage {
public:
    explicit HandleStorage(InternalAllocatorPtr allocator) : allocator_(allocator)
    {
        ASSERT(allocator_ != nullptr);
    };
    ~HandleStorage()
    {
        for (auto n : nodes_) {
            allocator_->Delete(n);
        }
        nodes_.clear();
    }

    NO_COPY_SEMANTIC(HandleStorage);
    NO_MOVE_SEMANTIC(HandleStorage);

private:
    static const uint32_t NODE_BLOCK_SIZE_LOG2 = 10;
    static const uint32_t NODE_BLOCK_SIZE = 1U << NODE_BLOCK_SIZE_LOG2;
    static const uint32_t NODE_BLOCK_SIZE_MASK = NODE_BLOCK_SIZE - 1;

    void ZapFreedHandles();

    // TaggedType has been specialized for js, Other types are empty implementation
    void UpdateHeapObject() {}

    // TaggedType has been specialized for js, Other types are empty implementation
    void VisitGCRoots([[maybe_unused]] const ObjectVisitor &cb) {}

    uintptr_t NewHandle(T value = 0);

    void FreeHandles(uint32_t beginIndex);

    uintptr_t GetNodeAddress(uint32_t index) const;

    uint32_t lastIndex_ {0};
    PandaVector<std::array<T, NODE_BLOCK_SIZE> *> nodes_;
    InternalAllocatorPtr allocator_ {nullptr};
    friend class ManagedThread;
    friend class HandleScope<T>;
    friend class LocalScope;
    friend class EscapeLocalScope;
    friend class EscapeHandleScope<T>;
};

}  // namespace panda

#endif  // PANDA_RUNTIME_HANDLE_STORAGE_H_
