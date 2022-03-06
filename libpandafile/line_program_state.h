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

#ifndef PANDA_LIBPANDAFILE_LINE_PROGRAM_STATE_H_
#define PANDA_LIBPANDAFILE_LINE_PROGRAM_STATE_H_

#include "file-inl.h"

namespace panda::panda_file {

class LineProgramState {
public:
    LineProgramState(const File &pf, File::EntityId file, size_t line, Span<const uint8_t> constant_pool)
        : pf_(pf), file_(file), line_(line), constant_pool_(constant_pool)
    {
    }
    ~LineProgramState() = default;
    DEFAULT_COPY_CTOR(LineProgramState)
    DEFAULT_MOVE_CTOR(LineProgramState)
    NO_COPY_OPERATOR(LineProgramState);
    NO_MOVE_OPERATOR(LineProgramState);

    void AdvanceLine(int32_t v)
    {
        line_ += static_cast<size_t>(v);
    }

    void AdvancePc(uint32_t v)
    {
        address_ += v;
    }

    void SetColumn(int32_t c)
    {
        column_ = static_cast<size_t>(c);
    }

    size_t GetColumn() const
    {
        return column_;
    }

    void SetFile(uint32_t offset)
    {
        file_ = File::EntityId(offset);
    }

    const uint8_t *GetFile() const
    {
        return pf_.GetStringData(file_).data;
    }

    bool HasFile() const
    {
        return file_.IsValid();
    }

    void SetSourceCode(uint32_t offset)
    {
        source_code_ = File::EntityId(offset);
    }

    const uint8_t *GetSourceCode() const
    {
        return pf_.GetStringData(source_code_).data;
    }

    bool HasSourceCode() const
    {
        return source_code_.IsValid();
    }

    size_t GetLine() const
    {
        return line_;
    }

    uint32_t GetAddress() const
    {
        return address_;
    }

    uint32_t ReadULeb128()
    {
        return panda_file::helpers::ReadULeb128(&constant_pool_);
    }

    int32_t ReadSLeb128()
    {
        return panda_file::helpers::ReadLeb128(&constant_pool_);
    }

    const File &GetPandaFile() const
    {
        return pf_;
    }

private:
    const File &pf_;

    File::EntityId file_;
    File::EntityId source_code_;
    size_t line_;
    size_t column_ {0};
    Span<const uint8_t> constant_pool_;

    uint32_t address_ {0};
};

}  // namespace panda::panda_file

#endif  // PANDA_LIBPANDAFILE_LINE_PROGRAM_STATE_H_
