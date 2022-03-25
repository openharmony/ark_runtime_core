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

#ifndef PANDA_LIBPANDABASE_UTILS_SMALL_VECTOR_H_
#define PANDA_LIBPANDABASE_UTILS_SMALL_VECTOR_H_

#include "utils/arch.h"
#include <algorithm>
#include <array>
#include <vector>

namespace panda {

/**
 * Helper class that provides main Panda's allocator interface: AdapterType, Adapter(), GetInstance().
 */
class StdAllocatorStub {
public:
    template <typename T>
    using AdapterType = std::allocator<T>;

    auto Adapter()
    {
        // We can't std::allocator<void> because it is removed in C++20
        return std::allocator<uint8_t>();
    }

    static StdAllocatorStub *GetInstance()
    {
        alignas(StdAllocatorStub *) static StdAllocatorStub instance;
        return &instance;
    }
};

/**
 * SmallVector stores `N` elements statically inside its static buffer. Static buffer shares memory with a std::vector
 * that will be created once the number of elements exceeds size of the static buffer - `N`.
 *
 * @tparam T Type of elements to store
 * @tparam N Number of elements to be stored statically
 * @tparam Allocator Allocator that will be used to allocate memory for the dynamic storage
 */
template <typename T, size_t N, typename Allocator = StdAllocatorStub>
class SmallVector {
    // Least bit of the pointer should not be used in a memory addressing, because we pack `allocated` flag there
    static_assert(alignof(Allocator *) > 1);
    // When N is zero, then consider using std::vector directly
    static_assert(N != 0);

    struct StaticBuffer {
        uint32_t size {0};
        std::array<T, N> data;
    };
    using VectorType = std::vector<T, typename Allocator::template AdapterType<T>>;

public:
    using value_type = typename VectorType::value_type;
    using reference = typename VectorType::reference;
    using const_reference = typename VectorType::const_reference;
    using pointer = typename VectorType::pointer;
    using const_pointer = typename VectorType::const_pointer;
    using difference_type = typename VectorType::difference_type;

    template <typename IteratorType, bool reverse>
    class Iterator
        : public std::iterator<std::random_access_iterator_tag, IteratorType, int32_t, IteratorType *, IteratorType &> {
        IteratorType *Add(difference_type v)
        {
            if constexpr (reverse) {  // NOLINT
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                return pointer_ -= v;
            } else {  // NOLINT
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                return pointer_ += v;
            }
        }

        IteratorType *Sub(difference_type v)
        {
            if constexpr (reverse) {  // NOLINT
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                return pointer_ + v;
            } else {  // NOLINT
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                return pointer_ - v;
            }
        }

    public:
        explicit Iterator(IteratorType *param_pointer) : pointer_(param_pointer) {}

        IteratorType *operator->()
        {
            return pointer_;
        }

        IteratorType &operator*()
        {
            return *pointer_;
        }

        Iterator &operator++()
        {
            pointer_ = Add(1);
            return *this;
        }

        Iterator operator++(int)  // NOLINT(cert-dcl21-cpp)
        {
            Iterator it(pointer_);
            pointer_ = Add(1);
            return it;
        }

        Iterator &operator--()
        {
            pointer_ = Sub(1);
            return *this;
        }

        Iterator operator--(int)  // NOLINT(cert-dcl21-cpp)
        {
            Iterator it(pointer_);
            pointer_ = Sub(1);
            return it;
        }

        Iterator &operator+=(difference_type n)
        {
            pointer_ = Add(n);
            return *this;
        }

        Iterator &operator-=(difference_type n)
        {
            pointer_ = Sub(n);
            return *this;
        }

        Iterator operator+(int32_t n) const
        {
            Iterator it(*this);
            it.pointer_ = it.Add(n);
            return it;
        }

        Iterator operator-(int32_t n) const
        {
            Iterator it(*this);
            it.pointer_ = it.Sub(n);
            return it;
        }

        difference_type operator-(const Iterator &rhs) const
        {
            if constexpr (reverse) {  // NOLINT
                return rhs.pointer_ - pointer_;
            }
            return pointer_ - rhs.pointer_;
        }

        bool operator==(const Iterator &rhs) const
        {
            return pointer_ == rhs.pointer_;
        }

        bool operator!=(const Iterator &rhs) const
        {
            return !(*this == rhs);
        }

        ~Iterator() = default;

        DEFAULT_COPY_SEMANTIC(Iterator);
        DEFAULT_NOEXCEPT_MOVE_SEMANTIC(Iterator);

    private:
        IteratorType *pointer_;
    };

