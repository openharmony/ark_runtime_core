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

#ifndef PANDA_RUNTIME_INCLUDE_OBJECT_ACCESSOR_INL_H_
#define PANDA_RUNTIME_INCLUDE_OBJECT_ACCESSOR_INL_H_

#include <securec.h>

#include "libpandabase/mem/mem.h"
#include "runtime/include/field.h"
#include "runtime/include/object_accessor.h"
#include "runtime/mem/gc/gc_barrier_set.h"

namespace panda {

/* static */
// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool is_volatile /* = false */, bool need_read_barrier /* = true */, bool is_dyn /* = false */>
inline ObjectHeader *ObjectAccessor::GetObject(const void *obj, size_t offset)
{
    // We don't have GC with read barriers now
    if (!is_dyn) {
        return reinterpret_cast<ObjectHeader *>(Get<object_pointer_type, is_volatile>(obj, offset));
    }
    return Get<ObjectHeader *, is_volatile>(obj, offset);
}

/* static */
// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool is_volatile /* = false */, bool need_write_barrier /* = true */, bool is_dyn /* = false */>
inline void ObjectAccessor::SetObject(void *obj, size_t offset, ObjectHeader *value)
{
    if (need_write_barrier) {
        auto *barrier_set = GetBarrierSet();
        auto gc_pre_barrier_type = barrier_set->GetPreType();

        if (!mem::IsEmptyBarrier(gc_pre_barrier_type)) {
            ObjectHeader *pre_val = GetObject<is_volatile, is_dyn>(obj, offset);
            barrier_set->PreBarrier(ToVoidPtr(ToUintPtr(obj) + offset), pre_val);
        }

        if (!is_dyn) {
            Set<object_pointer_type, is_volatile>(obj, offset, ToObjPtrType(value));
        } else {
            Set<ObjectHeader *, is_volatile>(obj, offset, value);
        }
        auto gc_post_barrier_type = barrier_set->GetPostType();
        if (!mem::IsEmptyBarrier(gc_post_barrier_type)) {
            barrier_set->PostBarrier(ToVoidPtr(ToUintPtr(obj)), value);
        }
    } else {
        if (!is_dyn) {
            Set<object_pointer_type, is_volatile>(obj, offset, ToObjPtrType(value));
        } else {
            Set<ObjectHeader *, is_volatile>(obj, offset, value);
        }
    }
}

/* static */
// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool is_volatile /* = false */, bool need_read_barrier /* = true */, bool is_dyn /* = false */>
inline ObjectHeader *ObjectAccessor::GetObject([[maybe_unused]] const ManagedThread *thread, const void *obj,
                                               size_t offset)
{
    // We don't have GC with read barriers now
    if (!is_dyn) {
        return reinterpret_cast<ObjectHeader *>(Get<object_pointer_type, is_volatile>(obj, offset));
    }
    return Get<ObjectHeader *, is_volatile>(obj, offset);
}

/* static */
// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool is_volatile /* = false */, bool need_write_barrier /* = true */, bool is_dyn /* = false */>
inline void ObjectAccessor::SetObject(const ManagedThread *thread, void *obj, size_t offset, ObjectHeader *value)
{
    if (need_write_barrier) {
        auto *barrier_set = GetBarrierSet(thread);
        if (!mem::IsEmptyBarrier(barrier_set->GetPreType())) {
            ObjectHeader *pre_val = GetObject<is_volatile, is_dyn>(obj, offset);
            barrier_set->PreBarrier(ToVoidPtr(ToUintPtr(obj) + offset), pre_val);
        }

        if (!is_dyn) {
            Set<object_pointer_type, is_volatile>(obj, offset, ToObjPtrType(value));
        } else {
            Set<ObjectHeader *, is_volatile>(obj, offset, value);
        }
        if (!mem::IsEmptyBarrier(barrier_set->GetPostType())) {
            barrier_set->PostBarrier(ToVoidPtr(ToUintPtr(obj)), value);
        }
    } else {
        if (!is_dyn) {
            Set<object_pointer_type, is_volatile>(obj, offset, ToObjPtrType(value));
        } else {
            Set<ObjectHeader *, is_volatile>(obj, offset, value);
        }
    }
}

/* static */
template <class T>
inline T ObjectAccessor::GetFieldPrimitive(const void *obj, const Field &field)
{
    if (UNLIKELY(field.IsVolatile())) {
        return GetPrimitive<T, true>(obj, field.GetOffset());
    }
    return GetPrimitive<T, false>(obj, field.GetOffset());
}

/* static */
template <class T>
inline void ObjectAccessor::SetFieldPrimitive(void *obj, const Field &field, T value)
{
    if (UNLIKELY(field.IsVolatile())) {
        SetPrimitive<T, true>(obj, field.GetOffset(), value);
    } else {
        SetPrimitive<T, false>(obj, field.GetOffset(), value);
    }
}

/* static */
// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool need_read_barrier /* = true */, bool is_dyn /* = false */>
inline ObjectHeader *ObjectAccessor::GetFieldObject(const void *obj, const Field &field)
{
    if (UNLIKELY(field.IsVolatile())) {
        return GetObject<true, need_read_barrier, is_dyn>(obj, field.GetOffset());
    }
    return GetObject<false, need_read_barrier, is_dyn>(obj, field.GetOffset());
}

/* static */
// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool need_write_barrier /* = true */, bool is_dyn /* = false */>
inline void ObjectAccessor::SetFieldObject(void *obj, const Field &field, ObjectHeader *value)
{
#ifdef PANDA_USE_32_BIT_POINTER
    ASSERT(IsInObjectsAddressSpace(ToUintPtr(value)));
#endif
    if (UNLIKELY(field.IsVolatile())) {
        SetObject<true, need_write_barrier, is_dyn>(obj, field.GetOffset(), value);
    } else {
        SetObject<false, need_write_barrier, is_dyn>(obj, field.GetOffset(), value);
    }
}

/* static */
// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool need_read_barrier /* = true */, bool is_dyn /* = false */>
inline ObjectHeader *ObjectAccessor::GetFieldObject(ManagedThread *thread, const void *obj, const Field &field)
{
    if (UNLIKELY(field.IsVolatile())) {
        return GetObject<true, need_read_barrier, is_dyn>(thread, obj, field.GetOffset());
    }
    return GetObject<false, need_read_barrier, is_dyn>(thread, obj, field.GetOffset());
}

/* static */
// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool need_write_barrier /* = true */, bool is_dyn /* = false */>
inline void ObjectAccessor::SetFieldObject(ManagedThread *thread, void *obj, const Field &field, ObjectHeader *value)
{
    if (UNLIKELY(field.IsVolatile())) {
        SetObject<true, need_write_barrier, is_dyn>(thread, obj, field.GetOffset(), value);
    } else {
        SetObject<false, need_write_barrier, is_dyn>(thread, obj, field.GetOffset(), value);
    }
}

/* static */
template <class T>
inline T ObjectAccessor::GetFieldPrimitive(const void *obj, size_t offset, std::memory_order memory_order)
{
    return Get<T>(obj, offset, memory_order);
}

/* static */
template <class T>
inline void ObjectAccessor::SetFieldPrimitive(void *obj, size_t offset, T value, std::memory_order memory_order)
{
    Set<T>(obj, offset, value, memory_order);
}

/* static */
// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool need_read_barrier /* = true */, bool is_dyn /* = false */>
inline ObjectHeader *ObjectAccessor::GetFieldObject(const void *obj, int offset, std::memory_order memory_order)
{
    if (!is_dyn) {
        return reinterpret_cast<ObjectHeader *>(Get<object_pointer_type>(obj, offset, memory_order));
    }
    return Get<ObjectHeader *>(obj, offset, memory_order);
}

/* static */
// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool need_write_barrier /* = true */, bool is_dyn /* = false */>
inline void ObjectAccessor::SetFieldObject(void *obj, size_t offset, ObjectHeader *value,
                                           std::memory_order memory_order)
{
    if (need_write_barrier) {
        auto *barrier_set = GetBarrierSet();
        auto gc_pre_barrier_type = barrier_set->GetPreType();

        if (!mem::IsEmptyBarrier(gc_pre_barrier_type)) {
            ObjectHeader *pre_val = GetFieldObject<is_dyn>(obj, offset, memory_order);
            barrier_set->PreBarrier(ToVoidPtr(ToUintPtr(obj) + offset), pre_val);
        }

        if (!is_dyn) {
            Set<object_pointer_type>(obj, offset, ToObjPtrType(value), memory_order);
        } else {
            Set<ObjectHeader *>(obj, offset, value, memory_order);
        }
        auto gc_post_barrier_type = barrier_set->GetPostType();
        if (!mem::IsEmptyBarrier(gc_post_barrier_type)) {
            barrier_set->PostBarrier(ToVoidPtr(ToUintPtr(obj)), value);
        }
    } else {
        if (!is_dyn) {
            Set<object_pointer_type>(obj, offset, ToObjPtrType(value), memory_order);
        } else {
            Set<ObjectHeader *>(obj, offset, value, memory_order);
        }
    }
}

/* static */
template <typename T>
inline std::pair<bool, T> ObjectAccessor::CompareAndSetFieldPrimitive(void *obj, size_t offset, T old_value,
                                                                      T new_value, std::memory_order memory_order,
                                                                      bool strong)
{
    uintptr_t raw_addr = reinterpret_cast<uintptr_t>(obj) + offset;
    ASSERT(IsInObjectsAddressSpace(raw_addr));
    auto *atomic_addr = reinterpret_cast<std::atomic<T> *>(raw_addr);
    if (strong) {
        return {atomic_addr->compare_exchange_strong(old_value, new_value, memory_order), old_value};
    }
    return {atomic_addr->compare_exchange_weak(old_value, new_value, memory_order), old_value};
}

/* static */
// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool need_write_barrier /* = true */, bool is_dyn /* = false */>
inline std::pair<bool, ObjectHeader *> ObjectAccessor::CompareAndSetFieldObject(void *obj, size_t offset,
                                                                                ObjectHeader *old_value,
                                                                                ObjectHeader *new_value,
                                                                                std::memory_order memory_order,
                                                                                bool strong)
{
    // update field with read barrier
    ObjectHeader *pre_val = GetObject<false, is_dyn>(obj, offset);
    bool success = false;
    ObjectHeader *result = nullptr;
    auto get_result = [&]() {
        if (is_dyn) {
            auto value =
                CompareAndSetFieldPrimitive<ObjectHeader *>(obj, offset, old_value, new_value, memory_order, strong);
            success = value.first;
            result = value.second;
        } else {
            auto value = CompareAndSetFieldPrimitive<object_pointer_type>(
                obj, offset, ToObjPtrType(old_value), ToObjPtrType(new_value), memory_order, strong);
            success = value.first;
            result = reinterpret_cast<ObjectHeader *>(value.second);
        }
    };

    if (need_write_barrier) {
        auto *barrier_set = GetBarrierSet();
        if (!mem::IsEmptyBarrier(barrier_set->GetPreType())) {
            barrier_set->PreBarrier(ToVoidPtr(ToUintPtr(obj) + offset), pre_val);
        }

        get_result();
        if (success && !mem::IsEmptyBarrier(barrier_set->GetPostType())) {
            barrier_set->PostBarrier(ToVoidPtr(ToUintPtr(obj)), new_value);
        }
        return {success, result};
    }

    get_result();
    return {success, result};
}

/* static */
template <typename T>
inline T ObjectAccessor::GetAndSetFieldPrimitive(void *obj, size_t offset, T value, std::memory_order memory_order)
{
    uintptr_t raw_addr = reinterpret_cast<uintptr_t>(obj) + offset;
    ASSERT(IsInObjectsAddressSpace(raw_addr));
    auto *atomic_addr = reinterpret_cast<std::atomic<T> *>(raw_addr);
    return atomic_addr->exchange(value, memory_order);
}

/* static */
// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool need_write_barrier /* = true */, bool is_dyn /* = false */>
inline ObjectHeader *ObjectAccessor::GetAndSetFieldObject(void *obj, size_t offset, ObjectHeader *value,
                                                          std::memory_order memory_order)
{
    // update field with read barrier
    ObjectHeader *pre_val = GetObject<false, is_dyn>(obj, offset);

    if (need_write_barrier) {
        auto *barrier_set = GetBarrierSet();
        if (!mem::IsEmptyBarrier(barrier_set->GetPreType())) {
            barrier_set->PreBarrier(ToVoidPtr(ToUintPtr(obj) + offset), pre_val);
        }
        ObjectHeader *result = is_dyn ? GetAndSetFieldPrimitive<ObjectHeader *>(obj, offset, value, memory_order)
                                      : reinterpret_cast<ObjectHeader *>(GetAndSetFieldPrimitive<object_pointer_type>(
                                            obj, offset, ToObjPtrType(value), memory_order));
        if (result != nullptr && !mem::IsEmptyBarrier(barrier_set->GetPostType())) {
            barrier_set->PostBarrier(ToVoidPtr(ToUintPtr(obj)), value);
        }

        return result;
    }

    return is_dyn ? GetAndSetFieldPrimitive<ObjectHeader *>(obj, offset, value, memory_order)
                  : reinterpret_cast<ObjectHeader *>(
                        GetAndSetFieldPrimitive<object_pointer_type>(obj, offset, ToObjPtrType(value), memory_order));
}

/* static */
template <typename T>
inline T ObjectAccessor::GetAndAddFieldPrimitive([[maybe_unused]] void *obj, [[maybe_unused]] size_t offset,
                                                 [[maybe_unused]] T value,
                                                 [[maybe_unused]] std::memory_order memory_order)
{
    if constexpr (std::is_same_v<T, uint8_t>) {  // NOLINT(readability-braces-around-statements)
        LOG(FATAL, RUNTIME) << "Could not do add for boolean";
        UNREACHABLE();
    } else {                                          // NOLINT(readability-misleading-indentation)
        if constexpr (std::is_floating_point_v<T>) {  // NOLINT(readability-braces-around-statements)
            // Atmoic fetch_add only defined in the atomic specializations for integral and pointer
            uintptr_t raw_addr = reinterpret_cast<uintptr_t>(obj) + offset;
            ASSERT(IsInObjectsAddressSpace(raw_addr));
            auto *atomic_addr = reinterpret_cast<std::atomic<T> *>(raw_addr);
            T old_value = atomic_addr->load(memory_order);
            T new_value;
            do {
                new_value = old_value + value;
            } while (!atomic_addr->compare_exchange_weak(old_value, new_value, memory_order));
            return old_value;
        } else {  // NOLINT(readability-misleading-indentation, readability-else-after-return)
            uintptr_t raw_addr = reinterpret_cast<uintptr_t>(obj) + offset;
            ASSERT(IsInObjectsAddressSpace(raw_addr));
            auto *atomic_addr = reinterpret_cast<std::atomic<T> *>(raw_addr);
            return atomic_addr->fetch_add(value, memory_order);
        }
    }
}

/* static */
template <typename T>
inline T ObjectAccessor::GetAndBitwiseOrFieldPrimitive([[maybe_unused]] void *obj, [[maybe_unused]] size_t offset,
                                                       [[maybe_unused]] T value,
                                                       [[maybe_unused]] std::memory_order memory_order)
{
    if constexpr (std::is_floating_point_v<T>) {  // NOLINT(readability-braces-around-statements)
        LOG(FATAL, RUNTIME) << "Could not do bitwise or for float/double";
        UNREACHABLE();
    } else {  // NOLINT(readability-misleading-indentation)
        uintptr_t raw_addr = reinterpret_cast<uintptr_t>(obj) + offset;
        ASSERT(IsInObjectsAddressSpace(raw_addr));
        auto *atomic_addr = reinterpret_cast<std::atomic<T> *>(raw_addr);
        return atomic_addr->fetch_or(value, memory_order);
    }
}

/* static */
template <typename T>
inline T ObjectAccessor::GetAndBitwiseAndFieldPrimitive([[maybe_unused]] void *obj, [[maybe_unused]] size_t offset,
                                                        [[maybe_unused]] T value,
                                                        [[maybe_unused]] std::memory_order memory_order)
{
    if constexpr (std::is_floating_point_v<T>) {  // NOLINT(readability-braces-around-statements)
        LOG(FATAL, RUNTIME) << "Could not do bitwise and for float/double";
        UNREACHABLE();
    } else {  // NOLINT(readability-misleading-indentation)
        uintptr_t raw_addr = reinterpret_cast<uintptr_t>(obj) + offset;
        ASSERT(IsInObjectsAddressSpace(raw_addr));
        auto *atomic_addr = reinterpret_cast<std::atomic<T> *>(raw_addr);
        return atomic_addr->fetch_and(value, memory_order);
    }
}

/* static */
template <typename T>
inline T ObjectAccessor::GetAndBitwiseXorFieldPrimitive([[maybe_unused]] void *obj, [[maybe_unused]] size_t offset,
                                                        [[maybe_unused]] T value,
                                                        [[maybe_unused]] std::memory_order memory_order)
{
    if constexpr (std::is_floating_point_v<T>) {  // NOLINT(readability-braces-around-statements)
        LOG(FATAL, RUNTIME) << "Could not do bitwise xor for float/double";
        UNREACHABLE();
    } else {  // NOLINT(readability-misleading-indentation)
        uintptr_t raw_addr = reinterpret_cast<uintptr_t>(obj) + offset;
        ASSERT(IsInObjectsAddressSpace(raw_addr));
        auto *atomic_addr = reinterpret_cast<std::atomic<T> *>(raw_addr);
        return atomic_addr->fetch_xor(value, memory_order);
    }
}

/* static */
// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_COMMENT_LOCATION)
template <bool need_write_barrier /* = true */>
inline void ObjectAccessor::SetDynObject(const ManagedThread *thread, void *obj, size_t offset, ObjectHeader *value)
{
    auto *addr = reinterpret_cast<ObjectHeader *>(ToUintPtr(obj) + offset);
    ASSERT(IsInObjectsAddressSpace(ToUintPtr(addr)));
    (void)memcpy_s(reinterpret_cast<void *>(addr), coretypes::TaggedValue::TaggedTypeSize(), &value,
                   coretypes::TaggedValue::TaggedTypeSize());
    auto gc_post_barrier_type = GetPostBarrierType(thread);
    if (need_write_barrier && !mem::IsEmptyBarrier(gc_post_barrier_type)) {
        GetBarrierSet(thread)->PostBarrier(ToVoidPtr(ToUintPtr(obj)), value);
    }
}

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_OBJECT_ACCESSOR_INL_H_
