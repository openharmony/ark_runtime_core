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

#include "os/file.h"
#include "utils/type_helpers.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>

namespace panda::os::file {

static int GetFlags(Mode mode)
{
    switch (mode) {
        case Mode::READONLY:
            return O_RDONLY;

        case Mode::READWRITE:
            return O_RDWR;

        case Mode::WRITEONLY:
            return O_WRONLY | O_CREAT | O_TRUNC;  // NOLINT(hicpp-signed-bitwise)

        case Mode::READWRITECREATE:
            return O_RDWR | O_CREAT;  // NOLINT(hicpp-signed-bitwise)

        default:
            break;
    }

    UNREACHABLE();
    return 0;
}

File Open(std::string_view filename, Mode mode)
{
    // NOLINTNEXTLINE(hicpp-signed-bitwise)
    const auto PERM = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    return File(open(filename.data(), GetFlags(mode), PERM));
}

}  // namespace panda::os::file
