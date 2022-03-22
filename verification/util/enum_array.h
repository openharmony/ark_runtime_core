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

#ifndef PANDA_VERIFICATION_UTIL_ENUM_ARRAY_H_
#define PANDA_VERIFICATION_UTIL_ENUM_ARRAY_H_

#include "macros.h"

#include <type_traits>

namespace panda::verifier {
template <typename T, typename Enum, Enum...>
class EnumArray {
public:
    template <typename... Args>
    EnumArray(Args...)
    {
    }
    T &operator[]([[maybe_unused]] Enum)
    {
        UNREACHABLE();
    }
    const T &operator[]([[maybe_unused]] Enum) const
    {
        UNREACHABLE();
    }
};

template <typename T, typename Enum, Enum E, Enum... Rest>
class EnumArray<T, Enum, E, Rest...> : public EnumArray<T, Enum, Rest...> {
    using Base = EnumArray<T, Enum, Rest...>;

public:
    template <typename... Args>
    EnumArray(Args... args) : Base(args...), T_ {args...}
    {
    }
    EnumArray(decltype(T {E}) *ptr [[maybe_unused]] = nullptr) : Base(), T_ {E} {}
    ~EnumArray() = default;
    T &operator[](Enum e)
    {
        if (e == E) {
            return T_;
        }
        return Base::operator[](e);
    }

    const T &operator[](Enum e) const
    {
        if (e == E) {
            return T_;
        }
        return Base::operator[](e);
    }

private:
    T T_;
};

template <typename T, typename Enum, Enum...>
class EnumArraySimple {
public:
    template <typename... Args>
    EnumArraySimple(Args...)
    {
    }
    T &operator[]([[maybe_unused]] Enum)
    {
        UNREACHABLE();
    }
    const T &operator[]([[maybe_unused]] Enum) const
    {
        UNREACHABLE();
    }
};

template <typename T, typename Enum, Enum E, Enum... Rest>
class EnumArraySimple<T, Enum, E, Rest...> : public EnumArraySimple<T, Enum, Rest...> {
    using Base = EnumArraySimple<T, Enum, Rest...>;

public:
    template <typename... Args>
    EnumArraySimple(Args... args) : Base(args...), T_ {args...}
    {
    }
    ~EnumArraySimple() = default;
    T &operator[](Enum e)
    {
        if (e == E) {
            return T_;
        }
        return Base::operator[](e);
    }

    const T &operator[](Enum e) const
    {
        if (e == E) {
            return T_;
        }
        return Base::operator[](e);
    }

private:
    T T_;
};

}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_UTIL_ENUM_ARRAY_H_
