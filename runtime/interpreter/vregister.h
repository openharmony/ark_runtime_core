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

#ifndef PANDA_RUNTIME_INTERPRETER_VREGISTER_H_
#define PANDA_RUNTIME_INTERPRETER_VREGISTER_H_

#include <cstddef>
#include <cstdint>

#include "libpandabase/macros.h"
#include "libpandabase/utils/bit_helpers.h"
#include "libpandabase/utils/bit_utils.h"
#include "libpandabase/utils/logger.h"
#include "runtime/include/coretypes/tagged_value.h"
#include "runtime/include/mem/panda_string.h"
#include "runtime/include/object_header.h"

namespace panda::interpreter {
// An uint64_t value is used for storing the tags of values. This kind of tags are compatible with Java and dynamic
// languages, and the tag is encoded as below.
// tag bits | [63-7] |     [6-4]     |      [3-1]      |      [0]        |
// usage    | unused |  object type  | primitive type  | IsObject flag   |
// details  | unused | @000: default | @011: INT       | @0: value is a  |
//          |        | @001: STRING  | @100: DOUBLE    | primitive value |
//          |        |               |                 | @1: value is a  |
//          |        |               |                 | object pointer  |

//
// All the fields' bits occupancy should be adaptive. For example, if we extend the 'IsObject flag' field by 1 bit,
// the 'IsObject flag' field will take bits [1-0] and the 'primitive type' field should take bits [4-2].
//
// This kind of tags are compatible with Java and dynamic languages, and that means if the lowest bit is 1,
// the value is an object pointer, otherwise, the value is a primitive value for both Java and dynamic languages.

// [0]
static constexpr uint8_t OBJECT_FLAG_SHIFT = 0;
static constexpr uint8_t OBJECT_FLAG_BITS = 1;
// [3-1]
static constexpr uint8_t PRIMITIVE_FIRST_SHIFT = OBJECT_FLAG_SHIFT + OBJECT_FLAG_BITS;
static constexpr uint8_t PRIMITIVE_TYPE_BITS = 3;
// [6-4]
static constexpr uint8_t OBJECT_FIRST_SHIFT = PRIMITIVE_FIRST_SHIFT + PRIMITIVE_TYPE_BITS;
static constexpr uint8_t OBJECT_TYPE_BITS = 3;

// OBJECT_FLAG_MASK is compatible with Java and dynamic languages, and 0x1 means the value is 'reference type' in Java
// and 'HeapObject' type in dynamic language.
static constexpr coretypes::TaggedType OBJECT_FLAG_MASK = 0x1;

// PrimitiveIndex's max capacity is (2 ^ PRIMITIVE_TYPE_BITS). If the number of values in PrimitiveIndex
// exceeds the capacity, PRIMITIVE_TYPE_BITS should be increased.
enum PrimitiveIndex : uint8_t { INT_IDX = 3, DOUBLE_IDX };

// ObjectIndex's max capacity is (2 ^ OBJECT_TYPE_BITS). If the number of values in ObjectIndex
// exceeds the capacity, ObjectIndex should be increased.
enum ObjectIndex : uint8_t { STRING_IDX = 1 };

enum TypeTag : uint64_t {
    // Tags of primitive types
    INT = (static_cast<uint64_t>(INT_IDX) << PRIMITIVE_FIRST_SHIFT),
    DOUBLE = (static_cast<uint64_t>(DOUBLE_IDX) << PRIMITIVE_FIRST_SHIFT),
    // Tags of object types
    OBJECT = OBJECT_FLAG_MASK,
    STRING = (static_cast<uint64_t>(STRING_IDX) << OBJECT_FIRST_SHIFT) | OBJECT_FLAG_MASK,
};

template <class T>
class VRegisterIface {
public:
    ALWAYS_INLINE inline void SetValue(int64_t v)
    {
        static_cast<T *>(this)->SetValue(v);
    }

    ALWAYS_INLINE inline int64_t GetValue() const
    {
        return static_cast<const T *>(this)->GetValue();
    }

    ALWAYS_INLINE inline void SetTag(uint64_t tag)
    {
        static_cast<T *>(this)->SetTag(tag);
    }

    ALWAYS_INLINE inline uint64_t GetTag() const
    {
        return static_cast<const T *>(this)->GetTag();
    }

    template <class M>
    ALWAYS_INLINE inline void MoveFrom(const VRegisterIface<M> &other)
    {
        ASSERT(!other.HasObject());
        SetValue(other.GetValue());
        MarkAsPrimitive();
    }

