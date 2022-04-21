/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#ifndef PANDA_LIBPANDABASE_OS_WINDOWS_TIME_H_
#define PANDA_LIBPANDABASE_OS_WINDOWS_TIME_H_

#include <chrono>
#include <sys/time.h>

namespace panda::os::time {
template <class T>
static uint64_t GetClockTime([[maybe_unused]] clockid_t clock)
{
    struct timeval time = {0, 0};
    if (gettimeofday(&time, nullptr) != -1) {
        auto duration = std::chrono::seconds {time.tv_sec} + std::chrono::microseconds {time.tv_usec};
        return std::chrono::duration_cast<T>(duration).count();
    }
    return 0;
}
}  // namespace panda::os::time

#endif  // PANDA_LIBPANDABASE_OS_WINDOWS_TIME_H_
