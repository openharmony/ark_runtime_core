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

#include <cstdlib>
#include <cinttypes>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include "utils/logger.h"

// NOLINTNEXTLINE(modernize-avoid-c-arrays)
static const char PANDA_TRACE_KEY[] = "PANDA_TRACE";
// NOLINTNEXTLINE(modernize-avoid-c-arrays)
static const char TRACE_MARKER_PATH[] = "/sys/kernel/debug/tracing/trace_marker";

namespace panda::trace::internal {

int g_trace_marker_fd = -1;
bool DoInit()
{
    if (g_trace_marker_fd != -1) {
        LOG(ERROR, TRACE) << "Already init.";
        return false;
    }

    const char *panda_trace_val = std::getenv(PANDA_TRACE_KEY);
    if (panda_trace_val == nullptr) {
        return false;
    }

    if (panda_trace_val != std::string("1")) {
        LOG(INFO, TRACE) << "Cannot init, " << PANDA_TRACE_KEY << "=" << panda_trace_val;
        return false;
    }

    // NOLINTNEXTLINE(hicpp-signed-bitwise,cppcoreguidelines-pro-type-vararg)
    g_trace_marker_fd = open(TRACE_MARKER_PATH, O_CLOEXEC | O_WRONLY);
    if (g_trace_marker_fd == -1) {
        PLOG(ERROR, TRACE) << "Cannot open file: " << TRACE_MARKER_PATH;
        return false;
    }

    LOG(INFO, TRACE) << "Trace enabled";
    return true;
}

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define WRITE_MESSAGE(...)                                                                           \
    do {                                                                                             \
        ASSERT(g_trace_marker_fd != -1);                                                             \
        if (UNLIKELY(dprintf(g_trace_marker_fd, __VA_ARGS__) < 0)) {                                 \
            LOG(ERROR, TRACE) << "Cannot write trace event. Try enabling tracing and run app again"; \
        }                                                                                            \
    } while (0)

void DoBeginTracePoint(const char *str)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    WRITE_MESSAGE("B|%d|%s", getpid(), str);
}

void DoEndTracePoint()
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    WRITE_MESSAGE("E|");
}

void DoIntTracePoint(const char *str, int32_t val)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    WRITE_MESSAGE("C|%d|%s|%d", getpid(), str, val);
}

void DoInt64TracePoint(const char *str, int64_t val)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    WRITE_MESSAGE("C|%d|%s|%" PRId64, getpid(), str, val);
}

}  // namespace panda::trace::internal
