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

#ifndef PANDA_LIBPANDABASE_OS_UNIX_FILE_H_
#define PANDA_LIBPANDABASE_OS_UNIX_FILE_H_

#include "os/error.h"
#include "utils/expected.h"
#include "utils/logger.h"

#include <array>
#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

namespace panda::os::unix::file {

class File {
public:
    explicit File(int fd) : fd_(fd) {}
    ~File() = default;
    DEFAULT_COPY_SEMANTIC(File);
    DEFAULT_MOVE_SEMANTIC(File);

    Expected<size_t, Error> Read(void *buf, size_t n) const
    {
        ssize_t res = read(fd_, buf, n);
        if (res < 0) {
            return Unexpected(Error(errno));
        }
        return {static_cast<size_t>(res)};
    }

    bool ReadAll(void *buf, size_t n) const
    {
        auto res = Read(buf, n);
        if (res) {
            return res.Value() == n;
        }
        return false;
    }

    Expected<size_t, Error> Write(const void *buf, size_t n) const
    {
        ssize_t res = write(fd_, buf, n);
        if (res < 0) {
            return Unexpected(Error(errno));
        }
        return {static_cast<size_t>(res)};
    }

    bool WriteAll(const void *buf, size_t n) const
    {
        auto res = Write(buf, n);
        if (res) {
            return res.Value() == n;
        }
        return false;
    }

    int Close()
    {
        int res = close(fd_);
        if (LIKELY(res == 0)) {
            fd_ = -1;
        }
        return res;
    }

    Expected<size_t, Error> GetFileSize() const
    {
#if PANDA_TARGET_MACOS
        struct stat st {
        };
        int r = fstat(fd_, &st);
#else
        struct stat64 st {
        };
        int r = fstat64(fd_, &st);
#endif
        if (r == 0) {
            return {static_cast<size_t>(st.st_size)};
        }
        return Unexpected(Error(errno));
    }

    bool IsValid() const
    {
        return fd_ != -1;
    }

    int GetFd() const
    {
        return fd_;
    }

    constexpr static std::string_view GetPathDelim()
    {
        return "/";
    }

    static Expected<std::string, Error> GetTmpPath()
    {
#if defined(PANDA_TARGET_MOBILE)
        return std::string("/data/local/tmp");
#else
        const char *temp = getenv("XDG_RUNTIME_DIR");
        temp = temp != nullptr ? temp : getenv("TMPDIR");
        temp = temp != nullptr ? temp : getenv("TMP");
        temp = temp != nullptr ? temp : getenv("TEMP");
        temp = temp != nullptr ? temp : "/tmp";
        return std::string(temp);
#endif
    }

    static Expected<std::string, Error> GetExecutablePath()
    {
        constexpr size_t BUFFER_SIZE = 1024;
        std::array<char, BUFFER_SIZE> buffer = {0};
        ssize_t len = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
        if (len == -1) {
            return Unexpected(Error(errno));
        }

        std::string::size_type pos = std::string(buffer.data()).find_last_of(File::GetPathDelim());
        return (pos != std::string::npos) ? std::string(buffer.data()).substr(0, pos) : std::string("");
    }

    static Expected<std::string, Error> GetAbsolutePath(std::string_view relative_path)
    {
        std::array<char, PATH_MAX> buffer = {0};
        auto fp = realpath(relative_path.data(), buffer.data());
        if (fp == nullptr) {
            return Unexpected(Error(errno));
        }
        return std::string(fp);
    }

    static bool IsDirectory(const std::string &path)
    {
        return HasStatMode(path, S_IFDIR);
    }

    static bool IsRegularFile(const std::string &path)
    {
        return HasStatMode(path, S_IFREG);
    }

    bool ClearData()
    {
        // Set length
        {
            int rc = ftruncate(fd_, 0);
            if (rc < 0) {
                PLOG(ERROR, RUNTIME) << "Failed to reset the length";
                return false;
            }
        }

        // Move offset
        {
            off_t rc = lseek(fd_, 0, SEEK_SET);
            if (rc == static_cast<off_t>(-1)) {
                PLOG(ERROR, RUNTIME) << "Failed to reset the offset";
                return false;
            }
            return true;
        }
    }

    bool Reset()
    {
        return lseek(fd_, 0, SEEK_SET) == 0;
    }

    bool SetSeek(off_t offset)
    {
        return lseek(fd_, offset, SEEK_SET) >= 0;
    }

    bool SetSeekEnd()
    {
        return lseek(fd_, 0, SEEK_END) == 0;
    }

private:
    int fd_;

    static bool HasStatMode(const std::string &path, uint16_t mode)
    {
        struct stat s = {};
        if (stat(path.c_str(), &s) != 0) {
            return false;
        }
        return static_cast<bool>(s.st_mode & mode);
    }
};

}  // namespace panda::os::unix::file

#endif  // PANDA_LIBPANDABASE_OS_UNIX_FILE_H_