    using iterator = Iterator<T, false>;
    using const_iterator = Iterator<const T, false>;
    using reverse_iterator = Iterator<T, true>;
    using const_reverse_iterator = Iterator<const T, true>;

    SmallVector() : allocator_(AddStaticFlag(Allocator::GetInstance()))
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        buffer_.size = 0;
    }

    explicit SmallVector(Allocator *allocator) : allocator_(AddStaticFlag(allocator))
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        buffer_.size = 0;
    }

    SmallVector(const SmallVector &other) : allocator_(other.allocator_)
    {
        if (other.IsStatic()) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            new (&buffer_) StaticBuffer(other.buffer_);
        } else {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            new (&vector_) VectorType(other.vector_);
        }
    }

    SmallVector(SmallVector &&other) noexcept : allocator_(other.allocator_)
    {
        if (other.IsStatic()) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            new (&buffer_) StaticBuffer(std::move(other.buffer_));
        } else {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            new (&vector_) VectorType(std::move(other.vector_));
        }
        other.ResetToStatic();
    }

    virtual ~SmallVector()
    {
        Destroy();
    }

    // NOLINTNEXTLINE(bugprone-unhandled-self-assignment, cert-oop54-cpp)
    SmallVector &operator=(const SmallVector &other)
    {
        if (&other == this) {
            return *this;
        }

        Destroy();
        allocator_ = other.allocator_;
        if (other.IsStatic()) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            buffer_ = other.buffer_;
        } else {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            new (&vector_) VectorType(other.vector_);
        }

        return *this;
    }

    SmallVector &operator=(SmallVector &&other) noexcept
    {
        Destroy();
        allocator_ = other.allocator_;
        if (other.IsStatic()) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            buffer_ = std::move(other.buffer_);
        } else {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            new (&vector_) VectorType(std::move(other.vector_));
        }
        other.ResetToStatic();
        return *this;
    }

    const_iterator begin() const
    {
        return const_iterator(&operator[](0));
    }

    iterator begin()
    {
        return iterator(&operator[](0));
    }

    const_iterator cbegin() const
    {
        return const_iterator(&operator[](0));
    }

    const_iterator end() const
    {
        return const_iterator(&operator[](size()));
    }

    iterator end()
    {
        return iterator(&operator[](size()));
    }

    const_iterator cend() const
    {
        return const_iterator(&operator[](size()));
    }

    auto rbegin() const
    {
        return const_reverse_iterator(&operator[](size() - 1));
    }

    auto rbegin()
    {
        return reverse_iterator(&operator[](size() - 1));
    }

    auto crbegin() const
    {
        return const_reverse_iterator(&operator[](size() - 1));
    }

    auto rend() const
    {
        return const_reverse_iterator(&operator[](0) - 1);
    }

    auto rend()
    {
        return reverse_iterator(&operator[](0) - 1);
    }

    auto crend() const
    {
        return const_reverse_iterator(&operator[](0) - 1);
    }

    size_t size() const
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        return IsStatic() ? buffer_.size : vector_.size();
    }

    size_t capacity() const
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        return IsStatic() ? buffer_.data.size() : vector_.capacity();
    }

    void push_back(const value_type &value)
    {
        if (!EnsureStaticSpace(1)) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            vector_.push_back(value);
        } else {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            new (&buffer_.data[buffer_.size++]) T(value);
        }
    }

    void push_back(value_type &&value)
    {
        if (!EnsureStaticSpace(1)) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            vector_.push_back(std::move(value));
        } else {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            new (&buffer_.data[buffer_.size++]) T(std::move(value));
        }
    }

    template <typename... _Args>
    reference emplace_back(_Args &&... values)
    {
        if (!EnsureStaticSpace(1)) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            return vector_.emplace_back(std::move(values)...);
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        return *(new (&buffer_.data[buffer_.size++]) T(std::move(values)...));
    }

    void resize(size_t size)
    {
        if (size <= this->size()) {
            if (IsStatic()) {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
                std::for_each(buffer_.data.begin() + size, buffer_.data.begin() + buffer_.size, [](T &v) { v.~T(); });
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
                buffer_.size = size;
            } else {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
                vector_.resize(size);
            }
        } else {
            if (EnsureStaticSpace(size - this->size())) {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
                std::for_each(buffer_.data.begin() + buffer_.size, buffer_.data.begin() + size,
                              [](T &v) { new (&v) T; });
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
                buffer_.size = size;
            } else {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
                vector_.resize(size);
            }
        }
    }

    void resize(size_t size, const value_type &val)
    {
        if (size <= this->size()) {
            if (IsStatic()) {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
                std::for_each(buffer_.data.begin() + size, buffer_.data.begin() + buffer_.size, [](T &v) { v.~T(); });
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
                buffer_.size = size;
            } else {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
                vector_.resize(size, val);
            }
        } else {
            if (EnsureStaticSpace(size - this->size())) {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
                std::for_each(buffer_.data.begin() + buffer_.size, buffer_.data.begin() + size,
                              [&val](T &v) { new (&v) T(val); });
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
                buffer_.size = size;
            } else {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
                vector_.resize(size, val);
            }
        }
    }

    void clear()
    {
        if (IsStatic()) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            std::for_each(buffer_.data.begin(), buffer_.data.begin() + buffer_.size, [](T &v) { v.~T(); });
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            buffer_.size = 0;
        } else {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            vector_.clear();
        }
    }

    reference operator[](size_t i)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        return IsStatic() ? buffer_.data[i] : vector_[i];
    }

    const_reference operator[](size_t i) const
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        return IsStatic() ? buffer_.data[i] : vector_[i];
    }

    bool IsStatic() const
    {
        return (bit_cast<uintptr_t>(allocator_) & 1U) != 0;
    }

