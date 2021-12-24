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

#include "code_data_accessor.h"

namespace panda::panda_file {

CodeDataAccessor::CatchBlock::CatchBlock(Span<const uint8_t> data)
{
    auto sp = data;
    type_idx_ = helpers::ReadULeb128(&sp) - 1;
    handler_pc_ = helpers::ReadULeb128(&sp);
    code_size_ = helpers::ReadULeb128(&sp);
    size_ = sp.data() - data.data();
}

CodeDataAccessor::TryBlock::TryBlock(Span<const uint8_t> data) : data_(data)
{
    start_pc_ = helpers::ReadULeb128(&data);
    length_ = helpers::ReadULeb128(&data);
    num_catches_ = helpers::ReadULeb128(&data);
    catch_blocks_sp_ = data;
}

CodeDataAccessor::CodeDataAccessor(const File &panda_file, File::EntityId code_id)
    : panda_file_(panda_file), code_id_(code_id), size_(0)
{
    auto sp = panda_file_.GetSpanFromId(code_id_);

    num_vregs_ = helpers::ReadULeb128(&sp);
    num_args_ = helpers::ReadULeb128(&sp);
    code_size_ = helpers::ReadULeb128(&sp);
    tries_size_ = helpers::ReadULeb128(&sp);
    instructions_ptr_ = sp.data();
    sp = sp.SubSpan(code_size_);
    try_blocks_sp_ = sp;
}

}  // namespace panda::panda_file
