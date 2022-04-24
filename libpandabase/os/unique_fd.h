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

#ifndef PANDA_LIBPANDABASE_OS_UNIQUE_FD_H_
#define PANDA_LIBPANDABASE_OS_UNIQUE_FD_H_

#ifdef PANDA_TARGET_UNIX
#include "os/unix/unique_fd.h"
#elif PANDA_TARGET_WINDOWS
#include "os/windows/unique_fd.h"
#else
#error "Unsupported platform"
#endif  // PANDA_TARGET_UNIX

#include "os/failure_retry.h"
#include "utils/logger.h"

#include <unistd.h>

namespace panda::os::unique_fd {
class UniqueFd {
public:
    explicit UniqueFd(int fd = -1) noexcept
    {
        Reset(fd);
    }

    UniqueFd(const UniqueFd &other_fd) = delete;
    UniqueFd &operator=(const UniqueFd &other_fd) = delete;

    UniqueFd(UniqueFd &&other_fd) noexcept
    {
        Reset(other_fd.Release());
    }

    UniqueFd &operator=(UniqueFd &&other_fd) noexcept
    {
        Reset(other_fd.Release());
        return *this;
    }

    ~UniqueFd()
    {
        Reset();
    }

    int Release() noexcept
    {
        int fd = fd_;
        fd_ = -1;
        return fd;
    }

    void Reset(int new_fd = -1)
    {
        if (fd_ != -1) {
            ASSERT(new_fd != fd_);
            DefaultCloser(fd_);
        }
        fd_ = new_fd;
    }

    int Get() const noexcept
    {
        return fd_;
    }

    bool IsValid() const noexcept
    {
        return fd_ != -1;
    }

private:
    static void DefaultCloser(int fd)
    {
        LOG_IF(PANDA_FAILURE_RETRY(::close(fd)) != 0, FATAL, COMMON) << "Incorrect fd: " << fd;
    }

    int fd_ = -1;
};
}  // namespace panda::os::unique_fd

#endif  // PANDA_LIBPANDABASE_OS_UNIQUE_FD_H_
