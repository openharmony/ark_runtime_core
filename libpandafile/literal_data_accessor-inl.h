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

#ifndef PANDA_LIBPANDAFILE_LITERAL_DATA_ACCESSOR_INL_H_
#define PANDA_LIBPANDAFILE_LITERAL_DATA_ACCESSOR_INL_H_

#include <string>
#include "file_items.h"
#include "literal_data_accessor.h"
#include "utils/utf.h"

namespace panda::panda_file {

inline size_t LiteralDataAccessor::GetLiteralValsNum(size_t index)
{
    auto sp = panda_file_.GetSpanFromId(File::EntityId(
        static_cast<uint32_t>(helpers::Read<sizeof(uint32_t)>(literal_data_sp_.SubSpan(index * ID_SIZE)))));
    auto num = helpers::Read<ID_SIZE>(&sp);
    return num;
}

template <class Callback>
inline void LiteralDataAccessor::EnumerateLiteralVals(File::EntityId id, const Callback &cb)
{
    auto sp = panda_file_.GetSpanFromId(id);
    auto literal_vals_num = helpers::Read<ID_SIZE>(&sp);
    LiteralValue value;

    for (size_t i = 0; i < literal_vals_num; i += 2U) {
        auto tag = static_cast<LiteralTag>(helpers::Read<TAG_SIZE>(&sp));
        switch (tag) {
            case LiteralTag::INTEGER:
                value = static_cast<uint32_t>(helpers::Read<sizeof(uint32_t)>(&sp));
                break;
            case LiteralTag::DOUBLE:
                value = bit_cast<double>(helpers::Read<sizeof(uint64_t)>(&sp));
                break;
            case LiteralTag::BOOL:
                value = static_cast<bool>(helpers::Read<sizeof(uint8_t)>(&sp));
                break;
            case LiteralTag::FLOAT:
                value = static_cast<float>(helpers::Read<sizeof(uint32_t)>(&sp));
                break;
            case LiteralTag::STRING: {
                value = static_cast<uint32_t>(helpers::Read<sizeof(uint32_t)>(&sp));
                break;
            }
            case LiteralTag::METHOD:
            case LiteralTag::GENERATORMETHOD:
                value = static_cast<uint32_t>(helpers::Read<sizeof(uint32_t)>(&sp));
                break;
            case LiteralTag::METHODAFFILIATE:
                value = static_cast<uint16_t>(helpers::Read<sizeof(uint16_t)>(&sp));
                break;
            case LiteralTag::ACCESSOR:
            case LiteralTag::NULLVALUE:
                value = static_cast<uint8_t>(helpers::Read<sizeof(uint8_t)>(&sp));
                break;
            // in statically-typed languages we don't need tag for every element,
            // thus treat literal array as array of one element with corresponding type
            case LiteralTag::ARRAY_I8:
            case LiteralTag::ARRAY_I16:
            case LiteralTag::ARRAY_I32:
            case LiteralTag::ARRAY_I64:
            case LiteralTag::ARRAY_F32:
            case LiteralTag::ARRAY_F64:
            case LiteralTag::ARRAY_STRING:
                value = panda_file_.GetIdFromPointer(sp.data()).GetOffset();
                i = literal_vals_num;
                break;
            default:
                UNREACHABLE();
                break;
        }
        cb(value, tag);
    }
}

template <class Callback>
inline void LiteralDataAccessor::EnumerateLiteralVals(size_t index, const Callback &cb)
{
    EnumerateLiteralVals(GetLiteralArrayId(index), cb);
}

}  // namespace panda::panda_file

#endif  // PANDA_LIBPANDAFILE_LITERAL_DATA_ACCESSOR_INL_H_
