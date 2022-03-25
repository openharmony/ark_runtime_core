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

#include "runtime/include/itable_builder.h"

#include "runtime/java/java_itable_builder.h"

namespace panda {

/* static */
PandaUniquePtr<ITableBuilder> ITableBuilder::CreateITableBuilder(LanguageContext ctx)
{
    // Currently only Java semantic is supported
    switch (ctx.GetLanguage()) {
        case panda_file::SourceLang::JAVA_8:
        case panda_file::SourceLang::PANDA_ASSEMBLY:
        case panda_file::SourceLang::ECMASCRIPT:
            return MakePandaUnique<JavaITableBuilder>();
        default:
            UNREACHABLE();
    }
}

}  // namespace panda
