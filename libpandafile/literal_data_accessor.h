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

#ifndef PANDA_LIBPANDAFILE_LITERAL_DATA_ACCESSOR_H_
#define PANDA_LIBPANDAFILE_LITERAL_DATA_ACCESSOR_H_

#include "field_data_accessor.h"
#include "file.h"
#include "helpers.h"
#include "utils/span.h"

namespace panda::panda_file {
using StringData = File::StringData;

/* LiteralTag could be extended by different language
// For example, JAVA could use it to represent Array of Integer
// by adding `INTARRAY` in the future
*/
enum class LiteralTag : uint8_t {
    TAGVALUE = 0x00,
    BOOL = 0x01,
    INTEGER = 0x02,
    FLOAT = 0x03,
    DOUBLE = 0x04,
    STRING = 0x05,
    METHOD = 0x06,
    GENERATORMETHOD = 0x07,
    ACCESSOR = 0x08,
    METHODAFFILIATE = 0x09,
    ARRAY_I8 = 0x0a,
    ARRAY_I16 = 0x0b,
    ARRAY_I32 = 0x0c,
    ARRAY_I64 = 0x0d,
    ARRAY_F32 = 0x0e,
    ARRAY_F64 = 0x0f,
    ARRAY_STRING = 0x10,
    NULLVALUE = 0xff
};

class LiteralDataAccessor {
public:
    LiteralDataAccessor(const File &panda_file, File::EntityId literal_data_id);
    ~LiteralDataAccessor() = default;
    DEFAULT_MOVE_CTOR(LiteralDataAccessor)
    DEFAULT_COPY_CTOR(LiteralDataAccessor)
    NO_MOVE_OPERATOR(LiteralDataAccessor);
    NO_COPY_OPERATOR(LiteralDataAccessor);

    template <class Callback>
    void EnumerateObjectLiterals(size_t index, const Callback &cb);

    template <class Callback>
    void EnumerateLiteralVals(size_t index, const Callback &cb);

    template <class Callback>
    void EnumerateLiteralVals(File::EntityId id, const Callback &cb);

    size_t GetLiteralValsNum(size_t index);

    uint32_t GetLiteralNum() const
    {
        return literal_num_;
    }

    const File &GetPandaFile() const
    {
        return panda_file_;
    }

    File::EntityId GetLiteralDataId() const
    {
        return literal_data_id_;
    }

    File::EntityId GetLiteralArrayId(size_t index) const
    {
        ASSERT(index < literal_num_);
        auto l_sp = literal_data_sp_.SubSpan(index * ID_SIZE);
        return File::EntityId(static_cast<uint32_t>(helpers::Read<sizeof(uint32_t)>(&l_sp)));
    }

    using LiteralValue = std::variant<bool, void *, uint8_t, uint16_t, uint32_t, uint64_t, float, double, StringData>;

private:
    const File &panda_file_;
    File::EntityId literal_data_id_;
    uint32_t literal_num_;
    Span<const uint8_t> literal_data_sp_ {nullptr, nullptr};
};

}  // namespace panda::panda_file

#endif  // PANDA_LIBPANDAFILE_LITERAL_DATA_ACCESSOR_H_
