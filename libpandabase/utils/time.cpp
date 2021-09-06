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

// NOLINTNEXTLINE(modernize-deprecated-headers, hicpp-deprecated-headers)
#include "time.h"
#include <chrono>

namespace panda::time {

template <class T>
static uint64_t GetCurrentTime(bool need_system)
{
    if (need_system) {
        return std::chrono::duration_cast<T>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
    return std::chrono::duration_cast<T>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

/**
 *  Return current time in milliseconds
 */
uint64_t GetCurrentTimeInMillis(bool need_system)
{
    return GetCurrentTime<std::chrono::milliseconds>(need_system);
}

/**
 *  Return current time in nanoseconds
 */
uint64_t GetCurrentTimeInNanos(bool need_system)
{
    return GetCurrentTime<std::chrono::nanoseconds>(need_system);
}

}  // namespace panda::time
