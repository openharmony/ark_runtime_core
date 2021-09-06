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

#ifndef PANDA_RUNTIME_INCLUDE_CORETYPES_TAGGED_VALUE_H_
#define PANDA_RUNTIME_INCLUDE_CORETYPES_TAGGED_VALUE_H_

#include <climits>
#include <cstddef>
#include "macros.h"
#include "utils/bit_utils.h"
#include "include/object_header.h"

namespace panda::coretypes {

//  Every double with all of its exponent bits set and its highest mantissa bit set is a quiet NaN.
//  That leaves 51 bits unaccounted for. We’ll avoid one of those so that we don’t step on Intel’s
//  “QNaN Floating-Point Indefinite” value, leaving us 50 bits. Those remaining bits can be anything.
//  so we use a special quietNaN as TaggedInt tag(highest 16bits as 0xFFFF), and need to encode double
//  to the value will begin with a 16-bit pattern within the range 0x0001..0xFFFE.

//  Nan-boxing pointer is used and the first four bytes are used as tag:
//    Object:             [0x0000] [48 bit direct pointer]
//    WeakRef:            [0x0000] [47 bits direct pointer] | 1 bit 1
//                   /    [0x0001] [48 bit any value]
//    TaggedDouble:       ......
//                   \    [0xFFFE] [48 bit any value]
//    TaggedInt:          [0xFFFF] [0x0000] [32 bit signed integer]
//
//  There are some special markers of Object:
//    False:       [56 bits 0] | 0x06          // 0110
//    True:        [56 bits 0] | 0x07          // 0111
//    Undefined:   [56 bits 0] | 0x0a          // 1010
//    Null:        [56 bits 0] | 0x02          // 0010
//    Hole:        [56 bits 0] | 0x00          // 0000

using TaggedType = uint64_t;

static const TaggedType NULL_POINTER = 0;

inline TaggedType ReinterpretDoubleToTaggedType(double value)
{
    return bit_cast<TaggedType>(value);
}

inline double ReinterpretTaggedTypeToDouble(TaggedType value)
{
    return bit_cast<double>(value);
}

class TaggedValue {
public:
    static constexpr size_t TAG_BITS_SIZE = 16;
    static constexpr size_t TAG_BITS_SHIFT = BitNumbers<TaggedType>() - TAG_BITS_SIZE;
    static_assert((TAG_BITS_SHIFT + TAG_BITS_SIZE) == sizeof(TaggedType) * CHAR_BIT, "Insufficient bits!");
    static constexpr TaggedType TAG_MASK = ((1ULL << TAG_BITS_SIZE) - 1ULL) << TAG_BITS_SHIFT;
    static constexpr TaggedType TAG_INT = 0xFFFFULL << TAG_BITS_SHIFT;
    static constexpr TaggedType TAG_OBJECT = 0x0000ULL << TAG_BITS_SHIFT;

    static constexpr TaggedType TAG_SPECIAL_MASK = 0xFFULL;
    static constexpr TaggedType TAG_SPECIAL_VALUE = 0x02ULL;
    static constexpr TaggedType TAG_BOOLEAN = 0x04ULL;
    static constexpr TaggedType TAG_UNDEFINED = 0x08ULL;
    static constexpr TaggedType TAG_EXCEPTION = 0x10ULL;
    static constexpr TaggedType TAG_WEAK_FILTER = 0x03ULL;
    static constexpr TaggedType VALUE_HOLE = TAG_OBJECT | 0x00ULL;
    static constexpr TaggedType TAG_WEAK_MASK = TAG_OBJECT | 0x01ULL;
    static constexpr TaggedType VALUE_NULL = TAG_OBJECT | TAG_SPECIAL_VALUE;
    static constexpr TaggedType VALUE_FALSE =
        TAG_OBJECT | TAG_BOOLEAN | TAG_SPECIAL_VALUE | static_cast<TaggedType>(false);
    static constexpr TaggedType VALUE_TRUE =
        TAG_OBJECT | TAG_BOOLEAN | TAG_SPECIAL_VALUE | static_cast<TaggedType>(true);
    static constexpr TaggedType VALUE_ZERO = TAG_INT | 0x00ULL;
    static constexpr TaggedType VALUE_UNDEFINED = TAG_OBJECT | TAG_SPECIAL_VALUE | TAG_UNDEFINED;
    static constexpr TaggedType VALUE_EXCEPTION = TAG_OBJECT | TAG_SPECIAL_VALUE | TAG_EXCEPTION;

