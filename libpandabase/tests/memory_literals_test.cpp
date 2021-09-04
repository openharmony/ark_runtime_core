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
#include <cassert>

#include "mem/mem.h"

namespace panda {

constexpr uint64_t SIZE_4G = (1ULL << 32);

// Integer overflow checking for memory literals
static_assert(4194304_KB == SIZE_4G);
static_assert(4096_MB == SIZE_4G);
static_assert(4_GB == SIZE_4G);

}  // namespace panda

int main()
{
    return 0;
}