    template <class M>
    ALWAYS_INLINE inline void MoveFromObj(const VRegisterIface<M> &other)
    {
        ASSERT(other.HasObject());
        SetValue(other.GetValue());
        MarkAsObject();
    }

    template <class M>
    ALWAYS_INLINE inline void Move(const VRegisterIface<M> &other)
    {
        SetValue(other.GetValue());
        SetTag(other.GetTag());
    }

    ALWAYS_INLINE inline void Set(int32_t value)
    {
        ASSERT(!HasObject());
        SetValue(value);
    }

    ALWAYS_INLINE inline void Set(uint32_t value)
    {
        ASSERT(!HasObject());
        SetValue(value);
    }

    ALWAYS_INLINE inline void Set(int64_t value)
    {
        ASSERT(!HasObject());
        SetValue(value);
    }

    ALWAYS_INLINE inline void Set(uint64_t value)
    {
        ASSERT(!HasObject());
        auto v = bit_cast<int64_t>(value);
        SetValue(v);
    }

    ALWAYS_INLINE inline void Set(double value)
    {
        ASSERT(!HasObject());
        auto v = bit_cast<int64_t>(value);
        SetValue(v);
    }

    ALWAYS_INLINE inline void Set(ObjectHeader *value)
    {
        ASSERT(HasObject());
        auto v = bit_cast<object_pointer_type>(value);
        SetValue(v);
    }

    ALWAYS_INLINE inline void SetPrimitive(int32_t value)
    {
        SetValue(value);
        MarkAsPrimitive();
    }

    ALWAYS_INLINE inline void SetPrimitive(int64_t value)
    {
        SetValue(value);
        MarkAsPrimitive();
    }

    ALWAYS_INLINE inline void SetPrimitive(double value)
    {
        auto v = bit_cast<int64_t>(value);
        SetValue(v);
        MarkAsPrimitive();
    }

    ALWAYS_INLINE inline int32_t Get() const
    {
        ASSERT(!HasObject());
        return static_cast<int32_t>(GetValue());
    }

    ALWAYS_INLINE inline float GetFloat() const
    {
        ASSERT(!HasObject());
        return static_cast<float>(bit_cast<double>(GetValue()));
    }

    ALWAYS_INLINE inline int64_t GetLong() const
    {
        ASSERT(!HasObject());
        return GetValue();
    }

    ALWAYS_INLINE inline double GetDouble() const
    {
        ASSERT(!HasObject());
        return bit_cast<double>(GetValue());
    }

    ALWAYS_INLINE inline ObjectHeader *GetReference() const
    {
        ASSERT(HasObject());
        return reinterpret_cast<ObjectHeader *>(static_cast<object_pointer_type>(GetValue()));
    }

    ALWAYS_INLINE inline void SetReference(ObjectHeader *obj)
    {
        auto v = down_cast<helpers::TypeHelperT<OBJECT_POINTER_SIZE * BYTE_SIZE, true>>(obj);
        SetValue(v);
        MarkAsObject();
    }

    ALWAYS_INLINE inline bool HasObject() const
    {
        return (GetTag() & OBJECT_MASK) != 0;
    }

    ALWAYS_INLINE inline void MarkAsObject()
    {
        SetTag(GetTag() | OBJECT_MASK);
    }

    ALWAYS_INLINE inline void MarkAsPrimitive()
    {
        SetTag(GetTag() & ~OBJECT_MASK);
    }

    template <typename M>
    ALWAYS_INLINE inline M GetAs() const
    {
        return ValueAccessor<M>::Get(*this);
    }

#ifndef NDEBUG
    ALWAYS_INLINE inline PandaString DumpVReg()
    {
        PandaStringStream values;
        if (HasObject() != 0) {
            values << "obj = " << std::hex << GetValue();
        } else {
            values << "pri = (i64) " << GetValue() << " | "
                   << "(f64) " << GetDouble() << " | "
                   << "(hex) " << std::hex << GetValue();
        }
        values << " | tag = " << GetTag();
        return values.str();
    }
#endif

private:
    template <typename M, typename = void>
    struct ValueAccessor {
        static M Get(const VRegisterIface<T> &vreg);
    };

    static constexpr uint64_t OBJECT_MASK = 0x1;

    static constexpr int8_t BYTE_SIZE = 8;
};

}  // namespace panda::interpreter

#endif  // PANDA_RUNTIME_INTERPRETER_VREGISTER_H_
