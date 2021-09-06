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

#ifndef PANDA_RUNTIME_INCLUDE_CORETYPES_ARRAY_INL_H_
#define PANDA_RUNTIME_INCLUDE_CORETYPES_ARRAY_INL_H_

#include <type_traits>

#include "runtime/include/coretypes/array.h"
#include "runtime/include/exceptions.h"
#include "runtime/include/object_accessor-inl.h"
#include "runtime/mem/vm_handle.h"

namespace panda::coretypes {

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <class T, bool is_volatile /* = false */>
inline T Array::GetPrimitive(size_t offset) const
{
    return ObjectAccessor::GetPrimitive<T, is_volatile>(this, GetDataOffset() + offset);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <class T, bool is_volatile /* = false */>
inline void Array::SetPrimitive(size_t offset, T value)
{
    ObjectAccessor::SetPrimitive<T, is_volatile>(this, GetDataOffset() + offset, value);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool is_volatile /* = false */, bool need_read_barrier /* = true */, bool is_dyn /* = false */>
inline ObjectHeader *Array::GetObject(int offset) const
{
    return ObjectAccessor::GetObject<is_volatile, need_read_barrier, is_dyn>(this, GetDataOffset() + offset);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool is_volatile /* = false */, bool need_write_barrier /* = true */, bool is_dyn /* = false */>
inline void Array::SetObject(size_t offset, ObjectHeader *value)
{
    ObjectAccessor::SetObject<is_volatile, need_write_barrier, is_dyn>(this, GetDataOffset() + offset, value);
}

template <class T>
inline T Array::GetPrimitive(size_t offset, std::memory_order memory_order) const
{
    return ObjectAccessor::GetFieldPrimitive<T>(this, GetDataOffset() + offset, memory_order);
}

template <class T>
inline void Array::SetPrimitive(size_t offset, T value, std::memory_order memory_order)
{
    ObjectAccessor::SetFieldPrimitive(this, GetDataOffset() + offset, value, memory_order);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool need_read_barrier /* = true */, bool is_dyn /* = false */>
inline ObjectHeader *Array::GetObject(size_t offset, std::memory_order memory_order) const
{
    return ObjectAccessor::GetFieldObject<need_read_barrier, is_dyn>(this, GetDataOffset() + offset, memory_order);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool need_write_barrier /* = true */, bool is_dyn /* = false */>
inline void Array::SetObject(size_t offset, ObjectHeader *value, std::memory_order memory_order)
{
    ObjectAccessor::SetFieldObject<need_write_barrier, is_dyn>(this, GetDataOffset() + offset, value, memory_order);
}

template <typename T>
inline bool Array::CompareAndSetPrimitive(size_t offset, T old_value, T new_value, std::memory_order memory_order,
                                          bool strong)
{
    return ObjectAccessor::CompareAndSetFieldPrimitive(this, GetDataOffset() + offset, old_value, new_value,
                                                       memory_order, strong)
        .first;
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool need_write_barrier /* = true */, bool is_dyn /* = false */>
inline bool Array::CompareAndSetObject(size_t offset, ObjectHeader *old_value, ObjectHeader *new_value,
                                       std::memory_order memory_order, bool strong)
{
    auto field_offset = GetDataOffset() + offset;
    return ObjectAccessor::CompareAndSetFieldObject<need_write_barrier, is_dyn>(this, field_offset, old_value,
                                                                                new_value, memory_order, strong)
        .first;
}

template <typename T>
inline T Array::CompareAndExchangePrimitive(size_t offset, T old_value, T new_value, std::memory_order memory_order,
                                            bool strong)
{
    return ObjectAccessor::CompareAndSetFieldPrimitive(this, GetDataOffset() + offset, old_value, new_value,
                                                       memory_order, strong)
        .second;
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool need_write_barrier /* = true */, bool is_dyn /* = false */>
inline ObjectHeader *Array::CompareAndExchangeObject(size_t offset, ObjectHeader *old_value, ObjectHeader *new_value,
                                                     std::memory_order memory_order, bool strong)
{
    auto field_offset = GetDataOffset() + offset;
    return ObjectAccessor::CompareAndSetFieldObject<need_write_barrier, is_dyn>(this, field_offset, old_value,
                                                                                new_value, memory_order, strong)
        .second;
}

template <typename T>
inline T Array::GetAndSetPrimitive(size_t offset, T value, std::memory_order memory_order)
{
    return ObjectAccessor::GetAndSetFieldPrimitive(this, GetDataOffset() + offset, value, memory_order);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool need_write_barrier /* = true */, bool is_dyn /* = false */>
inline ObjectHeader *Array::GetAndSetObject(size_t offset, ObjectHeader *value, std::memory_order memory_order)
{
    return ObjectAccessor::GetAndSetFieldObject<need_write_barrier, is_dyn>(this, GetDataOffset() + offset, value,
                                                                            memory_order);
}

template <typename T>
inline T Array::GetAndAddPrimitive(size_t offset, T value, std::memory_order memory_order)
{
    return ObjectAccessor::GetAndAddFieldPrimitive(this, GetDataOffset() + offset, value, memory_order);
}

template <typename T>
inline T Array::GetAndBitwiseOrPrimitive(size_t offset, T value, std::memory_order memory_order)
{
    return ObjectAccessor::GetAndBitwiseOrFieldPrimitive(this, GetDataOffset() + offset, value, memory_order);
}

template <typename T>
inline T Array::GetAndBitwiseAndPrimitive(size_t offset, T value, std::memory_order memory_order)
{
    return ObjectAccessor::GetAndBitwiseAndFieldPrimitive(this, GetDataOffset() + offset, value, memory_order);
}

template <typename T>
inline T Array::GetAndBitwiseXorPrimitive(size_t offset, T value, std::memory_order memory_order)
{
    return ObjectAccessor::GetAndBitwiseXorFieldPrimitive(this, GetDataOffset() + offset, value, memory_order);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <class T, bool need_write_barrier /* = true */, bool is_dyn /* = false */>
inline void Array::Set(array_size_t idx, T elem)
{
    constexpr bool IS_REF = std::is_pointer_v<T> && std::is_base_of_v<ObjectHeader, std::remove_pointer_t<T>>;

    static_assert(std::is_arithmetic_v<T> || IS_REF, "T should be arithmetic type or pointer to managed object type");

    size_t elem_size = (IS_REF && !is_dyn) ? sizeof(object_pointer_type) : sizeof(T);
    size_t offset = elem_size * idx;

    // Disable checks due to clang-tidy bug https://bugs.llvm.org/show_bug.cgi?id=32203
    // NOLINTNEXTLINE(readability-braces-around-statements)
    if constexpr (IS_REF) {
        ObjectAccessor::SetObject<false, need_write_barrier, is_dyn>(this, GetDataOffset() + offset, elem);
        // NOLINTNEXTLINE(readability-misleading-indentation)
    } else {
        ObjectAccessor::SetPrimitive(this, GetDataOffset() + offset, elem);
    }
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <class T, bool need_read_barrier /* = true */, bool is_dyn /* = false */>
inline T Array::Get(array_size_t idx) const
{
    constexpr bool IS_REF = std::is_pointer_v<T> && std::is_base_of_v<ObjectHeader, std::remove_pointer_t<T>>;

    static_assert(std::is_arithmetic_v<T> || IS_REF, "T should be arithmetic type or pointer to managed object type");

    size_t elem_size = (IS_REF && !is_dyn) ? sizeof(object_pointer_type) : sizeof(T);
    size_t offset = elem_size * idx;

    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if constexpr (IS_REF) {
        return ObjectAccessor::GetObject<false, need_read_barrier, is_dyn>(this, GetDataOffset() + offset);
    }

    return ObjectAccessor::GetPrimitive<T>(this, GetDataOffset() + offset);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <class T, bool need_write_barrier /* = true */, bool is_dyn /* = false */>
inline void Array::Set([[maybe_unused]] const ManagedThread *thread, array_size_t idx, T elem)
{
    constexpr bool IS_REF = std::is_pointer_v<T> && std::is_base_of_v<ObjectHeader, std::remove_pointer_t<T>>;

    static_assert(std::is_arithmetic_v<T> || IS_REF, "T should be arithmetic type or pointer to managed object type");

    size_t elem_size = (IS_REF && !is_dyn) ? sizeof(object_pointer_type) : sizeof(T);
    size_t offset = elem_size * idx;

    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if constexpr (IS_REF) {
        ObjectAccessor::SetObject<false, need_write_barrier, is_dyn>(thread, this, GetDataOffset() + offset, elem);
    } else {  // NOLINTNEXTLINE(readability-misleading-indentation)
        ObjectAccessor::SetPrimitive(this, GetDataOffset() + offset, elem);
    }
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <class T, bool need_read_barrier /* = true */, bool is_dyn /* = false */>
inline T Array::Get([[maybe_unused]] const ManagedThread *thread, array_size_t idx) const
{
    constexpr bool IS_REF = std::is_pointer_v<T> && std::is_base_of_v<ObjectHeader, std::remove_pointer_t<T>>;

    static_assert(std::is_arithmetic_v<T> || IS_REF, "T should be arithmetic type or pointer to managed object type");

    size_t elem_size = (IS_REF && !is_dyn) ? sizeof(object_pointer_type) : sizeof(T);
    size_t offset = elem_size * idx;

    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if constexpr (IS_REF) {
        return ObjectAccessor::GetObject<false, need_read_barrier, is_dyn>(thread, this, GetDataOffset() + offset);
    }
    return ObjectAccessor::GetPrimitive<T>(this, GetDataOffset() + offset);
}

/* static */
template <class DimIterator>
Array *Array::CreateMultiDimensionalArray(ManagedThread *thread, panda::Class *klass, uint32_t nargs,
                                          const DimIterator &iter, size_t dim_idx)
{
    auto arr_size = iter.Get(dim_idx);
    if (arr_size < 0) {
        panda::ThrowNegativeArraySizeException(arr_size);
        return nullptr;
    }
    [[maybe_unused]] HandleScope<ObjectHeader *> scope(thread);
    VMHandle<Array> handle(thread, Array::Create(klass, arr_size));

    // avoid recursive OOM.
    if (handle.GetPtr() == nullptr) {
        return nullptr;
    }
    auto *component = klass->GetComponentType();

    if (component->IsArrayClass() && dim_idx + 1 < nargs) {
        for (int32_t idx = 0; idx < arr_size; idx++) {
            auto *array = CreateMultiDimensionalArray(thread, component, nargs, iter, dim_idx + 1);
            if (array == nullptr) {
                return nullptr;
            }

            // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE)
            handle.GetPtr()->template Set<Array *>(idx, array);
        }
    }

    return handle.GetPtr();
}
}  // namespace panda::coretypes

#endif  // PANDA_RUNTIME_INCLUDE_CORETYPES_ARRAY_INL_H_
