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

#ifndef PANDA_LIBPANDABASE_OS_TIME_H_
#define PANDA_LIBPANDABASE_OS_TIME_H_

#if defined(PANDA_TARGET_UNIX)
#include "os/unix/time_unix.h"
#endif  // PANDA_TARGET_UNIX
#include <cstdint>

namespace panda::os::time {
#if defined(PANDA_TARGET_UNIX)
const auto GetClockTimeInMicro = panda::os::unix::time::GetClockTimeInMicro;  // NOLINT(readability-identifier-naming)
const auto GetClockTimeInMilli = panda::os::unix::time::GetClockTimeInMilli;  // NOLINT(readability-identifier-naming)
const auto GetClockTimeInThreadCpuTime =                                      // NOLINT(readability-identifier-naming)
    panda::os::unix::time::GetClockTimeInThreadCpuTime;
#else
uint64_t GetClockTimeInMicro();
uint64_t GetClockTimeInMilli();
uint64_t GetClockTimeInThreadCpuTime();
#endif  // PANDA_TARGET_UNIX

}  // namespace panda::os::time

#endif  // PANDA_LIBPANDABASE_OS_TIME_H_
