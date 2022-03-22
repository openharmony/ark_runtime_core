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

#ifndef PANDA_RUNTIME_INCLUDE_VALUE_INL_H_
#define PANDA_RUNTIME_INCLUDE_VALUE_INL_H_

#include "value.h"

namespace panda {

template <>
inline ObjectHeader *Value::GetAs() const
{
    return IsReference() ? std::get<1>(value_) : nullptr;
}

template <>
inline float Value::GetAs() const
{
    return bit_cast<float>(GetAs<uint32_t>());
}

template <>
inline double Value::GetAs() const
{
    return bit_cast<double>(GetAs<uint64_t>());
}

inline int64_t Value::GetAsLong()
{
    if (IsPrimitive()) {
        return GetAs<int64_t>();
    }
    return reinterpret_cast<int64_t>(GetAs<ObjectHeader *>());
}

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_VALUE_INL_H_
