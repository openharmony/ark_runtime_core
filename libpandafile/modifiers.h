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

#ifndef PANDA_LIBPANDAFILE_MODIFIERS_H_
#define PANDA_LIBPANDAFILE_MODIFIERS_H_

#include "utils/bit_utils.h"

#include <cstdint>

namespace panda {

static constexpr uint32_t ACC_PUBLIC = 0x0001;        // field, method, class
static constexpr uint32_t ACC_PRIVATE = 0x0002;       // field, method
static constexpr uint32_t ACC_PROTECTED = 0x0004;     // field, method
static constexpr uint32_t ACC_STATIC = 0x0008;        // field, method
static constexpr uint32_t ACC_FINAL = 0x0010;         // field, method, class
static constexpr uint32_t ACC_SUPER = 0x0020;         // class
static constexpr uint32_t ACC_SYNCHRONIZED = 0x0020;  // method
static constexpr uint32_t ACC_BRIDGE = 0x0040;        // method
static constexpr uint32_t ACC_VOLATILE = 0x0040;      // field
static constexpr uint32_t ACC_TRANSIENT = 0x0080;     // field,
static constexpr uint32_t ACC_VARARGS = 0x0080;       // method
static constexpr uint32_t ACC_NATIVE = 0x0100;        // method
static constexpr uint32_t ACC_INTERFACE = 0x0200;     // class
static constexpr uint32_t ACC_ABSTRACT = 0x0400;      // method, class
static constexpr uint32_t ACC_STRICT = 0x0800;        // method
static constexpr uint32_t ACC_SYNTHETIC = 0x1000;     // field, method, class
static constexpr uint32_t ACC_ANNOTATION = 0x2000;    // class
static constexpr uint32_t ACC_ENUM = 0x4000;          // field, class

static constexpr uint32_t ACC_FILE_MASK = 0xFFFF;

// Runtime internal modifiers
static constexpr uint32_t ACC_HAS_DEFAULT_METHODS = 0x00010000;       // class (runtime)
static constexpr uint32_t ACC_CONSTRUCTOR = 0x00010000;               // method (runtime)
static constexpr uint32_t ACC_DEFAULT_INTERFACE_METHOD = 0x00020000;  // method (runtime)
static constexpr uint32_t ACC_SINGLE_IMPL = 0x00040000;               // method (runtime)
static constexpr uint32_t ACC_INTRINSIC = 0x00200000;                 // method (runtime)

static constexpr uint32_t INTRINSIC_SHIFT = MinimumBitsToStore(ACC_INTRINSIC);
static constexpr uint32_t INTRINSIC_MASK = static_cast<uint32_t>(0xff) << INTRINSIC_SHIFT;

// Runtime internal language specific modifiers
static constexpr uint32_t ACC_PROXY = 0x00020000;            // class (java runtime)
static constexpr uint32_t ACC_FAST_NATIVE = 0x00080000;      // method (java runtime)
static constexpr uint32_t ACC_CRITICAL_NATIVE = 0x00100000;  // method (java runtime)

static constexpr uint32_t ACC_VERIFICATION_STATUS = 0x00400000;  // method (runtime)
static constexpr uint32_t VERIFICATION_STATUS_SHIFT = MinimumBitsToStore(ACC_VERIFICATION_STATUS);
static constexpr uint32_t VERIFICATION_STATUS_MASK = static_cast<uint32_t>(0x7) << VERIFICATION_STATUS_SHIFT;

static constexpr uint32_t ACC_COMPILATION_STATUS = 0x02000000;  // method (runtime)
static constexpr uint32_t COMPILATION_STATUS_SHIFT = MinimumBitsToStore(ACC_COMPILATION_STATUS);
static constexpr uint32_t COMPILATION_STATUS_MASK = static_cast<uint32_t>(0x7) << COMPILATION_STATUS_SHIFT;
}  // namespace panda

#endif  // PANDA_LIBPANDAFILE_MODIFIERS_H_
