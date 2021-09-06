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

#ifndef PANDA_VERIFICATION_UTIL_ABSTRACT_INDEX_H_
#define PANDA_VERIFICATION_UTIL_ABSTRACT_INDEX_H_

#include "index.h"

#include <limits>

namespace panda::verifier {
template <typename Int, typename Friend>
class AbstractIndex : private Index<Int> {
    using Base = Index<Int>;

public:
    AbstractIndex() = default;
    AbstractIndex(const AbstractIndex &) = default;
    AbstractIndex(AbstractIndex &&) = default;
    AbstractIndex &operator=(const AbstractIndex &) = default;
    AbstractIndex &operator=(AbstractIndex &&) = default;
    ~AbstractIndex() = default;

    bool IsValid() const
    {
        return Base::IsValid();
    }

    bool operator==(const AbstractIndex &rhs) const
    {
        return static_cast<Int>(static_cast<const Base &>(*this)) == static_cast<Int>(static_cast<const Base &>(rhs));
    }

    bool operator!=(const AbstractIndex &rhs) const
    {
        return !operator==(rhs);
    }

    bool operator<(const AbstractIndex &rhs) const
    {
        return static_cast<Int>(static_cast<const Base &>(*this)) < static_cast<Int>(static_cast<const Base &>(rhs));
    }

    bool operator<=(const AbstractIndex &rhs) const
    {
        return static_cast<Int>(static_cast<const Base &>(*this)) <= static_cast<Int>(static_cast<const Base &>(rhs));
    }

private:
    AbstractIndex(Int val) : Base {val} {}

    AbstractIndex &operator=(Int val)
    {
        Base::operator=(val);
        return *this;
    }

    void Invalidate()
    {
        Base::Invalidate();
    }

    operator Int() const
    {
        return Base::operator Int();
    }

    template <typename T>
    friend struct std::hash;

    friend Friend;
};
}  // namespace panda::verifier

namespace std {
template <typename Int, typename Friend>
struct hash<panda::verifier::AbstractIndex<Int, Friend>> {
    size_t operator()(const panda::verifier::AbstractIndex<Int, Friend> &i) const noexcept
    {
        return static_cast<size_t>(i.operator Int());
    }
};
}  // namespace std

#endif  // PANDA_VERIFICATION_UTIL_ABSTRACT_INDEX_H_
