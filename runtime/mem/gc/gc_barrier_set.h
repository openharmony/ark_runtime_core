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

#ifndef PANDA_RUNTIME_MEM_GC_GC_BARRIER_SET_H_
#define PANDA_RUNTIME_MEM_GC_GC_BARRIER_SET_H_

#include "libpandabase/mem/gc_barrier.h"
#include "runtime/include/mem/panda_containers.h"
#include "runtime/include/mem/panda_string.h"

namespace panda::mem {

/**
 * Base barrier set
 */
class GCBarrierSet {
public:
    GCBarrierSet() = delete;
    GCBarrierSet(mem::InternalAllocatorPtr allocator, BarrierType pre_type, BarrierType post_type)
        : pre_type_(pre_type),
          post_type_(post_type),
          pre_operands_(allocator->Adapter()),
          post_operands_(allocator->Adapter())
    {
    }

    NO_COPY_SEMANTIC(GCBarrierSet);
    NO_MOVE_SEMANTIC(GCBarrierSet);
    virtual ~GCBarrierSet() = 0;

    BarrierType GetPreType() const
    {
        ASSERT(IsPreBarrier(pre_type_));
        return pre_type_;
    }

    BarrierType GetPostType() const
    {
        ASSERT(IsPostBarrier(post_type_));
        return post_type_;
    }

    /**
     * Pre barrier. Used by interpreter.
     * @param obj_field_addr - address of field where we store. It can be unused in most cases
     * @param pre_val_addr - reference currently(before store/load happened) stored in the field
     */
    virtual void PreBarrier(const void *obj_field_addr, void *pre_val_addr) = 0;

    /**
     * Post barrier. Used by interpeter.
     * @param obj_addr - address of field where we store
     * @param val_addr - reference stored into or loaded from the field
     */
    virtual void PostBarrier(const void *obj_addr, void *val_addr) = 0;

    /**
     * Post barrier for array write. Used by interpeter.
     * @param obj_addr - address of the array object
     * @param size - size of the array object
     */
    virtual void PostBarrierArrayWrite(const void *obj_addr, size_t size) = 0;

    /**
     * Post barrier for writing in every field of an object. Used by interpeter.
     * @param object_addr - address of the object
     * @param size - size of the object
     */
    virtual void PostBarrierEveryObjectFieldWrite(const void *obj_addr, size_t size) = 0;

    /**
     * Get barrier operand (literal, function pointer, address etc. See enum BarrierType for details.
     * Should be used for barrier generation in Compiler.
     * @param name - string with name of operand
     * @return barrier operand (value is address or literal)
     */
    BarrierOperand GetBarrierOperand(BarrierPosition barrier_position, std::string_view name);

protected:
    /**
     * Add barrier operand if there are no operands with this name
     * @param barrier_position - pre or post position of barrier with added operand
     * @param name - name of operand
     * @param barrier_operand - operand
     */
    void AddBarrierOperand(BarrierPosition barrier_position, std::string_view name,
                           const BarrierOperand &barrier_operand)
    {
        if (barrier_position == BarrierPosition::BARRIER_POSITION_PRE) {
            ASSERT(pre_operands_.find(name) == pre_operands_.end());
            pre_operands_.insert({name.data(), barrier_operand});
        } else {
            ASSERT(barrier_position == BarrierPosition::BARRIER_POSITION_POST);
            ASSERT(post_operands_.find(name) == post_operands_.end());
            post_operands_.insert({name.data(), barrier_operand});
        }
    }

private:
    BarrierType pre_type_;   // Type of PRE barrier.
    BarrierType post_type_;  // Type of POST barrier.
    PandaMap<PandaString, BarrierOperand> pre_operands_;
    PandaMap<PandaString, BarrierOperand> post_operands_;
};

/**
 * BarrierSet with barriers do nothing
 */
class GCDummyBarrierSet : public GCBarrierSet {
public:
    explicit GCDummyBarrierSet(mem::InternalAllocatorPtr allocator)
        : GCBarrierSet(allocator, BarrierType::PRE_WRB_NONE, BarrierType::POST_WRB_NONE)
    {
    }

