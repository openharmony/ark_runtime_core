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

#include "mem/code_allocator.h"
#include "mem/pool_manager.h"
#include "mem/base_mem_stats.h"

#include "gtest/gtest.h"
#include "utils/logger.h"

namespace panda {

class CodeAllocatorTest : public testing::Test {
public:
    CodeAllocatorTest() {}

    ~CodeAllocatorTest() {}

protected:
    static constexpr size_t k1K = 1024;
    static constexpr size_t testHeapSize = k1K * k1K;

    static bool IsAligned(const void *ptr, size_t alignment)
    {
        return reinterpret_cast<uintptr_t>(ptr) % alignment == 0;
    }

    void SetUp() override
    {
        panda::mem::MemConfig::Initialize(0, 32_MB, 0, 32_MB);
        PoolManager::Initialize();
    }

    void TearDown() override
    {
        PoolManager::Finalize();
        panda::mem::MemConfig::Finalize();
    }
};

TEST_F(CodeAllocatorTest, AllocateBuffTest)
{
    BaseMemStats stats;
    CodeAllocator ca(&stats);
    uint8_t buff[] = {0xCC, 0xCC};
    void *code_buff = ca.AllocateCode(sizeof(buff), static_cast<void *>(&buff[0]));
    for (size_t i = 0; i < sizeof(buff); i++) {
        ASSERT_EQ(static_cast<uint8_t *>(code_buff)[i], 0xCC);
    }
    ASSERT_TRUE(IsAligned(code_buff, 4 * SIZE_1K));
}

}  // namespace panda
