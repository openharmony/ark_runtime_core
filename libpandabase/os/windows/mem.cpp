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

#include "os/mem.h"
#include "windows_mem.h"
#include "utils/type_helpers.h"
#include "utils/asan_interface.h"

#include <limits>

#include <windows.h>
#include <errno.h>
#include <io.h>

#include <type_traits>

#define MAP_FAILED (reinterpret_cast<void *>(-1))

namespace panda::os::mem {

static int mem_errno(const DWORD err, const int deferr)
{
    return err == 0 ? deferr : err;
}

static DWORD mem_protection_flags_for_page(const int prot)
{
    DWORD flags = 0;
    if (static_cast<unsigned>(prot) == MMAP_PROT_NONE) {
        return flags;
    }

    if ((static_cast<unsigned>(prot) & MMAP_PROT_EXEC) != 0) {
        flags = ((static_cast<unsigned>(prot) & MMAP_PROT_WRITE) != 0) ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ;
    } else {
        flags = ((static_cast<unsigned>(prot) & MMAP_PROT_WRITE) != 0) ? PAGE_READWRITE : PAGE_READONLY;
    }

    return flags;
}

static DWORD mem_protection_flags_for_file(const int prot)
{
    DWORD flags = 0;

    if (prot == MMAP_PROT_NONE) {
        return flags;
    }

    if ((static_cast<unsigned>(prot) & MMAP_PROT_READ) != 0) {
        flags |= FILE_MAP_READ;
    }
    if ((static_cast<unsigned>(prot) & MMAP_PROT_WRITE) != 0) {
        flags |= FILE_MAP_WRITE;
    }
    if ((static_cast<unsigned>(prot) & MMAP_PROT_EXEC) != 0) {
        flags |= FILE_MAP_EXECUTE;
    }

    return flags;
}

static DWORD mem_select_lower_bound(off_t off)
{
    using uoff_t = std::make_unsigned_t<off_t>;
    return (sizeof(off_t) <= sizeof(DWORD)) ? static_cast<DWORD>(off)
                                            : static_cast<DWORD>(static_cast<uoff_t>(off) & 0xFFFFFFFFL);
}

static DWORD mem_select_upper_bound(off_t off)
{
    constexpr uint32_t OFFSET_DWORD = 32;
    using uoff_t = std::make_unsigned_t<off_t>;
    return (sizeof(off_t) <= sizeof(DWORD))
               ? static_cast<DWORD>(0)
               : static_cast<DWORD>((static_cast<uoff_t>(off) >> OFFSET_DWORD) & 0xFFFFFFFFL);
}

void *mmap([[maybe_unused]] void *addr, size_t len, int prot, uint32_t flags, int fildes, off_t off)
{
    errno = 0;

    // Skip unsupported combinations of flags:
    if (len == 0 || (flags & MMAP_FLAG_FIXED) != 0 || prot == MMAP_PROT_EXEC) {
        errno = EINVAL;
        return MAP_FAILED;
    }

    HANDLE h =
        ((flags & MMAP_FLAG_ANONYMOUS) == 0) ? reinterpret_cast<HANDLE>(_get_osfhandle(fildes)) : INVALID_HANDLE_VALUE;
    if ((flags & MMAP_FLAG_ANONYMOUS) == 0 && h == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        return MAP_FAILED;
    }

    const auto prot_page = mem_protection_flags_for_page(prot);
    const off_t max_size = off + static_cast<off_t>(len);
    const auto max_size_low = mem_select_lower_bound(max_size);
    const auto max_size_high = mem_select_upper_bound(max_size);
    HANDLE fm = CreateFileMapping(h, nullptr, prot_page, max_size_high, max_size_low, nullptr);
    if (fm == nullptr) {
        errno = mem_errno(GetLastError(), EPERM);
        return MAP_FAILED;
    }

    const auto prot_file = mem_protection_flags_for_file(prot);
    const auto file_off_low = mem_select_lower_bound(off);
    const auto file_off_high = mem_select_upper_bound(off);
    void *map = MapViewOfFile(fm, prot_file, file_off_high, file_off_low, len);
    CloseHandle(fm);
    if (map == nullptr) {
        errno = mem_errno(GetLastError(), EPERM);
        return MAP_FAILED;
    }

    return map;
}

int munmap(void *addr, [[maybe_unused]] size_t len)
{
    if (UnmapViewOfFile(addr)) {
        return 0;
    }

    errno = mem_errno(GetLastError(), EPERM);
    return -1;
}

void MmapDeleter(std::byte *ptr, size_t size) noexcept
{
    if (ptr != nullptr) {
        munmap(ptr, size);
    }
}

BytePtr MapFile(file::File file, uint32_t prot, uint32_t flags, size_t size, size_t file_offset, void *hint)
{
    size_t map_offset = RoundDown(file_offset, GetPageSize());
    size_t offset = file_offset - map_offset;
    size_t map_size = size + offset;
    void *result = mmap(hint, map_size, prot, flags, file.GetFd(), map_offset);
    if (UNLIKELY(result == MAP_FAILED)) {
        return BytePtr(nullptr, 0, MmapDeleter);
    }

    return BytePtr(static_cast<std::byte *>(result) + offset, size, MmapDeleter);
}

uint32_t GetPageSize()
{
    constexpr size_t PAGE_SIZE = 4096;
    return PAGE_SIZE;
}

void *MapRWAnonymousRaw(size_t size, bool force_poison)
{
    ASSERT(size % GetPageSize() == 0);
    // NOLINTNEXTLINE(hicpp-signed-bitwise)
    void *result =
        mmap(nullptr, size, MMAP_PROT_READ | MMAP_PROT_WRITE, MMAP_FLAG_PRIVATE | MMAP_FLAG_ANONYMOUS, -1, 0);
    if (UNLIKELY(result == MAP_FAILED)) {
        result = nullptr;
    }
    if ((result != nullptr) && force_poison) {
        ASAN_POISON_MEMORY_REGION(result, size);
    }

    return result;
}

void *MapRWAnonymousWithAlignmentRaw(size_t size, size_t aligment_in_bytes, bool force_poison)
{
    ASSERT(aligment_in_bytes % GetPageSize() == 0);
    if (size == 0) {
        return nullptr;
    }
    void *result = MapRWAnonymousRaw(size + aligment_in_bytes, force_poison);
    if (result == nullptr) {
        return result;
    }
    auto allocated_mem = reinterpret_cast<uintptr_t>(result);
    uintptr_t aligned_mem = (allocated_mem & ~(aligment_in_bytes - 1U)) +
                            ((allocated_mem % aligment_in_bytes) != 0U ? aligment_in_bytes : 0U);
    ASSERT(aligned_mem >= allocated_mem);
    size_t unused_in_start = aligned_mem - allocated_mem;
    ASSERT(unused_in_start <= aligment_in_bytes);
    size_t unused_in_end = aligment_in_bytes - unused_in_start;
    if (unused_in_start != 0) {
        UnmapRaw(result, unused_in_start);
    }
    if (unused_in_end != 0) {
        auto end_part = reinterpret_cast<void *>(aligned_mem + size);
        UnmapRaw(end_part, unused_in_end);
    }
    return reinterpret_cast<void *>(aligned_mem);
}

void *AlignedAlloc(size_t alignment_in_bytes, size_t size)
{
    size_t aligned_size = (size + alignment_in_bytes - 1) & ~(alignment_in_bytes - 1);
    // Aligned_alloc is not supported on MingW. We need to call _aligned_malloc instead.
    auto ret = _aligned_malloc(aligned_size, alignment_in_bytes);
    // _aligned_malloc returns aligned pointer so just add assertion, no need to do runtime checks
    ASSERT(reinterpret_cast<uintptr_t>(ret) == (reinterpret_cast<uintptr_t>(ret) & ~(alignment_in_bytes - 1)));
    return ret;
}

std::optional<Error> UnmapRaw(void *mem, size_t size)
{
    ASAN_UNPOISON_MEMORY_REGION(mem, size);
    int res = munmap(mem, size);
    if (UNLIKELY(res == -1)) {
        return Error(errno);
    }

    return {};
}

std::optional<Error> TagAnonymousMemory([[maybe_unused]] const void *mem, [[maybe_unused]] size_t size,
                                        [[maybe_unused]] const char *tag)
{
    return {};
}

}  // namespace panda::os::mem
