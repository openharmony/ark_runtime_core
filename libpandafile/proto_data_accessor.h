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

#ifndef PANDA_LIBPANDAFILE_PROTO_DATA_ACCESSOR_H_
#define PANDA_LIBPANDAFILE_PROTO_DATA_ACCESSOR_H_

#include "file.h"
#include "file_items.h"

namespace panda::panda_file {

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions, hicpp-special-member-functions)
class ProtoDataAccessor {
public:
    ProtoDataAccessor(const File &panda_file, File::EntityId proto_id) : panda_file_(panda_file), proto_id_(proto_id) {}

    ~ProtoDataAccessor() = default;

    const File &GetPandaFile() const
    {
        return panda_file_;
    }

    File::EntityId GetProtoId() const
    {
        return proto_id_;
    }

    Span<const uint8_t> GetShorty() const
    {
        return panda_file_.GetSpanFromId(proto_id_);
    }

    template <class Callback>
    void EnumerateTypes(const Callback &c);

    uint32_t GetNumArgs();

    Type GetArgType(size_t idx) const;

    Type GetReturnType() const;

    File::EntityId GetReferenceType(size_t i);

    size_t GetSize()
    {
        if (size_ == 0) {
            SkipShorty();
        }

        return size_;
    }

private:
    void SkipShorty();

    Type GetType(size_t idx) const;

    const File &panda_file_;
    File::EntityId proto_id_;

    size_t elem_num_ {0};
    Span<const uint8_t> ref_types_sp_ {nullptr, nullptr};

    size_t size_ {0};
};

}  // namespace panda::panda_file

#endif  // PANDA_LIBPANDAFILE_PROTO_DATA_ACCESSOR_H_
