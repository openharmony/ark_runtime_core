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

#include <gtest/gtest.h>

#include <vector>

#include "libpandabase/mem/mem.h"
#include "runtime/include/class_helper.h"
#include "runtime/include/coretypes/class.h"
#include "runtime/include/coretypes/tagged_value.h"

namespace panda::test {

using TaggedValue = coretypes::TaggedValue;
static constexpr size_t OBJECT_POINTER_SIZE = sizeof(object_pointer_type);
static constexpr size_t POINTER_SIZE = ClassHelper::POINTER_SIZE;

TEST(ClassSizeTest, TestSizeOfEmptyClass)
{
    const size_t aligned_class_size = AlignUp(sizeof(Class), OBJECT_POINTER_SIZE);
    ASSERT_EQ(aligned_class_size, ClassHelper::ComputeClassSize(0, 0, 0, 0, 0, 0, 0, 0));
}

TEST(ClassSizeTest, TestSizeOfClassWithVtbl)
{
    const size_t aligned_class_size = AlignUp(sizeof(Class), OBJECT_POINTER_SIZE);
    const size_t vtbl_size = 5;
    ASSERT_EQ(aligned_class_size + vtbl_size * POINTER_SIZE,
              ClassHelper::ComputeClassSize(vtbl_size, 0, 0, 0, 0, 0, 0, 0));
}

TEST(ClassSizeTest, TestSizeOfClassWith8BitFields)
{
    const size_t aligned_class_size = AlignUp(sizeof(Class), OBJECT_POINTER_SIZE);
    const size_t num_8bit_fields = 1;
    ASSERT_EQ(aligned_class_size + num_8bit_fields * sizeof(int8_t),
              ClassHelper::ComputeClassSize(0, 0, num_8bit_fields, 0, 0, 0, 0, 0));
}

TEST(ClassSizeTest, TestSizeOfClassWith16BitFields)
{
    const size_t aligned_class_size = AlignUp(sizeof(Class), OBJECT_POINTER_SIZE);
    const size_t num_16bit_fields = 1;
    ASSERT_EQ(aligned_class_size + num_16bit_fields * sizeof(int16_t),
              ClassHelper::ComputeClassSize(0, 0, 0, num_16bit_fields, 0, 0, 0, 0));
}

TEST(ClassSizeTest, TestSizeOfClassWith32BitFields)
{
    const size_t aligned_class_size = AlignUp(sizeof(Class), OBJECT_POINTER_SIZE);
    const size_t num_32bit_fields = 1;
    ASSERT_EQ(aligned_class_size + num_32bit_fields * sizeof(int32_t),
              ClassHelper::ComputeClassSize(0, 0, 0, 0, num_32bit_fields, 0, 0, 0));
}

TEST(ClassSizeTest, TestSizeOfClassWith64BitFields)
{
    const size_t aligned_class_size = AlignUp(sizeof(Class), OBJECT_POINTER_SIZE);
    const size_t num_64bit_fields = 1;
    if (AlignUp(aligned_class_size, sizeof(int64_t)) == aligned_class_size) {
        ASSERT_EQ(aligned_class_size + num_64bit_fields * sizeof(int64_t),
                  ClassHelper::ComputeClassSize(0, 0, 0, 0, 0, num_64bit_fields, 0, 0));
    } else {
        ASSERT_EQ(AlignUp(aligned_class_size, sizeof(int64_t)) + num_64bit_fields * sizeof(int64_t),
                  ClassHelper::ComputeClassSize(0, 0, 0, 0, 0, num_64bit_fields, 0, 0));
    }
}

TEST(ClassSizeTest, TestSizeOfClassWithRefFields)
{
    const size_t aligned_class_size = AlignUp(sizeof(Class), OBJECT_POINTER_SIZE);
    const size_t num_ref_fields = 1;
    ASSERT_EQ(aligned_class_size + num_ref_fields * OBJECT_POINTER_SIZE,
              ClassHelper::ComputeClassSize(0, 0, 0, 0, 0, 0, num_ref_fields, 0));
}

TEST(ClassSizeTest, TestSizeOfClassWithAnyFields)
{
    const size_t aligned_class_size = AlignUp(sizeof(Class), OBJECT_POINTER_SIZE);
    const size_t num_any_fields = 1;
    if (AlignUp(aligned_class_size, TaggedValue::TaggedTypeSize()) == aligned_class_size) {
        ASSERT_EQ(aligned_class_size + num_any_fields * TaggedValue::TaggedTypeSize(),
                  ClassHelper::ComputeClassSize(0, 0, 0, 0, 0, 0, 0, num_any_fields));
    } else {
        ASSERT_EQ(AlignUp(aligned_class_size, TaggedValue::TaggedTypeSize()) +
                      num_any_fields * TaggedValue::TaggedTypeSize(),
                  ClassHelper::ComputeClassSize(0, 0, 0, 0, 0, 0, 0, num_any_fields));
    }
}

TEST(ClassSizeTest, TestHoleFilling)
{
    const size_t aligned_class_size = AlignUp(sizeof(Class), OBJECT_POINTER_SIZE);
    const size_t num_64bit_fields = 1;
    if (AlignUp(aligned_class_size, sizeof(int64_t)) != aligned_class_size) {
        const size_t num_8bit_fields = 1;
        ASSERT_EQ(AlignUp(aligned_class_size, sizeof(int64_t)) + num_64bit_fields * sizeof(int64_t),
                  ClassHelper::ComputeClassSize(0, 0, num_8bit_fields, 0, 0, num_64bit_fields, 0, 0));

        const size_t num_16bit_fields = 1;
        ASSERT_EQ(AlignUp(aligned_class_size, sizeof(int64_t)) + num_64bit_fields * sizeof(int64_t),
                  ClassHelper::ComputeClassSize(0, 0, 0, num_16bit_fields, 0, num_64bit_fields, 0, 0));

        const size_t num_32bit_fields = 1;
        ASSERT_EQ(AlignUp(aligned_class_size, sizeof(int64_t)) + num_64bit_fields * sizeof(int64_t),
                  ClassHelper::ComputeClassSize(0, 0, 0, 0, num_32bit_fields, num_64bit_fields, 0, 0));
    }

    const size_t num_any_fields = 1;
    if (AlignUp(aligned_class_size, TaggedValue::TaggedTypeSize()) != aligned_class_size) {
        const size_t num_8bit_fields = 1;
        ASSERT_EQ(AlignUp(aligned_class_size, TaggedValue::TaggedTypeSize()) +
                      num_any_fields * TaggedValue::TaggedTypeSize(),
                  ClassHelper::ComputeClassSize(0, 0, num_8bit_fields, 0, 0, num_any_fields, 0, 0));

        const size_t num_16bit_fields = 1;
        ASSERT_EQ(AlignUp(aligned_class_size, TaggedValue::TaggedTypeSize()) +
                      num_any_fields * TaggedValue::TaggedTypeSize(),
                  ClassHelper::ComputeClassSize(0, 0, 0, num_16bit_fields, 0, num_any_fields, 0, 0));

        const size_t num_32bit_fields = 1;
        ASSERT_EQ(AlignUp(aligned_class_size, TaggedValue::TaggedTypeSize()) +
                      num_any_fields * TaggedValue::TaggedTypeSize(),
                  ClassHelper::ComputeClassSize(0, 0, 0, 0, num_32bit_fields, num_any_fields, 0, 0));
    }
}

}  // namespace panda::test
