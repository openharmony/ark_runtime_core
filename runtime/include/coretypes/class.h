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

#ifndef PANDA_RUNTIME_INCLUDE_CORETYPES_CLASS_H_
#define PANDA_RUNTIME_INCLUDE_CORETYPES_CLASS_H_

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "libpandabase/utils/utf.h"
#include "runtime/include/class.h"
#include "runtime/include/object_header.h"

namespace panda::coretypes {

class Class : public ObjectHeader {
public:
    Class(const uint8_t *descriptor, uint32_t vtable_size, uint32_t imt_size, uint32_t klass_size)
        : ObjectHeader(), klass_(descriptor, panda_file::SourceLang::PANDA_ASSEMBLY, vtable_size, imt_size, klass_size)
    {
    }

    // We shouldn't init header_ here - because it has been memset(0) in object allocation,
    // otherwise it may cause data race while visiting object's class concurrently in gc.
    void InitClass(const uint8_t *descriptor, uint32_t vtable_size, uint32_t imt_size, uint32_t klass_size)
    {
        // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
        new (&klass_)
            panda::Class(descriptor, panda_file::SourceLang::PANDA_ASSEMBLY, vtable_size, imt_size, klass_size);
    }

    panda::Class *GetRuntimeClass()
    {
        return &klass_;
    }

    const panda::Class *GetRuntimeClass() const
    {
        return &klass_;
    }

    template <class T>
    T GetFieldPrimitive(const Field &field) const
    {
        return klass_.GetFieldPrimitive<T>(field);
    }

    template <class T>
    void SetFieldPrimitive(const Field &field, T value)
    {
        klass_.SetFieldPrimitive(field, value);
    }

    template <bool need_read_barrier = true>
    ObjectHeader *GetFieldObject(const Field &field) const
    {
        return klass_.GetFieldObject<need_read_barrier>(field);
    }

    template <bool need_write_barrier = true>
    void SetFieldObject(const Field &field, ObjectHeader *value)
    {
        klass_.SetFieldObject<need_write_barrier>(field, value);
    }

    static size_t GetSize(uint32_t klass_size)
    {
        return GetRuntimeClassOffset() + klass_size;
    }

    static constexpr size_t GetRuntimeClassOffset()
    {
        return MEMBER_OFFSET(Class, klass_);
    }

    static Class *FromRuntimeClass(panda::Class *klass)
    {
        return reinterpret_cast<Class *>(reinterpret_cast<uintptr_t>(klass) - GetRuntimeClassOffset());
    }

    ~Class() = default;

    NO_COPY_SEMANTIC(Class);
    NO_MOVE_SEMANTIC(Class);

private:
    panda::Class klass_;
};

// Klass field has variable size so it must be the last
static_assert(Class::GetRuntimeClassOffset() + sizeof(panda::Class) == sizeof(Class));

}  // namespace panda::coretypes

#endif  // PANDA_RUNTIME_INCLUDE_CORETYPES_CLASS_H_
