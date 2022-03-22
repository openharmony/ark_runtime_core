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
#include "os/mem.h"
#include "mem/mem.h"
#include <sys/mman.h>
#include "utils/asan_interface.h"

#include "gtest/gtest.h"

namespace panda {

class MMapFixedTest : public testing::Test {
protected:
    static constexpr uint64_t MAGIC_VALUE = 0xDEADBEAF;
    void DeathWrite64(uintptr_t addr) const
    {
        auto pointer = static_cast<uint64_t *>(ToVoidPtr(addr));
        *pointer = MAGIC_VALUE;
    }
};

TEST_F(MMapFixedTest, MMapAsanTest)
{
    static constexpr size_t OFFSET = 4_KB;
    static constexpr size_t MMAP_ALLOC_SIZE = OFFSET * 2;
    size_t page_size = panda::os::mem::GetPageSize();
    static_assert(OFFSET < panda::os::mem::MMAP_FIXED_MAGIC_ADDR_FOR_ASAN);
    static_assert(MMAP_ALLOC_SIZE > OFFSET);
    ASSERT_TRUE((MMAP_ALLOC_SIZE % page_size) == 0);
    uintptr_t cur_addr = panda::os::mem::MMAP_FIXED_MAGIC_ADDR_FOR_ASAN - OFFSET;
    cur_addr = AlignUp(cur_addr, page_size);
    ASSERT_TRUE((cur_addr % page_size) == 0);
    uintptr_t end_addr = panda::os::mem::MMAP_FIXED_MAGIC_ADDR_FOR_ASAN;
    end_addr = AlignUp(end_addr, sizeof(uint64_t));
    void *result =  // NOLINTNEXTLINE(hicpp-signed-bitwise)
        mmap(ToVoidPtr(cur_addr), MMAP_ALLOC_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1,
             0);
    ASSERT_TRUE(result != nullptr);
    ASSERT_TRUE(ToUintPtr(result) == cur_addr);
    while (cur_addr < end_addr) {
        DeathWrite64(cur_addr);
        cur_addr += sizeof(uint64_t);
    }
#if defined(PANDA_ASAN_ON)
    // Check Death:
    EXPECT_DEATH(DeathWrite64(end_addr), "");
#else
    // Writing must be finished successfully
    DeathWrite64(end_addr);
#endif
    munmap(result, MMAP_ALLOC_SIZE);
}

}  // namespace panda
