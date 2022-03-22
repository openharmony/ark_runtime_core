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

#ifndef PANDA_RUNTIME_MEM_FREELIST_H_
#define PANDA_RUNTIME_MEM_FREELIST_H_

#include <cstddef>
#include <cstdint>
#include <limits>

#include "libpandabase/mem/mem.h"
#include "libpandabase/utils/asan_interface.h"

namespace panda::mem::freelist {

class MemoryBlockHeader {
public:
    ATTRIBUTE_NO_SANITIZE_ADDRESS
    void Initialize(size_t size, MemoryBlockHeader *prev_header)
    {
        ASSERT((std::numeric_limits<size_t>::max() >> STATUS_BITS_SIZE) >= size);
        ASAN_UNPOISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
        prev_header_ = prev_header;
        size_ = size << STATUS_BITS_SIZE;
        ASAN_POISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
    }

    // Is this memory block used somewhere or not (i.e. it is free)
    ATTRIBUTE_NO_SANITIZE_ADDRESS
    bool IsUsed()
    {
        ASAN_UNPOISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
        bool used = !((size_ & USED_BIT_MASK_IN_PLACE) == 0x0);
        ASAN_POISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
        return used;
    }

    ATTRIBUTE_NO_SANITIZE_ADDRESS
    void SetUsed()
    {
        ASSERT(!IsUsed());
        ASAN_UNPOISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
        size_ = size_ | USED_BIT_MASK_IN_PLACE;
        ASAN_POISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
    }

    ATTRIBUTE_NO_SANITIZE_ADDRESS
    void SetUnused()
    {
        ASSERT(IsUsed());
        ASAN_UNPOISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
        size_ = size_ & (~USED_BIT_MASK_IN_PLACE);
        ASAN_POISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
    }

    // Is this memory block the last in the memory pool
    // (i.e. we can't compute next memory block via size)
    ATTRIBUTE_NO_SANITIZE_ADDRESS
    bool IsLastBlockInPool()
    {
        ASSERT(!IsPaddingHeader());
        ASAN_UNPOISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
        bool is_last_block_in_pool = !((size_ & LAST_BLOCK_IN_POOL_BIT_MASK_IN_PLACE) == 0x0);
        ASAN_POISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
        return is_last_block_in_pool;
    }

    ATTRIBUTE_NO_SANITIZE_ADDRESS
    void SetLastBlockInPool()
    {
        ASSERT(!IsLastBlockInPool());
        ASAN_UNPOISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
        size_ = size_ | LAST_BLOCK_IN_POOL_BIT_MASK_IN_PLACE;
        ASAN_POISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
    }

    // Is this memory block has some padding for alignment.
    // If yes, it is some hidden header and we have some extra header where all correct information has been stored.
    ATTRIBUTE_NO_SANITIZE_ADDRESS
    bool IsPaddingHeader()
    {
        ASAN_UNPOISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
        bool is_padding_header = GetPaddingStatus(size_) == PADDING_STATUS_PADDING_HEADER;
        ASAN_POISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
        return is_padding_header;
    }

    ATTRIBUTE_NO_SANITIZE_ADDRESS
    void SetAsPaddingHeader()
    {
        ASSERT(!IsPaddingSizeStoredAfterHeader());
        ASSERT(!IsPaddingHeaderStoredAfterHeader());
        ASSERT(!IsPaddingHeader());
        ASAN_UNPOISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
        size_ = SetPaddingStatus(size_, PADDING_STATUS_PADDING_HEADER);
        ASAN_POISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
    }

    // Is this memory block has some padding for alignment and we can get correct object memory address by using
    // padding size, which is stored after this header.
    ATTRIBUTE_NO_SANITIZE_ADDRESS
    bool IsPaddingSizeStoredAfterHeader()
    {
        ASAN_UNPOISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
        bool result = GetPaddingStatus(size_) == PADDING_STATUS_COMMON_HEADER_WITH_PADDING_SIZE;
        ASAN_POISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
        return result;
    }

