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

#ifndef PANDA_RUNTIME_INCLUDE_CORETYPES_DYN_OBJECTS_H_
#define PANDA_RUNTIME_INCLUDE_CORETYPES_DYN_OBJECTS_H_

#include "runtime/include/hclass.h"
#include "runtime/include/coretypes/array.h"
#include "runtime/include/coretypes/string.h"
#include "runtime/include/coretypes/tagged_value.h"
#include "runtime/include/mem/panda_containers.h"
#include "runtime/include/object_header.h"
#include "runtime/mem/vm_handle.h"

namespace panda::coretypes {
// Class for objects in dynamic languages like JavaScript
class DynClass : public ObjectHeader {
public:
    static DynClass *Cast(ObjectHeader *object)
    {
        ASSERT(object->ClassAddr<HClass>()->IsDynamicClass());
        return reinterpret_cast<DynClass *>(object);
    }

    HClass *GetHClass()
    {
        return reinterpret_cast<HClass *>(klass_);
    }

private:
    DynClass() : ObjectHeader() {}
    ~DynClass() = default;
    DEFAULT_COPY_SEMANTIC(DynClass);
    DEFAULT_MOVE_SEMANTIC(DynClass);

    // Actually HClass is located here.
    __extension__ uint8_t klass_[0];  // NOLINT(modernize-avoid-c-arrays)
};

class DynObject : public ObjectHeader {
public:
    static DynObject *Cast(ObjectHeader *object)
    {
        ASSERT(object->ClassAddr<HClass>()->IsDynamicClass());
        return reinterpret_cast<DynObject *>(object);
    }

    void SetBuiltinsCtorMode()
    {
        ClassAddr<HClass>()->SetBuiltinsCtorMode();
    }

    bool IsBuiltinsConstructor()
    {
        return ClassAddr<HClass>()->IsBuiltinsConstructor();
    }

    void SetCallable(bool flag)
    {
        ClassAddr<HClass>()->SetCallable(flag);
    }

    bool IsCallable()
    {
        return ClassAddr<HClass>()->IsCallable();
    }
};

}  // namespace panda::coretypes

#endif  // PANDA_RUNTIME_INCLUDE_CORETYPES_DYN_OBJECTS_H_
