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

#ifndef PANDA_RUNTIME_INCLUDE_TOOLING_PT_OBJECT_H_
#define PANDA_RUNTIME_INCLUDE_TOOLING_PT_OBJECT_H_

#include "runtime/include/tooling/pt_reference.h"
#include "libpandabase/macros.h"

namespace panda::tooling {
class PtObject {
public:
    explicit PtObject(PtReference *ref = nullptr) : ref_(ref) {}

    PtReference *GetReference() const
    {
        return ref_;
    }

    ~PtObject() = default;

    DEFAULT_COPY_SEMANTIC(PtObject);
    DEFAULT_MOVE_SEMANTIC(PtObject);

private:
    PtReference *ref_;
};
}  // namespace panda::tooling

#endif  // PANDA_RUNTIME_INCLUDE_TOOLING_PT_OBJECT_H_