    NO_COPY_SEMANTIC(GCDummyBarrierSet);
    NO_MOVE_SEMANTIC(GCDummyBarrierSet);
    ~GCDummyBarrierSet() override = default;

    void PreBarrier([[maybe_unused]] const void *obj_field_addr, [[maybe_unused]] void *pre_val_addr) override {}

    void PostBarrier([[maybe_unused]] const void *obj_addr, [[maybe_unused]] void *stored_val_addr) override {}

    void PostBarrierArrayWrite([[maybe_unused]] const void *obj_addr, [[maybe_unused]] size_t size) override {}

    void PostBarrierEveryObjectFieldWrite([[maybe_unused]] const void *obj_addr, [[maybe_unused]] size_t size) override
    {
    }
};

class GCGenBarrierSet : public GCBarrierSet {
public:
    GCGenBarrierSet(mem::InternalAllocatorPtr allocator, /* PRE ARGS: */ bool *concurrent_marking_flag,
                    objRefProcessFunc pre_store_func, /* POST ARGS: */
                    void *min_addr, uint8_t *card_table_addr, uint8_t card_bits, uint8_t dirty_card_value)
        : GCBarrierSet(allocator, BarrierType::PRE_SATB_BARRIER, BarrierType::POST_INTERGENERATIONAL_BARRIER),
          concurrent_marking_flag_(concurrent_marking_flag),
          pre_store_func_(pre_store_func),
          min_addr_(min_addr),
          card_table_addr_(card_table_addr),
          card_bits_(card_bits),
          dirty_card_value_(dirty_card_value)
    {
        // PRE
        AddBarrierOperand(
            BarrierPosition::BARRIER_POSITION_PRE, "CONCURRENT_MARKING_ADDR",
            BarrierOperand(BarrierOperandType::BOOL_ADDRESS, BarrierOperandValue(concurrent_marking_flag)));
        AddBarrierOperand(
            BarrierPosition::BARRIER_POSITION_PRE, "STORE_IN_BUFF_TO_MARK_FUNC",
            BarrierOperand(BarrierOperandType::FUNC_WITH_OBJ_REF_ADDRESS, BarrierOperandValue(pre_store_func)));
        // POST
        AddBarrierOperand(BarrierPosition::BARRIER_POSITION_POST, "MIN_ADDR",
                          BarrierOperand(BarrierOperandType::ADDRESS, BarrierOperandValue(min_addr)));
        AddBarrierOperand(BarrierPosition::BARRIER_POSITION_POST, "CARD_TABLE_ADDR",
                          BarrierOperand(BarrierOperandType::UINT8_ADDRESS, BarrierOperandValue(card_table_addr)));
        AddBarrierOperand(BarrierPosition::BARRIER_POSITION_POST, "CARD_BITS",
                          BarrierOperand(BarrierOperandType::UINT8_LITERAL, BarrierOperandValue(card_bits)));
        AddBarrierOperand(BarrierPosition::BARRIER_POSITION_POST, "DIRTY_VAL",
                          BarrierOperand(BarrierOperandType::UINT8_LITERAL, BarrierOperandValue(dirty_card_value)));
    }

    void PreBarrier(const void *obj_field_addr, void *pre_val_addr) override;

    void PostBarrier(const void *obj_addr, void *stored_val_addr) override;

    void PostBarrierArrayWrite(const void *obj_addr, size_t size) override;

    void PostBarrierEveryObjectFieldWrite(const void *obj_addr, size_t size) override;

    ~GCGenBarrierSet() override = default;

