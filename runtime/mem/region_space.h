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

#ifndef PANDA_RUNTIME_MEM_REGION_SPACE_H_
#define PANDA_RUNTIME_MEM_REGION_SPACE_H_

#include <atomic>
#include <cstdint>

#include "libpandabase/utils/list.h"
#include "runtime/include/mem/panda_containers.h"
#include "runtime/mem/object_helpers.h"
#include "runtime/mem/tlab.h"
#include "runtime/mem/rem_set.h"

namespace panda::mem {

enum RegionFlag {
    IS_EDEN = 1U,
    IS_SURVIVOR = 1U << 1U,
    IS_OLD = 1U << 2U,
    IS_LARGE_OBJECT = 1U << 3U,
    IS_NONMOVABLE = 1U << 4U,
};

static constexpr size_t DEFAULT_REGION_ALIGNMENT = 256_KB;
static constexpr size_t DEFAULT_REGION_SIZE = DEFAULT_REGION_ALIGNMENT;
static constexpr size_t DEFAULT_REGION_MASK = DEFAULT_REGION_ALIGNMENT - 1;

using RemSetT = RemSet<>;

class RegionSpace;
class Region {
public:
    NO_THREAD_SANITIZE explicit Region(RegionSpace *space, uintptr_t begin, uintptr_t end)
        : space_(space),
          begin_(begin),
          end_(end),
          top_(begin),
          flags_(0),
          live_bytes_(0),
          live_bitmap_(nullptr),
          mark_bitmap_(nullptr),
          rem_set_(nullptr),
          tlab_(nullptr)
    {
    }

    ~Region() = default;

    NO_COPY_SEMANTIC(Region);
    NO_MOVE_SEMANTIC(Region);

    void Destroy();

    RegionSpace *GetSpace()
    {
        return space_;
    }

    uintptr_t Begin()
    {
        return begin_;
    }

    uintptr_t End()
    {
        return end_;
    }

    uintptr_t Top()
    {
        return top_;
    }

    void SetTop(uintptr_t new_top)
    {
        top_ = new_top;
    }

    uint32_t GetLiveBytes() const
    {
        return live_bytes_;
    }

    uint32_t GetGarbageBytes() const
    {
        return (top_ - begin_) - GetLiveBytes();
    }

    void SetLiveBytes(uint32_t count)
    {
        live_bytes_ = count;
    }

    uint32_t CalcLiveBytes() const;

    MarkBitmap *GetLiveBitmap() const
    {
        return live_bitmap_;
    }

    MarkBitmap *GetMarkBitmap() const
    {
        return mark_bitmap_;
    }

    RemSetT *GetRemSet()
    {
        return rem_set_;
    }

    void AddFlag(RegionFlag flag)
    {
        flags_ |= flag;
    }

    void RmvFlag(RegionFlag flag)
    {
        // NOLINTNEXTLINE(hicpp-signed-bitwise)
        flags_ &= ~flag;
    }

    bool HasFlag(RegionFlag flag) const
    {
        return (flags_ & flag) != 0;
    }

    bool IsEden() const
    {
        return HasFlag(IS_EDEN);
    }

    void SetTLAB(TLAB *tlab)
    {
        tlab_ = tlab;
    }

    TLAB *GetTLAB() const
    {
        return tlab_;
    }

    size_t Size()
    {
        return end_ - ToUintPtr(this);
    }

    template <bool atomic = true>
    NO_THREAD_SANITIZE void *Alloc(size_t size, Alignment align = DEFAULT_ALIGNMENT);

    template <typename ObjectVisitor>
    void IterateOverObjects(const ObjectVisitor &visitor);

    bool IsInRange(const ObjectHeader *object) const
    {
        return ToUintPtr(object) >= begin_ && ToUintPtr(object) < end_;
    }

    bool IsInAllocRange(const ObjectHeader *object) const
    {
        return (ToUintPtr(object) >= begin_ && ToUintPtr(object) < top_) ||
               (tlab_ != nullptr && tlab_->ContainObject(object));
    }

    static bool IsAlignment(uintptr_t region_addr, size_t region_size)
    {
        return ((region_addr - HeapStartAddress()) % region_size) == 0;
    }

    constexpr static size_t HeadSize()
    {
        return AlignUp(sizeof(Region), DEFAULT_ALIGNMENT_IN_BYTES);
    }

    constexpr static size_t RegionSize(size_t object_size, size_t region_size)
    {
        return AlignUp(HeadSize() + object_size, region_size);
    }

