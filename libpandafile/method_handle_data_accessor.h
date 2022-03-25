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

#ifndef PANDA_LIBPANDAFILE_METHOD_HANDLE_DATA_ACCESSOR_H_
#define PANDA_LIBPANDAFILE_METHOD_HANDLE_DATA_ACCESSOR_H_

#include "file.h"
#include "file_items.h"

namespace panda::panda_file {

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions, hicpp-special-member-functions)
class MethodHandleDataAccessor {
public:
    MethodHandleDataAccessor(const File &panda_file, File::EntityId method_handle_id);

    ~MethodHandleDataAccessor() = default;

    MethodHandleType GetType() const
    {
        return type_;
    }

    size_t GetSize() const
    {
        return size_;
    }

    const File &GetPandaFile() const
    {
        return panda_file_;
    }

    File::EntityId GetMethodHandleId() const
    {
        return method_handle_id_;
    }

    File::EntityId GetEntityId() const
    {
        return File::EntityId(offset_);
    }

private:
    const File &panda_file_;
    File::EntityId method_handle_id_;

    MethodHandleType type_;
    uint32_t offset_ {0};

    size_t size_ {0};
};

}  // namespace panda::panda_file

#endif  // PANDA_LIBPANDAFILE_METHOD_HANDLE_DATA_ACCESSOR_H_