private:
    bool EnsureStaticSpace(size_t size)
    {
        if (!IsStatic()) {
            return false;
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        size_t size_required = buffer_.size + size;
        if (size_required > N) {
            MoveToVector(size_required);
            return false;
        }
        return true;
    }

    void MoveToVector(size_t reserved_size)
    {
        ASSERT(IsStatic());
        allocator_ = reinterpret_cast<Allocator *>(bit_cast<uintptr_t>(allocator_) & ~1LLU);
        VectorType tmp_vector(allocator_->Adapter());
        tmp_vector.reserve(reserved_size);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        std::copy(buffer_.data.begin(), buffer_.data.begin() + buffer_.size, std::back_inserter(tmp_vector));
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        new (&vector_) VectorType(std::move(tmp_vector));
    }

    void Destroy()
    {
        if (IsStatic()) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            std::for_each(buffer_.data.begin(), buffer_.data.begin() + buffer_.size, [](T &v) { v.~T(); });
        } else {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            vector_.~VectorType();
        }
    }

    void ResetToStatic()
    {
        allocator_ = AddStaticFlag(allocator_);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        buffer_.size = 0;
    }

    static Allocator *AddStaticFlag(Allocator *p)
    {
        return reinterpret_cast<Allocator *>((bit_cast<uintptr_t>(p) | 1U));
    }

private:
    union {
        StaticBuffer buffer_;
        VectorType vector_;
    };
    Allocator *allocator_ {AddStaticFlag(nullptr)};
};

}  // namespace panda

#endif  // PANDA_LIBPANDABASE_UTILS_SMALL_VECTOR_H_
