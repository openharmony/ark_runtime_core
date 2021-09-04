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

#ifndef PANDA_RUNTIME_TIMING_H_
#define PANDA_RUNTIME_TIMING_H_

#include <string>
#include <iostream>

#include "libpandabase/os/time.h"
#include "libpandabase/utils/time.h"
#include "runtime/include/mem/panda_containers.h"
#include "runtime/include/mem/panda_string.h"

namespace panda {
class Timing {
public:
    Timing() = default;

    ~Timing() = default;

    enum class TimeLabelType {
        BEGIN,
        END,
    };

    class TimeLabel {
    public:
        TimeLabel(std::string name, uint64_t time, uint64_t cpu_time, TimeLabelType type = TimeLabelType::BEGIN)
            : name_(std::move(name)), time_(time), cpu_time_(cpu_time), type_(type)
        {
        }

        ~TimeLabel() = default;

        TimeLabelType GetType() const
        {
            return type_;
        }

        std::string GetName() const
        {
            return name_;
        }

        uint64_t GetTime() const
        {
            return time_;
        }

        void SetTime(uint64_t duration)
        {
            time_ = duration;
        }

        uint64_t GetCPUTime() const
        {
            return cpu_time_;
        }

        void SetCPUTime(uint64_t duration)
        {
            cpu_time_ = duration;
        }

        DEFAULT_COPY_SEMANTIC(TimeLabel);
        DEFAULT_MOVE_SEMANTIC(TimeLabel);

    private:
        std::string name_;
        uint64_t time_;      //  After processed, time_ is used to save duration.
        uint64_t cpu_time_;  //  After processed, cpu_time_ is used to save duration.
        TimeLabelType type_;
    };

    void NewSection(const std::string &tag)
    {
        labels_.push_back(TimeLabel(tag, time::GetCurrentTimeInNanos(), panda::os::time::GetClockTimeInThreadCpuTime(),
                                    TimeLabelType::BEGIN));
    }

    void EndSection()
    {
        labels_.push_back(TimeLabel("", time::GetCurrentTimeInNanos(), panda::os::time::GetClockTimeInThreadCpuTime(),
                                    TimeLabelType::END));
    }

    PandaString Dump();

    void Reset()
    {
        labels_.clear();
    }

    static std::string PrettyTimeNs(uint64_t duration);

private:
    void Process();

    PandaVector<TimeLabel> labels_;

    NO_MOVE_SEMANTIC(Timing);
    NO_COPY_SEMANTIC(Timing);
};

class ScopedTiming {
public:
    // NOLINTNEXTLINE(google-runtime-references)
    ScopedTiming(const std::string &tag, Timing &timing) : timing_(timing)
    {
        timing_.NewSection(tag);
    }
    ~ScopedTiming()
    {
        timing_.EndSection();
    }

    NO_MOVE_SEMANTIC(ScopedTiming);
    NO_COPY_SEMANTIC(ScopedTiming);

private:
    Timing &timing_;
};

}  // namespace panda

#endif  // PANDA_RUNTIME_TIMING_H_
