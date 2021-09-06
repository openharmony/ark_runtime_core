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

#include "method_handle_data_accessor.h"
#include "helpers.h"

namespace panda::panda_file {

MethodHandleDataAccessor::MethodHandleDataAccessor(const File &panda_file, File::EntityId method_handle_id)
    : panda_file_(panda_file), method_handle_id_(method_handle_id)
{
    auto sp = panda_file_.GetSpanFromId(method_handle_id_);

    type_ = static_cast<MethodHandleType>(helpers::Read<sizeof(uint8_t)>(&sp));
    offset_ = helpers::ReadULeb128(&sp);

    size_ = panda_file_.GetIdFromPointer(sp.data()).GetOffset() - method_handle_id_.GetOffset();
}

}  // namespace panda::panda_file
