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

#ifndef PANDA_LIBPANDAFILE_FIELD_DATA_ACCESSOR_H_
#define PANDA_LIBPANDAFILE_FIELD_DATA_ACCESSOR_H_

#include "file.h"
#include "file_items.h"
#include "modifiers.h"
#include "type.h"

#include <variant>
#include <vector>
#include <optional>

namespace panda::panda_file {

class FieldDataAccessor {
public:
    FieldDataAccessor(const File &panda_file, File::EntityId field_id);

    ~FieldDataAccessor() = default;

    NO_COPY_SEMANTIC(FieldDataAccessor);
    NO_MOVE_SEMANTIC(FieldDataAccessor);

    bool IsExternal() const
    {
        return is_external_;
    }

    File::EntityId GetClassId() const
    {
        return File::EntityId(class_off_);
    }

    File::EntityId GetNameId() const
    {
        return File::EntityId(name_off_);
    }

    uint32_t GetType() const
    {
        return type_off_;
    }

    uint32_t GetAccessFlags() const
    {
        return access_flags_;
    }

    bool IsStatic() const
    {
        return (access_flags_ & ACC_STATIC) != 0;
    }

    bool IsVolatile() const
    {
        return (access_flags_ & ACC_VOLATILE) != 0;
    }

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

    bool IsFinal() const
    {
        return (access_flags_ & ACC_FINAL) != 0;
    }

    bool IsTransient() const
    {
        return (access_flags_ & ACC_TRANSIENT) != 0;
    }

    bool IsSynthetic() const
    {
        return (access_flags_ & ACC_SYNTHETIC) != 0;
    }

    bool IsEnum() const
    {
        return (access_flags_ & ACC_ENUM) != 0;
    }

    template <class T>
    std::optional<T> GetValue();

    template <class Callback>
    void EnumerateRuntimeAnnotations(const Callback &cb);

    template <class Callback>
    void EnumerateAnnotations(const Callback &cb);

    size_t GetSize()
    {
        if (size_ == 0) {
            SkipAnnotations();
        }

        return size_;
    }

    const File &GetPandaFile() const
    {
        return panda_file_;
    }

    File::EntityId GetFieldId() const
    {
        return field_id_;
    }

    uint32_t GetAnnotationsNumber();
    uint32_t GetRuntimeAnnotationsNumber();

private:
    using FieldValue = std::variant<uint32_t, uint64_t>;

    std::optional<FieldValue> GetValueInternal();

    void SkipValue();

    void SkipRuntimeAnnotations();

    void SkipAnnotations();

    const File &panda_file_;
    File::EntityId field_id_;

    bool is_external_ {false};

    uint32_t class_off_ {0};
    uint32_t type_off_ {0};
    uint32_t name_off_ {0};
    uint32_t access_flags_ {0};

    Span<const uint8_t> tagged_values_sp_ {nullptr, nullptr};
    Span<const uint8_t> runtime_annotations_sp_ {nullptr, nullptr};
    Span<const uint8_t> annotations_sp_ {nullptr, nullptr};

    size_t size_ {0};
};

}  // namespace panda::panda_file

#endif  // PANDA_LIBPANDAFILE_FIELD_DATA_ACCESSOR_H_
