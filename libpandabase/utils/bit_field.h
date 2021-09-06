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

#ifndef PANDA_LIBPANDABASE_UTILS_BIT_FIELD_H_
#define PANDA_LIBPANDABASE_UTILS_BIT_FIELD_H_

#include "macros.h"

namespace panda {

/*
 * Auxiliary static class that provides access to bits range within an integer value.
 */
template <typename T, size_t start, size_t bits_num = 1>
class BitField {
    static constexpr unsigned BITS_PER_BYTE = 8;

    static_assert(start < sizeof(uint64_t) * BITS_PER_BYTE, "Invalid position");
    static_assert(bits_num != 0U, "Invalid size");
    static_assert(bits_num <= sizeof(uint64_t) * BITS_PER_BYTE, "Invalid size");
    static_assert(bits_num + start <= sizeof(uint64_t) * BITS_PER_BYTE, "Invalid position + size");

public:
    using ValueType = T;
    static constexpr unsigned START_BIT = start;
    static constexpr unsigned END_BIT = start + bits_num;
    static constexpr unsigned SIZE = bits_num;

    /*
     * This is static class and should not be instantiated.
     */
    BitField() = delete;

    virtual ~BitField() = delete;

    NO_COPY_SEMANTIC(BitField);
    NO_MOVE_SEMANTIC(BitField);

    /*
     * Make BitField type follow right after current bit range.
     *
     *  If we have
     *   BitField<T, 0, 9>
     * then
     *   BitField<T, 0, 9>::NextField<T,3>
     * will be equal to
     *   BitField<T, 9, 3>
     *
     * It is helpful when we need to specify chain of fields.
     */
    template <typename T2, unsigned bits_num2>
    using NextField = BitField<T2, start + bits_num, bits_num2>;

    /*
     * Make Flag field follow right after current bit range.
     * Same as NextField, but no need to specify number of bits. It is always 1.
     */
    using NextFlag = BitField<bool, start + bits_num, 1>;

public:
    /*
     * Return mask of bit range, i.e. 0b1110 for BitField<T, 1, 3>
     */
    static constexpr uint64_t Mask()
    {
        return ((1LLU << bits_num) - 1) << start;
    }

    /*
     * Check if given value fits into the bit field
     */
    static constexpr bool IsValid(T value)
    {
        return (static_cast<uint64_t>(value) & ~((1LLU << bits_num) - 1)) == 0;
    }

    /*
     * Set 'value' to current bit range [START_BIT : START_BIT+END_BIT] within the 'stor' parameter.
     */
    template <typename Stor>
    static constexpr void Set(T value, Stor *stor)
    {
        *stor = (*stor & ~Mask()) | (static_cast<uint64_t>(value) << start);
    }

    /*
     * Return bit range [START_BIT : START_BIT+END_BIT] value from given integer 'value'
     */
    constexpr static T Get([[maybe_unused]] uint64_t value)
    {
        return static_cast<T>((value >> start) & ((1LLU << bits_num) - 1));
    }

    /*
     * Encode 'value' to current bit range [START_BIT : START_BIT+END_BIT] and return it
     */
    static uint64_t Encode(T value)
    {
        ASSERT(IsValid(value));
        return (static_cast<uint64_t>(value) << start);
    }

    /*
     * Update 'value' to current bit range [START_BIT : START_BIT+END_BIT] and return it
     */
    static uint64_t Update(uint64_t old_value, T value)
    {
        ASSERT(IsValid(value));
        return (old_value & ~Mask()) | (static_cast<uint64_t>(value) << start);
    }

    /*
     * Decode from value
     */
    static T Decode(uint64_t value)
    {
        return static_cast<T>((value >> start) & ((1LLU << bits_num) - 1));
    }
};

}  // namespace panda

#endif  // PANDA_LIBPANDABASE_UTILS_BIT_FIELD_H_
