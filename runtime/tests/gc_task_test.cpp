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
#include "runtime/include/gc_task.h"

namespace panda::test {

class GcTaskTest : public testing::Test {
};

TEST_F(GcTaskTest, TestPriority)
{
    ASSERT_LT(GCTaskCause::YOUNG_GC_CAUSE, GCTaskCause::OOM_CAUSE);
    ASSERT_LT(GCTaskCause::YOUNG_GC_CAUSE, GCTaskCause::EXPLICIT_CAUSE);
    ASSERT_LT(GCTaskCause::YOUNG_GC_CAUSE, GCTaskCause::HEAP_USAGE_THRESHOLD_CAUSE);
    ASSERT_LT(GCTaskCause::EXPLICIT_CAUSE, GCTaskCause::OOM_CAUSE);
}

}  // namespace panda::test
