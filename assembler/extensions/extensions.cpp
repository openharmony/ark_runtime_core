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

#include "extensions.h"
#include "ecmascript/ecmascript_meta.h"
#include "macros.h"

namespace panda::pandasm::extensions {

std::optional<Language> LanguageFromString(std::string_view lang)
{
    if (lang == "ECMAScript") {
        return Language::ECMASCRIPT;
    }

    if (lang == "PandaAssembly") {
        return Language::PANDA_ASSEMBLY;
    }

    return {};
}

std::string LanguageToString(const Language &lang)
{
    if (lang == Language::ECMASCRIPT) {
        return "ECMAScript";
    }

    if (lang == Language::PANDA_ASSEMBLY) {
        return "PandaAssembly";
    }

    return {};
}

std::string GetCtorName(Language lang)
{
    switch (lang) {
        case Language::ECMASCRIPT:
            return ".ctor";
        case Language::PANDA_ASSEMBLY:
            return ".ctor";
        default:
            break;
    }

    UNREACHABLE();
    return {};
}

std::string GetCctorName(Language lang)
{
    switch (lang) {
        case Language::ECMASCRIPT:
            return ".cctor";
        case Language::PANDA_ASSEMBLY:
            return ".cctor";
        default:
            break;
    }

    UNREACHABLE();
    return {};
}

/* static */
std::unique_ptr<RecordMetadata> MetadataExtension::CreateRecordMetadata(Language lang)
{
    switch (lang) {
        case Language::ECMASCRIPT:
            return std::make_unique<ecmascript::RecordMetadata>();
        case Language::PANDA_ASSEMBLY:
            return std::make_unique<RecordMetadata>();
        default:
            break;
    }

    UNREACHABLE();
    return {};
}

/* static */
std::unique_ptr<FieldMetadata> MetadataExtension::CreateFieldMetadata(Language lang)
{
    switch (lang) {
        case Language::ECMASCRIPT:
            return std::make_unique<FieldMetadata>();
        case Language::PANDA_ASSEMBLY:
            return std::make_unique<FieldMetadata>();
        default:
            break;
    }

    UNREACHABLE();
    return {};
}

/* static */
std::unique_ptr<FunctionMetadata> MetadataExtension::CreateFunctionMetadata(Language lang)
{
    switch (lang) {
        case Language::ECMASCRIPT:
            return std::make_unique<FunctionMetadata>();
        case Language::PANDA_ASSEMBLY:
            return std::make_unique<FunctionMetadata>();
        default:
            break;
    }

    UNREACHABLE();
    return {};
}

/* static */
std::unique_ptr<ParamMetadata> MetadataExtension::CreateParamMetadata(Language lang)
{
    switch (lang) {
        case Language::ECMASCRIPT:
            return std::make_unique<ParamMetadata>();
        case Language::PANDA_ASSEMBLY:
            return std::make_unique<ParamMetadata>();
        default:
            break;
    }

    UNREACHABLE();
    return {};
}

}  // namespace panda::pandasm::extensions