    NO_COPY_SEMANTIC(GCGenBarrierSet);
    NO_MOVE_SEMANTIC(GCGenBarrierSet);

private:
    // Store operands explicitly for interpreter perf
    // PRE BARRIER
    bool *concurrent_marking_flag_ {nullptr};
    objRefProcessFunc pre_store_func_ {nullptr};
    // POST BARRIER
    void *min_addr_ {nullptr};            //! Minimal address used by VM. Used as a base for card index calculation
    uint8_t *card_table_addr_ {nullptr};  //! Address of card table
    uint8_t card_bits_ {0};               //! how many bits encoded by card (i.e. size covered by card = 2^card_bits_)
    uint8_t dirty_card_value_ {0};        //! value of dirty card
};

class GCG1BarrierSet : public GCBarrierSet {
public:
    GCG1BarrierSet(mem::InternalAllocatorPtr allocator, /* PRE ARGS: */ bool *concurrent_marking_flag,
                   objRefProcessFunc pre_store_func, /* POST ARGS: */
                   void *min_addr, uint8_t *card_table_addr, uint8_t card_bits, uint8_t dirty_card_value,
                   std::function<void(const void *, const void *)> post_func, size_t region_size_bits_count)
        : GCBarrierSet(allocator, BarrierType::PRE_SATB_BARRIER, BarrierType::POST_INTERGENERATIONAL_BARRIER),
          concurrent_marking_flag_(concurrent_marking_flag),
          pre_store_func_(pre_store_func),
          min_addr_(min_addr),
          card_table_addr_(card_table_addr),
          card_bits_(card_bits),
          dirty_card_value_(dirty_card_value),
          post_func_(std::move(post_func)),
          region_size_bits_count_(region_size_bits_count)
    {
        // PRE
        AddBarrierOperand(
            BarrierPosition::BARRIER_POSITION_PRE, "CONCURRENT_MARKING_ADDR",
            BarrierOperand(BarrierOperandType::BOOL_ADDRESS, BarrierOperandValue(concurrent_marking_flag)));
        AddBarrierOperand(
            BarrierPosition::BARRIER_POSITION_PRE, "STORE_IN_BUFF_TO_MARK_FUNC",
            BarrierOperand(BarrierOperandType::FUNC_WITH_OBJ_REF_ADDRESS, BarrierOperandValue(pre_store_func)));
        // POST
        AddBarrierOperand(BarrierPosition::BARRIER_POSITION_POST, "MIN_ADDR",
                          BarrierOperand(BarrierOperandType::ADDRESS, BarrierOperandValue(min_addr)));
        AddBarrierOperand(BarrierPosition::BARRIER_POSITION_POST, "CARD_TABLE_ADDR",
                          BarrierOperand(BarrierOperandType::UINT8_ADDRESS, BarrierOperandValue(card_table_addr)));
        AddBarrierOperand(BarrierPosition::BARRIER_POSITION_POST, "CARD_BITS",
                          BarrierOperand(BarrierOperandType::UINT8_LITERAL, BarrierOperandValue(card_bits)));
        AddBarrierOperand(BarrierPosition::BARRIER_POSITION_POST, "DIRTY_VAL",
                          BarrierOperand(BarrierOperandType::UINT8_LITERAL, BarrierOperandValue(dirty_card_value)));
    }

    void PreBarrier(const void *obj_field_addr, void *pre_val_addr) override;

    void PostBarrier(const void *obj_addr, void *stored_val_addr) override;

    void PostBarrierArrayWrite(const void *obj_addr, size_t size) override;

    void PostBarrierEveryObjectFieldWrite(const void *obj_addr, size_t size) override;

    ~GCG1BarrierSet() override = default;

    NO_COPY_SEMANTIC(GCG1BarrierSet);
    NO_MOVE_SEMANTIC(GCG1BarrierSet);

private:
    using PostFuncT = std::function<void(const void *, const void *)>;
    // Store operands explicitly for interpreter perf
    // PRE BARRIER
    bool *concurrent_marking_flag_ {nullptr};
    objRefProcessFunc pre_store_func_ {nullptr};
    // POST BARRIER
    void *min_addr_ {nullptr};            //! Minimal address used by VM. Used as a base for card index calculation
    uint8_t *card_table_addr_ {nullptr};  //! Address of card table
    uint8_t card_bits_ {0};               //! how many bits encoded by card (i.e. size covered by card = 2^card_bits_)
    uint8_t dirty_card_value_ {0};        //! value of dirty card
    PostFuncT post_func_;                 //! function which is called for the post barrier if all conditions
    size_t region_size_bits_count_ {0};   //! how much bits needed for the region
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_GC_GC_BARRIER_SET_H_
