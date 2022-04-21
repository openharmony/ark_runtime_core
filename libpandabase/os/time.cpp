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

#include "os/time.h"

namespace panda::os::time {
/**
 *  Return current time in nanoseconds
 */
uint64_t GetClockTimeInMicro()
{
    return GetClockTime<std::chrono::microseconds>(CLOCK_MONOTONIC);
}

/**
 *  Return current time in milliseconds
 */
uint64_t GetClockTimeInMilli()
{
    return GetClockTime<std::chrono::milliseconds>(CLOCK_MONOTONIC);
}

/**
 *  Return thread CPU time in nanoseconds
 */
uint64_t GetClockTimeInThreadCpuTime()
{
    return GetClockTime<std::chrono::nanoseconds>(CLOCK_THREAD_CPUTIME_ID);
}
}  // namespace panda::os::time
