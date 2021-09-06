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

#include "verification/cflow/cflow_common.h"

#include "verification/util/str.h"

#include <cstdint>

namespace panda::verifier {

PandaString OffsetAsHexStr(const void *base, const void *ptr)
{
    auto offset = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ptr) - reinterpret_cast<uintptr_t>(base));
    const uint32_t BASE = 16U;
    return PandaString {"0x"} + NumToStr<PandaString>(offset, BASE, sizeof(uint32_t) * 0x2);
}

}  // namespace panda::verifier
