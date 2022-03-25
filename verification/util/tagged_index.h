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

#ifndef PANDA_VERIFICATION_UTIL_TAGGED_INDEX_H_
#define PANDA_VERIFICATION_UTIL_TAGGED_INDEX_H_

#include <limits>
#include <type_traits>

#include "macros.h"
#include "utils/bit_utils.h"

#include "index.h"

namespace panda::verifier {

template <typename Tag, typename Int = size_t>
class TaggedIndex {
    using UInt = std::make_unsigned_t<Int>;
    static constexpr size_t UIntBits = sizeof(UInt) * 8ULL;
    static constexpr size_t TagBits = UIntBits - panda::Clz(static_cast<UInt>(Tag::__LAST__) + static_cast<UInt>(1));
    static constexpr size_t IntBits = UIntBits - TagBits;
    static constexpr UInt VALUE_MASK = (static_cast<UInt>(1) << IntBits) - static_cast<UInt>(1);
    static constexpr size_t VALUE_SIGN_BIT = (static_cast<UInt>(1) << (IntBits - static_cast<size_t>(1)));
    static constexpr UInt MAX_VALUE = VALUE_MASK;
    static constexpr size_t TAG_SHIFT = IntBits;
    static constexpr UInt TAG_MASK = ((static_cast<UInt>(1) << TagBits) - static_cast<UInt>(1)) << TAG_SHIFT;
    static constexpr UInt INVALID = TAG_MASK;

public:
    TaggedIndex() = default;

    TaggedIndex(Tag tag, Int val)
    {
        SetTag(tag);
        SetInt(val);
        ASSERT(IsValid());
    }

    void SetInt(Int val)
    {
        ASSERT(IsValid());  // tag should be set before value
        if constexpr (std::is_signed_v<Int>) {
            if (val < 0) {
                ASSERT(static_cast<UInt>(-val) <= MAX_VALUE >> static_cast<UInt>(1));
            } else {
                ASSERT(static_cast<UInt>(val) <= MAX_VALUE >> static_cast<UInt>(1));
            }
        } else {
            ASSERT(static_cast<UInt>(val) <= MAX_VALUE);
        }
        Value_ &= ~VALUE_MASK;
        Value_ |= (static_cast<UInt>(val) & VALUE_MASK);
    }

    TaggedIndex &operator=(Int val)
    {
        SetInt(val);
        return *this;
    }

    void SetTag(Tag tag)
    {
        Value_ &= VALUE_MASK;
        Value_ |= static_cast<UInt>(tag) << TAG_SHIFT;
    }

    TaggedIndex &operator=(Tag tag)
    {
        SetTag(tag);
        return *this;
    }

    TaggedIndex(const TaggedIndex &) = default;

    TaggedIndex(TaggedIndex &&idx) : Value_ {idx.Value_}
    {
        idx.Invalidate();
    }

    TaggedIndex &operator=(const TaggedIndex &) = default;

    TaggedIndex &operator=(TaggedIndex &&idx)
    {
        Value_ = idx.Value_;
        idx.Invalidate();
        return *this;
    }

    ~TaggedIndex() = default;

    void Invalidate()
    {
        Value_ = INVALID;
    }

    bool IsValid() const
    {
        return Value_ != INVALID;
    }

    Tag GetTag() const
    {
        ASSERT(IsValid());
        return static_cast<Tag>(Value_ >> TAG_SHIFT);
    }

    Int GetInt() const
    {
        ASSERT(IsValid());
        UInt val = Value_ & VALUE_MASK;
        Int ival;
        if constexpr (std::is_signed_v<Int>) {
            if (val & VALUE_SIGN_BIT) {
                val |= TAG_MASK;  // sign-extend
                ival = static_cast<Int>(val);
            } else {
                ival = static_cast<Int>(val);
            }
        } else {
            ival = static_cast<Int>(val);
        }
        return ival;
    }

    Index<Int> GetIndex() const
    {
        if (IsValid()) {
            return GetInt();
        }
        return {};
    }

    operator Index<Int>() const
    {
        return GetIndex();
    }

    template <const Int INV>
    Index<Int, INV> GetIndex() const
    {
        ASSERT(static_cast<UInt>(INV) > MAX_VALUE);
        if (IsValid()) {
            return GetInt();
        }
        return {};
    }

    template <const Int INV>
    operator Index<Int, INV>() const
    {
        return GetIndex<INV>();
    }

    operator Int() const
    {
        ASSERT(IsValid());
        return GetInt();
    }

    operator Tag() const
    {
        ASSERT(IsValid());
        return GetTag();
    }

    bool operator==(const TaggedIndex rhs) const
    {
        ASSERT(IsValid());
        ASSERT(rhs.IsValid());
        return GetTag() == rhs.GetTag() && GetInt() == rhs.GetInt();
    }

    bool operator!=(const TaggedIndex rhs) const
    {
        ASSERT(IsValid());
        ASSERT(rhs.IsValid());
        return GetTag() != rhs.GetTag() || GetInt() != rhs.GetInt();
    }

private:
    UInt Value_ {INVALID};
    template <typename T>
    friend struct std::hash;
};

}  // namespace panda::verifier

namespace std {

template <typename Tag, typename Int>
struct hash<panda::verifier::TaggedIndex<Tag, Int>> {
    size_t operator()(const panda::verifier::TaggedIndex<Tag, Int> &i) const noexcept
    {
        return static_cast<size_t>(i.Value_);
    }
};

}  // namespace std

#endif  // PANDA_VERIFICATION_UTIL_TAGGED_INDEX_H_
