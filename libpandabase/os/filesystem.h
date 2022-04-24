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

#ifndef PANDA_LIBPANDABASE_OS_FILESYSTEM_H_
#define PANDA_LIBPANDABASE_OS_FILESYSTEM_H_

#include "macros.h"
#include <string>

#if defined(PANDA_TARGET_WINDOWS)
#ifndef NAME_MAX
constexpr size_t NAME_MAX = 255;
#endif  // NAME_MAX
#elif defined(USE_STD_FILESYSTEM)
#include <filesystem>
#else
#include <experimental/filesystem>
#endif

namespace panda::os {

std::string GetAbsolutePath(std::string_view path);

bool CreateDirectories(const std::string &folder_name);

}  // namespace panda::os

#endif  // PANDA_LIBPANDABASE_OS_FILESYSTEM_H_
