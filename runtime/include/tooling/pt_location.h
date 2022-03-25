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

#ifndef PANDA_RUNTIME_INCLUDE_TOOLING_PT_LOCATION_H_
#define PANDA_RUNTIME_INCLUDE_TOOLING_PT_LOCATION_H_

#include <cstring>

#include "libpandafile/file_items.h"
#include "libpandabase/macros.h"

namespace panda::tooling {
class PtLocation {
public:
    using EntityId = panda_file::File::EntityId;

    explicit PtLocation(const char *panda_file, EntityId method_id, uint32_t bytecode_offset)
        : panda_file_(panda_file), method_id_(method_id), bytecode_offset_(bytecode_offset)
    {
    }

    const char *GetPandaFile() const
    {
        return panda_file_;
    }

    EntityId GetMethodId() const
    {
        return method_id_;
    }

    uint32_t GetBytecodeOffset() const
    {
        return bytecode_offset_;
    }

    bool operator==(const PtLocation &location) const
    {
        return method_id_ == location.method_id_ && bytecode_offset_ == location.bytecode_offset_ &&
               ::strcmp(panda_file_, location.panda_file_) == 0;
    }

    ~PtLocation() = default;

    DEFAULT_COPY_SEMANTIC(PtLocation);
    DEFAULT_MOVE_SEMANTIC(PtLocation);

private:
    const char *panda_file_;
    EntityId method_id_;
    uint32_t bytecode_offset_ {0};
};

}  // namespace panda::tooling

#endif  // PANDA_RUNTIME_INCLUDE_TOOLING_PT_LOCATION_H_
