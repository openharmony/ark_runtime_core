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

#ifndef PANDA_LIBPANDAFILE_ARK_VERSION_H_
#define PANDA_LIBPANDAFILE_ARK_VERSION_H_

#include "file.h"

namespace panda::panda_file {
constexpr std::array<uint8_t, File::VERSION_SIZE> version {0, 0, 0, 1};

constexpr std::array<uint8_t, File::VERSION_SIZE> minVersion {0, 0, 0, 1};

std::string GetVersion(const std::array<uint8_t, File::VERSION_SIZE> &version);

}  // namespace panda::panda_file

#endif  // PANDA_LIBPANDAFILE_ARK_VERSION_H_
