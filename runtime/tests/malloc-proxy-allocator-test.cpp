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

#include <sys/mman.h>

#include <array>

#include "runtime/mem/malloc-proxy-allocator-inl.h"
#include "runtime/tests/allocator_test_base.h"

namespace panda::mem {

using MallocProxyNonObjectAllocator = MallocProxyAllocator<EmptyAllocConfigWithCrossingMap>;

class MallocProxyAllocatorTest : public AllocatorTest<MallocProxyNonObjectAllocator> {
public:
    MallocProxyAllocatorTest() = default;
    ~MallocProxyAllocatorTest() = default;

protected:
    static constexpr size_t SIZE_ALLOC = 1_KB;

    void AddMemoryPoolToAllocator([[maybe_unused]] MallocProxyNonObjectAllocator &allocator) override {}
    void AddMemoryPoolToAllocatorProtected([[maybe_unused]] MallocProxyNonObjectAllocator &allocator) override {}
    bool AllocatedByThisAllocator([[maybe_unused]] MallocProxyNonObjectAllocator &allocator,
                                  [[maybe_unused]] void *mem) override
    {
        return false;
    }
};

TEST_F(MallocProxyAllocatorTest, SimpleTest)
{
    static constexpr size_t SIZE = 23;
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    MallocProxyNonObjectAllocator allocator(mem_stats);
    void *a1;
    a1 = allocator.Alloc(SIZE);
    allocator.Free(a1);
    delete mem_stats;
}

TEST_F(MallocProxyAllocatorTest, AlignedAllocFreeTest)
{
    AlignedAllocFreeTest<1, SIZE_ALLOC>();
}

TEST_F(MallocProxyAllocatorTest, AllocFreeTest)
{
    AllocateFreeDifferentSizesTest<1, 4 * SIZE_ALLOC>();
}

TEST_F(MallocProxyAllocatorTest, AdapterTest)
{
    mem::MemStatsType *mem_stats = new mem::MemStatsType();
    MallocProxyNonObjectAllocator allocator(mem_stats);
    std::array<int, 20> arr {{12, 14, 3, 5, 43, 12, 22, 42, 89, 10, 89, 32, 43, 12, 43, 12, 54, 89, 27, 84}};

    std::vector<void *> v;
    for (auto i : arr) {
        auto *mem = allocator.Alloc(i);
        v.push_back(mem);
    }
    for (auto *mem : v) {
        allocator.Free(mem);
    }
    delete mem_stats;
}

}  // namespace panda::mem
