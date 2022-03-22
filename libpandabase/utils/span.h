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

#ifndef PANDA_LIBPANDABASE_UTILS_SPAN_H_
#define PANDA_LIBPANDABASE_UTILS_SPAN_H_

#include "macros.h"

#include <cstddef>
#include <iterator>

namespace panda {

/**
 * Similar to std::span that will come in C++20.
 */
template <class T>
class Span {
public:
    using ElementType = T;
    using value_type = std::remove_cv_t<T>;
    using ValueType = value_type;
    using Reference = T &;
    using ConstReference = const T &;
    using Iterator = T *;
    using ConstIterator = const T *;
    using ReverseIterator = std::reverse_iterator<Iterator>;
    using ConstReverseIterator = std::reverse_iterator<ConstIterator>;

    Span() = default;
    Span(Iterator data, size_t size) : data_(data), size_(size) {}
    constexpr Span(const Span &other) noexcept = default;
    Span(Span &&other) noexcept = default;
    ~Span() = default;

    // The following constructor is non-explicit to be aligned with std::span
    template <class U, size_t N>
    // NOLINTNEXTLINE(google-explicit-constructor,modernize-avoid-c-arrays)
    constexpr Span(U (&array)[N]) : Span(array, N)
    {
    }

    Span(Iterator begin, Iterator end) : Span(begin, end - begin) {}

    template <class Vector>
    explicit Span(Vector &v) : Span(v.data(), v.size())
    {
    }

    template <class Vector>
    explicit Span(const Vector &v) : Span(v.data(), v.size())
    {
    }

    constexpr Span &operator=(const Span &other) noexcept = default;
    Span &operator=(Span &&other) noexcept = default;

    // NOLINTNEXTLINE(readability-identifier-naming)
    Iterator begin()
    {
        return data_;
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    ConstIterator begin() const
    {
        return data_;
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    ConstIterator cbegin() const
    {
        return data_;
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    Iterator end()
    {
        return data_ + size_;  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    ConstIterator end() const
    {
        return data_ + size_;  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    ConstIterator cend() const
    {
        return data_ + size_;  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    ReverseIterator rbegin()
    {
        return ReverseIterator(end());
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    ConstReverseIterator rbegin() const
    {
        return ConstReverseIterator(end());
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    ConstReverseIterator crbegin() const
    {
        return ConstReverseIterator(cend());
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    ReverseIterator rend()
    {
        return ReverseIterator(begin());
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    ConstReverseIterator rend() const
    {
        return ConstReverseIterator(begin());
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    ConstReverseIterator crend() const
    {
        return ConstReverseIterator(cbegin());
    }

    // NOLINT(readability-identifier-naming)
    Reference operator[](size_t index)
    {
        ASSERT(index < size_);
        return data_[index];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }

    // NOLINT(readability-identifier-naming)
    ConstReference operator[](size_t index) const
    {
        ASSERT(index < size_);
        return data_[index];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }

    constexpr size_t Size() const
    {
        return size_;
    }

    constexpr size_t SizeBytes() const
    {
        return size_ * sizeof(ElementType);
    }

    constexpr bool Empty() const
    {
        return size_ == 0U;
    }

    Iterator Data()
    {
        return data_;
    }

    ConstIterator Data() const
    {
        return data_;
    }

    Span First(size_t length) const
    {
        ASSERT(length <= size_);
        return SubSpan(0, length);
    }

    Span Last(size_t length) const
    {
        ASSERT(length <= size_);
        return SubSpan(size_ - length, length);
    }

    Span SubSpan(size_t position, size_t length) const
    {
        ASSERT((position + length) <= size_);
        return Span(data_ + position, length);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }

    Span SubSpan(size_t position) const
    {
        ASSERT(position <= size_);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return Span(data_ + position, size_ - position);
    }

    template <typename SubT>
    Span<SubT> SubSpan(size_t position, size_t length) const
    {
        ASSERT((position * sizeof(T) + length * sizeof(SubT)) <= (size_ * sizeof(T)));
        ASSERT(((reinterpret_cast<uintptr_t>(data_ + position)) % alignof(SubT)) == 0);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return Span<SubT>(reinterpret_cast<SubT *>(data_ + position), length);
    }

    auto ToConst() const
    {
        return Span<const std::remove_const_t<T>>(data_, size_);
    }

    // Methods for compatibility with std containers
    // NOLINTNEXTLINE(readability-identifier-naming)
    size_t size() const
    {
        return size_;
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    size_t empty() const
    {
        return size() == 0;
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    Iterator data()
    {
        return data_;
    }

    // NOLINTNEXTLINE(readability-identifier-naming)
    ConstIterator data() const
    {
        return data_;
    }

private:
    Iterator data_ {nullptr};
    size_t size_ {0};
};

// Deduction guides
template <class U, size_t N>
Span(U (&)[N])->Span<U>;  // NOLINT(modernize-avoid-c-arrays)

template <class Vector>
Span(Vector &)->Span<typename Vector::value_type>;

template <class Vector>
Span(const Vector &)->Span<const typename Vector::value_type>;

// Non-member functions
template <class T>
Span<const std::byte> AsBytes(Span<T> s) noexcept
{
    return {reinterpret_cast<const std::byte *>(s.Data()), s.SizeBytes()};
}

template <class T, typename = std::enable_if_t<!std::is_const_v<T>>>
Span<std::byte> AsWritableBytes(Span<T> s) noexcept
{
    return {reinterpret_cast<std::byte *>(s.Data()), s.SizeBytes()};
}

}  // namespace panda

#endif  // PANDA_LIBPANDABASE_UTILS_SPAN_H_
