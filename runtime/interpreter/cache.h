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

#ifndef PANDA_RUNTIME_INTERPRETER_CACHE_H_
#define PANDA_RUNTIME_INTERPRETER_CACHE_H_

#include <array>
#include "libpandabase/utils/math_helpers.h"
#include "runtime/include/method.h"

namespace panda {

class InterpreterCache {
public:
    bool Has(const void *pc, Method *caller) const
    {
        const auto &entry = data_[GetIndex(pc)];
        return entry.pc == pc && entry.caller == caller;
    }

    template <class T>
    T *Get(const void *pc, Method *caller) const
    {
        if (UNLIKELY(!Has(pc, caller))) {
            return nullptr;
        }
        return static_cast<T *>(data_[GetIndex(pc)].item);
    }

    template <class T>
    void Set(const void *pc, T *item, Method *caller)
    {
        data_[GetIndex(pc)] = {pc, item, caller};
    }

    void Clear()
    {
        data_.fill({});
    }

private:
    static size_t GetIndex(const void *pc)
    {
        return panda::helpers::math::PowerOfTwoTableSlot(reinterpret_cast<size_t>(pc), N, 2U);
    }

    struct Entry {
        const void *pc {nullptr};
        void *item {nullptr};
        Method *caller {nullptr};
    };

    static constexpr size_t N = 256;
    static_assert(panda::helpers::math::IsPowerOfTwo(N));
    std::array<Entry, N> data_ {};
};

}  // namespace panda

#endif  // PANDA_RUNTIME_INTERPRETER_CACHE_H_
