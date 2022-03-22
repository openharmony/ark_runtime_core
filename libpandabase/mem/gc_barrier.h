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

#ifndef PANDA_LIBPANDABASE_MEM_GC_BARRIER_H_
#define PANDA_LIBPANDABASE_MEM_GC_BARRIER_H_

#include "utils/bit_field.h"

#include <cstdint>
#include <variant>

namespace panda::mem {

/**
 * Represents Pre and Post barriers.
 */
enum BarrierPosition : uint8_t {
    BARRIER_POSITION_PRE = 0x1,   // Should be inserted before each store/load when reference stored/loaded
    BARRIER_POSITION_POST = 0x0,  // Should be inserted after each store/load when reference stored/loaded
};

/**
 * Indicates if the barrier is for store or load.
 */
enum BarrierActionType : uint8_t {
    WRITE_BARRIER = 0x1,  // Should be used around store
    READ_BARRIER = 0x0,   // Should be used around load
};

namespace internal {
constexpr uint8_t BARRIER_POS_OFFSET = 0U;       // offset in bits for encoding position of barrier(pre or post)
constexpr uint8_t BARRIER_WRB_FLAG_OFFSET = 1U;  // offset in bits for WRB flag
}  // namespace internal

constexpr uint8_t EncodeBarrierType(uint8_t value, BarrierPosition position, BarrierActionType action_type)
{
    // NOLINTNEXTLINE(hicpp-signed-bitwise)
    return (value << 2U) | (position << internal::BARRIER_POS_OFFSET) |
           (action_type << internal::BARRIER_WRB_FLAG_OFFSET);
}

/**
 * Eencodes barrier for the compiler.
 * PreWrite barrier can be used for avoiding object loss.
 * PostWrite barrier can be used for tracking intergenerational or interregion references.
 */
enum BarrierType : uint8_t {
    PRE_WRB_NONE = EncodeBarrierType(1U, BarrierPosition::BARRIER_POSITION_PRE, BarrierActionType::WRITE_BARRIER),
    PRE_RB_NONE = EncodeBarrierType(1U, BarrierPosition::BARRIER_POSITION_PRE, BarrierActionType::READ_BARRIER),
    POST_WRB_NONE = EncodeBarrierType(1U, BarrierPosition::BARRIER_POSITION_POST, BarrierActionType::WRITE_BARRIER),
    POST_RB_NONE = EncodeBarrierType(1U, BarrierPosition::BARRIER_POSITION_POST, BarrierActionType::READ_BARRIER),
    /**
     * Pre barrier for SATB.
     * Pseudocode:
     * load CONCURRENT_MARKING_ADDR -> concurrent_marking
     * if (UNLIKELY(concurrent_marking)) {
     *     load obj.field -> pre_val  // note: if store volatile - we need to have volatile load here
     *     if (pre_val != nullptr) {
     *         call STORE_IN_BUFF_TO_MARK_FUNC(pre_val);
     *     }
     * }
     * store obj.field <- new_val // STORE for which barrier generated
     *
     * Runtime should provide these parameters:
     * CONCURRENT_MARKING_ADDR - address of bool flag for concurrent marking
     * STORE_IN_BUFF_TO_MARK_FUNC - address of function to store replaced reference
     */
    PRE_SATB_BARRIER = EncodeBarrierType(2U, BarrierPosition::BARRIER_POSITION_PRE, BarrierActionType::WRITE_BARRIER),
    /**
     * Post barrier. Intergenerational barrier for GCs with explicit continuous young gen space. Unconditional.
     * Can be fully encoded by compiler
     * Pseudocode:
     * store obj.field <- new_val // Store for which barrier is generated
     * load AddressOf(MIN_ADDR) -> min_addr
     * load AddressOf(CARD_TABLE_ADDR) -> card_table_addr
     * card_index = (AddressOf(obj) - min_addr) >> CARD_BITS   // shift right
     * card_addr = card_table_addr + card_index
     * store card_addr <- DIRTY_VAL
     *
     * Runtime should provide these parameters:
     * MIN_ADDR - minimal address used by runtime (it is required only to support 64-bit address)
     * CARD_TABLE_ADDR - address of the start of card table raw data array
     * CARD_BITS - how many bits covered by one card (probably it will be a literal)
     * DIRTY_VAL - literals representing dirty card
     *
     * Note: If the store is built with an expensive architecture (for example, in multithreading environment) -
     * consider creating a conditional barrier, i.e. check that card is not dirty before adding it to store.
     */
    POST_INTERGENERATIONAL_BARRIER =
        EncodeBarrierType(3U, BarrierPosition::BARRIER_POSITION_POST, BarrierActionType::WRITE_BARRIER),
    /**
     * Inter-region barrier. For GCs without explicit continuous young gen space.
     * Pseudocode:
     * store obj.field <- new_val // STORE for which barrier generated
     * // Check if new_val and address of field is in different regions
     * // (each region contain 2^REGION_SIZE_BITS and aligned with 2^REGION_SIZE_BITS bytes)
     * if ((AddressOf(obj) XOR AddressOf(new_val)) >> REGION_SIZE_BITS) != 0) {
     *     call UPDATE_CARD_FUNC(obj, new_val);
     * }
     *
     * Runtime should provide these parameters:
     * REGION_SIZE_BITS - log2 of the size of region
     * UPDATE_CARD_FUNC - function which updates card corresponding to the obj.field
     */
    POST_INTERREGION_BARRIER =
        EncodeBarrierType(4U, BarrierPosition::BARRIER_POSITION_POST, BarrierActionType::WRITE_BARRIER),
    /* Note: cosider two-level card table for pre-barrier */
};

constexpr bool IsPreBarrier(BarrierType barrier_type)
{
    return BitField<uint8_t, internal::BARRIER_POS_OFFSET, 1>::Get(barrier_type) ==
           BarrierPosition::BARRIER_POSITION_PRE;
}

constexpr bool IsPostBarrier(BarrierType barrier_type)
{
    return BitField<uint8_t, internal::BARRIER_POS_OFFSET, 1>::Get(barrier_type) ==
           BarrierPosition::BARRIER_POSITION_POST;
}

constexpr bool IsWriteBarrier(BarrierType barrier_type)
{
    return BitField<uint8_t, internal::BARRIER_WRB_FLAG_OFFSET, 1>::Get(barrier_type) ==
           BarrierActionType::WRITE_BARRIER;
}

constexpr bool IsReadBarrier(BarrierType barrier_type)
{
    return BitField<uint8_t, internal::BARRIER_WRB_FLAG_OFFSET, 1>::Get(barrier_type) ==
           BarrierActionType::READ_BARRIER;
}

static_assert(IsPreBarrier(BarrierType::PRE_SATB_BARRIER));
static_assert(IsWriteBarrier(BarrierType::PRE_SATB_BARRIER));
static_assert(IsPostBarrier(BarrierType::POST_INTERGENERATIONAL_BARRIER));
static_assert(IsWriteBarrier(BarrierType::POST_INTERGENERATIONAL_BARRIER));
static_assert(IsPostBarrier(BarrierType::POST_INTERREGION_BARRIER));
static_assert(IsWriteBarrier(BarrierType::POST_INTERREGION_BARRIER));

constexpr bool IsEmptyBarrier(BarrierType barrier_type)
{
    return (barrier_type == BarrierType::PRE_WRB_NONE) || (barrier_type == BarrierType::POST_WRB_NONE) ||
           (barrier_type == BarrierType::PRE_RB_NONE) || (barrier_type == BarrierType::POST_RB_NONE);
}

using objRefProcessFunc = void (*)(void *);

enum class BarrierOperandType {
    ADDRESS = 0,                // just an address (void*)
    BOOL_ADDRESS,               // contains address of bool value (bool*)
    UINT8_ADDRESS,              // contains address of uint8_t value
    FUNC_WITH_OBJ_REF_ADDRESS,  // contains address of function with this sig: void foo(void* );
    UINT8_LITERAL,              // contains uint8_t value
};

using BarrierOperandValue = std::variant<void *, bool *, uint8_t *, objRefProcessFunc, uint8_t>;

class BarrierOperand {
public:
    // NOLINTNEXTLINE(modernize-pass-by-value)
    BarrierOperand(BarrierOperandType barrier_operand_type, BarrierOperandValue barrier_operand_value)
        : barrier_operand_type_(barrier_operand_type), barrier_operand_value_(barrier_operand_value)
    {
    }

    inline BarrierOperandType GetType()
    {
        return barrier_operand_type_;
    }

    inline BarrierOperandValue GetValue()
    {
        return barrier_operand_value_;
    }

    virtual ~BarrierOperand() = default;

    DEFAULT_COPY_SEMANTIC(BarrierOperand);
    DEFAULT_MOVE_SEMANTIC(BarrierOperand);

private:
    BarrierOperandType barrier_operand_type_;
    BarrierOperandValue barrier_operand_value_;
};

}  // namespace panda::mem

#endif  // PANDA_LIBPANDABASE_MEM_GC_BARRIER_H_
