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

#ifndef PANDA_LIBPANDABASE_OS_LIBRARY_LOADER_H_
#define PANDA_LIBPANDABASE_OS_LIBRARY_LOADER_H_

#ifdef PANDA_TARGET_UNIX
#include "os/unix/library_loader.h"
#else
#error "Unsupported platform"
#endif

#include "os/error.h"
#include "utils/expected.h"

#include <string_view>

namespace panda::os::library_loader {

#ifdef PANDA_TARGET_UNIX
using LibraryHandle = panda::os::unix::library_loader::LibraryHandle;
#endif

Expected<LibraryHandle, Error> Load(std::string_view filename);

Expected<void *, Error> ResolveSymbol(const LibraryHandle &handle, std::string_view name);

}  // namespace panda::os::library_loader

#endif  // PANDA_LIBPANDABASE_OS_LIBRARY_LOADER_H_
