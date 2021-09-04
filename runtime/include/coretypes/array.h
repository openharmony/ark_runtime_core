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

#ifndef PANDA_RUNTIME_INCLUDE_CORETYPES_ARRAY_H_
#define PANDA_RUNTIME_INCLUDE_CORETYPES_ARRAY_H_

#include <securec.h>
#include <cstddef>
#include <cstdint>

#include "libpandabase/macros.h"
#include "libpandabase/mem/mem.h"
#include "libpandabase/mem/space.h"
#include "libpandabase/utils/span.h"
#include "libpandafile/bytecode_instruction-inl.h"
#include "runtime/include/class-inl.h"
#include "runtime/include/language_context.h"
#include "runtime/include/object_header.h"
#include "runtime/mem/heap_manager.h"
#include "runtime/include/coretypes/tagged_value.h"

namespace panda {
class ManagedThread;
class PandaVM;
}  // namespace panda

namespace panda::interpreter {
template <BytecodeInstruction::Format format>
class DimIterator;
}  // namespace panda::interpreter

namespace panda::coretypes {
class DynClass;
using array_size_t = panda::array_size_t;
using array_ssize_t = panda::array_ssize_t;

class Array : public ObjectHeader {
public:
    static constexpr array_size_t MAX_ARRAY_INDEX = std::numeric_limits<array_size_t>::max();

    static Array *Cast(ObjectHeader *object)
    {
        return reinterpret_cast<Array *>(object);
    }

    static Array *Create(panda::Class *array_class, const uint8_t *data, array_size_t length,
                         panda::SpaceType space_type = panda::SpaceType::SPACE_TYPE_OBJECT);

    static Array *Create(panda::Class *array_class, array_size_t length,
                         panda::SpaceType space_type = panda::SpaceType::SPACE_TYPE_OBJECT);

    static Array *Create(DynClass *dynarrayclass, array_size_t length,
                         panda::SpaceType space_type = panda::SpaceType::SPACE_TYPE_OBJECT);

    static Array *CreateTagged(const PandaVM *vm, panda::BaseClass *array_class, array_size_t length,
                               panda::SpaceType space_type = panda::SpaceType::SPACE_TYPE_OBJECT,
                               TaggedValue init_value = TaggedValue::Undefined());

    static size_t ComputeSize(size_t elem_size, array_size_t length)
    {
        ASSERT(elem_size != 0);
        size_t size = sizeof(Array) + elem_size * length;
#ifdef PANDA_TARGET_32
        size_t size_limit = (std::numeric_limits<size_t>::max() - sizeof(Array)) / elem_size;
        if (UNLIKELY(size_limit < static_cast<size_t>(length))) {
            return 0;
        }
#endif
        return size;
    }

    array_size_t GetLength() const
    {
        return length_.load(std::memory_order_relaxed);
    }

    uint32_t *GetData()
    {
        return data_;
    }

    const uint32_t *GetData() const
    {
        return data_;
    }

    template <class T, bool is_volatile = false>
    T GetPrimitive(size_t offset) const;

    template <class T, bool is_volatile = false>
    void SetPrimitive(size_t offset, T value);

    template <bool is_volatile = false, bool need_read_barrier = true, bool is_dyn = false>
    ObjectHeader *GetObject(int offset) const;

    template <bool is_volatile = false, bool need_write_barrier = true, bool is_dyn = false>
    void SetObject(size_t offset, ObjectHeader *value);

    template <class T>
    T GetPrimitive(size_t offset, std::memory_order memory_order) const;

    template <class T>
    void SetPrimitive(size_t offset, T value, std::memory_order memory_order);

    template <bool need_read_barrier = true, bool is_dyn = false>
    ObjectHeader *GetObject(size_t offset, std::memory_order memory_order) const;

    template <bool need_write_barrier = true, bool is_dyn = false>
    void SetObject(size_t offset, ObjectHeader *value, std::memory_order memory_order);

