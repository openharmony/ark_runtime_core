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

#include <cstdint>

#include "runtime/include/runtime_options.h"
#include "runtime/interpreter/frame.h"

namespace panda::test {

Frame *CreateFrame(size_t nregs, Method *method, Frame *prev)
{
    Frame *mem = static_cast<Frame *>(aligned_alloc(8U, panda::Frame::GetSize(nregs)));
    return (new (mem) panda::Frame(method, prev, nregs));
}

void FreeFrame(Frame *f)
{
    std::free(f);
}

TEST(Frame, Test)
{
    Frame *f = panda::test::CreateFrame(2, nullptr, nullptr);
    f->GetVReg(0).MarkAsObject();
    EXPECT_TRUE(f->GetVReg(0).HasObject());

    f->GetVReg(0).MarkAsPrimitive();
    EXPECT_FALSE(f->GetVReg(0).HasObject());

    int64_t v64 = 0x1122334455667788;
    f->GetVReg(0).MarkAsObject();
    f->GetVReg(0).SetPrimitive(v64);
    EXPECT_EQ(f->GetVReg(0).GetLong(), v64);
    EXPECT_EQ(f->GetVReg(0).GetAs<int64_t>(), v64);

    f->GetVReg(1).MarkAsObject();
    f->GetVReg(1).MoveFrom(f->GetVReg(0));
    EXPECT_FALSE(f->GetVReg(0).HasObject());
    EXPECT_EQ(f->GetVReg(0).Get(), static_cast<int32_t>(v64));

    f->GetVReg(1).MarkAsObject();
    f->GetVReg(1).MoveFrom(f->GetVReg(0));
    EXPECT_FALSE(f->GetVReg(0).HasObject());
    EXPECT_EQ(f->GetVReg(0).GetLong(), v64);

    ObjectHeader *ptr = reinterpret_cast<ObjectHeader *>(0x11223344);
    f->GetVReg(0).SetReference(ptr);
    f->GetVReg(1).MarkAsPrimitive();
    f->GetVReg(1).MoveFromObj(f->GetVReg(0));
    EXPECT_TRUE(f->GetVReg(0).HasObject());
    EXPECT_EQ(f->GetVReg(0).GetReference(), ptr);

    int32_t v32 = 0x11223344;
    f->GetVReg(0).MarkAsObject();
    f->GetVReg(0).SetPrimitive(v32);
    EXPECT_EQ(f->GetVReg(0).Get(), v32);
    EXPECT_EQ(f->GetVReg(0).GetAs<int32_t>(), v32);

    int16_t v16 = 0x1122;
    f->GetVReg(0).MarkAsObject();
    f->GetVReg(0).SetPrimitive(v16);
    EXPECT_EQ(f->GetVReg(0).Get(), v16);
    EXPECT_EQ(f->GetVReg(0).GetAs<int32_t>(), v16);

    int8_t v8 = 0x11;
    f->GetVReg(0).MarkAsObject();
    f->GetVReg(0).SetPrimitive(v8);
    EXPECT_EQ(f->GetVReg(0).Get(), v8);
    EXPECT_EQ(f->GetVReg(0).GetAs<int32_t>(), v8);

    float f32 = 123.5;
    f->GetVReg(0).MarkAsObject();
    f->GetVReg(0).SetPrimitive(f32);
    EXPECT_EQ(f->GetVReg(0).GetFloat(), f32);
    EXPECT_EQ(f->GetVReg(0).GetAs<float>(), f32);

    double f64 = 456.7;
    f->GetVReg(0).MarkAsObject();
    f->GetVReg(0).SetPrimitive(f64);
    EXPECT_EQ(f->GetVReg(0).GetDouble(), f64);
    EXPECT_EQ(f->GetVReg(0).GetAs<double>(), f64);

    panda::test::FreeFrame(f);
}

}  // namespace panda::test
