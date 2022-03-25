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

#ifndef PANDA_RUNTIME_INCLUDE_CORETYPES_STRING_INL_H_
#define PANDA_RUNTIME_INCLUDE_CORETYPES_STRING_INL_H_

#include <type_traits>

#include "runtime/include/coretypes/string.h"
#include "runtime/include/exceptions.h"
#include "runtime/include/runtime.h"

namespace panda::coretypes {

template <bool verify>
inline uint16_t String::At(int32_t index)
{
    int32_t length = GetLength();
    if (verify) {
        if ((index < 0) || (index >= length)) {
            panda::ThrowStringIndexOutOfBoundsException(index, length);
            return 0;
        }
    }
    if (!IsUtf16()) {
        Span<uint8_t> sp(GetDataMUtf8(), length);
        return sp[index];
    }
    Span<uint16_t> sp(GetDataUtf16(), length);
    return sp[index];
}

}  // namespace panda::coretypes

#endif  // PANDA_RUNTIME_INCLUDE_CORETYPES_STRING_INL_H_