    ATTRIBUTE_NO_SANITIZE_ADDRESS
    void SetPaddingSizeStoredAfterHeader()
    {
        ASSERT(!IsPaddingSizeStoredAfterHeader());
        ASSERT(!IsPaddingHeaderStoredAfterHeader());
        ASSERT(!IsPaddingHeader());
        ASAN_UNPOISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
        size_ = SetPaddingStatus(size_, PADDING_STATUS_COMMON_HEADER_WITH_PADDING_SIZE);
        ASAN_POISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
    }

    ATTRIBUTE_NO_SANITIZE_ADDRESS
    void SetPaddingSize(size_t size)
    {
        ASSERT(IsPaddingSizeStoredAfterHeader());
        ASAN_UNPOISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
        ASAN_UNPOISON_MEMORY_REGION(GetRawMemory(), sizeof(size_t));
        auto size_pointer = static_cast<size_t *>(GetRawMemory());
        *size_pointer = size;
        ASAN_UNPOISON_MEMORY_REGION(GetRawMemory(), sizeof(size_t));
        ASAN_POISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
    }

    ATTRIBUTE_NO_SANITIZE_ADDRESS
    size_t GetPaddingSize()
    {
        ASSERT(IsPaddingSizeStoredAfterHeader());
        ASAN_UNPOISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
        ASAN_UNPOISON_MEMORY_REGION(GetRawMemory(), sizeof(size_t));
        auto size_pointer = static_cast<size_t *>(GetRawMemory());
        size_t size = *size_pointer;
        ASAN_UNPOISON_MEMORY_REGION(GetRawMemory(), sizeof(size_t));
        ASAN_POISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
        return size;
    }

    // Is this memory block has some padding for alignment and we have padding header just after this header,
    // So, to compute object memory address we need to add padding header size.
    ATTRIBUTE_NO_SANITIZE_ADDRESS
    bool IsPaddingHeaderStoredAfterHeader()
    {
        ASAN_UNPOISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
        bool result = GetPaddingStatus(size_) == PADDING_STATUS_COMMON_HEADER_WITH_PADDING_HEADER;
        ASAN_POISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
        return result;
    }

    ATTRIBUTE_NO_SANITIZE_ADDRESS
    void SetPaddingHeaderStoredAfterHeader()
    {
        ASSERT(!IsPaddingSizeStoredAfterHeader());
        ASSERT(!IsPaddingHeaderStoredAfterHeader());
        ASSERT(!IsPaddingHeader());
        ASAN_UNPOISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
        size_ = SetPaddingStatus(size_, PADDING_STATUS_COMMON_HEADER_WITH_PADDING_HEADER);
        ASAN_POISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
    }

    ATTRIBUTE_NO_SANITIZE_ADDRESS
    size_t GetSize()
    {
        ASSERT(!IsPaddingHeader());
        ASAN_UNPOISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
        size_t size = size_ >> STATUS_BITS_SIZE;
        ASAN_POISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
        return size;
    }

    ATTRIBUTE_NO_SANITIZE_ADDRESS
    MemoryBlockHeader *GetPrevHeader()
    {
        ASAN_UNPOISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
        MemoryBlockHeader *prev = prev_header_;
        ASAN_POISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
        return prev;
    }

    ATTRIBUTE_NO_SANITIZE_ADDRESS
    MemoryBlockHeader *GetNextHeader()
    {
        if (IsLastBlockInPool()) {
            return nullptr;
        }
        ASAN_UNPOISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
        auto next = static_cast<MemoryBlockHeader *>(ToVoidPtr(ToUintPtr(GetRawMemory()) + GetSize()));
        ASAN_POISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
        return next;
    }

    MemoryBlockHeader *GetPrevUsedHeader()
    {
        MemoryBlockHeader *prev = GetPrevHeader();
        if (prev != nullptr) {
            if (!prev->IsUsed()) {
                prev = prev->GetPrevHeader();
                if (prev != nullptr) {
                    // We can't have 2 free consistent memory blocks
                    ASSERT(prev->IsUsed());
                }
            }
        }
        return prev;
    }

