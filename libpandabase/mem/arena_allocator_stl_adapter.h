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

#ifndef PANDA_LIBPANDABASE_MEM_ARENA_ALLOCATOR_STL_ADAPTER_H_
#define PANDA_LIBPANDABASE_MEM_ARENA_ALLOCATOR_STL_ADAPTER_H_

#include "arena_allocator.h"

namespace panda {

// Adapter for use of ArenaAllocator in STL containers.
template <typename T, bool use_oom_handler>
class ArenaAllocatorAdapter;

template <bool use_oom_handler>
class ArenaAllocatorAdapter<void, use_oom_handler> {
public:
    using value_type = void;
    using pointer = void *;
    using const_pointer = const void *;

    template <typename U>
    struct Rebind {
        using other = ArenaAllocatorAdapter<U, use_oom_handler>;
    };

    template <typename U>
    using rebind = Rebind<U>;

    explicit ArenaAllocatorAdapter(ArenaAllocatorT<use_oom_handler> *allocator) : allocator_(allocator) {}
    template <typename U>
    // NOLINTNEXTLINE(google-explicit-constructor)
    ArenaAllocatorAdapter(const ArenaAllocatorAdapter<U, use_oom_handler> &other) : allocator_(other.allocator_)
    {
    }
    ArenaAllocatorAdapter(const ArenaAllocatorAdapter &) = default;
    ArenaAllocatorAdapter &operator=(const ArenaAllocatorAdapter &) = default;
    ArenaAllocatorAdapter(ArenaAllocatorAdapter &&) = default;
    ArenaAllocatorAdapter &operator=(ArenaAllocatorAdapter &&) = default;
    ~ArenaAllocatorAdapter() = default;

private:
    ArenaAllocatorT<use_oom_handler> *allocator_;

    template <typename U, bool use_oom_handle>
    friend class ArenaAllocatorAdapter;
};

template <typename T, bool use_oom_handler = false>
class ArenaAllocatorAdapter {
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
        using other = ArenaAllocatorAdapter<U, use_oom_handler>;
    };

    template <typename U>
    using rebind = Rebind<U>;

    explicit ArenaAllocatorAdapter(ArenaAllocatorT<use_oom_handler> *allocator) : allocator_(allocator) {}
    template <typename U>
    // NOLINTNEXTLINE(google-explicit-constructor)
    ArenaAllocatorAdapter(const ArenaAllocatorAdapter<U, use_oom_handler> &other) : allocator_(other.allocator_)
    {
    }
    ArenaAllocatorAdapter(const ArenaAllocatorAdapter &) = default;
    ArenaAllocatorAdapter &operator=(const ArenaAllocatorAdapter &) = default;
    ArenaAllocatorAdapter(ArenaAllocatorAdapter &&other) noexcept
    {
        allocator_ = other.allocator_;
        other.allocator_ = nullptr;
    }
    ArenaAllocatorAdapter &operator=(ArenaAllocatorAdapter &&other) noexcept
    {
        allocator_ = other.allocator_;
        other.allocator_ = nullptr;
        return *this;
    }
    ~ArenaAllocatorAdapter() = default;

    // NOLINTNEXTLINE(readability-identifier-naming)
    size_type max_size() const
    {
        return static_cast<size_type>(-1) / sizeof(T);
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
                     [[maybe_unused]] typename ArenaAllocatorAdapter<void, use_oom_handler>::pointer ptr = nullptr)
    {
        ASSERT(n <= max_size());
        return allocator_->template AllocArray<T>(n);  // CODECHECK-NOLINT(C_RULE_ID_HORIZON_SPACE)
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    void deallocate([[maybe_unused]] pointer p, [[maybe_unused]] size_type n) {}

    template <typename U, typename... Args>
    void construct(U *p, Args &&... args)  // NOLINT(readability-identifier-naming)
    {
        // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
        ::new (static_cast<void *>(p)) U(std::forward<Args>(args)...);
    }
    template <typename U>
    void destroy(U *p)  // NOLINT(readability-identifier-naming)
    {
        p->~U();
    }

private:
    ArenaAllocatorT<use_oom_handler> *allocator_ {nullptr};

    template <typename U, bool use_oom_handle>
    friend class ArenaAllocatorAdapter;

    template <typename U, bool use_oom_handle>
    // NOLINTNEXTLINE(readability-redundant-declaration)
    friend inline bool operator==(const ArenaAllocatorAdapter<U, use_oom_handle> &lhs,
                                  const ArenaAllocatorAdapter<U, use_oom_handle> &rhs);
};

template <typename T, bool use_oom_handle>
inline bool operator==(const ArenaAllocatorAdapter<T, use_oom_handle> &lhs,
                       const ArenaAllocatorAdapter<T, use_oom_handle> &rhs)
{
    return lhs.allocator_ == rhs.allocator_;
}

template <typename T, bool use_oom_handle>
inline bool operator!=(const ArenaAllocatorAdapter<T, use_oom_handle> &lhs,
                       const ArenaAllocatorAdapter<T, use_oom_handle> &rhs)
{
    return !(lhs == rhs);
}

template <bool use_oom_handler>
inline ArenaAllocatorAdapter<void, use_oom_handler> ArenaAllocatorT<use_oom_handler>::Adapter()
{
    return ArenaAllocatorAdapter<void, use_oom_handler>(this);
}

}  // namespace panda

#endif  // PANDA_LIBPANDABASE_MEM_ARENA_ALLOCATOR_STL_ADAPTER_H_
