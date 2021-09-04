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

#ifndef PANDA_RUNTIME_INCLUDE_TIME_UTILS_H_
#define PANDA_RUNTIME_INCLUDE_TIME_UTILS_H_

#include <cstdint>

#include "libpandabase/macros.h"
#include "runtime/include/mem/panda_string.h"

namespace panda::time {

constexpr size_t MILLISECONDS_IN_SECOND = 1000;
constexpr size_t PRECISION_FOR_TIME = 3;

/**
 *  Measures time from creation to deletion of an object
 */
class Timer {
public:
    explicit Timer(uint64_t *duration, bool need_restart = false);
    NO_COPY_SEMANTIC(Timer);
    NO_MOVE_SEMANTIC(Timer);
    ~Timer();

private:
    uint64_t *duration_;
    uint64_t start_time_;
};

/**
 *  Return current time in readable format
 */
PandaString GetCurrentTimeString();

PandaString GetCurrentTimeString(const char *format);

}  // namespace panda::time

#endif  // PANDA_RUNTIME_INCLUDE_TIME_UTILS_H_
