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
#include "mem/mem.h"
#include "mem/mmap_mem_pool-inl.h"

#include "gtest/gtest.h"

namespace panda {

class MMapMemPoolTest : public testing::Test {
public:
    MMapMemPoolTest()
    {
        instance_ = nullptr;
    }

    ~MMapMemPoolTest()
    {
        if (instance_ != nullptr) {
            delete instance_;
        }
        FinalizeMemConfig();
    }

protected:
    MmapMemPool *CreateMMapMemPool(size_t object_pool_size = 0, size_t internal_size = 0, size_t compiler_size = 0,
                                   size_t code_size = 0)
    {
        ASSERT(instance_ == nullptr);
        SetupMemConfig(object_pool_size, internal_size, compiler_size, code_size);
        instance_ = new MmapMemPool();
        return instance_;
    }

private:
    void SetupMemConfig(size_t object_pool_size, size_t internal_size, size_t compiler_size, size_t code_size) const
    {
        mem::MemConfig::Initialize(object_pool_size, internal_size, compiler_size, code_size);
    }

    void FinalizeMemConfig() const
    {
        mem::MemConfig::Finalize();
    }

    MmapMemPool *instance_;
};

TEST_F(MMapMemPoolTest, HeapOOMTest)
{
    MmapMemPool *memPool = CreateMMapMemPool(4_MB);
    ASSERT_TRUE(
        memPool->AllocPool(4_MB, SpaceType::SPACE_TYPE_HUMONGOUS_OBJECT, AllocatorType::HUMONGOUS_ALLOCATOR).GetMem() !=
        nullptr);
    ASSERT_TRUE(
        memPool->AllocPool(4_MB, SpaceType::SPACE_TYPE_HUMONGOUS_OBJECT, AllocatorType::HUMONGOUS_ALLOCATOR).GetMem() ==
        nullptr);
    ASSERT_TRUE(memPool->AllocPool(4_MB, SpaceType::SPACE_TYPE_NON_MOVABLE_OBJECT, AllocatorType::HUMONGOUS_ALLOCATOR)
                    .GetMem() == nullptr);
    ASSERT_TRUE(memPool->AllocPool(4_MB, SpaceType::SPACE_TYPE_OBJECT, AllocatorType::HUMONGOUS_ALLOCATOR).GetMem() ==
                nullptr);
}

TEST_F(MMapMemPoolTest, HeapOOMAndAllocInOtherSpacesTest)
{
    MmapMemPool *memPool = CreateMMapMemPool(4_MB, 1_MB, 1_MB, 1_MB);
    ASSERT_TRUE(memPool->AllocPool(4_MB, SpaceType::SPACE_TYPE_OBJECT, AllocatorType::BUMP_ALLOCATOR).GetMem() !=
                nullptr);
    ASSERT_TRUE(memPool->AllocPool(4_MB, SpaceType::SPACE_TYPE_OBJECT, AllocatorType::BUMP_ALLOCATOR).GetMem() ==
                nullptr);
    ASSERT_TRUE(memPool->AllocPool(1_MB, SpaceType::SPACE_TYPE_COMPILER, AllocatorType::BUMP_ALLOCATOR).GetMem() !=
                nullptr);
    ASSERT_TRUE(memPool->AllocPool(1_MB, SpaceType::SPACE_TYPE_CODE, AllocatorType::BUMP_ALLOCATOR).GetMem() !=
                nullptr);
    ASSERT_TRUE(memPool->AllocPool(1_MB, SpaceType::SPACE_TYPE_INTERNAL, AllocatorType::BUMP_ALLOCATOR).GetMem() !=
                nullptr);
}

TEST_F(MMapMemPoolTest, GetAllocatorInfoTest)
{
    static constexpr AllocatorType ALLOC_TYPE = AllocatorType::BUMP_ALLOCATOR;
    static constexpr size_t POOL_SIZE = 4_MB;
    static constexpr size_t POINTER_POOL_OFFSET = 1_MB;
    ASSERT_TRUE(POINTER_POOL_OFFSET < POOL_SIZE);
    MmapMemPool *memPool = CreateMMapMemPool(POOL_SIZE * 2, 0, 0, 0);
    int *allocator_addr = new int();
    Pool pool_with_alloc_addr = memPool->AllocPool(POOL_SIZE, SpaceType::SPACE_TYPE_OBJECT, ALLOC_TYPE, allocator_addr);
    Pool pool_without_alloc_addr = memPool->AllocPool(POOL_SIZE, SpaceType::SPACE_TYPE_OBJECT, ALLOC_TYPE);
    ASSERT_TRUE(pool_with_alloc_addr.GetMem() != nullptr);
    ASSERT_TRUE(pool_without_alloc_addr.GetMem() != nullptr);

    void *first_pool_pointer = ToVoidPtr(ToUintPtr(pool_with_alloc_addr.GetMem()) + POINTER_POOL_OFFSET);
    ASSERT_TRUE(ToUintPtr(memPool->GetAllocatorInfoForAddr(first_pool_pointer).GetAllocatorHeaderAddr()) ==
                ToUintPtr(allocator_addr));
    ASSERT_TRUE(memPool->GetAllocatorInfoForAddr(first_pool_pointer).GetType() == ALLOC_TYPE);
    ASSERT_TRUE(ToUintPtr(memPool->GetStartAddrPoolForAddr(first_pool_pointer)) ==
                ToUintPtr(pool_with_alloc_addr.GetMem()));

    void *second_pool_pointer = ToVoidPtr(ToUintPtr(pool_without_alloc_addr.GetMem()) + POINTER_POOL_OFFSET);
    ASSERT_TRUE(ToUintPtr(memPool->GetAllocatorInfoForAddr(second_pool_pointer).GetAllocatorHeaderAddr()) ==
                ToUintPtr(pool_without_alloc_addr.GetMem()));
    ASSERT_TRUE(memPool->GetAllocatorInfoForAddr(second_pool_pointer).GetType() == ALLOC_TYPE);
    ASSERT_TRUE(ToUintPtr(memPool->GetStartAddrPoolForAddr(second_pool_pointer)) ==
                ToUintPtr(pool_without_alloc_addr.GetMem()));

    delete allocator_addr;
}

TEST_F(MMapMemPoolTest, CheckLimitsForInternalSpacesTest)
{
#ifndef PANDA_TARGET_32
    MmapMemPool *memPool = CreateMMapMemPool(1_GB, 5_GB, 5_GB, 5_GB);
    Pool object_pool = memPool->AllocPool(1_GB, SpaceType::SPACE_TYPE_OBJECT, AllocatorType::BUMP_ALLOCATOR);
    Pool internal_pool = memPool->AllocPool(5_GB, SpaceType::SPACE_TYPE_COMPILER, AllocatorType::BUMP_ALLOCATOR);
    Pool code_pool = memPool->AllocPool(5_GB, SpaceType::SPACE_TYPE_CODE, AllocatorType::BUMP_ALLOCATOR);
    Pool compiler_pool = memPool->AllocPool(5_GB, SpaceType::SPACE_TYPE_INTERNAL, AllocatorType::BUMP_ALLOCATOR);
    // Check that these pools have been created successfully
    ASSERT_TRUE(object_pool.GetMem() != nullptr);
    ASSERT_TRUE(internal_pool.GetMem() != nullptr);
    ASSERT_TRUE(code_pool.GetMem() != nullptr);
    ASSERT_TRUE(compiler_pool.GetMem() != nullptr);
    // Check that part of internal pools are located in 64 bits address space
    ASSERT_TRUE((ToUintPtr(internal_pool.GetMem()) + internal_pool.GetSize() - 1U) >
                std::numeric_limits<uint32_t>::max());
    ASSERT_TRUE((ToUintPtr(code_pool.GetMem()) + code_pool.GetSize() - 1U) > std::numeric_limits<uint32_t>::max());
    ASSERT_TRUE((ToUintPtr(compiler_pool.GetMem()) + compiler_pool.GetSize() - 1U) >
                std::numeric_limits<uint32_t>::max());
#endif
}

TEST_F(MMapMemPoolTest, PoolReturnTest)
{
    MmapMemPool *memPool = CreateMMapMemPool(8_MB);
    auto pool1 = memPool->AllocPool(4_MB, SpaceType::SPACE_TYPE_OBJECT, AllocatorType::HUMONGOUS_ALLOCATOR);
    ASSERT_TRUE(pool1.GetMem() != nullptr);
    auto pool2 = memPool->AllocPool(4_MB, SpaceType::SPACE_TYPE_OBJECT, AllocatorType::HUMONGOUS_ALLOCATOR);
    ASSERT_TRUE(pool2.GetMem() != nullptr);
    auto pool3 = memPool->AllocPool(4_MB, SpaceType::SPACE_TYPE_OBJECT, AllocatorType::HUMONGOUS_ALLOCATOR);
    ASSERT_TRUE(pool3.GetMem() == nullptr);
    memPool->FreePool(pool1.GetMem(), pool1.GetSize());
    memPool->FreePool(pool2.GetMem(), pool2.GetSize());
    auto pool4 = memPool->AllocPool(6_MB, SpaceType::SPACE_TYPE_OBJECT, AllocatorType::HUMONGOUS_ALLOCATOR);
    ASSERT_TRUE(pool4.GetMem() != nullptr);
    auto pool5 = memPool->AllocPool(1_MB, SpaceType::SPACE_TYPE_OBJECT, AllocatorType::HUMONGOUS_ALLOCATOR);
    ASSERT_TRUE(pool5.GetMem() != nullptr);
    auto pool6 = memPool->AllocPool(1_MB, SpaceType::SPACE_TYPE_OBJECT, AllocatorType::HUMONGOUS_ALLOCATOR);
    ASSERT_TRUE(pool6.GetMem() != nullptr);
    memPool->FreePool(pool6.GetMem(), pool6.GetSize());
    memPool->FreePool(pool4.GetMem(), pool4.GetSize());
    memPool->FreePool(pool5.GetMem(), pool5.GetSize());
    auto pool7 = memPool->AllocPool(8_MB, SpaceType::SPACE_TYPE_OBJECT, AllocatorType::HUMONGOUS_ALLOCATOR);
    ASSERT_TRUE(pool7.GetMem() != nullptr);
}

}  // namespace panda
