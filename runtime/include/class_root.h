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

#ifndef PANDA_RUNTIME_INCLUDE_CLASS_ROOT_H_
#define PANDA_RUNTIME_INCLUDE_CLASS_ROOT_H_

namespace panda {

enum class ClassRoot {
    V = 0,
    U1,
    I8,
    U8,
    I16,
    U16,
    I32,
    U32,
    I64,
    U64,
    F32,
    F64,
    TAGGED,
    ARRAY_U1,
    ARRAY_I8,
    ARRAY_U8,
    ARRAY_I16,
    ARRAY_U16,
    ARRAY_I32,
    ARRAY_U32,
    ARRAY_I64,
    ARRAY_U64,
    ARRAY_F32,
    ARRAY_F64,
    ARRAY_TAGGED,
    CLASS,
    OBJECT,
    STRING,
    ARRAY_CLASS,
    ARRAY_STRING,
    LAST_CLASS_ROOT_ENTRY = ARRAY_STRING  // Must be the last in this enum
};

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_CLASS_ROOT_H_
