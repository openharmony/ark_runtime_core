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

#ifndef PANDA_RUNTIME_MEM_ALLOCATOR_ADAPTER_H_
#define PANDA_RUNTIME_MEM_ALLOCATOR_ADAPTER_H_

#include "runtime/include/mem/allocator.h"

namespace panda::mem {

template <typename T, AllocScope AllocScopeT>
class AllocatorAdapter;

template <AllocScope AllocScopeT>
class AllocatorAdapter<void, AllocScopeT> {
public:
    using value_type = void;
    using pointer = void *;
    using const_pointer = const void *;

    template <typename U>
    struct Rebind {
        using other = AllocatorAdapter<U, AllocScopeT>;
    };

    template <typename U>
    using rebind = Rebind<U>;

    explicit AllocatorAdapter(Allocator *allocator = InternalAllocator<>::GetInternalAllocatorFromRuntime())
        : allocator_(allocator)
    {
    }
    template <typename U>
    // NOLINTNEXTLINE(google-explicit-constructor)
    AllocatorAdapter(const AllocatorAdapter<U, AllocScopeT> &other) : allocator_(other.allocator_)
    {
    }
    AllocatorAdapter(const AllocatorAdapter &) = default;
    AllocatorAdapter &operator=(const AllocatorAdapter &) = default;

    AllocatorAdapter(AllocatorAdapter &&other) noexcept
    {
        allocator_ = other.allocator_;
        other.allocator_ = nullptr;
    }

    AllocatorAdapter &operator=(AllocatorAdapter &&other) noexcept
    {
        allocator_ = other.allocator_;
        other.allocator_ = nullptr;
        return *this;
    }

    ~AllocatorAdapter() = default;

private:
    Allocator *allocator_;

    template <typename U, AllocScope TypeT>
    friend class AllocatorAdapter;
};

template <typename T, AllocScope AllocScopeT = AllocScope::GLOBAL>
class AllocatorAdapter {
public:
    using value_type = T;
    using pointer = T *;
    using reference = T &;
    using const_pointer = const T *;
    using const_reference = const T &;
    using size_type = size_t;
    using difference_type = ptrdiff_t;

    template <typename U>
    struct Rebind {
        using other = AllocatorAdapter<U, AllocScopeT>;
    };

    template <typename U>
    using rebind = Rebind<U>;

    explicit AllocatorAdapter(Allocator *allocator = InternalAllocator<>::GetInternalAllocatorFromRuntime())
        : allocator_(allocator)
    {
    }
    template <typename U>
    // NOLINTNEXTLINE(google-explicit-constructor)
    AllocatorAdapter(const AllocatorAdapter<U, AllocScopeT> &other) : allocator_(other.allocator_)
    {
    }
    AllocatorAdapter(const AllocatorAdapter &) = default;
    AllocatorAdapter &operator=(const AllocatorAdapter &) = default;

    AllocatorAdapter(AllocatorAdapter &&other) noexcept
    {
        allocator_ = other.allocator_;
        other.allocator_ = nullptr;
    }

    AllocatorAdapter &operator=(AllocatorAdapter &&other) noexcept
    {
        allocator_ = other.allocator_;
        other.allocator_ = nullptr;
        return *this;
    }

    ~AllocatorAdapter() = default;

    // NOLINTNEXTLINE(readability-identifier-naming)
    pointer allocate(size_type size, [[maybe_unused]] const void *hint = nullptr)
    {
        return allocator_->AllocArray<T>(size);
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    void deallocate(pointer ptr, [[maybe_unused]] size_type size)
    {
        allocator_->Free(ptr);
    }

    template <typename U, typename... Args>
    void construct(U *ptr, Args &&... args)  // NOLINT(readability-identifier-naming)
    {
        ::new (static_cast<void *>(ptr)) U(std::forward<Args>(args)...);
    }

    template <typename U>
    void destroy(U *ptr)  // NOLINT(readability-identifier-naming)
    {
        ptr->~U();
    }

    template <typename U>
    bool operator==(const AllocatorAdapter<U> &other)
    {
        return this->allocator_ == other.allocator_;
    }

    template <typename U>
    bool operator!=(const AllocatorAdapter<U> &other)
    {
        return this->allocator_ != other.allocator_;
    }

private:
    Allocator *allocator_;

    template <typename U, AllocScope TypeT>
    friend class AllocatorAdapter;
};

template <AllocScope AllocScopeT>
inline AllocatorAdapter<void, AllocScopeT> Allocator::Adapter()
{
    return AllocatorAdapter<void, AllocScopeT>(this);
}

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_ALLOCATOR_ADAPTER_H_
