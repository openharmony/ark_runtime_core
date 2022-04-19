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

#ifndef PANDA_VERIFICATION_UTIL_SATURATED_ENUM_H_
#define PANDA_VERIFICATION_UTIL_SATURATED_ENUM_H_

#include "macros.h"

#include <utility>

namespace panda::verifier {

/*
 possible options:
  1. initial value UNDEFINED and any op will lead to fatal error, add constructors etc for proper initialization
    pros: more safety and robustness to programmer errors, cons: more code, more complexity, etc
  2. initial value is least significant one.
    pros: simplicity, cons: less robust, does not help to detect logic errors in program
*/

template <typename Enum, Enum...>
class SaturatedEnum;

template <typename Enum, Enum E>
class SaturatedEnum<Enum, E> {
public:
    SaturatedEnum &operator=(Enum e)
    {
        value_ = e;
        return *this;
    }

    SaturatedEnum &operator|=(Enum e)
    {
        Set(e);
        return *this;
    }

    bool operator[](Enum e) const
    {
        return Check(e, false);
    }

    operator Enum() const
    {
        return value_;
    }

    template <typename Handler>
    void EnumerateValues(Handler &&handler) const
    {
        Enumerate(std::move(handler), false);
    }

protected:
#ifndef NDEBUG
    bool Check(Enum e, bool prev_set) const
#else
    bool Check([[maybe_unused]] Enum e, bool prev_set) const
#endif
    {
        // to catch missed enum members
        ASSERT(e == E);
        return prev_set || value_ == E;
    }

    void Set(Enum e)
    {
        ASSERT(e == E);
        value_ = e;
    }

    template <typename Handler>
    void Enumerate(Handler &&handler, bool prev_set) const
    {
        prev_set |= value_ == E;
        if (prev_set) {
            handler(E);
        }
    }

    Enum value_ {E};
};

template <typename Enum, Enum E, Enum... Rest>
class SaturatedEnum<Enum, E, Rest...> : public SaturatedEnum<Enum, Rest...> {
    using Base = SaturatedEnum<Enum, Rest...>;

public:
    SaturatedEnum &operator=(Enum e)
    {
        Base::operator=(e);
        return *this;
    }

    SaturatedEnum &operator|=(Enum e)
    {
        Set(e);
        return *this;
    }

    bool operator[](Enum e) const
    {
        return Check(e, false);
    }

    operator Enum() const
    {
        return Base::value_;
    }

    template <typename Handler>
    void EnumerateValues(Handler &&handler) const
    {
        Enumerate(std::move(handler), false);
    }

protected:
    bool Check(Enum e, bool prev_set) const
    {
        prev_set = prev_set || (Base::value_ == E);
        if (e == E) {
            return prev_set;
        }
        return Base::Check(e, prev_set);
    }

    void Set(Enum e)
    {
        if (Base::value_ == E) {
            return;
        }
        if (e == E) {
            Base::operator=(e);
            return;
        }
        Base::Set(e);
    }

    template <typename Handler>
    void Enumerate(Handler &&handler, bool prev_set) const
    {
        prev_set = prev_set || (Base::value_ == E);
        if (prev_set && !handler(E)) {
            return;
        }
        Base::template Enumerate<Handler>(std::move(handler), prev_set);
    }
};

}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_UTIL_SATURATED_ENUM_H_