    static constexpr size_t DOUBLE_ENCODE_OFFSET_BIT = 48;
    static constexpr TaggedType DOUBLE_ENCODE_OFFSET = 1ULL << DOUBLE_ENCODE_OFFSET_BIT;

    TaggedValue(void *) = delete;

    constexpr TaggedValue() : value_(NULL_POINTER) {}

    constexpr explicit TaggedValue(TaggedType v) : value_(v) {}

    constexpr explicit TaggedValue(int v) : value_(static_cast<TaggedType>(v) | TAG_INT) {}

    explicit TaggedValue(unsigned int v)
    {
        if (static_cast<int32_t>(v) < 0) {
            value_ = TaggedValue(static_cast<double>(v)).GetRawData();
            return;
        }
        value_ = TaggedValue(static_cast<int32_t>(v)).GetRawData();
    }

    explicit TaggedValue(int64_t v)
    {
        if (UNLIKELY(static_cast<int32_t>(v) != v)) {
            value_ = TaggedValue(static_cast<double>(v)).GetRawData();
            return;
        }
        value_ = TaggedValue(static_cast<int32_t>(v)).GetRawData();
    }

    constexpr explicit TaggedValue(bool v)
        : value_(static_cast<TaggedType>(v) | TAG_OBJECT | TAG_BOOLEAN | TAG_SPECIAL_VALUE)
    {
    }

    explicit TaggedValue(double v)
    {
        ASSERT_PRINT(!IsImpureNaN(v), "pureNaN will break the encoding of tagged double: "
                                          << std::hex << ReinterpretDoubleToTaggedType(v));
        value_ = ReinterpretDoubleToTaggedType(v) + DOUBLE_ENCODE_OFFSET;
    }

    explicit TaggedValue(ObjectHeader *v) : value_(static_cast<TaggedType>(ToUintPtr(v))) {}

    explicit TaggedValue(const ObjectHeader *v) : value_(static_cast<TaggedType>(ToUintPtr(v))) {}

    inline void CreateWeakRef()
    {
        ASSERT_PRINT(IsHeapObject() && ((value_ & TAG_WEAK_FILTER) == 0U),
                     "The least significant two bits of TaggedValue are not zero.");
        value_ = value_ | TAG_WEAK_MASK;
    }

    inline void RemoveWeakTag()
    {
        ASSERT_PRINT(IsHeapObject() && ((value_ & TAG_WEAK_MASK) == 1U), "The tagged value is not a weak ref.");
        value_ = value_ & (~TAG_WEAK_FILTER);
    }

    inline TaggedValue CreateAndGetWeakRef()
    {
        ASSERT_PRINT(IsHeapObject() && ((value_ & TAG_WEAK_FILTER) == 0U),
                     "The least significant two bits of TaggedValue are not zero.");
        return TaggedValue(value_ | TAG_WEAK_MASK);
    }

    inline bool IsWeak() const
    {
        return IsHeapObject() && ((value_ & TAG_WEAK_MASK) == 1U);
    }

    inline bool IsDouble() const
    {
        return !IsInt() && !IsObject();
    }

    inline bool IsInt() const
    {
        return (value_ & TAG_MASK) == TAG_INT;
    }

    inline bool IsSpecial() const
    {
        return ((value_ & (~TAG_SPECIAL_MASK)) == 0U) && (((value_ & TAG_SPECIAL_VALUE) != 0U) || IsHole());
    }

    inline bool IsObject() const
    {
        return ((value_ & TAG_MASK) == TAG_OBJECT);
    }

    inline bool IsHeapObject() const
    {
        return IsObject() && !IsSpecial();
    }

    inline bool IsNumber() const
    {
        return !IsObject();
    }

