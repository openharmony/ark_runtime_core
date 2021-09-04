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

#ifndef PANDA_LIBPANDAFILE_CLASS_DATA_ACCESSOR_H_
#define PANDA_LIBPANDAFILE_CLASS_DATA_ACCESSOR_H_

#include "file.h"
#include "file_items.h"
#include "field_data_accessor.h"
#include "method_data_accessor.h"

#include <optional>

namespace panda::panda_file {

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions, hicpp-special-member-functions)
class ClassDataAccessor {
public:
    ClassDataAccessor(const File &panda_file, File::EntityId class_id);

    ~ClassDataAccessor() = default;

    File::EntityId GetSuperClassId() const
    {
        return File::EntityId(super_class_off_);
    }

    bool IsInterface() const
    {
        return (access_flags_ & ACC_INTERFACE) != 0;
    }

    uint32_t GetAccessFlags() const
    {
        return access_flags_;
    }

    uint32_t GetFieldsNumber() const
    {
        return num_fields_;
    }

    uint32_t GetMethodsNumber() const
    {
        return num_methods_;
    }

    uint32_t GetIfacesNumber() const
    {
        return num_ifaces_;
    }

    uint32_t GetAnnotationsNumber();

    uint32_t GetRuntimeAnnotationsNumber();

    File::EntityId GetInterfaceId(size_t idx) const;

    template <class Callback>
    void EnumerateInterfaces(const Callback &cb);

    template <class Callback>
    void EnumerateRuntimeAnnotations(const Callback &cb);

    template <class Callback>
    void EnumerateAnnotations(const Callback &cb);

    std::optional<SourceLang> GetSourceLang();

    std::optional<File::EntityId> GetSourceFileId();

    template <class Callback>
    void EnumerateFields(const Callback &cb);

    template <class Callback>
    void EnumerateMethods(const Callback &cb);

    size_t GetSize()
    {
        if (size_ == 0) {
            SkipMethods();
        }

        return size_;
    }

    const File &GetPandaFile() const
    {
        return panda_file_;
    }

    File::EntityId GetClassId() const
    {
        return class_id_;
    }

    const uint8_t *GetDescriptor() const
    {
        return name_.data;
    }

private:
    void SkipSourceLang();

    void SkipRuntimeAnnotations();

    void SkipAnnotations();

    void SkipSourceFile();

    void SkipFields();

    void SkipMethods();

    const File &panda_file_;
    File::EntityId class_id_;

    File::StringData name_;
    uint32_t super_class_off_;
    uint32_t access_flags_;
    uint32_t num_fields_;
    uint32_t num_methods_;
    uint32_t num_ifaces_;

    Span<const uint8_t> ifaces_offsets_sp_ {nullptr, nullptr};
    Span<const uint8_t> source_lang_sp_ {nullptr, nullptr};
    Span<const uint8_t> runtime_annotations_sp_ {nullptr, nullptr};
    Span<const uint8_t> annotations_sp_ {nullptr, nullptr};
    Span<const uint8_t> source_file_sp_ {nullptr, nullptr};
    Span<const uint8_t> fields_sp_ {nullptr, nullptr};
    Span<const uint8_t> methods_sp_ {nullptr, nullptr};

    size_t size_;
};

}  // namespace panda::panda_file

#endif  // PANDA_LIBPANDAFILE_CLASS_DATA_ACCESSOR_H_
