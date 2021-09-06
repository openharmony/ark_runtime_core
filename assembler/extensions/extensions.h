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

#ifndef PANDA_ASSEMBLER_EXTENSIONS_EXTENSIONS_H_
#define PANDA_ASSEMBLER_EXTENSIONS_EXTENSIONS_H_

#include <memory>
#include <optional>

#include "meta.h"

namespace panda::pandasm::extensions {

enum class Language { ECMASCRIPT, PANDA_ASSEMBLY };

std::optional<Language> LanguageFromString(std::string_view lang);

std::string LanguageToString(const Language &lang);

std::string GetCtorName(Language lang);

std::string GetCctorName(Language lang);

class MetadataExtension {
public:
    static std::unique_ptr<RecordMetadata> CreateRecordMetadata(Language lang);

    static std::unique_ptr<FieldMetadata> CreateFieldMetadata(Language lang);

    static std::unique_ptr<FunctionMetadata> CreateFunctionMetadata(Language lang);

    static std::unique_ptr<ParamMetadata> CreateParamMetadata(Language lang);
};

}  // namespace panda::pandasm::extensions

#endif  // PANDA_ASSEMBLER_EXTENSIONS_EXTENSIONS_H_