    inline bool IsBoolean() const
    {
        return value_ == VALUE_FALSE || value_ == VALUE_TRUE;
    }

    inline double GetDouble() const
    {
        ASSERT_PRINT(IsDouble(), "can not convert TaggedValue to Double : " << std::hex << value_);
        return ReinterpretTaggedTypeToDouble(value_ - DOUBLE_ENCODE_OFFSET);
    }

    inline int GetInt() const
    {
        ASSERT_PRINT(IsInt(), "can not convert TaggedValue to Int :" << std::hex << value_);
        return static_cast<int>(value_ & (~TAG_MASK));
    }

    inline TaggedType GetRawData() const
    {
        return value_;
    }

    inline ObjectHeader *GetHeapObject() const
    {
        ASSERT_PRINT(IsHeapObject() && ((value_ & TAG_WEAK_FILTER) == 0U),
                     "can not convert TaggedValue to HeapObject :" << std::hex << value_);
        return reinterpret_cast<ObjectHeader *>(value_);
    }

    //  This function returns the heap object pointer which may have the weak tag.
    inline ObjectHeader *GetRawHeapObject() const
    {
        ASSERT_PRINT(IsHeapObject(), "can not convert TaggedValue to HeapObject :" << std::hex << value_);
        return reinterpret_cast<ObjectHeader *>(value_);
    }

    inline ObjectHeader *GetWeakReferent() const
    {
        ASSERT_PRINT(IsWeak(), "can not convert TaggedValue to WeakRef HeapObject :" << std::hex << value_);
        return reinterpret_cast<ObjectHeader *>(value_ & (~TAG_WEAK_MASK));
    }

    static inline TaggedType Cast(void *ptr)
    {
        ASSERT_PRINT(sizeof(void *) == TaggedTypeSize(), "32bit platform is not support yet");
        return static_cast<TaggedType>(ToUintPtr(ptr));
    }

    inline bool IsFalse() const
    {
        return value_ == VALUE_FALSE;
    }

    inline bool IsTrue() const
    {
        return value_ == VALUE_TRUE;
    }

    inline bool IsUndefined() const
    {
        return value_ == VALUE_UNDEFINED;
    }

    inline bool IsNull() const
    {
        return value_ == VALUE_NULL;
    }

    inline bool IsUndefinedOrNull() const
    {
        return IsNull() || IsUndefined();
    }

    inline bool IsHole() const
    {
        return value_ == VALUE_HOLE;
    }

    inline bool IsException() const
    {
        return value_ == VALUE_EXCEPTION;
    }

    static inline constexpr TaggedValue False()
    {
        return TaggedValue(VALUE_FALSE);
    }

    static inline constexpr TaggedValue True()
    {
        return TaggedValue(VALUE_TRUE);
    }

    static inline constexpr TaggedValue Undefined()
    {
        return TaggedValue(VALUE_UNDEFINED);
    }

    static inline constexpr TaggedValue Null()
    {
        return TaggedValue(VALUE_NULL);
    }

    static inline constexpr TaggedValue Hole()
    {
        return TaggedValue(VALUE_HOLE);
    }

    static inline constexpr TaggedValue Exception()
    {
        return TaggedValue(VALUE_EXCEPTION);
    }

    static inline constexpr size_t TaggedTypeSize()
    {
        return sizeof(TaggedType);
    }

    static inline bool IsImpureNaN(double value)
    {
        // Tests if the double value would break tagged double encoding.
        return bit_cast<TaggedType>(value) >= (TAG_INT - DOUBLE_ENCODE_OFFSET);
    }

    inline bool operator==(const TaggedValue &other) const
    {
        return value_ == other.value_;
    }

    inline bool operator!=(const TaggedValue &other) const
    {
        return value_ != other.value_;
    }

    ~TaggedValue() = default;

    DEFAULT_COPY_SEMANTIC(TaggedValue);
    DEFAULT_MOVE_SEMANTIC(TaggedValue);

private:
    TaggedType value_;
};

}  // namespace panda::coretypes

#endif  // PANDA_RUNTIME_INCLUDE_CORETYPES_TAGGED_VALUE_H_
