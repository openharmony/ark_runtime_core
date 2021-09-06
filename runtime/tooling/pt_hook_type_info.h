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

#ifndef PANDA_RUNTIME_TOOLING_PT_HOOK_TYPE_INFO_H_
#define PANDA_RUNTIME_TOOLING_PT_HOOK_TYPE_INFO_H_

#include <array>
#include "runtime/include/tooling/debug_interface.h"

namespace panda::tooling {
class PtHookTypeInfo {
public:
    explicit PtHookTypeInfo(bool defalutValue)
    {
        for (auto &v : is_enabled_) {
            v = defalutValue;
        }
    }

    bool IsEnabled(const PtHookType type) const
    {
        return is_enabled_.at(ToIndex(type));
    }

    void Enable(const PtHookType type)
    {
        is_enabled_[ToIndex(type)] = true;
    }

    void Disable(const PtHookType type)
    {
        is_enabled_[ToIndex(type)] = false;
    }

    void EnableAll()
    {
        is_enabled_.fill(true);
    }

    void DisableAll()
    {
        is_enabled_.fill(false);
    }

    ~PtHookTypeInfo() = default;
    DEFAULT_COPY_SEMANTIC(PtHookTypeInfo);
    DEFAULT_MOVE_SEMANTIC(PtHookTypeInfo);

private:
    static constexpr size_t ToIndex(const PtHookType type)
    {
        auto index = static_cast<size_t>(type);
        ASSERT(index < HOOKS_COUNT);
        return index;
    }

    static constexpr size_t HOOKS_COUNT = static_cast<size_t>(PtHookType::PT_HOOK_TYPE_COUNT);
    std::array<bool, HOOKS_COUNT> is_enabled_ {};
};

}  // namespace panda::tooling

#endif  // PANDA_RUNTIME_TOOLING_PT_HOOK_TYPE_INFO_H_
