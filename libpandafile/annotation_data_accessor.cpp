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

#include "annotation_data_accessor.h"
#include "file_items.h"

namespace panda::panda_file {

AnnotationDataAccessor::AnnotationDataAccessor(const File &panda_file, File::EntityId annotation_id)
    : panda_file_(panda_file), annotation_id_(annotation_id)
{
    auto sp = panda_file_.GetSpanFromId(annotation_id_);
    auto class_idx = helpers::Read<IDX_SIZE>(&sp);
    class_off_ = panda_file_.ResolveClassIndex(annotation_id_, class_idx).GetOffset();
    count_ = helpers::Read<COUNT_SIZE>(&sp);
    size_ = ID_SIZE + COUNT_SIZE + count_ * (ID_SIZE + VALUE_SIZE) + count_ * TYPE_TAG_SIZE;

    elements_sp_ = sp;
    elements_tags_ = sp.SubSpan(count_ * (ID_SIZE + VALUE_SIZE));
}

AnnotationDataAccessor::Elem AnnotationDataAccessor::GetElement(size_t i) const
{
    ASSERT(i < count_);
    auto sp = elements_sp_.SubSpan(i * (ID_SIZE + VALUE_SIZE));
    uint32_t name = helpers::Read<ID_SIZE>(&sp);
    uint32_t value = helpers::Read<VALUE_SIZE>(&sp);
    return AnnotationDataAccessor::Elem(panda_file_, File::EntityId(name), value);
}

AnnotationDataAccessor::Tag AnnotationDataAccessor::GetTag(size_t i) const
{
    ASSERT(i < count_);
    auto sp = elements_tags_.SubSpan(i * TYPE_TAG_SIZE);
    char item = helpers::Read<TYPE_TAG_SIZE>(&sp);
    return AnnotationDataAccessor::Tag(item);
}

}  // namespace panda::panda_file
