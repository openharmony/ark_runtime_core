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

#ifndef PANDA_LIBPANDAFILE_CODE_DATA_ACCESSOR_INL_H_
#define PANDA_LIBPANDAFILE_CODE_DATA_ACCESSOR_INL_H_

#include "code_data_accessor.h"

namespace panda::panda_file {

template <class Callback>
inline void CodeDataAccessor::TryBlock::EnumerateCatchBlocks(const Callback &cb)
{
    auto sp = catch_blocks_sp_;
    for (size_t i = 0; i < num_catches_; i++) {
        CatchBlock catch_block(sp);
        if (!cb(catch_block)) {
            return;
        }
        sp = sp.SubSpan(catch_block.GetSize());
    }
    size_ = sp.data() - data_.data();
}

inline void CodeDataAccessor::TryBlock::SkipCatchBlocks()
{
    EnumerateCatchBlocks([](const CatchBlock & /* unused */) { return true; });
}

template <class Callback>
inline void CodeDataAccessor::EnumerateTryBlocks(const Callback &cb)
{
    auto sp = try_blocks_sp_;
    for (size_t i = 0; i < tries_size_; i++) {
        TryBlock try_block(sp);
        if (!cb(try_block)) {
            return;
        }
        sp = sp.SubSpan(try_block.GetSize());
    }
    size_ = panda_file_.GetIdFromPointer(sp.data()).GetOffset() - code_id_.GetOffset();
}

inline void CodeDataAccessor::SkipTryBlocks()
{
    EnumerateTryBlocks([](const TryBlock & /* unused */) { return true; });
}

}  // namespace panda::panda_file

#endif  // PANDA_LIBPANDAFILE_CODE_DATA_ACCESSOR_INL_H_
