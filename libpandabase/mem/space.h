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

#ifndef PANDA_LIBPANDABASE_MEM_SPACE_H_
#define PANDA_LIBPANDABASE_MEM_SPACE_H_

#include "utils/type_helpers.h"

namespace panda {

/**
 * SpaceType and GCCollectMode provide information that needs to be collected from some allocators
 */
enum class SpaceType : size_t {
    SPACE_TYPE_UNDEFINED = 0,
    SPACE_TYPE_OBJECT,              // Space for objects (all non-humongous sizes)
    SPACE_TYPE_HUMONGOUS_OBJECT,    // Space for humongous objects
    SPACE_TYPE_NON_MOVABLE_OBJECT,  // Space for non-movable objects
    SPACE_TYPE_INTERNAL,            // Space for runtime internal needs
    SPACE_TYPE_CODE,                // Space for compiled code
    SPACE_TYPE_COMPILER,            // Space for memory allocation in compiler

    SPACE_TYPE_LAST  // Count of different types
};

constexpr SpaceType ToSpaceType(size_t index)
{
    return static_cast<SpaceType>(index);
}

constexpr size_t SPACE_TYPE_SIZE = helpers::ToUnderlying(SpaceType::SPACE_TYPE_LAST);

constexpr bool IsHeapSpace(SpaceType space_type)
{
    return (space_type == SpaceType::SPACE_TYPE_OBJECT) || (space_type == SpaceType::SPACE_TYPE_HUMONGOUS_OBJECT) ||
           (space_type == SpaceType::SPACE_TYPE_NON_MOVABLE_OBJECT);
}

inline const char *SpaceTypeToString(SpaceType type)
{
    switch (type) {
        case SpaceType::SPACE_TYPE_UNDEFINED:
            return "Undefined Space";
        case SpaceType::SPACE_TYPE_OBJECT:
            return "Object Space";
        case SpaceType::SPACE_TYPE_HUMONGOUS_OBJECT:
            return "Humongous Object Space";
        case SpaceType::SPACE_TYPE_NON_MOVABLE_OBJECT:
            return "Non Movable Space";
        case SpaceType::SPACE_TYPE_INTERNAL:
            return "Internal Space";
        case SpaceType::SPACE_TYPE_CODE:
            return "Code Space";
        case SpaceType::SPACE_TYPE_COMPILER:
            return "Compiler Space";
        default:
            return "Unknown Space";
    }
}

}  // namespace panda

#endif  // PANDA_LIBPANDABASE_MEM_SPACE_H_
