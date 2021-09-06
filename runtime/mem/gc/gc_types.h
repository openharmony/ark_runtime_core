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

#ifndef PANDA_RUNTIME_MEM_GC_GC_TYPES_H_
#define PANDA_RUNTIME_MEM_GC_GC_TYPES_H_

#include <array>
#include <string_view>

namespace panda::mem {

enum class GCExecutionMode {
    GC_STW_NO_MT,  // Stop-the-world, single thread
    GC_EXECUTION_MODE_LAST = GC_STW_NO_MT
};

constexpr GCExecutionMode GC_EXECUTION_MODE = GCExecutionMode::GC_STW_NO_MT;

enum class GCType {
    INVALID_GC = 0,
    EPSILON_GC,
    STW_GC,
    HYBRID_GC,
    GEN_GC,
    G1_GC,
    GCTYPE_LAST = G1_GC,
};

constexpr bool IsGenerationalGCType(const GCType gc_type)
{
    bool ret = false;
    ASSERT(gc_type != GCType::INVALID_GC);
    switch (gc_type) {
        case GCType::GEN_GC:
        case GCType::G1_GC:
            ret = true;
            break;
        case GCType::INVALID_GC:
        case GCType::EPSILON_GC:
        case GCType::STW_GC:
        case GCType::HYBRID_GC:
            break;
        default:
            UNREACHABLE();
            break;
    }
    return ret;
}

constexpr size_t ToIndex(GCType type)
{
    return static_cast<size_t>(type);
}

constexpr size_t GC_TYPE_SIZE = ToIndex(GCType::GCTYPE_LAST) + 1;
constexpr std::array<char const *, GC_TYPE_SIZE> GC_NAMES = {"Invalid GC", "Epsilon GC", "Stop-The-World GC",
                                                             "Hybrid GC", "Generation GC"};

constexpr bool StringsEqual(char const *a, char const *b)
{
    return std::string_view(a) == b;
}

static_assert(StringsEqual(GC_NAMES[ToIndex(GCType::INVALID_GC)], "Invalid GC"));
static_assert(StringsEqual(GC_NAMES[ToIndex(GCType::EPSILON_GC)], "Epsilon GC"));
static_assert(StringsEqual(GC_NAMES[ToIndex(GCType::STW_GC)], "Stop-The-World GC"));
static_assert(StringsEqual(GC_NAMES[ToIndex(GCType::HYBRID_GC)], "Hybrid GC"));
static_assert(StringsEqual(GC_NAMES[ToIndex(GCType::GEN_GC)], "Generation GC"));

inline GCType GCTypeFromString(std::string_view gc_type_str)
{
    if (gc_type_str == "epsilon") {
        return GCType::EPSILON_GC;
    }
    if (gc_type_str == "stw") {
        return GCType::STW_GC;
    }
    if (gc_type_str == "gen-gc") {
        return GCType::GEN_GC;
    }
    if (gc_type_str == "hybrid-gc") {
        return GCType::HYBRID_GC;
    }
    if (gc_type_str == "g1-gc") {
        return GCType::G1_GC;
    }
    return GCType::INVALID_GC;
}

inline std::string_view GCStringFromType(GCType gc_type)
{
    if (gc_type == GCType::EPSILON_GC) {
        return "epsilon";
    }
    if (gc_type == GCType::STW_GC) {
        return "stw";
    }
    if (gc_type == GCType::GEN_GC) {
        return "gen-gc";
    }
    if (gc_type == GCType::HYBRID_GC) {
        return "hybrid-gc";
    }
    return "invalid-gc";
}

enum GCCollectMode : uint8_t {
    GC_NONE = 0,          // Non collected objects
    GC_MINOR = 1U,        // Objects collected at the minor GC
    GC_MAJOR = 1U << 1U,  // Objects collected at the major GC (MAJOR usually includes MINOR)
    GC_FULL = 1U << 2U,   // Can collect objects from some spaces which very rarely contains GC candidates
    // NOLINTNEXTLINE(hicpp-signed-bitwise)
    GC_ALL = GC_MINOR | GC_MAJOR | GC_FULL,  // Can collect objects at any phase
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_GC_GC_TYPES_H_
