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

#ifndef PANDA_LIBPANDAFILE_METHOD_DATA_ACCESSOR_H_
#define PANDA_LIBPANDAFILE_METHOD_DATA_ACCESSOR_H_

#include "file.h"
#include "file_items.h"
#include "modifiers.h"

namespace panda::panda_file {

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions, hicpp-special-member-functions)
class MethodDataAccessor {
public:
    MethodDataAccessor(const File &panda_file, File::EntityId method_id);

    ~MethodDataAccessor() = default;

    bool IsExternal() const
    {
        return is_external_;
    }

    bool IsStatic() const
    {
        return (access_flags_ & ACC_STATIC) != 0;
    }

    bool IsAbstract() const
    {
        return (access_flags_ & ACC_ABSTRACT) != 0;
    }

    bool IsNative() const
    {
        return (access_flags_ & ACC_NATIVE) != 0;
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

    bool IsSynthetic() const
    {
        return (access_flags_ & ACC_SYNTHETIC) != 0;
    }

    File::EntityId GetClassId() const
    {
        return File::EntityId(class_off_);
    }

    File::Index GetClassIdx() const
    {
        return class_idx_;
    }

    File::EntityId GetNameId() const
    {
        return File::EntityId(name_off_);
    };

    File::EntityId GetProtoId() const
    {
        return File::EntityId(proto_off_);
    }

    uint32_t GetAccessFlags() const
    {
        return access_flags_;
    }

    std::optional<File::EntityId> GetCodeId();

    std::optional<SourceLang> GetSourceLang();

    template <class Callback>
    void EnumerateRuntimeAnnotations(Callback cb);

    template <typename Callback>
    void EnumerateTypesInProto(Callback cb);

    std::optional<File::EntityId> GetRuntimeParamAnnotationId();

    std::optional<File::EntityId> GetDebugInfoId();

    template <class Callback>
    void EnumerateAnnotations(Callback cb);

    std::optional<File::EntityId> GetParamAnnotationId();

    size_t GetSize()
    {
        if (size_ == 0) {
            SkipParamAnnotation();
        }

        return size_;
    }

    const File &GetPandaFile() const
    {
        return panda_file_;
    }

    File::EntityId GetMethodId() const
    {
        return method_id_;
    }

    uint32_t GetAnnotationsNumber();
    uint32_t GetRuntimeAnnotationsNumber();

    uint32_t GetNumericalAnnotation(uint32_t field_id);

private:
    void SkipCode();

    void SkipSourceLang();

    void SkipRuntimeAnnotations();

    void SkipRuntimeParamAnnotation();

    void SkipDebugInfo();

    void SkipAnnotations();

    void SkipParamAnnotation();

    const File &panda_file_;
    File::EntityId method_id_;

    bool is_external_ {false};

    uint16_t class_idx_ {0};
    uint16_t proto_idx_ {0};
    uint32_t class_off_ {0};
    uint32_t proto_off_ {0};
    uint32_t name_off_ {0};
    uint32_t access_flags_ {0};

    Span<const uint8_t> tagged_values_sp_ {nullptr, nullptr};
    Span<const uint8_t> source_lang_sp_ {nullptr, nullptr};
    Span<const uint8_t> runtime_annotations_sp_ {nullptr, nullptr};
    Span<const uint8_t> runtime_param_annotation_sp_ {nullptr, nullptr};
    Span<const uint8_t> debug_sp_ {nullptr, nullptr};
    Span<const uint8_t> annotations_sp_ {nullptr, nullptr};
    Span<const uint8_t> param_annotation_sp_ {nullptr, nullptr};

    size_t size_ {0};
};

}  // namespace panda::panda_file

#endif  // PANDA_LIBPANDAFILE_METHOD_DATA_ACCESSOR_H_
