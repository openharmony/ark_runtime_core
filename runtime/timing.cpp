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

#include "runtime/timing.h"

#include <iomanip>

#include "utils/logger.h"

namespace panda {

constexpr uint64_t NS_PER_SECOND = 1000000000;
constexpr uint64_t NS_PER_MILLISECOND = 1000000;
constexpr uint64_t NS_PER_MICROSECOND = 1000;

std::string Timing::PrettyTimeNs(uint64_t duration)
{
    uint64_t time_uint;
    std::string time_uint_name;
    uint64_t main_part;
    uint64_t fractional_part;
    if (duration > NS_PER_SECOND) {
        time_uint = NS_PER_SECOND;
        main_part = duration / time_uint;
        fractional_part = duration % time_uint / NS_PER_MILLISECOND;
        time_uint_name = "s";
    } else if (duration > NS_PER_MILLISECOND) {
        time_uint = NS_PER_MILLISECOND;
        main_part = duration / time_uint;
        fractional_part = duration % time_uint / NS_PER_MICROSECOND;
        time_uint_name = "ms";
    } else {
        time_uint = NS_PER_MICROSECOND;
        main_part = duration / time_uint;
        fractional_part = duration % time_uint;
        time_uint_name = "us";
    }
    std::stringstream ss;
    constexpr size_t FRACTION_WIDTH = 3U;
    ss << main_part << "." << std::setfill('0') << std::setw(FRACTION_WIDTH) << fractional_part << time_uint_name;
    return ss.str();
}

void Timing::Process()
{
    PandaStack<PandaVector<TimeLabel>::iterator> label_stack;
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (auto it = labels_.begin(); it != labels_.end(); it++) {
        if (it->GetType() == TimeLabelType::BEGIN) {
            label_stack.push(it);
            continue;
        }
        auto begin_it = label_stack.top();
        label_stack.pop();
        uint64_t duration = it->GetTime() - begin_it->GetTime();
        uint64_t cpu_duration = it->GetCPUTime() - begin_it->GetCPUTime();
        begin_it->SetTime(duration);
        begin_it->SetCPUTime(cpu_duration);
    }
}

PandaString Timing::Dump()
{
    Process();
    PandaStringStream ss;
    std::string indent = "    ";
    size_t indent_count = 0;
    for (auto &label : labels_) {
        if (label.GetType() == TimeLabelType::BEGIN) {
            for (size_t i = 0; i < indent_count; i++) {
                ss << indent;
            }
            ss << label.GetName() << " " << PrettyTimeNs(label.GetCPUTime()) << "/" << PrettyTimeNs(label.GetTime())
               << std::endl;
            indent_count++;
            continue;
        }
        if (indent_count > 0U) {
            indent_count--;
        }
    }
    return ss.str();
}

}  // namespace panda
