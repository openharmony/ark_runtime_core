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

#include <gtest/gtest.h>

#include "runtime/include/class-inl.h"
#include "runtime/include/class_linker.h"
#include "runtime/include/coretypes/array.h"
#include "runtime/include/runtime.h"

namespace panda::coretypes::test {

class ArrayTest : public testing::Test {
public:
    ArrayTest()
    {
        // We need to create a runtime instance to be able to create strings.
        options_.SetShouldLoadBootPandaFiles(false);
        options_.SetShouldInitializeIntrinsics(false);
        Runtime::Create(options_);
        thread_ = panda::MTManagedThread::GetCurrent();
        thread_->ManagedCodeBegin();
    }

    ~ArrayTest()
    {
        thread_->ManagedCodeEnd();
        Runtime::Destroy();
    }

protected:
    panda::MTManagedThread *thread_ {nullptr};
    RuntimeOptions options_;
};

static size_t GetArrayObjectSize(panda::Class *klass, size_t n)
{
    return sizeof(Array) + klass->GetComponentSize() * n;
}

static void TestArrayObjectSize(ClassRoot class_root, uint32_t n)
{
    std::string msg = "Test with class_root ";
    msg += static_cast<int>(class_root);

    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto *klass = Runtime::GetCurrent()->GetClassLinker()->GetExtension(ctx)->GetClassRoot(class_root);

    Array *array = Array::Create(klass, n);
    ASSERT_NE(array, nullptr) << msg;

    ASSERT_EQ(array->ObjectSize(), GetArrayObjectSize(klass, n)) << msg;
}

TEST_F(ArrayTest, ObjectSize)
{
    TestArrayObjectSize(ClassRoot::ARRAY_U1, 10);
    TestArrayObjectSize(ClassRoot::ARRAY_I8, 10);
    TestArrayObjectSize(ClassRoot::ARRAY_U8, 10);
    TestArrayObjectSize(ClassRoot::ARRAY_I16, 10);
    TestArrayObjectSize(ClassRoot::ARRAY_U16, 10);
    TestArrayObjectSize(ClassRoot::ARRAY_I32, 10);
    TestArrayObjectSize(ClassRoot::ARRAY_U32, 10);
    TestArrayObjectSize(ClassRoot::ARRAY_I64, 10);
    TestArrayObjectSize(ClassRoot::ARRAY_U64, 10);
    TestArrayObjectSize(ClassRoot::ARRAY_F32, 10);
    TestArrayObjectSize(ClassRoot::ARRAY_F64, 10);
}

}  // namespace panda::coretypes::test
