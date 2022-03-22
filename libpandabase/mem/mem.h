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

#ifndef PANDA_LIBPANDABASE_MEM_MEM_H_
#define PANDA_LIBPANDABASE_MEM_MEM_H_

#include "macros.h"
#include "utils/math_helpers.h"

#include <cstddef>
#include <cstdint>
#include <cmath>
#include <functional>

namespace panda {

namespace mem {
class GCRoot;

class MemStatsAdditionalInfo;
class MemStatsDefault;
class MemRange;

#ifndef NDEBUG
using MemStatsType = MemStatsAdditionalInfo;
#else
using MemStatsType = MemStatsDefault;
#endif
}  // namespace mem

class ObjectHeader;

#ifdef PANDA_USE_32_BIT_POINTER
using object_pointer_type = uint32_t;
#else
using object_pointer_type = uintptr_t;
#endif

constexpr size_t OBJECT_POINTER_SIZE = sizeof(object_pointer_type);

/**
 * \brief Logarithmic/bit alignment
 */
enum Alignment {
    LOG_ALIGN_2 = 2,
    LOG_ALIGN_3 = 3,
    LOG_ALIGN_4 = 4,
    LOG_ALIGN_5 = 5,
    LOG_ALIGN_6 = 6,
    LOG_ALIGN_7 = 7,
    LOG_ALIGN_8 = 8,
    LOG_ALIGN_9 = 9,
    LOG_ALIGN_10 = 10,
    LOG_ALIGN_11 = 11,
    LOG_ALIGN_12 = 12,
    LOG_ALIGN_13 = 13,
    LOG_ALIGN_MIN = LOG_ALIGN_2,
    LOG_ALIGN_MAX = LOG_ALIGN_13,
};

/**
 * \brief gets alignment in bytes.
 * @param logAlignment - logarithmic alignment
 * @return alignment in bytes
 */
constexpr size_t GetAlignmentInBytes(const Alignment LOG_ALIGNMENT)
{
    return 1U << static_cast<uint32_t>(LOG_ALIGNMENT);
}

/**
 * \brief returns log2 for alignment in bytes
 * @param ALIGNMENT_IN_BYTES - should be a power of 2
 * @return alignment in bits
 */
constexpr Alignment GetLogAlignment(const uint32_t ALIGNMENT_IN_BYTES)
{
    using helpers::math::GetIntLog2;
    // check if it is a power of 2
    ASSERT((ALIGNMENT_IN_BYTES != 0) && !(ALIGNMENT_IN_BYTES & (ALIGNMENT_IN_BYTES - 1)));
    ASSERT(GetIntLog2(ALIGNMENT_IN_BYTES) >= Alignment::LOG_ALIGN_MIN);
    ASSERT(GetIntLog2(ALIGNMENT_IN_BYTES) <= Alignment::LOG_ALIGN_MAX);
    return static_cast<Alignment>(GetIntLog2(ALIGNMENT_IN_BYTES));
}

constexpr size_t AlignUp(size_t value, size_t alignment)
{
    return (value + alignment - 1U) & ~(alignment - 1U);
}

constexpr size_t AlignDown(size_t value, size_t alignment)
{
    return value & ~(alignment - 1U);
}

template <class T>
inline uintptr_t ToUintPtr(T *val)
{
    return reinterpret_cast<uintptr_t>(val);
}

inline uintptr_t ToUintPtr(std::nullptr_t)
{
    return reinterpret_cast<uintptr_t>(nullptr);
}

template <class T>
inline T *ToNativePtr(uintptr_t val)
{
    return reinterpret_cast<T *>(val);
}

inline void *ToVoidPtr(uintptr_t val)
{
    return reinterpret_cast<void *>(val);
}

constexpr Alignment DEFAULT_ALIGNMENT = GetLogAlignment(alignof(uintptr_t));
constexpr size_t DEFAULT_ALIGNMENT_IN_BYTES = GetAlignmentInBytes(DEFAULT_ALIGNMENT);

constexpr size_t GetAlignedObjectSize(size_t size)
{
    return AlignUp(size, DEFAULT_ALIGNMENT_IN_BYTES);
}

/*
    uint64_t return type usage in memory literals for giving
    compile-time error in case of integer overflow
*/

constexpr uint64_t SHIFT_KB = 10ULL;
constexpr uint64_t SHIFT_MB = 20ULL;
constexpr uint64_t SHIFT_GB = 30ULL;

constexpr uint64_t operator"" _KB(long double count)
{
    return count * (1ULL << SHIFT_KB);
}

// NOLINTNEXTLINE(google-runtime-int)
constexpr uint64_t operator"" _KB(unsigned long long count)
{
    return count * (1ULL << SHIFT_KB);
}

constexpr uint64_t operator"" _MB(long double count)
{
    return count * (1ULL << SHIFT_MB);
}

// NOLINTNEXTLINE(google-runtime-int)
constexpr uint64_t operator"" _MB(unsigned long long count)
{
    return count * (1ULL << SHIFT_MB);
}

constexpr uint64_t operator"" _GB(long double count)
{
    return count * (1ULL << SHIFT_GB);
}

// NOLINTNEXTLINE(google-runtime-int)
constexpr uint64_t operator"" _GB(unsigned long long count)
{
    return count * (1ULL << SHIFT_GB);
}

constexpr uint64_t SIZE_1K = 1_KB;
constexpr uint64_t SIZE_1M = 1_MB;
constexpr uint64_t SIZE_1G = 1_GB;

constexpr uint64_t PANDA_MAX_HEAP_SIZE = 4_GB;
constexpr size_t PANDA_POOL_ALIGNMENT_IN_BYTES = 256_KB;

constexpr size_t PANDA_DEFAULT_POOL_SIZE = 1_MB;
constexpr size_t PANDA_DEFAULT_ARENA_SIZE = 1_MB;
constexpr size_t PANDA_DEFAULT_ALLOCATOR_POOL_SIZE = 4_MB;
static_assert(PANDA_DEFAULT_POOL_SIZE % PANDA_POOL_ALIGNMENT_IN_BYTES == 0);
static_assert(PANDA_DEFAULT_ARENA_SIZE % PANDA_POOL_ALIGNMENT_IN_BYTES == 0);
static_assert(PANDA_DEFAULT_ALLOCATOR_POOL_SIZE % PANDA_POOL_ALIGNMENT_IN_BYTES == 0);

static constexpr Alignment DEFAULT_FRAME_ALIGNMENT = LOG_ALIGN_6;

constexpr uintptr_t PANDA_32BITS_HEAP_START_ADDRESS = AlignUp(72_KB, PANDA_POOL_ALIGNMENT_IN_BYTES);
constexpr uint64_t PANDA_32BITS_HEAP_END_OBJECTS_ADDRESS = 4_GB;

inline bool IsInObjectsAddressSpace([[maybe_unused]] uintptr_t address)
{
#ifdef PANDA_USE_32_BIT_POINTER
    return address == ToUintPtr(nullptr) ||
           (address >= PANDA_32BITS_HEAP_START_ADDRESS && address < PANDA_32BITS_HEAP_END_OBJECTS_ADDRESS);
#else  // In this case, all 64 bits addresses are valid
    return true;
#endif
}

template <class T>
inline object_pointer_type ToObjPtrType(T *val)
{
    ASSERT(IsInObjectsAddressSpace(ToUintPtr(val)));
    return static_cast<object_pointer_type>(ToUintPtr(val));
}

inline object_pointer_type ToObjPtrType(std::nullptr_t)
{
    return static_cast<object_pointer_type>(ToUintPtr(nullptr));
}

enum class ObjectStatus : bool {
    DEAD_OBJECT,
    ALIVE_OBJECT,
};

using MemVisitor = std::function<void(void *mem, size_t size)>;
using GCObjectVisitor = std::function<ObjectStatus(ObjectHeader *)>;
using ObjectMoveVisitor = std::add_pointer<size_t(void *mem)>::type;
using ObjectVisitor = std::function<void(ObjectHeader *)>;
/**
 * from_object is object from which we found to_object by reference.
 */
using ObjectVisitorEx = std::function<void(ObjectHeader *from_object, ObjectHeader *to_object)>;
using ObjectChecker = std::function<bool(const ObjectHeader *)>;
using GCRootVisitor = std::function<void(const mem::GCRoot &)>;
using MemRangeChecker = std::function<bool(mem::MemRange &)>;

inline bool NoFilterChecker([[maybe_unused]] const ObjectHeader *object_header)
{
    return true;
}

inline ObjectStatus GCKillEmAllVisitor([[maybe_unused]] ObjectHeader *mem)
{
    return ObjectStatus::DEAD_OBJECT;
}

}  // namespace panda

#endif  // PANDA_LIBPANDABASE_MEM_MEM_H_