    template <bool cross_region = false>
    static Region *AddrToRegion(const void *addr, size_t mask = DEFAULT_REGION_MASK)
    {
        // if it is possible that (object address - region start addr) larger than region alignment,
        // we should get the region start address from mmappool which records it in allocator info
        if constexpr (cross_region) {  // NOLINTNEXTLINE(readability-braces-around-statements)
            auto region_addr = PoolManager::GetMmapMemPool()->GetStartAddrPoolForAddr(const_cast<void *>(addr));
            return reinterpret_cast<Region *>(region_addr);
        }

        // else get region address in quick way
        uintptr_t start_addr = HeapStartAddress();
        return reinterpret_cast<Region *>(((ToUintPtr(addr) - start_addr) & ~mask) + start_addr);
    }

    static uintptr_t HeapStartAddress()
    {
        // see MmapMemPool about the object space start address
#if defined(PANDA_USE_32_BIT_POINTER) && !defined(PANDA_TARGET_WINDOWS)
        return PANDA_32BITS_HEAP_START_ADDRESS;
#else
        return PoolManager::GetMmapMemPool()->GetMinObjectAddress();
#endif
    }

    InternalAllocatorPtr GetInternalAllocator();

    void CreateRemSet();

    MarkBitmap *CreateMarkBitmap();

    void SwapMarkBitmap()
    {
        std::swap(live_bitmap_, mark_bitmap_);
    }

    void SetMarkBit(ObjectHeader *object);

#ifndef NDEBUG
    NO_THREAD_SANITIZE bool IsAllocating()
    {
        return reinterpret_cast<std::atomic<bool> *>(&is_allocating_)->load(std::memory_order_relaxed);
    }

    NO_THREAD_SANITIZE bool IsIterating()
    {
        return reinterpret_cast<std::atomic<bool> *>(&is_iterating_)->load(std::memory_order_relaxed);
    }

    NO_THREAD_SANITIZE bool SetAllocating(bool value)
    {
        if (IsIterating()) {
            return false;
        }
        reinterpret_cast<std::atomic<bool> *>(&is_allocating_)->store(value, std::memory_order_relaxed);
        return true;
    }

    NO_THREAD_SANITIZE bool SetIterating(bool value)
    {
        if (IsAllocating()) {
            return false;
        }
        reinterpret_cast<std::atomic<bool> *>(&is_iterating_)->store(value, std::memory_order_relaxed);
        return true;
    }
#endif

    DListNode *AsListNode()
    {
        return &node_;
    }

    static Region *AsRegion(DListNode *node)
    {
        return reinterpret_cast<Region *>(ToUintPtr(node) - MEMBER_OFFSET(Region, node_));
    }

private:
    DListNode node_;
    RegionSpace *space_;
    uintptr_t begin_;
    uintptr_t end_;
    uintptr_t top_;
    uint32_t flags_;
    uint32_t live_bytes_;
    MarkBitmap *live_bitmap_;  // records live objects for old region
    MarkBitmap *mark_bitmap_;  // mark bitmap used in current gc marking phase
    RemSetT *rem_set_;         // remember set(old region -> eden/survivor region)
    TLAB *tlab_;               // pointer to thread tlab
#ifndef NDEBUG
    bool is_allocating_ = false;
    bool is_iterating_ = false;
#endif
};

// RegionBlock is used for allocate regions from a continuous big memory block
// |--------------------------|
// |.....RegionBlock class....|
// |--------------------------|
// |.......regions_end_.......|--------|
// |.......regions_begin_.....|----|   |
// |--------------------------|    |   |
//                                 |   |
// |   Continuous Mem Block   |    |   |
// |--------------------------|    |   |
// |...........Region.........|<---|   |
// |...........Region.........|        |
// |...........Region.........|        |
// |..........................|        |
// |..........................|        |
// |..........................|        |
// |..........................|        |
// |..........................|        |
// |..........................|        |
// |..........................|        |
// |...........Region.........|<-------|
class RegionBlock {
public:
    RegionBlock(size_t region_size, InternalAllocatorPtr allocator) : region_size_(region_size), allocator_(allocator)
    {
    }

    ~RegionBlock()
    {
        if (!occupied_.Empty()) {
            allocator_->Free(occupied_.Data());
        }
    }

    NO_COPY_SEMANTIC(RegionBlock);
    NO_MOVE_SEMANTIC(RegionBlock);

    void Init(uintptr_t regions_begin, uintptr_t regions_end);

    Region *AllocRegion();

    Region *AllocLargeRegion(size_t large_region_size);

    void FreeRegion(Region *region, bool release_pages = true);

    bool IsAddrInRange(const void *addr) const
    {
        return ToUintPtr(addr) < regions_end_ && ToUintPtr(addr) >= regions_begin_;
    }

