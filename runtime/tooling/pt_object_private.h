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

#ifndef PANDA_RUNTIME_TOOLING_PT_OBJECT_PRIVATE_H_
#define PANDA_RUNTIME_TOOLING_PT_OBJECT_PRIVATE_H_

#include "runtime/include/tooling/pt_object.h"
#include "pt_reference_private.h"
#include "macros.h"

namespace panda::tooling {
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
class PtScopedObjectPrivate {
public:
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    explicit PtScopedObjectPrivate(ObjectHeader *objectHeader)
    {
        ASSERT(objectHeader != nullptr);
        PtLocalReference *localRef = PtCreateLocalReference(objectHeader);
        object_ = PtObject(localRef);
    }

    ~PtScopedObjectPrivate()
    {
        PtDestroyLocalReference(reinterpret_cast<PtLocalReference *>(object_.GetReference()));
    }

    PtObject GetObject() const
    {
        return object_;
    }

    NO_COPY_SEMANTIC(PtScopedObjectPrivate);
    NO_MOVE_SEMANTIC(PtScopedObjectPrivate);

private:
    PtObject object_ {nullptr};
};

}  // namespace panda::tooling

#endif  // PANDA_RUNTIME_TOOLING_PT_OBJECT_PRIVATE_H_
