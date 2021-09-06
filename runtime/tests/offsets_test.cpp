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

#include "runtime/include/thread.h"
#include "runtime/include/method.h"

#include <gtest/gtest.h>

namespace panda {

#define CHECK_OFFSET(klass, member)                      \
    do {                                                 \
        ASSERT_EQ(MEMBER_OFFSET(klass, member), offset); \
        offset += klass::ELEMENTS_ALIGN;                 \
    } while (0)

TEST(Offsets, Thread)
{
    size_t offset = 0;
    ASSERT_EQ(MEMBER_OFFSET(ManagedThread, stor_32_), std::is_polymorphic_v<ManagedThread> * sizeof(uint64_t));
    CHECK_OFFSET(ManagedThread::StoragePacked32, is_compiled_frame_);
    CHECK_OFFSET(ManagedThread::StoragePacked32, fts_);
    ASSERT_EQ(ManagedThread::StoragePacked32::ELEMENTS_NUM * ManagedThread::StoragePacked32::ELEMENTS_ALIGN, offset);

    offset = 0;
    ASSERT_EQ(MEMBER_OFFSET(ManagedThread, stor_ptr_),
              MEMBER_OFFSET(ManagedThread, stor_32_) + ManagedThread::StoragePacked32::GetSize());
    CHECK_OFFSET(ManagedThread::StoragePackedPtr, object_);
    CHECK_OFFSET(ManagedThread::StoragePackedPtr, frame_);
    CHECK_OFFSET(ManagedThread::StoragePackedPtr, exception_);
    CHECK_OFFSET(ManagedThread::StoragePackedPtr, native_pc_);
    CHECK_OFFSET(ManagedThread::StoragePackedPtr, tlab_);
    CHECK_OFFSET(ManagedThread::StoragePackedPtr, card_table_addr_);
    CHECK_OFFSET(ManagedThread::StoragePackedPtr, card_table_min_addr_);
    CHECK_OFFSET(ManagedThread::StoragePackedPtr, concurrent_marking_addr_);
    CHECK_OFFSET(ManagedThread::StoragePackedPtr, string_class_ptr_);
    ASSERT_EQ(ManagedThread::StoragePackedPtr::ELEMENTS_NUM * ManagedThread::StoragePackedPtr::ELEMENTS_ALIGN, offset);
}

TEST(Offsets, Method)
{
    size_t offset = 0;
    ASSERT_EQ(MEMBER_OFFSET(Method, stor_32_), std::is_polymorphic_v<Method> * sizeof(uint64_t));
    CHECK_OFFSET(Method::StoragePacked32, access_flags_);
    CHECK_OFFSET(Method::StoragePacked32, vtable_index_);
    CHECK_OFFSET(Method::StoragePacked32, num_args_);
    CHECK_OFFSET(Method::StoragePacked32, hotness_counter_);
    ASSERT_EQ(Method::StoragePacked32::ELEMENTS_NUM * Method::StoragePacked32::ELEMENTS_ALIGN, offset);

    offset = 0;
    ASSERT_EQ(MEMBER_OFFSET(Method, stor_ptr_), MEMBER_OFFSET(Method, stor_32_) + Method::StoragePacked32::GetSize());
    CHECK_OFFSET(Method::StoragePackedPtr, class_);
    CHECK_OFFSET(Method::StoragePackedPtr, compiled_entry_point_);
    CHECK_OFFSET(Method::StoragePackedPtr, native_pointer_);
    ASSERT_EQ(Method::StoragePackedPtr::ELEMENTS_NUM * Method::StoragePackedPtr::ELEMENTS_ALIGN, offset);
}

}  // namespace panda
