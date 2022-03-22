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

#include "code_allocator.h"
#include "mem/base_mem_stats.h"
#include "os/mem.h"
#include "trace/trace.h"

#include <securec.h>
#include <cstring>

namespace panda {

const Alignment CodeAllocator::PAGE_LOG_ALIGN = GetLogAlignment(os::mem::GetPageSize());

CodeAllocator::CodeAllocator(BaseMemStats *mem_stats)
    : arenaAllocator_([&]() {
          trace::ScopedTrace scoped_trace(__PRETTY_FUNCTION__);
          // Do not set up mem_stats in internal arena allocator, because we will manage memstats here.
          return ArenaAllocator(SpaceType::SPACE_TYPE_CODE, nullptr);
      }()),
      memStats_(mem_stats),
      codeRangeStart_(nullptr),
      codeRangeEnd_(nullptr)
{
    ASSERT(PAGE_LOG_ALIGN >= LOG_ALIGN_MIN);
    ASSERT(PAGE_LOG_ALIGN <= LOG_ALIGN_MAX);
}

CodeAllocator::~CodeAllocator()
{
    codeRangeStart_ = nullptr;
    codeRangeEnd_ = nullptr;
}

void *CodeAllocator::AllocateCode(size_t size, const void *code_buff)
{
    trace::ScopedTrace scoped_trace("Allocate Code");
    void *code_ptr = arenaAllocator_.Alloc(size, PAGE_LOG_ALIGN);
    if (UNLIKELY(code_ptr == nullptr || memcpy_s(code_ptr, size, code_buff, size) != EOK)) {
        return nullptr;
    }
    ProtectCode(os::mem::MapRange<std::byte>(static_cast<std::byte *>(code_ptr), size));
    memStats_->RecordAllocateRaw(size, SpaceType::SPACE_TYPE_CODE);
    CodeRangeUpdate(code_ptr, size);
    return code_ptr;
}

os::mem::MapRange<std::byte> CodeAllocator::AllocateCodeUnprotected(size_t size)
{
    trace::ScopedTrace scoped_trace("Allocate Code");
    void *code_ptr = arenaAllocator_.Alloc(size, PAGE_LOG_ALIGN);
    if (UNLIKELY(code_ptr == nullptr)) {
        return os::mem::MapRange<std::byte>(nullptr, 0);
    }
    memStats_->RecordAllocateRaw(size, SpaceType::SPACE_TYPE_CODE);
    CodeRangeUpdate(code_ptr, size);
    return os::mem::MapRange<std::byte>(static_cast<std::byte *>(code_ptr), size);
}

/* static */
void CodeAllocator::ProtectCode(os::mem::MapRange<std::byte> mem_range)
{
    mem_range.MakeReadExec();
}

bool CodeAllocator::InAllocatedCodeRange(const void *pc)
{
    os::memory::ReadLockHolder rlock(code_range_lock_);
    return (pc >= codeRangeStart_) && (pc <= codeRangeEnd_);
}

void CodeAllocator::CodeRangeUpdate(void *ptr, size_t size)
{
    os::memory::WriteLockHolder rwlock(code_range_lock_);
    if (ptr < codeRangeStart_ || codeRangeStart_ == nullptr) {
        codeRangeStart_ = ptr;
    }
    void *buffer_end = ToVoidPtr(ToUintPtr(ptr) + size);
    if (buffer_end > codeRangeEnd_ || codeRangeEnd_ == nullptr) {
        codeRangeEnd_ = buffer_end;
    }
}

}  // namespace panda
