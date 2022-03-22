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

#ifndef PANDA_LIBPANDABASE_OS_FILE_H_
#define PANDA_LIBPANDABASE_OS_FILE_H_

#include "macros.h"

#if defined(PANDA_TARGET_UNIX)
#include "os/unix/file.h"
#elif defined(PANDA_TARGET_WINDOWS)
#include "os/windows/file.h"
#else
#error "Unsupported platform"
#endif

#include <string>

namespace panda::os::file {

#if defined(PANDA_TARGET_UNIX)
using File = panda::os::unix::file::File;
#elif defined(PANDA_TARGET_WINDOWS)
using File = panda::os::windows::file::File;
#endif

class FileHolder {
public:
    explicit FileHolder(File file) : file_(file) {}

    ~FileHolder()
    {
        file_.Close();
    }

private:
    File file_;

    NO_COPY_SEMANTIC(FileHolder);
    NO_MOVE_SEMANTIC(FileHolder);
};

enum class Mode : uint32_t { READONLY, WRITEONLY, READWRITE, READWRITECREATE };

File Open(std::string_view filename, Mode mode);

}  // namespace panda::os::file

#endif  // PANDA_LIBPANDABASE_OS_FILE_H_
