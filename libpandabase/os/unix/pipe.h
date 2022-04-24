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

#ifndef PANDA_LIBPANDABASE_OS_UNIX_PIPE_H_
#define PANDA_LIBPANDABASE_OS_UNIX_PIPE_H_

#include "utils/expected.h"
#include "os/error.h"
#include "os/unique_fd.h"

#include <utility>
#include <optional>

namespace panda::os::unix {

using UniqueFd = panda::os::unique_fd::UniqueFd;

std::pair<UniqueFd, UniqueFd> CreatePipe();

int SetFdNonblocking(const UniqueFd &fd);

Expected<size_t, Error> ReadFromPipe(const UniqueFd &pipe_fd, void *buf, size_t size);

Expected<size_t, Error> WriteToPipe(const UniqueFd &pipe_fd, const void *buf, size_t size);

enum class EventType { READY };

Expected<size_t, Error> WaitForEvent(const UniqueFd *handles, size_t size, EventType type);

std::optional<Error> Dup2(const UniqueFd &source, const UniqueFd &target);

}  // namespace panda::os::unix

#endif  // PANDA_LIBPANDABASE_OS_UNIX_PIPE_H_
