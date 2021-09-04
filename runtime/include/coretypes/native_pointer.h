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

#ifndef PANDA_RUNTIME_INCLUDE_CORETYPES_NATIVE_POINTER_H_
#define PANDA_RUNTIME_INCLUDE_CORETYPES_NATIVE_POINTER_H_

#include "runtime/include/language_context.h"
#include "runtime/include/object_header.h"

namespace panda::coretypes {

// Used for the requirement of ACE that wants to associated a registered C++ resource with a JSObject.
class NativePointer : public ObjectHeader {
public:
    inline void *GetExternalPointer() const
    {
        return external_pointer_;
    }

    inline void SetExternalPointer(void *external_pointer)
    {
        external_pointer_ = external_pointer;
    }

    static NativePointer *Cast(ObjectHeader *object)
    {
        return static_cast<NativePointer *>(object);
    }

    static constexpr uint32_t GetExternalPointerOffset()
    {
        return MEMBER_OFFSET(NativePointer, external_pointer_);
    }

    ~NativePointer() = default;
    DEFAULT_COPY_SEMANTIC(NativePointer);
    DEFAULT_MOVE_SEMANTIC(NativePointer);

private:
    NativePointer() : ObjectHeader() {}

    void *external_pointer_ {nullptr};
};

}  // namespace panda::coretypes

#endif  // PANDA_RUNTIME_INCLUDE_CORETYPES_NATIVE_POINTER_H_
