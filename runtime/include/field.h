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

#ifndef PANDA_RUNTIME_INCLUDE_FIELD_H_
#define PANDA_RUNTIME_INCLUDE_FIELD_H_

#include <cstdint>
#include <atomic>

#include "intrinsics.h"
#include "libpandafile/file.h"
#include "libpandafile/file_items.h"
#include "libpandafile/modifiers.h"
#include "runtime/include/compiler_interface.h"
namespace panda {

class Class;

class ClassLinkerErrorHandler;

class Field {
public:
    using UniqId = uint64_t;

    Field(Class *klass, const panda_file::File *pf, panda_file::File::EntityId file_id, uint32_t access_flags,
          panda_file::Type type)
        : class_(klass), panda_file_(pf), file_id_(file_id), access_flags_(access_flags), type_(type)
    {
    }

    Class *GetClass() const
    {
        return class_;
    }

    void SetClass(Class *cls)
    {
        class_ = cls;
    }

    static constexpr uint32_t GetClassOffset()
    {
        return MEMBER_OFFSET(Field, class_);
    }

    const panda_file::File *GetPandaFile() const
    {
        return panda_file_;
    }

    panda_file::File::EntityId GetFileId() const
    {
        return file_id_;
    }

    uint32_t GetAccessFlags() const
    {
        return access_flags_;
    }

    uint32_t GetOffset() const
    {
        return offset_;
    }

    void SetOffset(uint32_t offset)
    {
        offset_ = offset;
    }

    static constexpr uint32_t GetOffsetOffset()
    {
        return MEMBER_OFFSET(Field, offset_);
    }

    Class *ResolveTypeClass(ClassLinkerErrorHandler *error_handler = nullptr) const;

    panda_file::Type GetType() const
    {
        return type_;
    }

    panda_file::File::StringData GetName() const;

    bool IsPublic() const
    {
        return (access_flags_ & ACC_PUBLIC) != 0;
    }

    bool IsPrivate() const
    {
        return (access_flags_ & ACC_PRIVATE) != 0;
    }

    bool IsProtected() const
    {
        return (access_flags_ & ACC_PROTECTED) != 0;
    }

    bool IsStatic() const
    {
        return (access_flags_ & ACC_STATIC) != 0;
    }

    bool IsVolatile() const
    {
        return (access_flags_ & ACC_VOLATILE) != 0;
    }

    bool IsFinal() const
    {
        return (access_flags_ & ACC_FINAL) != 0;
    }

    static inline UniqId CalcUniqId(const panda_file::File *file, panda_file::File::EntityId file_id)
    {
        constexpr uint64_t HALF = 32ULL;
        uint64_t uid = file->GetFilenameHash();
        uid <<= HALF;
        uid |= file_id.GetOffset();
        return uid;
    }

    UniqId GetUniqId() const
    {
        return CalcUniqId(panda_file_, file_id_);
    }

    ~Field() = default;

    NO_COPY_SEMANTIC(Field);
    NO_MOVE_SEMANTIC(Field);

private:
    Class *class_;
    const panda_file::File *panda_file_;
    panda_file::File::EntityId file_id_;
    uint32_t access_flags_;
    panda_file::Type type_;
    uint32_t offset_ {0};
};

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_FIELD_H_
