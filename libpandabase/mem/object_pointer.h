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

#ifndef PANDA_LIBPANDABASE_MEM_OBJECT_POINTER_H_
#define PANDA_LIBPANDABASE_MEM_OBJECT_POINTER_H_

#include "mem.h"

namespace panda {

/**
 * @class ObjectPointer<class Object>
 *
 * \brief This class is wrapper for object types.
 *
 * Wraps pointer Object * into class ObjectPointer<Object> and provides interfaces to work with it as a pointer.
 * This is needed to use object pointers of size 32 bits in 64-bit architectures.
 */
template <class Object>
class ObjectPointer final {
public:
    ObjectPointer() = default;
    // NOLINTNEXTLINE(google-explicit-constructor)
    ObjectPointer(Object *object) : object_(ToObjPtrType(object))
    {
#ifdef PANDA_USE_32_BIT_POINTER
        ASSERT(IsInObjectsAddressSpace(ToUintPtr(object)));
#endif
    }
    // NOLINTNEXTLINE(google-explicit-constructor)
    ObjectPointer(std::nullptr_t a_nullptr) noexcept : object_(ToObjPtrType(a_nullptr)) {}

    DEFAULT_COPY_SEMANTIC(ObjectPointer);
    DEFAULT_NOEXCEPT_MOVE_SEMANTIC(ObjectPointer);

    ObjectPointer &operator=(std::nullptr_t a_nullptr)
    {
        object_ = ToObjPtrType(a_nullptr);
        return *this;
    }

    ObjectPointer &operator=(const Object *object)
    {
#ifdef PANDA_USE_32_BIT_POINTER
        ASSERT(IsInObjectsAddressSpace(ToUintPtr(object)));
#endif
        object_ = ToObjPtrType(object);
        return *this;
    }

    ALWAYS_INLINE Object *operator->()
    {
        ASSERT(object_ != 0);
        return ToObjectPtr(object_);
    }

    // NOLINTNEXTLINE(google-explicit-constructor)
    ALWAYS_INLINE operator Object *()
    {
        return ToObjectPtr(object_);
    }

    ALWAYS_INLINE Object &operator*()
    {
        ASSERT(object_ != 0);
        return *ToObjectPtr(object_);
    }

    ALWAYS_INLINE bool operator==(const ObjectPointer &other) const noexcept
    {
        return object_ == other.object_;
    }

    ALWAYS_INLINE bool operator!=(const ObjectPointer &other) const noexcept
    {
        return object_ != other.object_;
    }

    ALWAYS_INLINE bool operator==(Object *other) const noexcept
    {
        return ToObjectPtr(object_) == other;
    }

    ALWAYS_INLINE bool operator!=(Object *other) const noexcept
    {
        return ToObjectPtr(object_) != other;
    }

    ALWAYS_INLINE bool operator==(std::nullptr_t) const noexcept
    {
        return ToObjectPtr(object_) == nullptr;
    }

    ALWAYS_INLINE bool operator!=(std::nullptr_t) const noexcept
    {
        return ToObjectPtr(object_) != nullptr;
    }

    ALWAYS_INLINE Object &operator[](size_t index)
    {
        return ToObjectPtr(object_)[index];
    }

    ALWAYS_INLINE const Object &operator[](size_t index) const
    {
        return ToObjectPtr(object_)[index];
    }

    template <class U>
    ALWAYS_INLINE U ReinterpretCast() const noexcept
    {
        return reinterpret_cast<U>(ToObjectPtr(object_));
    }

    ~ObjectPointer() = default;

private:
    object_pointer_type object_ {};

    ALWAYS_INLINE static Object *ToObjectPtr(const object_pointer_type pointer) noexcept
    {
        return ToNativePtr<Object>(static_cast<uintptr_t>(pointer));
    }
};

// size of ObjectPointer<T> must be equal size of object_pointer_type
static_assert(sizeof(ObjectPointer<bool>) == sizeof(object_pointer_type));

}  // namespace panda

#endif  // PANDA_LIBPANDABASE_MEM_OBJECT_POINTER_H_
