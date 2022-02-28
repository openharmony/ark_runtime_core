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

#ifndef PANDA_RUNTIME_MEM_VM_HANDLE_H_
#define PANDA_RUNTIME_MEM_VM_HANDLE_H_

#include "libpandabase/macros.h"
#include "runtime/handle_base.h"
#include "runtime/include/managed_thread.h"
#include "runtime/handle_scope.h"

namespace panda {

using TaggedType = coretypes::TaggedType;
using TaggedValue = coretypes::TaggedValue;

// VMHandle should be used in language-agnostic part of runtime
template <typename T>
class VMHandle : public HandleBase {
public:
    inline explicit VMHandle() : HandleBase(reinterpret_cast<uintptr_t>(nullptr)) {}

    explicit VMHandle(ManagedThread *thread, ObjectHeader *object)
    {
        if (object != nullptr) {
            address_ = thread->GetTopScope<ObjectHeader *>()->NewHandle(object);
        } else {
            address_ = reinterpret_cast<uintptr_t>(nullptr);
        }
    }

    ~VMHandle() = default;

    NO_COPY_SEMANTIC(VMHandle);
    DEFAULT_NOEXCEPT_MOVE_SEMANTIC(VMHandle);

    T *GetPtr() const
    {
        if (address_ == reinterpret_cast<uintptr_t>(nullptr)) {
            return nullptr;
        }
        return *(reinterpret_cast<T **>(GetAddress()));
    }

    explicit operator T *() const
    {
        return GetPtr();
    }

    T *operator->() const
    {
        return GetPtr();
    }
};

}  // namespace panda

#endif  // PANDA_RUNTIME_MEM_VM_HANDLE_H_
