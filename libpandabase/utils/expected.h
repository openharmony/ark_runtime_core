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

#ifndef PANDA_LIBPANDABASE_UTILS_EXPECTED_H_
#define PANDA_LIBPANDABASE_UTILS_EXPECTED_H_

#include <variant>

#include "macros.h"

namespace panda {

template <class E>
class Unexpected final {
public:
    explicit Unexpected(E e) noexcept(std::is_nothrow_move_constructible_v<E>) : e_(std::move(e)) {}

    const E &Value() const &noexcept
    {
        return e_;
    }
    E &Value() & noexcept
    {
        return e_;
    }
    E &&Value() && noexcept
    {
        return std::move(e_);
    }

    ~Unexpected() = default;

    DEFAULT_COPY_SEMANTIC(Unexpected);
    NO_MOVE_SEMANTIC(Unexpected);

private:
    E e_;
};

class ExpectedConfig final {
public:
#ifdef NDEBUG
    static constexpr bool RELEASE = true;
#else
    static constexpr bool RELEASE = false;
#endif
};

// Simplified implementation of proposed std::expected
// Note that as with std::expected, program that tries to instantiate
// Expected with T or E for a reference type is ill-formed.
template <class T, class E>
class Expected final {
public:
    Expected() noexcept : v_(T()) {}
    // The following constructors are non-explicit to be aligned with std::expected
    // NOLINTNEXTLINE(google-explicit-constructor)
    Expected(T v) noexcept(std::is_nothrow_move_constructible_v<T>) : v_(std::move(v)) {}
    // NOLINTNEXTLINE(google-explicit-constructor)
    Expected(Unexpected<E> e) noexcept(std::is_nothrow_move_constructible_v<E>) : v_(std::move(e.Value())) {}

    bool HasValue() const noexcept
    {
        return std::holds_alternative<T>(v_);
    }

    explicit operator bool() const noexcept
    {
        return HasValue();
    }

    const E &Error() const &noexcept(ExpectedConfig::RELEASE)
    {
        ASSERT(!HasValue());
        return std::get<E>(v_);
    }

    E &Error() & noexcept(ExpectedConfig::RELEASE)
    {
        ASSERT(!HasValue());
        return std::get<E>(v_);
    }

    E &&Error() && noexcept(ExpectedConfig::RELEASE)
    {
        ASSERT(!HasValue());
        return std::move(std::get<E>(v_));
    }

    const T &Value() const &noexcept(ExpectedConfig::RELEASE)
    {
        ASSERT(HasValue());
        return std::get<T>(v_);
    }

    // NOLINTNEXTLINE(bugprone-exception-escape)
    T &Value() & noexcept(ExpectedConfig::RELEASE)
    {
        ASSERT(HasValue());
        return std::get<T>(v_);
    }

    T &&Value() && noexcept(ExpectedConfig::RELEASE)
    {
        ASSERT(HasValue());
        return std::move(std::get<T>(v_));
    }

    const T &operator*() const &noexcept(ExpectedConfig::RELEASE)
    {
        return Value();
    }

    T &operator*() & noexcept(ExpectedConfig::RELEASE)
    {
        return Value();
    }

    T &&operator*() && noexcept(ExpectedConfig::RELEASE)
    {
        return std::move(*this).Value();
    }

    template <class U = T>
    T ValueOr(U &&v) const &
    {
        if (HasValue()) {
            return Value();
        }
        return std::forward<U>(v);
    }

    template <class U = T>
    T ValueOr(U &&v) &&
    {
        if (HasValue()) {
            return Value();
        }
        return std::forward<U>(v);
    }

    ~Expected() = default;

    DEFAULT_COPY_SEMANTIC(Expected);
    DEFAULT_MOVE_SEMANTIC(Expected);

private:
    std::variant<T, E> v_;
};

}  // namespace panda

#endif  // PANDA_LIBPANDABASE_UTILS_EXPECTED_H_
