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

#include "os/filesystem.h"
#include "macros.h"
#if defined PANDA_TARGET_MOBILE || defined PANDA_TARGET_LINUX || defined PANDA_TARGET_ARM32 || \
    defined PANDA_TARGET_ARM64
#include <sys/stat.h>
#endif
#if defined(PANDA_TARGET_WINDOWS)
#include <fileapi.h>
#endif

namespace panda::os {

bool CreateDirectories(const std::string &folder_name)
{
#ifdef PANDA_TARGET_MOBILE
    constexpr auto DIR_PERMISSIONS = 0777;
    auto res = mkdir(folder_name.c_str(), DIR_PERMISSIONS);
    return res == 0;
#elif PANDA_TARGET_MACOS || PANDA_TARGET_OHOS
    return std::filesystem::create_directories(std::filesystem::path(folder_name));
#elif PANDA_TARGET_WINDOWS
    return CreateDirectory(folder_name.c_str(), NULL);
#else
    constexpr auto DIR_PERMISSIONS = 0777;
    auto res = mkdir(folder_name.c_str(), DIR_PERMISSIONS);
    return res == 0;
#endif
}

}  // namespace panda::os
