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

#include "file_format_version.h"

#include <cstring>
#include <cstdio>

namespace panda::panda_file {

std::string GetVersion(const std::array<uint8_t, File::VERSION_SIZE> &v)
{
    std::string versionstr;
    for (size_t i = 0; i < File::VERSION_SIZE; i++) {
        versionstr += std::to_string(v[i]);
        if (i == (File::VERSION_SIZE - 1)) {
            break;
        }
        versionstr += ".";
    }

    return versionstr;
}

}  // namespace panda::panda_file
