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

#ifndef PANDA_LIBPANDAFILE_DEBUG_DATA_ACCESSOR_INL_H_
#define PANDA_LIBPANDAFILE_DEBUG_DATA_ACCESSOR_INL_H_

#include "debug_data_accessor.h"
#include "helpers.h"

namespace panda::panda_file {

template <class Callback>
inline void DebugInfoDataAccessor::EnumerateParameters(const Callback &cb)
{
    auto sp = parameters_sp_;

    for (size_t i = 0; i < num_params_; i++) {
        File::EntityId id(helpers::ReadULeb128(&sp));
        cb(id);
    }

    constant_pool_size_sp_ = sp;
}

inline void DebugInfoDataAccessor::SkipParameters()
{
    EnumerateParameters([](File::EntityId /* unused */) {});
}

inline Span<const uint8_t> DebugInfoDataAccessor::GetConstantPool()
{
    if (constant_pool_size_sp_.data() == nullptr) {
        SkipParameters();
    }

    auto sp = constant_pool_size_sp_;

    uint32_t size = helpers::ReadULeb128(&sp);
    line_num_program_off_sp_ = sp.SubSpan(size);

    return sp.First(size);
}

inline void DebugInfoDataAccessor::SkipConstantPool()
{
    GetConstantPool();
}

inline const uint8_t *DebugInfoDataAccessor::GetLineNumberProgram()
{
    if (line_num_program_off_sp_.data() == nullptr) {
        SkipConstantPool();
    }

    auto sp = line_num_program_off_sp_;
    uint32_t index = helpers::ReadULeb128(&sp);
    auto line_num_program_id = panda_file_.ResolveLineNumberProgramIndex(index);

    size_ = panda_file_.GetIdFromPointer(sp.data()).GetOffset() - debug_info_id_.GetOffset();

    return panda_file_.GetSpanFromId(line_num_program_id).data();
}

inline void DebugInfoDataAccessor::SkipLineNumberProgram()
{
    GetLineNumberProgram();
}

}  // namespace panda::panda_file

#endif  // PANDA_LIBPANDAFILE_DEBUG_DATA_ACCESSOR_INL_H_
