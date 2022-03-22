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

#ifndef PANDA_LIBPANDAFILE_CLASS_DATA_ACCESSOR_INL_H_
#define PANDA_LIBPANDAFILE_CLASS_DATA_ACCESSOR_INL_H_

#include "class_data_accessor.h"
#include "field_data_accessor-inl.h"
#include "method_data_accessor-inl.h"

#include "helpers.h"

namespace panda::panda_file {

inline void ClassDataAccessor::SkipSourceLang()
{
    GetSourceLang();
}

inline void ClassDataAccessor::SkipRuntimeAnnotations()
{
    EnumerateRuntimeAnnotations([](File::EntityId /* unused */) {});
}

inline void ClassDataAccessor::SkipAnnotations()
{
    EnumerateAnnotations([](File::EntityId /* unused */) {});
}

inline void ClassDataAccessor::SkipSourceFile()
{
    GetSourceFileId();
}

inline void ClassDataAccessor::SkipFields()
{
    EnumerateFields([](const FieldDataAccessor & /* unused */) {});
}

inline void ClassDataAccessor::SkipMethods()
{
    EnumerateMethods([](const MethodDataAccessor & /* unused */) {});
}

template <class Callback>
inline void ClassDataAccessor::EnumerateInterfaces(const Callback &cb)
{
    auto sp = ifaces_offsets_sp_;

    for (size_t i = 0; i < num_ifaces_; i++) {
        auto index = helpers::Read<IDX_SIZE>(&sp);
        cb(panda_file_.ResolveClassIndex(class_id_, index));
    }
}

inline File::EntityId ClassDataAccessor::GetInterfaceId(size_t idx) const
{
    ASSERT(idx < num_ifaces_);
    auto sp = ifaces_offsets_sp_.SubSpan(idx * IDX_SIZE);
    auto index = helpers::Read<IDX_SIZE>(&sp);
    return panda_file_.ResolveClassIndex(class_id_, index);
}

inline std::optional<SourceLang> ClassDataAccessor::GetSourceLang()
{
    return helpers::GetOptionalTaggedValue<SourceLang>(source_lang_sp_, ClassTag::SOURCE_LANG,
                                                       &runtime_annotations_sp_);
}

template <class Callback>
inline void ClassDataAccessor::EnumerateRuntimeAnnotations(const Callback &cb)
{
    if (runtime_annotations_sp_.data() == nullptr) {
        SkipSourceLang();
    }

    helpers::EnumerateTaggedValues<File::EntityId, ClassTag, Callback>(
        runtime_annotations_sp_, ClassTag::RUNTIME_ANNOTATION, cb, &annotations_sp_);
}

template <class Callback>
inline void ClassDataAccessor::EnumerateAnnotations(const Callback &cb)
{
    if (annotations_sp_.data() == nullptr) {
        SkipRuntimeAnnotations();
    }

    helpers::EnumerateTaggedValues<File::EntityId, ClassTag, Callback>(annotations_sp_, ClassTag::ANNOTATION, cb,
                                                                       &source_file_sp_);
}

inline std::optional<File::EntityId> ClassDataAccessor::GetSourceFileId()
{
    if (source_file_sp_.data() == nullptr) {
        SkipAnnotations();
    }

    auto v = helpers::GetOptionalTaggedValue<File::EntityId>(source_file_sp_, ClassTag::SOURCE_FILE, &fields_sp_);

    fields_sp_ = fields_sp_.SubSpan(TAG_SIZE);  // NOTHING tag

    return v;
}

template <class Callback, class Accessor>
static void EnumerateClassElements(const File &pf, Span<const uint8_t> sp, size_t elem_num, const Callback &cb,
                                   Span<const uint8_t> *next)
{
    for (size_t i = 0; i < elem_num; i++) {
        File::EntityId id = pf.GetIdFromPointer(sp.data());
        Accessor data_accessor(pf, id);
        cb(data_accessor);
        sp = sp.SubSpan(data_accessor.GetSize());
    }

    *next = sp;
}

template <class Callback>
inline void ClassDataAccessor::EnumerateFields(const Callback &cb)
{
    if (fields_sp_.data() == nullptr) {
        SkipSourceFile();
    }

    EnumerateClassElements<Callback, FieldDataAccessor>(panda_file_, fields_sp_, num_fields_, cb, &methods_sp_);
}

template <class Callback>
inline void ClassDataAccessor::EnumerateMethods(const Callback &cb)
{
    if (methods_sp_.data() == nullptr) {
        SkipFields();
    }

    Span<const uint8_t> sp {nullptr, nullptr};
    EnumerateClassElements<Callback, MethodDataAccessor>(panda_file_, methods_sp_, num_methods_, cb, &sp);

    size_ = panda_file_.GetIdFromPointer(sp.data()).GetOffset() - class_id_.GetOffset();
}

inline uint32_t ClassDataAccessor::GetAnnotationsNumber()
{
    size_t n = 0;
    EnumerateAnnotations([&n](File::EntityId /* unused */) { n++; });
    return n;
}

inline uint32_t ClassDataAccessor::GetRuntimeAnnotationsNumber()
{
    size_t n = 0;
    EnumerateRuntimeAnnotations([&n](File::EntityId /* unused */) { n++; });
    return n;
}

}  // namespace panda::panda_file

#endif  // PANDA_LIBPANDAFILE_CLASS_DATA_ACCESSOR_INL_H_
