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

#include "runtime/tests/pygote_space_allocator_test_base.h"

namespace panda::mem {

class PygoteSpaceAllocatorGenTest : public PygoteSpaceAllocatorTest {
public:
    PygoteSpaceAllocatorGenTest()
    {
        options_.SetShouldLoadBootPandaFiles(false);
        options_.SetShouldInitializeIntrinsics(false);
        options_.SetStartAsZygote(true);
        options_.SetGcType("gen-gc");
        Runtime::Create(options_);
        thread_ = panda::MTManagedThread::GetCurrent();
        thread_->ManagedCodeBegin();
    }

    ~PygoteSpaceAllocatorGenTest()
    {
        thread_->ManagedCodeEnd();
        Runtime::Destroy();
    }
};

TEST_F(PygoteSpaceAllocatorGenTest, InitAllocTest)
{
    InitAllocTest();
}

TEST_F(PygoteSpaceAllocatorGenTest, ForkedAllocTest)
{
    ForkedAllocTest();
}

TEST_F(PygoteSpaceAllocatorGenTest, NonMovableLiveObjectAllocTest)
{
    NonMovableLiveObjectAllocTest();
}

TEST_F(PygoteSpaceAllocatorGenTest, NonMovableUnliveObjectAllocTest)
{
    NonMovableUnliveObjectAllocTest();
}

TEST_F(PygoteSpaceAllocatorGenTest, MovableLiveObjectAllocTest)
{
    MovableLiveObjectAllocTest();
}

TEST_F(PygoteSpaceAllocatorGenTest, MovableUnliveObjectAllocTest)
{
    MovableUnliveObjectAllocTest();
}

TEST_F(PygoteSpaceAllocatorGenTest, MuchObjectAllocTest)
{
    MuchObjectAllocTest();
}

}  // namespace panda::mem