    Region *GetAllocatedRegion(const void *addr) const
    {
        ASSERT(IsAddrInRange(addr));
        os::memory::LockHolder lock(lock_);
        return occupied_[RegionIndex(addr)];
    }

    size_t GetFreeRegionsNum() const
    {
        os::memory::LockHolder lock(lock_);
        return occupied_.Size() - num_used_regions_;
    }

private:
    Region *RegionAt(size_t index) const
    {
        return reinterpret_cast<Region *>(regions_begin_ + index * region_size_);
    }

    size_t RegionIndex(const void *addr) const
    {
        return (ToUintPtr(addr) - regions_begin_) / region_size_;
    }

    size_t region_size_;
    InternalAllocatorPtr allocator_;
    uintptr_t regions_begin_ = 0;
    uintptr_t regions_end_ = 0;
    size_t num_used_regions_ = 0;
    Span<Region *> occupied_ GUARDED_BY(lock_);
    mutable os::memory::Mutex lock_;
};

// RegionPool supports to work in three ways:
// 1.alloc region in pre-allocated buffer(RegionBlock)
// 2.alloc region in mmap pool directly
// 3.mixed above two ways
class RegionPool {
public:
    explicit RegionPool(size_t region_size, bool extend, InternalAllocatorPtr allocator)
        : block_(region_size, allocator), region_size_(region_size), allocator_(allocator), extend_(extend)
    {
    }

    Region *NewRegion(RegionSpace *space, SpaceType space_type, AllocatorType allocator_type, size_t region_size);

    void FreeRegion(Region *region, bool release_pages = true);

    void InitRegionBlock(uintptr_t regions_begin, uintptr_t regions_end)
    {
        block_.Init(regions_begin, regions_end);
    }

    bool IsAddrInPoolRange(const void *addr) const
    {
        return block_.IsAddrInRange(addr) || IsAddrInExtendPoolRange(addr);
    }

    template <bool cross_region = false>
    Region *GetRegion(const void *addr) const
    {
        if (block_.IsAddrInRange(addr)) {
            return block_.GetAllocatedRegion(addr);
        }
        if (IsAddrInExtendPoolRange(addr)) {
            return Region::AddrToRegion<cross_region>(addr, region_size_ - 1);
        }
        return nullptr;
    }

    size_t GetFreeRegionsNumInRegionBlock() const
    {
        return block_.GetFreeRegionsNum();
    }

    InternalAllocatorPtr GetInternalAllocator()
    {
        return allocator_;
    }

    ~RegionPool() = default;
    NO_COPY_SEMANTIC(RegionPool);
    NO_MOVE_SEMANTIC(RegionPool);

private:
    bool IsAddrInExtendPoolRange(const void *addr) const
    {
        if (extend_) {
            AllocatorInfo alloc_info = PoolManager::GetMmapMemPool()->GetAllocatorInfoForAddr(const_cast<void *>(addr));
            return alloc_info.GetAllocatorHeaderAddr() == this;
        }
        return false;
    }

    RegionBlock block_;
    size_t region_size_;
    InternalAllocatorPtr allocator_;
    bool extend_ = true;
};

class RegionSpace {
public:
    explicit RegionSpace(SpaceType space_type, AllocatorType allocator_type, RegionPool *region_pool)
        : space_type_(space_type), allocator_type_(allocator_type), region_pool_(region_pool)
    {
    }

    virtual ~RegionSpace()
    {
        FreeAllRegions();
    }

    NO_COPY_SEMANTIC(RegionSpace);
    NO_MOVE_SEMANTIC(RegionSpace);

    Region *NewRegion(size_t region_size);

    void FreeRegion(Region *region);

    void FreeAllRegions();

    template <typename RegionVisitor>
    void IterateRegions(RegionVisitor visitor);

    RegionPool *GetPool() const
    {
        return region_pool_;
    }

    Region *GetRegion(const ObjectHeader *object) const
    {
        auto *region = region_pool_->GetRegion(object);

        // check if the region is allocated by this space
        return (region != nullptr && region->GetSpace() == this) ? region : nullptr;
    }

    bool ContainObject(const ObjectHeader *object) const
    {
        return GetRegion(object) != nullptr;
    }

    bool IsLive(const ObjectHeader *object) const
    {
        auto *region = GetRegion(object);

        // check if the object is live in the range
        return region != nullptr && region->IsInAllocRange(object);
    }

private:
    void DestroyRegion(Region *region)
    {
        region->Destroy();
        region_pool_->FreeRegion(region);
    }

    SpaceType space_type_;

    // related allocator type
    AllocatorType allocator_type_;

    // underlying shared region pool
    RegionPool *region_pool_;

    // region allocated by this space
    DList regions_;
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_REGION_SPACE_H_
