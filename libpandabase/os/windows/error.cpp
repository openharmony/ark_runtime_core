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

#include "os/error.h"

namespace panda::os {

std::string Error::ToString() const
{
    if (std::holds_alternative<std::string>(err_)) {
        return std::get<std::string>(err_);
    }

    constexpr size_t BUFSIZE = 256;
    int err = std::get<int>(err_);

    char res[BUFSIZE];
    strerror_s(res, BUFSIZE, err);

    return std::string(res);
}

}  // namespace panda::os
