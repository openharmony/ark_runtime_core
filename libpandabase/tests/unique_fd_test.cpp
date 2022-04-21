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

#include "os/unique_fd.h"

#include <gtest/gtest.h>
#include <utility>
#include <unistd.h>

namespace panda::os::unique_fd {

enum testValue { DEFAULT_VALUE = -1, STDIN_VALUE, STDOUT_VALUE, STDERR_VALUE };

struct DuplicateFD {
    int stdinValue = ::dup(STDIN_VALUE);
    int stdoutValue = ::dup(STDOUT_VALUE);
    int stferrValue = ::dup(STDERR_VALUE);
};

TEST(UniqueFd, Construct)
{
    DuplicateFD dupDF;
    auto fd_a = UniqueFd();
    auto fd_b = UniqueFd(dupDF.stdinValue);
    auto fd_c = UniqueFd(dupDF.stdoutValue);
    auto fd_d = UniqueFd(dupDF.stferrValue);

    EXPECT_EQ(fd_a.Get(), DEFAULT_VALUE);
    EXPECT_EQ(fd_b.Get(), dupDF.stdinValue);
    EXPECT_EQ(fd_c.Get(), dupDF.stdoutValue);
    EXPECT_EQ(fd_d.Get(), dupDF.stferrValue);

    auto fd_e = std::move(fd_a);
    auto fd_f = std::move(fd_b);
    auto fd_g = std::move(fd_c);
    auto fd_h = std::move(fd_d);

    EXPECT_EQ(fd_a.Get(), DEFAULT_VALUE);
    EXPECT_EQ(fd_b.Get(), DEFAULT_VALUE);
    EXPECT_EQ(fd_c.Get(), DEFAULT_VALUE);
    EXPECT_EQ(fd_d.Get(), DEFAULT_VALUE);
    EXPECT_EQ(fd_e.Get(), DEFAULT_VALUE);
    EXPECT_EQ(fd_f.Get(), dupDF.stdinValue);
    EXPECT_EQ(fd_g.Get(), dupDF.stdoutValue);
    EXPECT_EQ(fd_h.Get(), dupDF.stferrValue);
}

TEST(UniqueFd, Equal)
{
    DuplicateFD dupDF;
    auto fd_a = UniqueFd();
    auto fd_b = UniqueFd(dupDF.stdinValue);
    auto fd_c = UniqueFd(dupDF.stdoutValue);
    auto fd_d = UniqueFd(dupDF.stferrValue);

    auto fd_e = UniqueFd();
    auto fd_f = UniqueFd();
    auto fd_g = UniqueFd();
    auto fd_h = UniqueFd();
    fd_e = std::move(fd_a);
    fd_f = std::move(fd_b);
    fd_g = std::move(fd_c);
    fd_h = std::move(fd_d);

    EXPECT_EQ(fd_a.Get(), DEFAULT_VALUE);
    EXPECT_EQ(fd_b.Get(), DEFAULT_VALUE);
    EXPECT_EQ(fd_c.Get(), DEFAULT_VALUE);
    EXPECT_EQ(fd_d.Get(), DEFAULT_VALUE);
    EXPECT_EQ(fd_e.Get(), DEFAULT_VALUE);
    EXPECT_EQ(fd_f.Get(), dupDF.stdinValue);
    EXPECT_EQ(fd_g.Get(), dupDF.stdoutValue);
    EXPECT_EQ(fd_h.Get(), dupDF.stferrValue);
}

TEST(UniqueFd, Release)
{
    DuplicateFD dupDF;
    auto fd_a = UniqueFd();
    auto fd_b = UniqueFd(dupDF.stdinValue);
    auto fd_c = UniqueFd(dupDF.stdoutValue);
    auto fd_d = UniqueFd(dupDF.stferrValue);

    auto num_a = fd_a.Release();
    auto num_b = fd_b.Release();
    auto num_c = fd_c.Release();
    auto num_d = fd_d.Release();

    EXPECT_EQ(fd_a.Get(), DEFAULT_VALUE);
    EXPECT_EQ(fd_b.Get(), DEFAULT_VALUE);
    EXPECT_EQ(fd_c.Get(), DEFAULT_VALUE);
    EXPECT_EQ(fd_d.Get(), DEFAULT_VALUE);
    EXPECT_EQ(num_a, DEFAULT_VALUE);
    EXPECT_EQ(num_b, dupDF.stdinValue);
    EXPECT_EQ(num_c, dupDF.stdoutValue);
    EXPECT_EQ(num_d, dupDF.stferrValue);
}

TEST(UniqueFd, Reset)
{
    DuplicateFD dupDF;

    auto num_a = DEFAULT_VALUE;
    auto num_b = dupDF.stdinValue;
    auto num_c = dupDF.stdoutValue;
    auto num_d = dupDF.stferrValue;

    auto fd_a = UniqueFd();
    auto fd_b = UniqueFd();
    auto fd_c = UniqueFd();
    auto fd_d = UniqueFd();

    fd_a.Reset(num_a);
    fd_b.Reset(num_b);
    fd_c.Reset(num_c);
    fd_d.Reset(num_d);

    EXPECT_EQ(fd_a.Get(), DEFAULT_VALUE);
    EXPECT_EQ(fd_b.Get(), dupDF.stdinValue);
    EXPECT_EQ(fd_c.Get(), dupDF.stdoutValue);
    EXPECT_EQ(fd_d.Get(), dupDF.stferrValue);
}

}  // namespace panda::os::unique_fd
