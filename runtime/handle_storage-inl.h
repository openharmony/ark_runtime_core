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

#ifndef PANDA_RUNTIME_HANDLE_STORAGE_INL_H_
#define PANDA_RUNTIME_HANDLE_STORAGE_INL_H_

#include "runtime/handle_storage.h"
#include "runtime/mem/object_helpers.h"

namespace panda {
template <typename T>
inline uintptr_t HandleStorage<T>::GetNodeAddress(uint32_t index) const
{
    uint32_t id = index >> NODE_BLOCK_SIZE_LOG2;
    uint32_t offset = index & NODE_BLOCK_SIZE_MASK;
    auto node = nodes_[id];
    auto location = &(*node)[offset];
    return reinterpret_cast<uintptr_t>(location);
}

template <typename T>
inline uintptr_t HandleStorage<T>::NewHandle(T value)
{
    uint32_t nid = lastIndex_ >> NODE_BLOCK_SIZE_LOG2;
    uint32_t offset = lastIndex_ & NODE_BLOCK_SIZE_MASK;
    if (nodes_.size() <= nid) {
        auto n = allocator_->New<std::array<T, NODE_BLOCK_SIZE>>();
        nodes_.push_back(n);
    }
    auto node = nodes_[nid];
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_REDUNDANT_INIT)
    auto loc = &(*node)[offset];
    *loc = value;
    lastIndex_++;
    return reinterpret_cast<uintptr_t>(loc);
}

template <typename T>
inline void HandleStorage<T>::FreeHandles(uint32_t beginIndex)
{
    lastIndex_ = beginIndex;
#ifndef NDEBUG
    ZapFreedHandles();
#endif
    uint32_t nid = lastIndex_ >> NODE_BLOCK_SIZE_LOG2;
    // reserve at least one block for perf.
    nid++;
    for (size_t i = nid; i < nodes_.size(); ++i) {
        allocator_->Delete(nodes_[i]);
    }
    if (nid < nodes_.size()) {
        nodes_.erase(nodes_.begin() + nid, nodes_.end());
    }
}

template <typename T>
void HandleStorage<T>::ZapFreedHandles()
{
    uint32_t nid = lastIndex_ >> NODE_BLOCK_SIZE_LOG2;
    uint32_t offset = lastIndex_ & NODE_BLOCK_SIZE_MASK;
    for (size_t i = nid; i < nodes_.size(); ++i) {
        auto node = nodes_.at(i);
        if (i != nid) {
            node->fill(reinterpret_cast<T>(static_cast<uint64_t>(0)));
        } else {
            for (uint32_t j = offset; j < NODE_BLOCK_SIZE; ++j) {
                node->at(j) = reinterpret_cast<T>(static_cast<uint64_t>(0));
            }
        }
    }
}

template <>
inline void HandleStorage<coretypes::TaggedType>::UpdateHeapObject()
{
    if (lastIndex_ == 0) {
        return;
    }
    uint32_t nid = lastIndex_ >> NODE_BLOCK_SIZE_LOG2;
    uint32_t offset = lastIndex_ & NODE_BLOCK_SIZE_MASK;
    for (uint32_t i = 0; i <= nid; ++i) {
        auto node = nodes_.at(i);
        uint32_t count = (i != nid) ? NODE_BLOCK_SIZE : offset;
        for (uint32_t j = 0; j < count; ++j) {
            coretypes::TaggedValue obj(node->at(j));
            if (obj.IsHeapObject() && obj.GetHeapObject()->IsForwarded()) {
                (*node)[j] = coretypes::TaggedValue(panda::mem::GetForwardAddress(obj.GetHeapObject())).GetRawData();
            }
        }
    }
}

template <>
inline void HandleStorage<coretypes::TaggedType>::VisitGCRoots([[maybe_unused]] const ObjectVisitor &cb)
{
    if (lastIndex_ == 0) {
        return;
    }
    uint32_t nid = lastIndex_ >> NODE_BLOCK_SIZE_LOG2;
    uint32_t offset = lastIndex_ & NODE_BLOCK_SIZE_MASK;
    if (offset == 0) {
        nid -= 1;
        offset = NODE_BLOCK_SIZE;
    }
    for (uint32_t i = 0; i <= nid; ++i) {
        auto node = nodes_.at(i);
        uint32_t count = (i != nid) ? NODE_BLOCK_SIZE : offset;
        for (uint32_t j = 0; j < count; ++j) {
            coretypes::TaggedValue obj(node->at(j));
            if (obj.IsHeapObject()) {
                cb(obj.GetHeapObject());
            }
        }
    }
}

template <>
inline void HandleStorage<ObjectHeader *>::UpdateHeapObject()
{
    if (lastIndex_ == 0) {
        return;
    }
    uint32_t nid = lastIndex_ >> NODE_BLOCK_SIZE_LOG2;
    uint32_t offset = lastIndex_ & NODE_BLOCK_SIZE_MASK;
    if (offset == 0) {
        nid -= 1;
        offset = NODE_BLOCK_SIZE;
    }
    for (uint32_t i = 0; i <= nid; ++i) {
        auto node = nodes_.at(i);
        uint32_t count = (i != nid) ? NODE_BLOCK_SIZE : offset;
        for (uint32_t j = 0; j < count; ++j) {
            auto *obj = reinterpret_cast<ObjectHeader *>(node->at(j));
            if (obj->IsForwarded()) {
                (*node)[j] = ::panda::mem::GetForwardAddress(obj);
            }
        }
    }
}

template <>
inline void HandleStorage<ObjectHeader *>::VisitGCRoots([[maybe_unused]] const ObjectVisitor &cb)
{
    if (lastIndex_ == 0) {
        return;
    }
    uint32_t nid = lastIndex_ >> NODE_BLOCK_SIZE_LOG2;
    uint32_t offset = lastIndex_ & NODE_BLOCK_SIZE_MASK;
    for (uint32_t i = 0; i <= nid; ++i) {
        auto node = nodes_.at(i);
        uint32_t count = (i != nid) ? NODE_BLOCK_SIZE : offset;
        for (uint32_t j = 0; j < count; ++j) {
            auto obj = reinterpret_cast<ObjectHeader *>(node->at(j));
            cb(obj);
        }
    }
}

template class HandleStorage<coretypes::TaggedType>;

}  // namespace panda

#endif  // PANDA_RUNTIME_HANDLE_STORAGE_INL_H_