    template <typename T>
    bool CompareAndSetPrimitive(size_t offset, T old_value, T new_value, std::memory_order memory_order, bool strong);

    template <bool need_write_barrier = true, bool is_dyn = false>
    bool CompareAndSetObject(size_t offset, ObjectHeader *old_value, ObjectHeader *new_value,
                             std::memory_order memory_order, bool strong);

    template <typename T>
    T CompareAndExchangePrimitive(size_t offset, T old_value, T new_value, std::memory_order memory_order, bool strong);

    template <bool need_write_barrier = true, bool is_dyn = false>
    ObjectHeader *CompareAndExchangeObject(size_t offset, ObjectHeader *old_value, ObjectHeader *new_value,
                                           std::memory_order memory_order, bool strong);

    template <typename T>
    T GetAndSetPrimitive(size_t offset, T value, std::memory_order memory_order);

    template <bool need_write_barrier = true, bool is_dyn = false>
    ObjectHeader *GetAndSetObject(size_t offset, ObjectHeader *value, std::memory_order memory_order);

    template <typename T>
    T GetAndAddPrimitive(size_t offset, T value, std::memory_order memory_order);

    template <typename T>
    T GetAndBitwiseOrPrimitive(size_t offset, T value, std::memory_order memory_order);

    template <typename T>
    T GetAndBitwiseAndPrimitive(size_t offset, T value, std::memory_order memory_order);

    template <typename T>
    T GetAndBitwiseXorPrimitive(size_t offset, T value, std::memory_order memory_order);

    template <class T, bool need_write_barrier = true, bool is_dyn = false>
    void Set(array_size_t idx, T elem);

    template <class T, bool need_read_barrier = true, bool is_dyn = false>
    T Get(array_size_t idx) const;

    // Pass thread parameter to speed up interpreter
    template <class T, bool need_write_barrier = true, bool is_dyn = false>
    void Set([[maybe_unused]] const ManagedThread *thread, array_size_t idx, T elem);

    template <class T, bool need_read_barrier = true, bool is_dyn = false>
    T Get([[maybe_unused]] const ManagedThread *thread, array_size_t idx) const;

    size_t ObjectSize() const
    {
        return ComputeSize(ClassAddr<panda::Class>()->GetComponentSize(), length_);
    }

    static constexpr uint32_t GetLengthOffset()
    {
        return MEMBER_OFFSET(Array, length_);
    }

    static constexpr uint32_t GetDataOffset()
    {
        return MEMBER_OFFSET(Array, data_);
    }

    template <class DimIterator>
    static Array *CreateMultiDimensionalArray(ManagedThread *thread, panda::Class *klass, uint32_t nargs,
                                              const DimIterator &iter, size_t dim_idx = 0);

private:
    void SetLength(array_size_t length)
    {
        length_.store(length, std::memory_order_relaxed);
    }

    std::atomic<array_size_t> length_;
    // Align with 64bits, because dynamic language data is always 64bits
    __extension__ alignas(sizeof(uint64_t)) uint32_t data_[0];  // NOLINT(modernize-avoid-c-arrays)
};

static_assert(Array::GetLengthOffset() == sizeof(ObjectHeader));
static_assert(Array::GetDataOffset() == AlignUp(Array::GetLengthOffset() + sizeof(array_size_t), sizeof(uint64_t)));
static_assert(Array::GetDataOffset() % sizeof(uint64_t) == 0);

#ifdef PANDA_TARGET_64
constexpr uint32_t ARRAY_LENGTH_OFFSET = 8U;
static_assert(ARRAY_LENGTH_OFFSET == panda::coretypes::Array::GetLengthOffset());
constexpr uint32_t ARRAY_DATA_OFFSET = 16U;
static_assert(ARRAY_DATA_OFFSET == panda::coretypes::Array::GetDataOffset());
#endif

}  // namespace panda::coretypes

#endif  // PANDA_RUNTIME_INCLUDE_CORETYPES_ARRAY_H_
