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

#ifndef PANDA_LIBPANDABASE_UTILS_ALIGNED_STORAGE_H_
#define PANDA_LIBPANDABASE_UTILS_ALIGNED_STORAGE_H_

#include "libpandabase/utils/bit_utils.h"

namespace panda {

/**
 * Aligned storage with aligned elements.
 *
 * @tparam StructAlign Alignment of the structure
 * @tparam ElementsAlign Alignment of the elements
 * @tparam ElementsNum Number of elements in the structure, used for static checkers
 */
template <size_t StructAlign, size_t ElementsAlign, size_t ElementsNum>
struct alignas(StructAlign) AlignedStorage {
    template <typename T>
    using Aligned __attribute__((aligned(ElementsAlign))) = T;

    static constexpr size_t STRUCT_ALIGN = StructAlign;
    static constexpr size_t ELEMENTS_ALIGN = ElementsAlign;
    static constexpr size_t ELEMENTS_NUM = ElementsNum;

    static constexpr size_t GetSize()
    {
        return RoundUp(ElementsNum * ElementsAlign, StructAlign);
    }

    static constexpr size_t ConvertOffset(size_t dst_align, size_t offset)
    {
        return (dst_align > ElementsAlign) ? (offset / (dst_align / ElementsAlign))
                                           : (offset / (ElementsAlign / dst_align));
    }
};

}  // namespace panda

#endif  // PANDA_LIBPANDABASE_UTILS_ALIGNED_STORAGE_H_
