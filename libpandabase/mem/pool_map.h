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

#ifndef PANDA_LIBPANDABASE_MEM_POOL_MAP_H_
#define PANDA_LIBPANDABASE_MEM_POOL_MAP_H_

#include <cstddef>
#include <array>
#include "macros.h"
#include "mem.h"
#include "space.h"

namespace panda {

enum class AllocatorType {
    UNDEFINED,
    RUNSLOTS_ALLOCATOR,
    FREELIST_ALLOCATOR,
    HUMONGOUS_ALLOCATOR,
    ARENA_ALLOCATOR,
    TLAB_ALLOCATOR,
    BUMP_ALLOCATOR,
    REGION_ALLOCATOR,
    FRAME_ALLOCATOR,
    BUMP_ALLOCATOR_WITH_TLABS,
};

class AllocatorInfo {
public:
    explicit constexpr AllocatorInfo(AllocatorType type, const void *addr) : type_(type), header_addr_(addr)
    {
        // We can't create AllocatorInfo without correct pointer to the allocator header.
        ASSERT(header_addr_ != nullptr);
    }

    AllocatorType GetType() const
    {
        return type_;
    }

    const void *GetAllocatorHeaderAddr() const
    {
        return header_addr_;
    }

    virtual ~AllocatorInfo() = default;

    DEFAULT_COPY_SEMANTIC(AllocatorInfo);
    DEFAULT_MOVE_SEMANTIC(AllocatorInfo);

private:
    AllocatorType type_;
    const void *header_addr_;
};

// PoolMap is used to manage all pools which have been given to the allocators.
// It can be used to find which allocator has been used to allocate an object.
class PoolMap {
public:
    void AddPoolToMap(const void *pool_addr, size_t pool_size, SpaceType space_type, AllocatorType allocator_type,
                      const void *allocator_addr);
    void RemovePoolFromMap(void *pool_addr, size_t pool_size);
    // Get Allocator info for the object allocated at this address.
    AllocatorInfo GetAllocatorInfo(const void *addr) const;

    void *GetFirstByteOfPoolForAddr(const void *addr);

    SpaceType GetSpaceType(const void *addr) const;

private:
    static constexpr uint64_t POOL_MAP_COVERAGE = PANDA_MAX_HEAP_SIZE;
    static constexpr size_t POOL_MAP_GRANULARITY = PANDA_POOL_ALIGNMENT_IN_BYTES;
    static constexpr size_t POOL_MAP_SIZE = POOL_MAP_COVERAGE / POOL_MAP_GRANULARITY;

    static constexpr bool FIRST_BYTE_IN_SEGMENT_VALUE = true;

    class PoolInfo {
    public:
        void Initialize(bool first_byte_in_segment, SpaceType space_type, AllocatorType allocator_type,
                        const void *allocator_addr)
        {
            ASSERT(first_byte_in_segment_ == FIRST_BYTE_IN_SEGMENT_VALUE);
            ASSERT(allocator_type_ == AllocatorType::UNDEFINED);
            first_byte_in_segment_ = first_byte_in_segment;
            allocator_addr_ = allocator_addr;
            space_type_ = space_type;
            allocator_type_ = allocator_type;
        }

        void Destroy()
        {
            first_byte_in_segment_ = FIRST_BYTE_IN_SEGMENT_VALUE;
            allocator_addr_ = nullptr;
            allocator_type_ = AllocatorType::UNDEFINED;
            space_type_ = SpaceType::SPACE_TYPE_UNDEFINED;
        }

        bool IsFirstByteInSegment() const
        {
            return first_byte_in_segment_ == FIRST_BYTE_IN_SEGMENT_VALUE;
        }

        AllocatorType GetAllocatorType() const
        {
            return allocator_type_;
        }

        const void *GetAllocatorAddr() const
        {
            return allocator_addr_;
        }

        SpaceType GetSpaceType() const
        {
            return space_type_;
        }

    private:
        bool first_byte_in_segment_ {FIRST_BYTE_IN_SEGMENT_VALUE};
        AllocatorType allocator_type_ {AllocatorType::UNDEFINED};
        SpaceType space_type_ {SpaceType::SPACE_TYPE_UNDEFINED};
        const void *allocator_addr_ = nullptr;
    };

    static size_t AddrToMapNum(const void *addr)
    {
        size_t map_num = ToUintPtr(addr) / POOL_MAP_GRANULARITY;
        ASSERT(map_num < POOL_MAP_SIZE);
        return map_num;
    }

    static void *MapNumToAddr(size_t map_num)
    {
        // Checking overflow
        ASSERT(static_cast<uint64_t>(map_num) * POOL_MAP_GRANULARITY == map_num * POOL_MAP_GRANULARITY);
        return ToVoidPtr(map_num * POOL_MAP_GRANULARITY);
    }

    void *GetFirstByteInSegment(const void *addr);

    // Only for debug
    bool IsEmpty() const
    {
        for (auto i : pool_map_) {
            if (i.GetSpaceType() != SpaceType::SPACE_TYPE_UNDEFINED) {
                return false;
            }
        }
        return true;
    }

    std::array<PoolInfo, POOL_MAP_SIZE> pool_map_;

    friend class PoolMapTest;
};

}  // namespace panda

#endif  // PANDA_LIBPANDABASE_MEM_POOL_MAP_H_
