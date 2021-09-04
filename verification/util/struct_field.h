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

#ifndef PANDA_VERIFICATION_UTIL_STRUCT_FIELD_H_
#define PANDA_VERIFICATION_UTIL_STRUCT_FIELD_H_

namespace panda::verifier {
template <typename S, typename T>
struct struct_field {
    size_t offset;
    struct_field(size_t p_offst) : offset {p_offst} {}
    T &of(S &s) const
    {
        return *reinterpret_cast<T *>(reinterpret_cast<uintptr_t>(&s) + offset);
    }
};
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_UTIL_STRUCT_FIELD_H_
