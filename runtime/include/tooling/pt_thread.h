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

#ifndef PANDA_RUNTIME_INCLUDE_TOOLING_PT_THREAD_H_
#define PANDA_RUNTIME_INCLUDE_TOOLING_PT_THREAD_H_

#include <cstdint>
#include "libpandabase/macros.h"

namespace panda::tooling {
class PtThread {
public:
    explicit PtThread(uint32_t id) : id_(id) {}

    bool operator==(const PtThread &other)
    {
        return id_ == other.id_;
    }

    uint32_t GetId() const
    {
        return id_;
    }

    static const PtThread NONE;

    ~PtThread() = default;

    DEFAULT_COPY_SEMANTIC(PtThread);
    DEFAULT_MOVE_SEMANTIC(PtThread);

private:
    uint32_t id_;
};

}  // namespace panda::tooling

#endif  // PANDA_RUNTIME_INCLUDE_TOOLING_PT_THREAD_H_
