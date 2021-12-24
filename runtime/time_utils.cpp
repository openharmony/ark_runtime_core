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

#include "runtime/include/time_utils.h"

#include <iomanip>

#include "libpandabase/utils/time.h"

namespace panda::time {

constexpr size_t TIME_BUFF_LENGTH = 100;

Timer::Timer(uint64_t *duration, bool need_restart) : duration_(duration), start_time_(GetCurrentTimeInNanos())
{
    if (need_restart) {
        *duration = 0;
    }
}

Timer::~Timer()
{
    *duration_ += GetCurrentTimeInNanos() - start_time_;
}

PandaString GetCurrentTimeString()
{
    PandaOStringStream result_stream;
    auto time_now = GetCurrentTimeInMillis(true);
    time_t millisecond = time_now % MILLISECONDS_IN_SECOND;
    time_t seconds = time_now / MILLISECONDS_IN_SECOND;

    constexpr int DATE_BUFFER_SIZE = 16;
    PandaString date_buffer;
    date_buffer.resize(DATE_BUFFER_SIZE);
    std::tm *now = std::localtime(&seconds);
    ASSERT(now != nullptr);
    if (std::strftime(date_buffer.data(), DATE_BUFFER_SIZE, "%b %d %T", now) == 0U) {
        return "";
    }
    // Because strftime returns a string in C format
    date_buffer[DATE_BUFFER_SIZE - 1] = '.';
    result_stream << date_buffer << std::setfill('0') << std::setw(PRECISION_FOR_TIME) << millisecond;
    return result_stream.str();
}

PandaString GetCurrentTimeString(const char *format)
{
    std::string date {};
    std::time_t time = std::time(nullptr);
    std::tm *now = std::localtime(&time);
    ASSERT(now != nullptr);
    std::array<char, TIME_BUFF_LENGTH> buffer {};
    if (std::strftime(buffer.data(), buffer.size(), format, now) != 0) {
        date = std::string {buffer.data()};
    }
    return ConvertToString(!date.empty() ? date : "1970-01-01 00:00:00");
}

}  // namespace panda::time