    MemoryBlockHeader *GetNextUsedHeader()
    {
        MemoryBlockHeader *next = GetNextHeader();
        if (next != nullptr) {
            if (!next->IsUsed()) {
                next = next->GetNextHeader();
                if (next != nullptr) {
                    // We can't have 2 free consistent memory blocks
                    ASSERT(next->IsUsed());
                }
            }
        }
        return next;
    }

    ATTRIBUTE_NO_SANITIZE_ADDRESS
    void SetPrevHeader(MemoryBlockHeader *header)
    {
        ASAN_UNPOISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
        prev_header_ = header;
        ASAN_POISON_MEMORY_REGION(this, sizeof(MemoryBlockHeader));
    }

    bool CanBeCoalescedWithNext()
    {
        if (IsLastBlockInPool()) {
            return false;
        }
        return !GetNextHeader()->IsUsed();
    }

    bool CanBeCoalescedWithPrev()
    {
        if (GetPrevHeader() == nullptr) {
            return false;
        }
        return !GetPrevHeader()->IsUsed();
    }

    void *GetMemory()
    {
        void *mem_pointer = GetRawMemory();
        if (IsPaddingHeaderStoredAfterHeader()) {
            return ToVoidPtr(ToUintPtr(mem_pointer) + sizeof(MemoryBlockHeader));
        }
        if (IsPaddingSizeStoredAfterHeader()) {
            return ToVoidPtr(ToUintPtr(mem_pointer) + GetPaddingSize());
        }
        return mem_pointer;
    }

private:
    enum STATUS_BITS : size_t {
        USED_BIT_SIZE = 1U,
        LAST_BLOCK_IN_POOL_BIT_SIZE = 1U,
        PADDIND_STATUS_SIZE = 2U,
        STATUS_BITS_SIZE = PADDIND_STATUS_SIZE + LAST_BLOCK_IN_POOL_BIT_SIZE + USED_BIT_SIZE,

        USED_BIT_POS = 0U,
        LAST_BLOCK_IN_POOL_BIT_POS = USED_BIT_POS + USED_BIT_SIZE,
        PADDIND_STATUS_POS = LAST_BLOCK_IN_POOL_BIT_POS + LAST_BLOCK_IN_POOL_BIT_SIZE,

        USED_BIT_MASK = (1U << USED_BIT_SIZE) - 1U,
        USED_BIT_MASK_IN_PLACE = USED_BIT_MASK << USED_BIT_POS,

        LAST_BLOCK_IN_POOL_BIT_MASK = (1U << LAST_BLOCK_IN_POOL_BIT_SIZE) - 1U,
        LAST_BLOCK_IN_POOL_BIT_MASK_IN_PLACE = LAST_BLOCK_IN_POOL_BIT_MASK << LAST_BLOCK_IN_POOL_BIT_POS,

        PADDIND_STATUS_MASK = (1U << PADDIND_STATUS_SIZE) - 1U,
        PADDIND_STATUS_MASK_IN_PLACE = PADDIND_STATUS_MASK << PADDIND_STATUS_POS,

        // A common header with object stored just after the header
        PADDING_STATUS_COMMON_HEADER = 0U,
        // A special padding header, which is used to find the common header of this memory.
        // This object required special alignment, that's why we created some padding between
        // the common header of this memory and place where the object is stored.
        PADDING_STATUS_PADDING_HEADER = PADDING_STATUS_COMMON_HEADER + 1U,
        // A common header for aligned object which required some padding.
        // The padding size is stored in size_t variable just after the common header
        PADDING_STATUS_COMMON_HEADER_WITH_PADDING_SIZE = PADDING_STATUS_PADDING_HEADER + 1U,
        // A common header for aligned object which required some padding.
        // The padding header is stored just after the common header
        PADDING_STATUS_COMMON_HEADER_WITH_PADDING_HEADER = PADDING_STATUS_COMMON_HEADER_WITH_PADDING_SIZE + 1U,
    };

    static size_t GetPaddingStatus(size_t size)
    {
        return (size & PADDIND_STATUS_MASK_IN_PLACE) >> PADDIND_STATUS_POS;
    }

