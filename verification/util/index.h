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

#ifndef PANDA_VERIFICATION_UTIL_INDEX_H_
#define PANDA_VERIFICATION_UTIL_INDEX_H_

#include "macros.h"

#include <limits>

namespace panda::verifier {
// similar to std::optional, but much more effective for numeric types
// when not all range of values is required and some value may be used
// as dedicated invalid value
template <typename Int, const Int INVALID = std::numeric_limits<Int>::max()>
class Index {
public:
    Index() : Value_ {INVALID} {}
    Index(Int val) : Value_ {val}
    {
        ASSERT(IsValid());
    }
    Index &operator=(Int val)
    {
        Value_ = val;
        ASSERT(IsValid());
        return *this;
    }
    Index(const Index &) = default;
    Index(Index &&idx) : Value_ {idx.Value_}
    {
        idx.Invalidate();
    }
    Index &operator=(const Index &) = default;
    Index &operator=(Index &&idx)
    {
        Value_ = idx.Value_;
        idx.Invalidate();
        return *this;
    }
    ~Index() = default;

    bool operator==(const Index &other)
    {
        return Value_ == other.Value_;
    };
    bool operator!=(const Index &other)
    {
        return !(*this == other);
    };

    void Invalidate()
    {
        Value_ = INVALID;
    }

    bool IsValid() const
    {
        return Value_ != INVALID;
    }

    // for contextual conversion in if/while/etc.
    explicit operator bool() const
    {
        return IsValid();
    }

    operator Int() const
    {
        ASSERT(IsValid());
        return Value_;
    }

    Int operator*() const
    {
        ASSERT(IsValid());
        return Value_;
    }

    template <typename T>
    explicit operator T() const
    {
        ASSERT(IsValid());
        return static_cast<T>(Value_);
    }

private:
    Int Value_;
    template <typename T>
    friend struct std::hash;
};
}  // namespace panda::verifier

namespace std {
template <typename Int, const Int I>
struct hash<panda::verifier::Index<Int, I>> {
    size_t operator()(const panda::verifier::Index<Int, I> &i) const noexcept
    {
        return static_cast<size_t>(i.Value_);
    }
};
}  // namespace std

#endif  // PANDA_VERIFICATION_UTIL_INDEX_H_
