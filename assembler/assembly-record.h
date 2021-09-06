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

#ifndef PANDA_ASSEMBLER_ASSEMBLY_RECORD_H_
#define PANDA_ASSEMBLER_ASSEMBLY_RECORD_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "assembly-field.h"
#include "assembly-file-location.h"
#include "extensions/extensions.h"
#include "ide_helpers.h"

namespace panda::pandasm {

struct Record {
    std::string name = "";
    bool conflict = false; /* Name conflicts with panda primitive types. Need special handle. */
    extensions::Language language;
    std::unique_ptr<RecordMetadata> metadata;
    std::vector<Field> field_list; /* class fields list */
    size_t params_num = 0;
    bool body_presence = false;
    SourceLocation body_location;
    std::string source_file; /* The file in which the record is defined or empty */
    std::optional<FileLocation> file_location;

    Record(std::string s, extensions::Language lang, size_t b_l, size_t b_r, std::string f_c, bool d, size_t l_n)
        : name(std::move(s)),
          language(lang),
          metadata(extensions::MetadataExtension::CreateRecordMetadata(lang)),
          file_location({f_c, b_l, b_r, l_n, d})
    {
    }

    Record(std::string s, extensions::Language lang)
        : name(std::move(s)), language(lang), metadata(extensions::MetadataExtension::CreateRecordMetadata(lang))
    {
    }

    bool HasImplementation() const
    {
        return !metadata->IsForeign();
    }
};

}  // namespace panda::pandasm

#endif  // PANDA_ASSEMBLER_ASSEMBLY_RECORD_H_
