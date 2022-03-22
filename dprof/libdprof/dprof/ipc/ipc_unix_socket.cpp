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

#include "ipc_unix_socket.h"

#include "os/unix/failure_retry.h"
#include "utils/logger.h"
#include "securec.h"

#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace panda::dprof::ipc {
constexpr char SOCKET_NAME[] = "\0dprof.socket";  // NOLINT(modernize-avoid-c-arrays)
static_assert(sizeof(SOCKET_NAME) <= sizeof(static_cast<sockaddr_un *>(nullptr)->sun_path), "Socket name too large");

os::unix::UniqueFd CreateUnixServerSocket(int backlog)
{
    os::unix::UniqueFd sock(PANDA_FAILURE_RETRY(::socket(AF_UNIX, SOCK_STREAM, 0)));

    int opt = 1;
    if (PANDA_FAILURE_RETRY(::setsockopt(sock.Get(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) == -1) {
        PLOG(ERROR, DPROF) << "setsockopt() failed";
        return os::unix::UniqueFd();
    }

    struct sockaddr_un serverAddr {
    };
    if (memset_s(&serverAddr, sizeof(serverAddr), 0, sizeof(serverAddr)) != EOK) {
        PLOG(ERROR, DPROF) << "CreateUnixServerSocket memset_s failed";
        UNREACHABLE();
    }
    serverAddr.sun_family = AF_UNIX;
    if (memcpy_s(serverAddr.sun_path, sizeof(SOCKET_NAME), SOCKET_NAME, sizeof(SOCKET_NAME)) != EOK) {
        PLOG(ERROR, DPROF) << "CreateUnixServerSocket memcpy_s failed";
        UNREACHABLE();
    }
    if (PANDA_FAILURE_RETRY(::bind(sock.Get(), reinterpret_cast<struct sockaddr *>(&serverAddr), sizeof(serverAddr))) ==
        -1) {
        PLOG(ERROR, DPROF) << "bind() failed";
        return os::unix::UniqueFd();
    }

    if (::listen(sock.Get(), backlog) == -1) {
        PLOG(ERROR, DPROF) << "listen() failed";
        return os::unix::UniqueFd();
    }

    return sock;
}

os::unix::UniqueFd CreateUnixClientSocket()
{
    os::unix::UniqueFd sock(PANDA_FAILURE_RETRY(::socket(AF_UNIX, SOCK_STREAM, 0)));
    if (!sock.IsValid()) {
        PLOG(ERROR, DPROF) << "socket() failed";
        return os::unix::UniqueFd();
    }

    struct sockaddr_un serverAddr {
    };
    if (memset_s(&serverAddr, sizeof(serverAddr), 0, sizeof(serverAddr)) != EOK) {
        PLOG(ERROR, DPROF) << "CreateUnixClientSocket memset_s failed";
        UNREACHABLE();
    }
    serverAddr.sun_family = AF_UNIX;
    if (memcpy_s(serverAddr.sun_path, sizeof(SOCKET_NAME), SOCKET_NAME, sizeof(SOCKET_NAME)) != EOK) {
        PLOG(ERROR, DPROF) << "CreateUnixClientSocket memcpy_s failed";
        UNREACHABLE();
    }
    if (PANDA_FAILURE_RETRY(
            ::connect(sock.Get(), reinterpret_cast<struct sockaddr *>(&serverAddr), sizeof(serverAddr))) == -1) {
        PLOG(ERROR, DPROF) << "connect() failed";
        return os::unix::UniqueFd();
    }

    return sock;
}

bool SendAll(int fd, const void *buf, int len)
{
    const char *p = reinterpret_cast<const char *>(buf);
    int total = 0;
    while (total < len) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        int n = PANDA_FAILURE_RETRY(::send(fd, p + total, len, 0));
        if (n == -1) {
            PLOG(ERROR, DPROF) << "send() failed";
            return false;
        }
        total += n;
        len -= n;
    }
    return true;
}

bool WaitDataTimeout(int fd, int timeoutMs)
{
    struct pollfd pfd {
    };
    pfd.fd = fd;
    pfd.events = POLLIN;

    int rc = PANDA_FAILURE_RETRY(::poll(&pfd, 1, timeoutMs));
    if (rc == 1) {
        // Success
        return true;
    }
    if (rc == -1) {
        // Error
        PLOG(ERROR, DPROF) << "poll() failed";
        return false;
    }
    if (rc == 0) {
        // Timeout
        LOG(ERROR, DPROF) << "Timeout, cannot recv data";
        return false;
    }

    UNREACHABLE();
    return false;
}

int RecvTimeout(int fd, void *buf, int len, int timeoutMs)
{
    if (!WaitDataTimeout(fd, timeoutMs)) {
        LOG(ERROR, DPROF) << "Cannot get access to data";
        return -1;
    }

    int n = PANDA_FAILURE_RETRY(::recv(fd, buf, len, 0));
    if (n == -1) {
        PLOG(ERROR, DPROF) << "Cannot recv data, len=" << len;
        return -1;
    }
    if (n == 0) {
        // socket was closed
        return 0;
    }
    if (n != len) {
        LOG(ERROR, DPROF) << "Cannot recv data id, len=" << len << " n=" << n;
        return -1;
    }

    return len;
}
}  // namespace panda::dprof::ipc
