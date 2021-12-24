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

#ifndef PANDA_LIBPANDAFILE_CODE_DATA_ACCESSOR_H_
#define PANDA_LIBPANDAFILE_CODE_DATA_ACCESSOR_H_

#include "file-inl.h"

namespace panda::panda_file {

class CodeDataAccessor {
public:
    class TryBlock {
    public:
        explicit TryBlock(Span<const uint8_t> data);

        ~TryBlock() = default;

        NO_COPY_SEMANTIC(TryBlock);
        NO_MOVE_SEMANTIC(TryBlock);

        uint32_t GetStartPc() const
        {
            return start_pc_;
        }

        uint32_t GetLength() const
        {
            return length_;
        }

        uint32_t GetNumCatches() const
        {
            return num_catches_;
        }

        template <class Callback>
        void EnumerateCatchBlocks(const Callback &cb);

        size_t GetSize()
        {
            if (size_ == 0) {
                SkipCatchBlocks();
            }

            return size_;
        }

    private:
        void SkipCatchBlocks();

        Span<const uint8_t> data_;

        uint32_t start_pc_ {0};
        uint32_t length_ {0};
        uint32_t num_catches_ {0};
        Span<const uint8_t> catch_blocks_sp_ {nullptr, nullptr};

        size_t size_ {0};
    };

    class CatchBlock {
    public:
        explicit CatchBlock(Span<const uint8_t> data);

        ~CatchBlock() = default;

        NO_COPY_SEMANTIC(CatchBlock);
        NO_MOVE_SEMANTIC(CatchBlock);

        uint32_t GetTypeIdx() const
        {
            return type_idx_;
        }

        uint32_t GetHandlerPc() const
        {
            return handler_pc_;
        }

        uint32_t GetCodeSize() const
        {
            return code_size_;
        }

        size_t GetSize() const
        {
            return size_;
        }

    private:
        uint32_t type_idx_ {0};
        uint32_t handler_pc_ {0};
        uint32_t code_size_ {0};

        size_t size_ {0};
    };

    CodeDataAccessor(const File &panda_file, File::EntityId code_id);

    ~CodeDataAccessor() = default;

    NO_COPY_SEMANTIC(CodeDataAccessor);
    NO_MOVE_SEMANTIC(CodeDataAccessor);

    uint32_t GetNumVregs() const
    {
        return num_vregs_;
    }

    uint32_t GetNumArgs() const
    {
        return num_args_;
    }

    uint32_t GetCodeSize() const
    {
        return code_size_;
    }

    uint32_t GetTriesSize() const
    {
        return tries_size_;
    }

    const uint8_t *GetInstructions() const
    {
        return instructions_ptr_;
    }

    template <class Callback>
    void EnumerateTryBlocks(const Callback &cb);

    size_t GetSize()
    {
        if (size_ == 0) {
            SkipTryBlocks();
        }

        return size_;
    }

    const File &GetPandaFile() const
    {
        return panda_file_;
    }

    File::EntityId GetCodeId() const
    {
        return code_id_;
    }

private:
    void SkipTryBlocks();

    const File &panda_file_;
    File::EntityId code_id_;

    uint32_t num_vregs_ {0};
    uint32_t num_args_ {0};
    uint32_t code_size_ {0};
    uint32_t tries_size_ {0};
    const uint8_t *instructions_ptr_ {nullptr};
    Span<const uint8_t> try_blocks_sp_ {nullptr, nullptr};

    size_t size_;
};

}  // namespace panda::panda_file

#endif  // PANDA_LIBPANDAFILE_CODE_DATA_ACCESSOR_H_
