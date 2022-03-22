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

#ifndef PANDA_VERIFICATION_TYPE_TYPE_SYSTEM_KIND_H_
#define PANDA_VERIFICATION_TYPE_TYPE_SYSTEM_KIND_H_

namespace panda::verifier {
enum class TypeSystemKind {
    PANDA,
    JAVA_0,
    JAVA_1,
    JAVA_2,
    JAVA_3,
    JAVA_4,
    JAVA_5,
    JAVA_6,
    JAVA_7,
    JAVA_8,
    JAVA_9,
    JAVA_10,
    JAVA_11,
    JAVA_12,
    JAVA_13,
    JAVA_14,
    JAVA_15,
    __LAST__ = JAVA_15
};

}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_TYPE_TYPE_SYSTEM_KIND_H_
