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

#ifndef PANDA_RUNTIME_INCLUDE_VALUE_H_
#define PANDA_RUNTIME_INCLUDE_VALUE_H_

#include <cstdarg>
#include <cstdint>
#include <type_traits>
#include <variant>

#include "libpandafile/file_items.h"

namespace panda {

class ObjectHeader;

class Value {
public:
    template <class T>
    explicit Value(T value)
    {
        // Disable checks due to clang-tidy bug https://bugs.llvm.org/show_bug.cgi?id=32203
        // NOLINTNEXTLINE(readability-braces-around-statements, hicpp-braces-around-statements, bugprone-branch-clone)
        if constexpr (std::is_integral_v<T>) {
            value_ = static_cast<int64_t>(value);
            // NOLINTNEXTLINE(readability-braces-around-statements, readability-misleading-indentation)
        } else if constexpr (std::is_same_v<T, double>) {
            value_ = bit_cast<int64_t>(value);
            // NOLINTNEXTLINE(readability-braces-around-statements, readability-misleading-indentation)
        } else if constexpr (std::is_same_v<T, float>) {
            value_ = bit_cast<int32_t>(value);
        } else {  // NOLINTNEXTLINE(readability-misleading-indentation)
            value_ = value;
        }
    }

    Value(int64_t value, int64_t tag) : value_(DecodedTaggedValue(value, tag)) {}

    template <class T>
    T GetAs() const
    {
        static_assert(std::is_integral_v<T>, "T must be integral type");
        ASSERT(IsPrimitive());
        return static_cast<T>(std::get<0>(value_));
    }

    int64_t GetAsLong();

    DecodedTaggedValue GetDecodedTaggedValue() const
    {
        return IsDecodedTaggedValue() ? std::get<2>(value_) : DecodedTaggedValue(0, 0);
    }

    bool IsReference() const
    {
        return std::holds_alternative<ObjectHeader *>(value_);
    }

    bool IsPrimitive() const
    {
        return std::holds_alternative<int64_t>(value_);
    }

    bool IsDecodedTaggedValue() const
    {
        return std::holds_alternative<DecodedTaggedValue>(value_);
    }

    ObjectHeader **GetGCRoot()
    {
        ASSERT(IsReference());
        return &std::get<ObjectHeader *>(value_);
    }

private:
    std::variant<int64_t, ObjectHeader *, DecodedTaggedValue> value_;
};

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_VALUE_H_
