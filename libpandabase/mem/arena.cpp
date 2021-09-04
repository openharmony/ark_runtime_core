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

#include "arena.h"

#include <memory>

#include "macros.h"
#include "utils/logger.h"
#include "utils/asan_interface.h"

namespace panda {

Arena::Arena(size_t buff_size, void *buff) : Arena(buff_size, buff, ARENA_DEFAULT_ALIGNMENT) {}

Arena::Arena(size_t buff_size, void *buff, Alignment start_alignment)
    : buff_(buff),
      startPos_(ToVoidPtr(AlignUp(ToUintPtr(buff), GetAlignmentInBytes(start_alignment)))),
      curPos_(startPos_),
      size_(buff_size)
{
    ASSERT(ToUintPtr(buff) == AlignUp(ToUintPtr(buff), GetAlignmentInBytes(ARENA_DEFAULT_ALIGNMENT)));
    ASAN_POISON_MEMORY_REGION(buff_, size_);
    LOG(INFO, ALLOC) << "Arena: created with buff addr = " << buff << " size = " << buff_size;
}

Arena::~Arena()
{
    ASAN_UNPOISON_MEMORY_REGION(buff_, size_);
    LOG(INFO, ALLOC) << "Destroy Arena buff addr = " << buff_ << " size = " << size_;
}

void *Arena::Alloc(size_t size, Alignment alignment)
{
    size_t free_size = GetFreeSize();
    void *ret = std::align(GetAlignmentInBytes(alignment), size, curPos_, free_size);
    if (ret != nullptr) {
        ASAN_UNPOISON_MEMORY_REGION(ret, size);
        curPos_ = ToVoidPtr(ToUintPtr(ret) + size);
    }
    LOG(DEBUG, ALLOC) << "Arena::Alloc size = " << size << " alignment = " << alignment << " at addr = " << ret;
    return ret;
}

void *Arena::AlignedAlloc(size_t size, [[maybe_unused]] Alignment alignment)
{
    ASSERT(AlignUp(ToUintPtr(curPos_), GetAlignmentInBytes(alignment)) == ToUintPtr(curPos_));
    ASSERT(AlignUp(size, GetAlignmentInBytes(alignment)) == size);
    void *ret = nullptr;
    uintptr_t new_cur_pos = ToUintPtr(curPos_) + size;
    if (new_cur_pos <= (ToUintPtr(buff_) + size_)) {
        ret = curPos_;
        curPos_ = ToVoidPtr(new_cur_pos);
        ASAN_UNPOISON_MEMORY_REGION(ret, size);
    }
    return ret;
}

void Arena::LinkTo(Arena *arena)
{
    LOG(DEBUG, ALLOC) << "Link arena " << this << " to " << arena;
    ASSERT(next_ == nullptr);
    next_ = arena;
}

void Arena::ClearNextLink()
{
    next_ = nullptr;
}

Arena *Arena::GetNextArena() const
{
    return next_;
}

size_t Arena::GetFreeSize() const
{
    ASSERT(ToUintPtr(curPos_) >= ToUintPtr(GetStartPos()));
    return size_ - (ToUintPtr(curPos_) - ToUintPtr(GetStartPos()));
}

size_t Arena::GetOccupiedSize() const
{
    ASSERT(ToUintPtr(curPos_) >= ToUintPtr(GetStartPos()));
    return ToUintPtr(curPos_) - ToUintPtr(GetStartPos());
}

void *Arena::GetArenaEnd() const
{
    return ToVoidPtr(size_ + ToUintPtr(buff_));
}

void *Arena::GetAllocatedEnd() const
{
    return ToVoidPtr(ToUintPtr(GetStartPos()) + GetOccupiedSize());
}

void *Arena::GetAllocatedStart() const
{
    return GetStartPos();
}

bool Arena::InArena(const void *mem) const
{
    return (ToUintPtr(curPos_) > ToUintPtr(mem)) && (ToUintPtr(GetStartPos()) <= ToUintPtr(mem));
}

void Arena::Free(void *mem)
{
    ASSERT(InArena(mem));
    ASAN_POISON_MEMORY_REGION(mem, ToUintPtr(curPos_) - ToUintPtr(mem));
    curPos_ = mem;
}

void Arena::Resize(size_t new_size)
{
    size_t old_size = GetOccupiedSize();
    ASSERT(new_size <= old_size);
    curPos_ = ToVoidPtr(ToUintPtr(GetStartPos()) + new_size);
    ASAN_POISON_MEMORY_REGION(curPos_, old_size - new_size);
}

void Arena::Reset()
{
    Resize(0);
}

void Arena::ExpandArena(const void *extra_buff, size_t size)
{
    ASSERT(ToUintPtr(extra_buff) == AlignUp(ToUintPtr(extra_buff), DEFAULT_ALIGNMENT_IN_BYTES));
    ASSERT(ToUintPtr(extra_buff) == ToUintPtr(GetArenaEnd()));
    ASAN_POISON_MEMORY_REGION(extra_buff, size);
    LOG(DEBUG, ALLOC) << "Expand arena: Add " << size << " bytes to the arena " << this;
    size_ += size;
}

}  // namespace panda
