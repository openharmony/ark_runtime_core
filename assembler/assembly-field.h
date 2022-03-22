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

#ifndef PANDA_ASSEMBLER_ASSEMBLY_FIELD_H_
#define PANDA_ASSEMBLER_ASSEMBLY_FIELD_H_

#include <memory>
#include <string>

#include "assembly-type.h"
#include "extensions/extensions.h"
#include "meta.h"

namespace panda::pandasm {

struct Field {
    Type type;
    std::string name;
    std::unique_ptr<FieldMetadata> metadata;
    size_t line_of_def = 0;
    std::string whole_line = ""; /* The line in which the field is defined */
                                 /*  Or line in which the field is met, if the field is not defined */
    size_t bound_left = 0;
    size_t bound_right = 0;
    bool is_defined = true;

    explicit Field(extensions::Language lang) : metadata(extensions::MetadataExtension::CreateFieldMetadata(lang)) {}
};

}  // namespace panda::pandasm

#endif  // PANDA_ASSEMBLER_ASSEMBLY_FIELD_H_
