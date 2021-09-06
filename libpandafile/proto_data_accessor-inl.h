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

#ifndef PANDA_LIBPANDAFILE_PROTO_DATA_ACCESSOR_INL_H_
#define PANDA_LIBPANDAFILE_PROTO_DATA_ACCESSOR_INL_H_

#include "helpers.h"

#include "file_items.h"
#include "proto_data_accessor.h"

#include <limits>

namespace panda::panda_file {

constexpr size_t SHORTY_ELEM_SIZE = sizeof(uint16_t);
constexpr size_t SHORTY_ELEM_WIDTH = 4;
constexpr size_t SHORTY_ELEM_MASK = 0xf;
constexpr size_t SHORTY_ELEM_PER16 = std::numeric_limits<uint16_t>::digits / SHORTY_ELEM_WIDTH;

inline void ProtoDataAccessor::SkipShorty()
{
    EnumerateTypes([](Type /* unused */) {});
}

template <class Callback>
inline void ProtoDataAccessor::EnumerateTypes(const Callback &c)
{
    auto sp = panda_file_.GetSpanFromId(proto_id_);

    uint32_t v = helpers::Read<SHORTY_ELEM_SIZE>(&sp);
    uint32_t num_ref = 0;
    size_ = SHORTY_ELEM_SIZE;
    while (v != 0) {
        size_t shift = (elem_num_ % SHORTY_ELEM_PER16) * SHORTY_ELEM_WIDTH;
        uint8_t elem = (v >> shift) & SHORTY_ELEM_MASK;
        if (elem == 0) {
            break;
        }

        Type t(static_cast<Type::TypeId>(elem));
        c(t);
        if (!t.IsPrimitive()) {
            ++num_ref;
        }

        ++elem_num_;

        if ((elem_num_ % SHORTY_ELEM_PER16) == 0) {
            v = helpers::Read<SHORTY_ELEM_SIZE>(&sp);
            size_ += SHORTY_ELEM_SIZE;
        }
    }

    size_ += num_ref * IDX_SIZE;

    ref_types_sp_ = sp;
}

inline uint32_t ProtoDataAccessor::GetNumArgs()
{
    if (ref_types_sp_.data() == nullptr) {
        SkipShorty();
    }

    return elem_num_ - 1;
}

inline File::EntityId ProtoDataAccessor::GetReferenceType(size_t i)
{
    if (ref_types_sp_.data() == nullptr) {
        SkipShorty();
    }

    auto sp = ref_types_sp_.SubSpan(i * IDX_SIZE);
    auto class_idx = helpers::Read<IDX_SIZE>(&sp);
    return panda_file_.ResolveClassIndex(proto_id_, class_idx);
}

inline Type ProtoDataAccessor::GetType(size_t idx) const
{
    size_t block_idx = idx / SHORTY_ELEM_PER16;
    size_t elem_shift = (idx % SHORTY_ELEM_PER16) * SHORTY_ELEM_WIDTH;

    auto sp = panda_file_.GetSpanFromId(proto_id_);
    sp = sp.SubSpan(SHORTY_ELEM_SIZE * block_idx);

    uint32_t v = helpers::Read<SHORTY_ELEM_SIZE>(&sp);
    return Type(static_cast<Type::TypeId>((v >> elem_shift) & SHORTY_ELEM_MASK));
}

inline Type ProtoDataAccessor::GetReturnType() const
{
    return GetType(0);
}

inline Type ProtoDataAccessor::GetArgType(size_t idx) const
{
    return GetType(idx + 1);
}

}  // namespace panda::panda_file

#endif  // PANDA_LIBPANDAFILE_PROTO_DATA_ACCESSOR_INL_H_
