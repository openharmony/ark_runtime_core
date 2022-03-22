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

#include "runtime/mem/tlab.h"

#include "libpandabase/utils/logger.h"
#include "runtime/mem/object_helpers.h"

namespace panda::mem {

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_TLAB_ALLOCATOR(level) LOG(level, ALLOC) << "TLAB: "

TLAB::TLAB(void *address, size_t size)
{
    prev_tlab_ = nullptr;
    next_tlab_ = nullptr;
    Fill(address, size);
    LOG_TLAB_ALLOCATOR(DEBUG) << "Construct a new TLAB at addr " << std::hex << address << " with size " << std::dec
                              << size;
}

void TLAB::Fill(void *address, size_t size)
{
    ASSERT(ToUintPtr(address) == AlignUp(ToUintPtr(address), DEFAULT_ALIGNMENT_IN_BYTES));
    memory_start_addr_ = address;
    memory_end_addr_ = ToVoidPtr(ToUintPtr(address) + size);
    cur_free_position_ = address;
    ASAN_POISON_MEMORY_REGION(memory_start_addr_, GetSize());
    LOG_TLAB_ALLOCATOR(DEBUG) << "Fill a TLAB with buffer at addr " << std::hex << address << " with size " << std::dec
                              << size;
}

TLAB::~TLAB()
{
    LOG_TLAB_ALLOCATOR(DEBUG) << "Destroy a TLAB at addr " << std::hex << memory_start_addr_ << " with size "
                              << std::dec << GetSize();
}

void TLAB::Destroy()
{
    LOG_TLAB_ALLOCATOR(DEBUG) << "Destroy the TLAB at addr " << std::hex << this;
    ASAN_UNPOISON_MEMORY_REGION(memory_start_addr_, GetSize());
}

void *TLAB::Alloc(size_t size)
{
    void *ret = nullptr;
    size_t free_size = GetFreeSize();
    size_t requested_size = GetAlignedObjectSize(size);
    if (requested_size <= free_size) {
        ASSERT(ToUintPtr(cur_free_position_) == AlignUp(ToUintPtr(cur_free_position_), DEFAULT_ALIGNMENT_IN_BYTES));
        ret = cur_free_position_;
        ASAN_UNPOISON_MEMORY_REGION(ret, size);
        cur_free_position_ = ToVoidPtr(ToUintPtr(cur_free_position_) + requested_size);
    }
    LOG_TLAB_ALLOCATOR(DEBUG) << "Alloc size = " << size << " at addr = " << ret;
    return ret;
}

void TLAB::IterateOverObjects(const std::function<void(ObjectHeader *object_header)> &object_visitor)
{
    LOG_TLAB_ALLOCATOR(DEBUG) << __func__ << " started";
    auto *cur_ptr = memory_start_addr_;
    void *end_ptr = cur_free_position_;
    while (cur_ptr < end_ptr) {
        auto object_header = static_cast<ObjectHeader *>(cur_ptr);
        size_t object_size = GetObjectSize(cur_ptr);
        object_visitor(object_header);
        cur_ptr = ToVoidPtr(AlignUp(ToUintPtr(cur_ptr) + object_size, DEFAULT_ALIGNMENT_IN_BYTES));
    }
    LOG_TLAB_ALLOCATOR(DEBUG) << __func__ << " finished";
}

void TLAB::IterateOverObjectsInRange(const std::function<void(ObjectHeader *object_header)> &mem_visitor,
                                     const MemRange &mem_range)
{
    LOG_TLAB_ALLOCATOR(DEBUG) << __func__ << " started";
    if (!GetMemRangeForOccupiedMemory().IsIntersect(mem_range)) {
        return;
    }
    void *current_ptr = memory_start_addr_;
    void *end_ptr = ToVoidPtr(std::min(ToUintPtr(cur_free_position_), mem_range.GetEndAddress() + 1));
    void *start_iterate_pos = ToVoidPtr(std::max(ToUintPtr(current_ptr), mem_range.GetStartAddress()));
    while (current_ptr < start_iterate_pos) {
        size_t object_size = GetObjectSize(static_cast<ObjectHeader *>(current_ptr));
        current_ptr = ToVoidPtr(AlignUp(ToUintPtr(current_ptr) + object_size, DEFAULT_ALIGNMENT_IN_BYTES));
    }
    while (current_ptr < end_ptr) {
        auto object_header = static_cast<ObjectHeader *>(current_ptr);
        size_t object_size = GetObjectSize(current_ptr);
        mem_visitor(object_header);
        current_ptr = ToVoidPtr(AlignUp(ToUintPtr(current_ptr) + object_size, DEFAULT_ALIGNMENT_IN_BYTES));
    }
    LOG_TLAB_ALLOCATOR(DEBUG) << __func__ << " finished";
}

bool TLAB::ContainObject(const ObjectHeader *obj)
{
    return (ToUintPtr(cur_free_position_) > ToUintPtr(obj)) && (ToUintPtr(memory_start_addr_) <= ToUintPtr(obj));
}

bool TLAB::IsLive(const ObjectHeader *obj)
{
    ASSERT(ContainObject(obj));
    return ContainObject(obj);
}

#undef LOG_TLAB_ALLOCATOR

}  // namespace panda::mem
