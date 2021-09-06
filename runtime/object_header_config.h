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

#ifndef PANDA_RUNTIME_OBJECT_HEADER_CONFIG_H_
#define PANDA_RUNTIME_OBJECT_HEADER_CONFIG_H_

#include <cstdint>
#include <cstdlib>

#include "libpandabase/mem/mem.h"

namespace panda {

using array_size_t = uint32_t;
using array_ssize_t = int32_t;

class MarkWord;
template <size_t PSIZE>
class LowEndConfig;
template <size_t PSIZE>
class HighEndConfig;

// For now this value is hardcoded:
#define HIGHENDSYSTEM

// We have possible 2 variants of configuration - for High-end devices and for Low-end devices
#ifdef HIGHENDSYSTEM
using MemoryModelConfig = HighEndConfig<OBJECT_POINTER_SIZE>;
#else
using MemoryModelConfig = LowEndConfig<OBJECT_POINTER_SIZE>;
#endif

// Config for High-end devices with hash stored inside object header and 32 bits pointer
template <>
class HighEndConfig<sizeof(uint32_t)> {
public:
    using Size = uint32_t;
    static constexpr Size BITS = 32U;
    static constexpr Size LOCK_THREADID_SIZE = 13U;
    static constexpr bool IS_HASH_IN_OBJ_HEADER = true;
};

// Config for High-end devices with hash stored inside object header and 64 bits pointer
template <>
class HighEndConfig<sizeof(uint64_t)> {
public:
    using Size = uint64_t;
    static constexpr Size BITS = 64UL;
    static constexpr Size LOCK_THREADID_SIZE = 29UL;
    static constexpr bool IS_HASH_IN_OBJ_HEADER = true;
};

// Config for Low-end devices with hash stored inside object header and 32 bits pointer
template <>
class LowEndConfig<sizeof(uint32_t)> {
public:
    using Size = uint16_t;
    static constexpr Size BITS = 16U;
    static constexpr Size LOCK_THREADID_SIZE = 7U;
    static constexpr bool IS_HASH_IN_OBJ_HEADER = true;
};

}  // namespace panda

#endif  // PANDA_RUNTIME_OBJECT_HEADER_CONFIG_H_
