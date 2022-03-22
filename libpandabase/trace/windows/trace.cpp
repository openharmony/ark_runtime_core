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

#include <cstdint>
#include "macros.h"
#include "utils/logger.h"

namespace panda::trace::internal {

int g_trace_marker_fd = -1;

bool DoInit()
{
    LOG(ERROR, TRACE) << "Tracing not implemented for this platform.";
    return false;
}

void DoBeginTracePoint([[maybe_unused]] const char *str)
{
    UNREACHABLE();
}

void DoEndTracePoint()
{
    UNREACHABLE();
}

void DoIntTracePoint([[maybe_unused]] const char *str, [[maybe_unused]] int32_t val)
{
    UNREACHABLE();
}

void DoInt64TracePoint([[maybe_unused]] const char *str, [[maybe_unused]] int64_t val)
{
    UNREACHABLE();
}

}  // namespace panda::trace::internal
