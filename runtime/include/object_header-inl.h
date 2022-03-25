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

#ifndef PANDA_RUNTIME_INCLUDE_OBJECT_HEADER_INL_H_
#define PANDA_RUNTIME_INCLUDE_OBJECT_HEADER_INL_H_

#include "runtime/include/class-inl.h"
#include "runtime/include/field.h"
#include "runtime/include/object_accessor-inl.h"
#include "runtime/include/object_header.h"

namespace panda {

inline bool ObjectHeader::IsInstanceOf(Class *klass)
{
    return klass->IsAssignableFrom(ClassAddr<Class>());
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_COMMENT_LOCATION)
template <class T, bool is_volatile /* = false */>
inline T ObjectHeader::GetFieldPrimitive(size_t offset) const
{
    return ObjectAccessor::GetPrimitive<T, is_volatile>(this, offset);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_COMMENT_LOCATION)
template <class T, bool is_volatile /* = false */>
inline void ObjectHeader::SetFieldPrimitive(size_t offset, T value)
{
    ObjectAccessor::SetPrimitive<T, is_volatile>(this, offset, value);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool is_volatile /* = false */, bool need_read_barrier /* = true */, bool is_dyn /* = false */>
inline ObjectHeader *ObjectHeader::GetFieldObject(int offset) const
{
    return ObjectAccessor::GetObject<is_volatile, need_read_barrier, is_dyn>(this, offset);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool is_volatile /* = false */, bool need_write_barrier /* = true */, bool is_dyn /* = false */>
inline void ObjectHeader::SetFieldObject(size_t offset, ObjectHeader *value)
{
    ObjectAccessor::SetObject<is_volatile, need_write_barrier, is_dyn>(this, offset, value);
}

template <class T>
inline T ObjectHeader::GetFieldPrimitive(const Field &field) const
{
    return ObjectAccessor::GetFieldPrimitive<T>(this, field);
}

template <class T>
inline void ObjectHeader::SetFieldPrimitive(const Field &field, T value)
{
    ObjectAccessor::SetFieldPrimitive(this, field, value);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool need_read_barrier /* = true */, bool is_dyn /* = false */>
inline ObjectHeader *ObjectHeader::GetFieldObject(const Field &field) const
{
    return ObjectAccessor::GetFieldObject<need_read_barrier, is_dyn>(this, field);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool need_write_barrier /* = true */, bool is_dyn /* = false */>
inline void ObjectHeader::SetFieldObject(const Field &field, ObjectHeader *value)
{
    ObjectAccessor::SetFieldObject<need_write_barrier, is_dyn>(this, field, value);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool need_read_barrier /* = true */, bool is_dyn /* = false */>
inline ObjectHeader *ObjectHeader::GetFieldObject(ManagedThread *thread, const Field &field)
{
    return ObjectAccessor::GetFieldObject<need_read_barrier, is_dyn>(thread, this, field);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool need_write_barrier /* = true */, bool is_dyn /* = false */>
inline void ObjectHeader::SetFieldObject(ManagedThread *thread, const Field &field, ObjectHeader *value)
{
    ObjectAccessor::SetFieldObject<need_write_barrier, is_dyn>(thread, this, field, value);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool is_volatile /* = false */, bool need_write_barrier /* = true */, bool is_dyn /* = false */>
inline void ObjectHeader::SetFieldObject(ManagedThread *thread, size_t offset, ObjectHeader *value)
{
    ObjectAccessor::SetObject<is_volatile, need_write_barrier, is_dyn>(thread, this, offset, value);
}

template <class T>
inline T ObjectHeader::GetFieldPrimitive(size_t offset, std::memory_order memory_order) const
{
    return ObjectAccessor::GetFieldPrimitive<T>(this, offset, memory_order);
}

template <class T>
inline void ObjectHeader::SetFieldPrimitive(size_t offset, T value, std::memory_order memory_order)
{
    ObjectAccessor::SetFieldPrimitive(this, offset, value, memory_order);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool need_read_barrier /* = true */, bool is_dyn /* = false */>
inline ObjectHeader *ObjectHeader::GetFieldObject(size_t offset, std::memory_order memory_order) const
{
    return ObjectAccessor::GetFieldObject<need_read_barrier, is_dyn>(this, offset, memory_order);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool need_write_barrier /* = true */, bool is_dyn /* = false */>
inline void ObjectHeader::SetFieldObject(size_t offset, ObjectHeader *value, std::memory_order memory_order)
{
    ObjectAccessor::SetFieldObject<need_write_barrier, is_dyn>(this, offset, value, memory_order);
}

template <typename T>
inline bool ObjectHeader::CompareAndSetFieldPrimitive(size_t offset, T old_value, T new_value,
                                                      std::memory_order memory_order, bool strong)
{
    return ObjectAccessor::CompareAndSetFieldPrimitive(this, offset, old_value, new_value, memory_order, strong).first;
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool need_write_barrier /* = true */, bool is_dyn /* = false */>
inline bool ObjectHeader::CompareAndSetFieldObject(size_t offset, ObjectHeader *old_value, ObjectHeader *new_value,
                                                   std::memory_order memory_order, bool strong)
{
    return ObjectAccessor::CompareAndSetFieldObject<need_write_barrier, is_dyn>(this, offset, old_value, new_value,
                                                                                memory_order, strong)
        .first;
}

template <typename T>
inline T ObjectHeader::CompareAndExchangeFieldPrimitive(size_t offset, T old_value, T new_value,
                                                        std::memory_order memory_order, bool strong)
{
    return ObjectAccessor::CompareAndSetFieldPrimitive(this, offset, old_value, new_value, memory_order, strong).second;
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool need_write_barrier /* = true */, bool is_dyn /* = false */>
inline ObjectHeader *ObjectHeader::CompareAndExchangeFieldObject(size_t offset, ObjectHeader *old_value,
                                                                 ObjectHeader *new_value,
                                                                 std::memory_order memory_order, bool strong)
{
    return ObjectAccessor::CompareAndSetFieldObject<need_write_barrier, is_dyn>(this, offset, old_value, new_value,
                                                                                memory_order, strong)
        .second;
}

template <typename T>
inline T ObjectHeader::GetAndSetFieldPrimitive(size_t offset, T value, std::memory_order memory_order)
{
    return ObjectAccessor::GetAndSetFieldPrimitive(this, offset, value, memory_order);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool need_write_barrier /* = true */, bool is_dyn /* = false */>
inline ObjectHeader *ObjectHeader::GetAndSetFieldObject(size_t offset, ObjectHeader *value,
                                                        std::memory_order memory_order)
{
    return ObjectAccessor::GetAndSetFieldObject<need_write_barrier, is_dyn>(this, offset, value, memory_order);
}

template <typename T>
inline T ObjectHeader::GetAndAddFieldPrimitive(size_t offset, T value, std::memory_order memory_order)
{
    return ObjectAccessor::GetAndAddFieldPrimitive(this, offset, value, memory_order);
}

template <typename T>
inline T ObjectHeader::GetAndBitwiseOrFieldPrimitive(size_t offset, T value, std::memory_order memory_order)
{
    return ObjectAccessor::GetAndBitwiseOrFieldPrimitive(this, offset, value, memory_order);
}

template <typename T>
inline T ObjectHeader::GetAndBitwiseAndFieldPrimitive(size_t offset, T value, std::memory_order memory_order)
{
    return ObjectAccessor::GetAndBitwiseAndFieldPrimitive(this, offset, value, memory_order);
}

template <typename T>
inline T ObjectHeader::GetAndBitwiseXorFieldPrimitive(size_t offset, T value, std::memory_order memory_order)
{
    return ObjectAccessor::GetAndBitwiseXorFieldPrimitive(this, offset, value, memory_order);
}

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_OBJECT_HEADER_INL_H_
