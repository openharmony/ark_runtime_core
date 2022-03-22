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

#include "class_data_accessor.h"
#include "helpers.h"

#include "utils/leb128.h"
#include "utils/utf.h"

namespace panda::panda_file {

ClassDataAccessor::ClassDataAccessor(const File &panda_file, File::EntityId class_id)
    : panda_file_(panda_file),
      class_id_(class_id),
      name_(),
      super_class_off_(0),
      access_flags_(0),
      num_fields_(0),
      num_methods_(0),
      num_ifaces_(0),
      size_(0)
{
    ASSERT(!panda_file.IsExternal(class_id));
    auto sp = panda_file_.GetSpanFromId(class_id_);
    name_.utf16_length = helpers::ReadULeb128(&sp);
    name_.data = sp.data();
    sp = sp.SubSpan(utf::Mutf8Size(name_.data) + 1);  // + 1 for null byte

    super_class_off_ = helpers::Read<ID_SIZE>(&sp);
    access_flags_ = helpers::ReadULeb128(&sp);
    num_fields_ = helpers::ReadULeb128(&sp);
    num_methods_ = helpers::ReadULeb128(&sp);

    auto tag = static_cast<ClassTag>(sp[0]);
    while (tag != ClassTag::NOTHING && tag < ClassTag::SOURCE_LANG) {
        sp = sp.SubSpan(1);

        if (tag == ClassTag::INTERFACES) {
            num_ifaces_ = helpers::ReadULeb128(&sp);
            ifaces_offsets_sp_ = sp;
            sp = sp.SubSpan(IDX_SIZE * num_ifaces_);
        }

        tag = static_cast<ClassTag>(sp[0]);
    }

    source_lang_sp_ = sp;
    if (tag == ClassTag::NOTHING) {
        annotations_sp_ = sp;
        source_file_sp_ = sp;
        fields_sp_ = sp.SubSpan(TAG_SIZE);  // skip NOTHING tag
    }
}

}  // namespace panda::panda_file
