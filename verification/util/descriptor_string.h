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

#ifndef PANDA_VERIFICATION_UTIL_DESCRIPTOR_STRING_H_
#define PANDA_VERIFICATION_UTIL_DESCRIPTOR_STRING_H_

#include "libpandabase/utils/hash.h"
#include "libpandabase/utils/utf.h"

#include "macros.h"

#include <cstring>
#include <type_traits>

namespace panda::verifier {

namespace mode {

struct ExactCmp {
};
struct NonExactCmp {
};
}  // namespace mode

// NB!: non-owning wrapper for mutf8 strings
template <typename Mode = mode::ExactCmp>
class DescriptorString {
    static_assert(std::is_same_v<Mode, mode::ExactCmp> || std::is_same_v<Mode, mode::NonExactCmp>);

public:
    DescriptorString(const uint8_t *mutf8_str)
        : hash_ {PseudoFnvHashString(mutf8_str)},
          mutf8_str_len_ {std::strlen(utf::Mutf8AsCString(mutf8_str))},
          mutf8_str_ {mutf8_str}
    {
    }

    DescriptorString() = default;
    DescriptorString(const DescriptorString &other) = default;
    DescriptorString(DescriptorString &&) = default;
    DescriptorString &operator=(const DescriptorString &) = default;
    DescriptorString &operator=(DescriptorString &&) = default;

    bool operator==(const DescriptorString &rhs) const
    {
        ASSERT(IsValid());
        if (mutf8_str_ == rhs.mutf8_str_) {
            ASSERT(hash_ == rhs.hash_ && mutf8_str_len_ == rhs.mutf8_str_len_);
            return true;
        }
        if (hash_ != rhs.hash_) {
            ASSERT(mutf8_str_ != rhs.mutf8_str_);
            return false;
        }
        if (mutf8_str_len_ != rhs.mutf8_str_len_) {
            return false;
        }
        if constexpr (std::is_same_v<Mode, mode::ExactCmp>) {
            return utf::IsEqual(mutf8_str_, rhs.mutf8_str_);
        }
        // NB! Regarding mode::NonExactCmp
        // Assumption: probability of different strings with same len and hash is very low
        // Use it only if there is the need for speed
        return true;
    }

    bool operator!=(const DescriptorString &rhs) const
    {
        return !operator==(rhs);
    }

    operator const uint8_t *() const
    {
        return AsMutf8();
    }

    const uint8_t *AsMutf8() const
    {
        ASSERT(IsValid());
        return mutf8_str_;
    }

    bool IsValid() const
    {
        return mutf8_str_ != nullptr;
    }

    size_t GetLength() const
    {
        return mutf8_str_len_;
    }

private:
    uint32_t hash_ = 0;
    size_t mutf8_str_len_ = 0;
    const uint8_t *mutf8_str_ = nullptr;

    template <typename T>
    friend struct std::hash;
};

template <typename Mode>
std::ostream &operator<<(std::ostream &os, const DescriptorString<Mode> &str)
{
    return os << utf::Mutf8AsCString(str.AsMutf8());
}

}  // namespace panda::verifier

namespace std {

template <typename Mode>
struct hash<panda::verifier::DescriptorString<Mode>> {
    size_t operator()(const panda::verifier::DescriptorString<Mode> &desc_str) const noexcept
    {
        return static_cast<size_t>(desc_str.hash_);
    }
};

}  // namespace std

#endif  // PANDA_VERIFICATION_UTIL_DESCRIPTOR_STRING_H_
