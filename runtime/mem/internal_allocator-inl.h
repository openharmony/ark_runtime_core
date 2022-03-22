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

#ifndef PANDA_RUNTIME_MEM_INTERNAL_ALLOCATOR_INL_H_
#define PANDA_RUNTIME_MEM_INTERNAL_ALLOCATOR_INL_H_

#include "runtime/mem/malloc-proxy-allocator-inl.h"
#include "runtime/mem/freelist_allocator-inl.h"
#include "runtime/mem/humongous_obj_allocator-inl.h"
#include "runtime/mem/internal_allocator.h"
#include "runtime/mem/runslots_allocator-inl.h"

namespace panda::mem {

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_INTERNAL_ALLOCATOR(level) LOG(level, ALLOC) << "InternalAllocator: "

template <InternalAllocatorConfig Config>
template <class T>
T *InternalAllocator<Config>::AllocArray(size_t size)
{
    return static_cast<T *>(this->Alloc(sizeof(T) * size));
}

template <InternalAllocatorConfig Config>
template <typename T, typename... Args>
std::enable_if_t<!std::is_array_v<T>, T *> InternalAllocator<Config>::New(Args &&... args)
{
    void *p = Alloc(sizeof(T));
    if (UNLIKELY(p == nullptr)) {
        return nullptr;
    }
    new (p) T(std::forward<Args>(args)...);
    return reinterpret_cast<T *>(p);
}

template <InternalAllocatorConfig Config>
template <typename T>
std::enable_if_t<is_unbounded_array_v<T>, std::remove_extent_t<T> *> InternalAllocator<Config>::New(size_t size)
{
    static constexpr size_t SIZE_BEFORE_DATA_OFFSET = AlignUp(sizeof(size_t), DEFAULT_ALIGNMENT_IN_BYTES);
    using element_type = std::remove_extent_t<T>;
    void *p = Alloc(SIZE_BEFORE_DATA_OFFSET + sizeof(element_type) * size);
    *static_cast<size_t *>(p) = size;
    element_type *data = ToNativePtr<element_type>(ToUintPtr(p) + SIZE_BEFORE_DATA_OFFSET);
    element_type *current_element = data;
    for (size_t i = 0; i < size; ++i, ++current_element) {
        new (current_element) element_type();
    }
    return data;
}

template <InternalAllocatorConfig Config>
template <class T>
void InternalAllocator<Config>::Delete(T *ptr)
{
    if constexpr (std::is_class_v<T>) {
        ptr->~T();
    }
    Free(ptr);
}

template <InternalAllocatorConfig Config>
template <typename T>
void InternalAllocator<Config>::DeleteArray(T *data)
{
    static constexpr size_t SIZE_BEFORE_DATA_OFFSET = AlignUp(sizeof(size_t), DEFAULT_ALIGNMENT_IN_BYTES);
    void *p = ToVoidPtr(ToUintPtr(data) - SIZE_BEFORE_DATA_OFFSET);
    size_t size = *static_cast<size_t *>(p);
    if constexpr (std::is_class_v<T>) {
        for (size_t i = 0; i < size; ++i, ++data) {
            data->~T();
        }
    }
    Free(p);
}

template <InternalAllocatorConfig Config>
template <typename MemVisitor>
void InternalAllocator<Config>::VisitAndRemoveAllPools(MemVisitor mem_visitor)
{
    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if constexpr (Config == InternalAllocatorConfig::PANDA_ALLOCATORS) {
        runslots_allocator_->VisitAndRemoveAllPools(mem_visitor);
        freelist_allocator_->VisitAndRemoveAllPools(mem_visitor);
        humongous_allocator_->VisitAndRemoveAllPools(mem_visitor);
    }
}

template <InternalAllocatorConfig Config>
template <typename MemVisitor>
void InternalAllocator<Config>::VisitAndRemoveFreePools(MemVisitor mem_visitor)
{
    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if constexpr (Config == InternalAllocatorConfig::PANDA_ALLOCATORS) {
        runslots_allocator_->VisitAndRemoveFreePools(mem_visitor);
        freelist_allocator_->VisitAndRemoveFreePools(mem_visitor);
        humongous_allocator_->VisitAndRemoveFreePools(mem_visitor);
    }
}

#undef LOG_INTERNAL_ALLOCATOR

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_INTERNAL_ALLOCATOR_INL_H_
