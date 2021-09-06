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

#ifndef PANDA_RUNTIME_INCLUDE_TOOLING_PT_VALUE_H_
#define PANDA_RUNTIME_INCLUDE_TOOLING_PT_VALUE_H_

#include <cstdint>
#include "libpandabase/macros.h"

namespace panda::tooling {
class PtValueMeta {
public:
    explicit PtValueMeta(uint64_t data = 0) : data_(data) {}

    uint64_t GetData() const
    {
        return data_;
    }

    void SetData(uint64_t data)
    {
        data_ = data;
    }

    ~PtValueMeta() = default;

    DEFAULT_COPY_SEMANTIC(PtValueMeta);
    DEFAULT_MOVE_SEMANTIC(PtValueMeta);

private:
    uint64_t data_;  // Language dependent data
};

class PtValue {
public:
    explicit PtValue(int64_t value = 0, PtValueMeta meta = PtValueMeta()) : value_(value), meta_(meta) {}

    int64_t GetValue() const
    {
        return value_;
    }

    void SetValue(int64_t value)
    {
        value_ = value;
    }

    PtValueMeta GetMeta() const
    {
        return meta_;
    }

    void SetMeta(PtValueMeta meta)
    {
        meta_ = meta;
    }

    ~PtValue() = default;

    DEFAULT_COPY_SEMANTIC(PtValue);
    DEFAULT_MOVE_SEMANTIC(PtValue);

private:
    int64_t value_;
    PtValueMeta meta_;
};

}  // namespace panda::tooling

#endif  // PANDA_RUNTIME_INCLUDE_TOOLING_PT_VALUE_H_
