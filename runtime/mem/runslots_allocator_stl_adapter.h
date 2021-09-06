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

#ifndef PANDA_RUNTIME_MEM_RUNSLOTS_ALLOCATOR_STL_ADAPTER_H_
#define PANDA_RUNTIME_MEM_RUNSLOTS_ALLOCATOR_STL_ADAPTER_H_

#include "runtime/mem/runslots_allocator.h"

namespace panda::mem {

// Adapter for use of RunSlotsAllocator in STL containers.
template <typename T, typename AllocConfigT, typename LockConfigT>
class RunSlotsAllocatorAdapter;

template <typename AllocConfigT, typename LockConfigT>
class RunSlotsAllocatorAdapter<void, AllocConfigT, LockConfigT> {
public:
    using value_type = void;
    using pointer = void *;
    using const_pointer = const void *;

    template <typename U>
    struct Rebind {
        using other = RunSlotsAllocatorAdapter<U, AllocConfigT, LockConfigT>;
    };

    template <typename U>
    using rebind = Rebind<U>;

    explicit RunSlotsAllocatorAdapter(RunSlotsAllocator<AllocConfigT, LockConfigT> *allocator) : allocator_(allocator)
    {
    }
    template <typename U>
    // NOLINTNEXTLINE(google-explicit-constructor)
    RunSlotsAllocatorAdapter(const RunSlotsAllocatorAdapter<U, AllocConfigT, LockConfigT> &other)
        : allocator_(other.allocator_)
    {
    }
    DEFAULT_COPY_SEMANTIC(RunSlotsAllocatorAdapter);
    DEFAULT_MOVE_SEMANTIC(RunSlotsAllocatorAdapter);
    ~RunSlotsAllocatorAdapter() = default;

private:
    RunSlotsAllocator<AllocConfigT, LockConfigT> *allocator_;

    template <typename U, typename AllocConfigT_, typename LockConfigT_>
    friend class RunSlotsAllocatorAdapter;
};

template <typename T, typename AllocConfigT, typename LockConfigT>
class RunSlotsAllocatorAdapter {
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
        using other = RunSlotsAllocatorAdapter<U, AllocConfigT, LockConfigT>;
    };

    template <typename U>
    using rebind = Rebind<U>;

    explicit RunSlotsAllocatorAdapter(AllocConfigT *allocator) : allocator_(allocator) {}
    template <typename U>
    // NOLINTNEXTLINE(google-explicit-constructor)
    RunSlotsAllocatorAdapter(const RunSlotsAllocatorAdapter<U, AllocConfigT, LockConfigT> &other)
        : allocator_(other.allocator_)
    {
    }
    RunSlotsAllocatorAdapter(const RunSlotsAllocatorAdapter &) = default;
    RunSlotsAllocatorAdapter &operator=(const RunSlotsAllocatorAdapter &) = default;
    RunSlotsAllocatorAdapter(RunSlotsAllocatorAdapter &&other) noexcept
    {
        allocator_ = other.allocator_;
        other.allocator_ = nullptr;
    }
    RunSlotsAllocatorAdapter &operator=(RunSlotsAllocatorAdapter &&other) noexcept
    {
        allocator_ = other.allocator_;
        other.allocator_ = nullptr;
        return *this;
    }
    ~RunSlotsAllocatorAdapter() = default;

    // NOLINTNEXTLINE(readability-identifier-naming)
    size_type max_size() const
    {
        return RunSlots<>::MaxSlotSize() / sizeof(T);
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    pointer address(reference x) const
    {
        return &x;
    }
    // NOLINTNEXTLINE(readability-identifier-naming)
    const_pointer address(const_reference x) const
    {
        return &x;
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    pointer allocate(size_type n,
                     [[maybe_unused]]
                     typename RunSlotsAllocatorAdapter<void, AllocConfigT, LockConfigT>::pointer ptr = nullptr)
    {
        ASSERT(n <= max_size());
        return allocator_->template AllocArray<T>(n);
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    void deallocate([[maybe_unused]] pointer p, [[maybe_unused]] size_type n)
    {
        allocator_->Free(static_cast<void *>(p));
    }

    template <typename U, typename... Args>
    void construct(U *p, Args &&... args)
    {  // NOLINT(readability-identifier-naming)
        ::new (static_cast<void *>(p)) U(std::forward<Args>(args)...);
    }
    template <typename U>
    void destroy(U *p)
    {  // NOLINT(readability-identifier-naming)
        p->~U();
    }

private:
    RunSlotsAllocator<AllocConfigT, LockConfigT> *allocator_ {nullptr};

    template <typename U, typename AllocConfigT_, typename LockConfigT_>
    friend class RunSlotsAllocatorAdapter;

    template <typename U>
    // NOLINTNEXTLINE(readability-redundant-declaration)
    friend inline bool operator==(const RunSlotsAllocatorAdapter &,
                                  const RunSlotsAllocatorAdapter<U, AllocConfigT, LockConfigT> &rhs);
};

template <typename T, typename AllocConfigT, typename LockConfigT>
inline bool operator==(const RunSlotsAllocatorAdapter<T, AllocConfigT, LockConfigT> &lhs,
                       const RunSlotsAllocatorAdapter<T, AllocConfigT, LockConfigT> &rhs)
{
    return lhs.allocator_ == rhs.allocator_;
}

template <typename T, typename AllocConfigT, typename LockConfigT>
inline bool operator!=(const RunSlotsAllocatorAdapter<T, AllocConfigT, LockConfigT> &lhs,
                       const RunSlotsAllocatorAdapter<T, AllocConfigT, LockConfigT> &rhs)
{
    return !(lhs == rhs);
}

template <typename AllocConfigT, typename LockConfigT>
inline RunSlotsAllocatorAdapter<void, AllocConfigT, LockConfigT> RunSlotsAllocator<AllocConfigT, LockConfigT>::Adapter()
{
    return RunSlotsAllocatorAdapter<void, AllocConfigT, LockConfigT>(this);
}

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_RUNSLOTS_ALLOCATOR_STL_ADAPTER_H_
