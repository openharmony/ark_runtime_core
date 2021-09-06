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

#ifndef PANDA_LIBPANDAFILE_DEBUG_DATA_ACCESSOR_H_
#define PANDA_LIBPANDAFILE_DEBUG_DATA_ACCESSOR_H_

#include "file.h"

#include "utils/span.h"

namespace panda::panda_file {

class DebugInfoDataAccessor {
public:
    DebugInfoDataAccessor(const File &panda_file, File::EntityId debug_info_id);

    ~DebugInfoDataAccessor() = default;

    NO_COPY_SEMANTIC(DebugInfoDataAccessor);
    NO_MOVE_SEMANTIC(DebugInfoDataAccessor);

    uint32_t GetLineStart() const
    {
        return line_start_;
    }

    uint32_t GetNumParams() const
    {
        return num_params_;
    }

    template <class Callback>
    void EnumerateParameters(const Callback &cb);

    Span<const uint8_t> GetConstantPool();

    const uint8_t *GetLineNumberProgram();

    size_t GetSize()
    {
        if (size_ == 0) {
            SkipLineNumberProgram();
        }

        return size_;
    }

    const File &GetPandaFile() const
    {
        return panda_file_;
    }

    File::EntityId GetDebugInfoId() const
    {
        return debug_info_id_;
    }

private:
    void SkipParameters();

    void SkipConstantPool();

    void SkipLineNumberProgram();

    const File &panda_file_;
    File::EntityId debug_info_id_;

    uint32_t line_start_ {0};
    uint32_t num_params_ {0};
    Span<const uint8_t> parameters_sp_ {nullptr, nullptr};
    Span<const uint8_t> constant_pool_size_sp_ {nullptr, nullptr};
    Span<const uint8_t> line_num_program_off_sp_ {nullptr, nullptr};

    size_t size_ {0};
};

}  // namespace panda::panda_file

#endif  // PANDA_LIBPANDAFILE_DEBUG_DATA_ACCESSOR_H_
