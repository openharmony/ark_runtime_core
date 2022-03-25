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

#ifndef PANDA_VERIFICATION_UTIL_FLAGS_H_
#define PANDA_VERIFICATION_UTIL_FLAGS_H_

#include "macros.h"

#include <cstdint>

namespace panda::verifier {
template <typename UInt, typename Enum, Enum...>
class FlagsForEnum;

template <typename UInt, typename Enum, Enum Flag>
class FlagsForEnum<UInt, Enum, Flag> {
public:
    class ConstBit {
    public:
        ConstBit(UInt bit_mask, const UInt &given_flags) : mask {bit_mask}, flags {given_flags} {};
        ConstBit() = delete;
        ConstBit(const ConstBit &) = delete;
        ConstBit(ConstBit &&) = delete;
        ~ConstBit() = default;
        operator bool() const
        {
            return (flags & mask) != 0;
        }

    protected:
        const UInt mask;
        const UInt &flags;
    };

    class Bit : public ConstBit {
    public:
        Bit(UInt bit_mask, UInt &given_flags) : ConstBit {bit_mask, given_flags} {};
        ~Bit() = default;
        Bit &operator=(bool b)
        {
            UInt &proper_flags = const_cast<UInt &>(ConstBit::flags);
            if (b) {
                proper_flags |= ConstBit::mask;
            } else {
                proper_flags &= ~ConstBit::mask;
            }
            return *this;
        }
    };

    template <typename Handler>
    void EnumerateFlags(Handler &&handler) const
    {
        if (ConstBit {mask, flags_} == true) {
            handler(Flag);
        }
    }

#ifndef NDEBUG
    ConstBit operator[](Enum f) const
    {
        ASSERT(f == Flag);
        return {mask, flags_};
    }

    Bit operator[](Enum f)
    {
        ASSERT(f == Flag);
        return {mask, flags_};
    }
#else
    ConstBit operator[](Enum /* unused */) const
    {
        return {mask, flags_};
    }

    Bit operator[](Enum /* unused */)
    {
        return {mask, flags_};
    }
#endif

protected:
    constexpr static UInt mask = static_cast<UInt>(1);
    UInt flags_ {0};
};

template <typename UInt, typename Enum, Enum Flag, Enum... Rest>
class FlagsForEnum<UInt, Enum, Flag, Rest...> : public FlagsForEnum<UInt, Enum, Rest...> {
    using Base = FlagsForEnum<UInt, Enum, Rest...>;

public:
    typename Base::ConstBit operator[](Enum f) const
    {
        if (f == Flag) {
            return {mask, Base::flags_};
        }
        return Base::operator[](f);
    }

    typename Base::Bit operator[](Enum f)
    {
        if (f == Flag) {
            return {mask, Base::flags_};
        }
        return Base::operator[](f);
    }

    template <typename Handler>
    void EnumerateFlags(Handler &&handler) const
    {
        if (typename Base::ConstBit {mask, Base::flags_} == true && !handler(Flag)) {
            return;
        }
        Base::template EnumerateFlags<Handler>(std::move(handler));
    }

protected:
    constexpr static UInt mask = Base::mask << static_cast<UInt>(1);
    static_assert(mask != 0, "too many flags for UInt size");
};
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_UTIL_FLAGS_H_
