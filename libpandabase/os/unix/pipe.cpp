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

#include "pipe.h"

#include "os/failure_retry.h"

#include <vector>
#include <array>
#include <cerrno>

#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

namespace panda::os::unix {

std::pair<UniqueFd, UniqueFd> CreatePipe()
{
    constexpr size_t FD_NUM = 2;
    std::array<int, FD_NUM> fds {};
    // NOLINTNEXTLINE(android-cloexec-pipe)
    if (PANDA_FAILURE_RETRY(pipe(fds.data())) == -1) {
        return std::pair<UniqueFd, UniqueFd>();
    }
    return std::pair<UniqueFd, UniqueFd>(UniqueFd(fds[0]), UniqueFd(fds[1]));
}

int SetFdNonblocking(const UniqueFd &fd)
{
    size_t flags;
#ifdef O_NONBLOCK
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    int res = fcntl(fd.Get(), F_GETFL, 0);
    if (res < 0) {
        flags = 0;
    } else {
        flags = static_cast<size_t>(res);
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg, hicpp-signed-bitwise)
    return fcntl(fd.Get(), F_SETFL, flags | O_NONBLOCK);
#else
    flags = 1;
    return ioctl(fd, FIONBIO, &flags);
#endif
}

Expected<size_t, Error> ReadFromPipe(const UniqueFd &pipe_fd, void *buf, size_t size)
{
    ssize_t bytes_read = PANDA_FAILURE_RETRY(read(pipe_fd.Get(), buf, size));
    if (bytes_read < 0) {
        return Unexpected(Error(errno));
    }
    return {static_cast<size_t>(bytes_read)};
}

Expected<size_t, Error> WriteToPipe(const UniqueFd &pipe_fd, const void *buf, size_t size)
{
    ssize_t bytes_written = PANDA_FAILURE_RETRY(write(pipe_fd.Get(), buf, size));
    if (bytes_written < 0) {
        return Unexpected(Error(errno));
    }
    return {static_cast<size_t>(bytes_written)};
}

Expected<size_t, Error> WaitForEvent(const UniqueFd *handles, size_t size, EventType type)
{
    uint16_t poll_events;
    switch (type) {
        case EventType::READY:
            poll_events = POLLIN;
            break;

        default:
            return Unexpected(Error("Unknown event type"));
    }

    // Initialize poll set
    std::vector<pollfd> pollfds(size);
    for (size_t i = 0; i < size; i++) {
        pollfds[i].fd = handles[i].Get();  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        pollfds[i].events = static_cast<int16_t>(poll_events);
    }

    while (true) {
        int res = PANDA_FAILURE_RETRY(poll(pollfds.data(), size, -1));
        if (res == -1) {
            return Unexpected(Error(errno));
        }

        for (size_t i = 0; i < size; i++) {
            if ((static_cast<size_t>(pollfds[i].revents) & poll_events) == poll_events) {
                return {i};
            }
        }
    }
}

std::optional<Error> Dup2(const UniqueFd &source, const UniqueFd &target)
{
    if (!source.IsValid()) {
        return Error("Source fd is invalid");
    }
    if (PANDA_FAILURE_RETRY(dup2(source.Get(), target.Get())) == -1) {
        return Error(errno);
    }
    return {};
}

}  // namespace panda::os::unix