    static size_t SetPaddingStatus(size_t size, STATUS_BITS status)
    {
        size = size & (~PADDIND_STATUS_MASK_IN_PLACE);
        return size | (static_cast<size_t>(status) << PADDIND_STATUS_POS);
    }

    void *GetRawMemory()
    {
        return ToVoidPtr(ToUintPtr(this) + sizeof(MemoryBlockHeader));
    }

    size_t size_ {0};
    MemoryBlockHeader *prev_header_ {nullptr};
};

class FreeListHeader : public MemoryBlockHeader {
public:
    ATTRIBUTE_NO_SANITIZE_ADDRESS
    FreeListHeader *GetNextFree()
    {
        ASSERT(!IsUsed());
        ASAN_UNPOISON_MEMORY_REGION(this, sizeof(FreeListHeader));
        FreeListHeader *next_free = next_free_;
        ASAN_POISON_MEMORY_REGION(this, sizeof(FreeListHeader));
        return next_free;
    }

    ATTRIBUTE_NO_SANITIZE_ADDRESS
    FreeListHeader *GetPrevFree()
    {
        ASSERT(!IsUsed());
        ASAN_UNPOISON_MEMORY_REGION(this, sizeof(FreeListHeader));
        FreeListHeader *prev_free = prev_free_;
        ASAN_POISON_MEMORY_REGION(this, sizeof(FreeListHeader));
        return prev_free;
    }

    ATTRIBUTE_NO_SANITIZE_ADDRESS
    void SetNextFree(FreeListHeader *link)
    {
        ASSERT(!IsUsed());
        ASAN_UNPOISON_MEMORY_REGION(this, sizeof(FreeListHeader));
        next_free_ = link;
        ASAN_POISON_MEMORY_REGION(this, sizeof(FreeListHeader));
    }

    ATTRIBUTE_NO_SANITIZE_ADDRESS
    void SetPrevFree(FreeListHeader *link)
    {
        ASSERT(!IsUsed());
        ASAN_UNPOISON_MEMORY_REGION(this, sizeof(FreeListHeader));
        prev_free_ = link;
        ASAN_POISON_MEMORY_REGION(this, sizeof(FreeListHeader));
    }

    ATTRIBUTE_NO_SANITIZE_ADDRESS
    void InsertPrev(FreeListHeader *link)
    {
        ASSERT(!IsUsed());
        ASSERT(link != nullptr);
        ASSERT(!link->IsUsed());
        ASAN_UNPOISON_MEMORY_REGION(this, sizeof(FreeListHeader));
        if (prev_free_ != nullptr) {
            prev_free_->SetNextFree(link);
        }
        link->SetNextFree(this);
        link->SetPrevFree(prev_free_);
        prev_free_ = link;
        ASAN_POISON_MEMORY_REGION(this, sizeof(FreeListHeader));
    }

    ATTRIBUTE_NO_SANITIZE_ADDRESS
    void InsertNext(FreeListHeader *link)
    {
        ASSERT(!IsUsed());
        ASSERT(link != nullptr);
        ASSERT(!link->IsUsed());
        ASAN_UNPOISON_MEMORY_REGION(this, sizeof(FreeListHeader));
        if (next_free_ != nullptr) {
            next_free_->SetPrevFree(link);
        }
        link->SetNextFree(next_free_);
        link->SetPrevFree(this);
        next_free_ = link;
        ASAN_POISON_MEMORY_REGION(this, sizeof(FreeListHeader));
    }

    ATTRIBUTE_NO_SANITIZE_ADDRESS
    void PopFromFreeList()
    {
        ASSERT(!IsUsed());
        ASAN_UNPOISON_MEMORY_REGION(this, sizeof(FreeListHeader));
        if (next_free_ != nullptr) {
            next_free_->SetPrevFree(prev_free_);
        }
        if (prev_free_ != nullptr) {
            prev_free_->SetNextFree(next_free_);
        }
        ASAN_POISON_MEMORY_REGION(this, sizeof(FreeListHeader));
    }

private:
    FreeListHeader *next_free_ {nullptr};
    FreeListHeader *prev_free_ {nullptr};
};

}  // namespace panda::mem::freelist

#endif  // PANDA_RUNTIME_MEM_FREELIST_H_
