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
#include "utils/type_helpers.h"
#include "utils/asan_interface.h"
#include "utils/tsan_interface.h"

#include <limits>
#include <sys/mman.h>
#include <unistd.h>

#include <type_traits>

#if defined(__GLIBC__) || defined(PANDA_TARGET_MOBILE)
#include <malloc.h>
#endif

namespace panda::os::mem {

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
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
    if (result == MAP_FAILED) {
        return BytePtr(nullptr, 0, MmapDeleter);
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    return BytePtr(static_cast<std::byte *>(result) + offset, size, offset, MmapDeleter);
}

BytePtr MapExecuted(size_t size)
{
    // By design caller should pass valid size, so don't do any additional checks except ones that
    // mmap do itself
    // NOLINTNEXTLINE(hicpp-signed-bitwise)
    void *result = mmap(nullptr, size, PROT_EXEC | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (result == reinterpret_cast<void *>(-1)) {
        result = nullptr;
    }

    return BytePtr(static_cast<std::byte *>(result), (result == nullptr) ? 0 : size, MmapDeleter);
}

std::optional<Error> MakeMemWithProtFlag(void *mem, size_t size, int prot)
{
    int r = mprotect(mem, size, prot);
    if (r != 0) {
        return Error(errno);
    }
    return {};
}

std::optional<Error> MakeMemReadExec(void *mem, size_t size)
{
    // NOLINTNEXTLINE(hicpp-signed-bitwise)
    return MakeMemWithProtFlag(mem, size, PROT_EXEC | PROT_READ);
}

std::optional<Error> MakeMemReadWrite(void *mem, size_t size)
{
    // NOLINTNEXTLINE(hicpp-signed-bitwise)
    return MakeMemWithProtFlag(mem, size, PROT_WRITE | PROT_READ);
}

std::optional<Error> MakeMemReadOnly(void *mem, size_t size)
{
    return MakeMemWithProtFlag(mem, size, PROT_READ);
}

uintptr_t AlignDownToPageSize(uintptr_t addr)
{
    const auto SYS_PAGE_SIZE = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    addr &= ~(SYS_PAGE_SIZE - 1);
    return addr;
}

void *AlignedAlloc(size_t alignment_in_bytes, size_t size)
{
    size_t aligned_size = (size + alignment_in_bytes - 1) & ~(alignment_in_bytes - 1);
#if defined PANDA_TARGET_MOBILE || defined PANDA_TARGET_MACOS
    void *ret = nullptr;
    int r = posix_memalign(reinterpret_cast<void **>(&ret), alignment_in_bytes, aligned_size);
    if (r != 0) {
        std::cerr << "posix_memalign failed, code: " << r << std::endl;
        ASSERT(0);
    }
#else
    auto ret = aligned_alloc(alignment_in_bytes, aligned_size);
#endif
    ASSERT(reinterpret_cast<uintptr_t>(ret) == (reinterpret_cast<uintptr_t>(ret) & ~(alignment_in_bytes - 1)));
    return ret;
}

void AlignedFree(void *mem)
{
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(mem);
}

static uint32_t GetPageSizeFromOs()
{
    // NOLINTNEXTLINE(google-runtime-int)
    long sz = sysconf(_SC_PAGESIZE);
    LOG_IF(sz == -1, FATAL, RUNTIME) << "Can't get page size from OS";
    return static_cast<uint32_t>(sz);
}

uint32_t GetPageSize()
{
    // NOLINTNEXTLINE(google-runtime-int)
    static uint32_t sz = GetPageSizeFromOs();
    return sz;
}

void *MapRWAnonymousRaw(size_t size, bool force_poison)
{
    ASSERT(size % GetPageSize() == 0);
    // NOLINTNEXTLINE(hicpp-signed-bitwise)
    void *result = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (result == reinterpret_cast<void *>(-1)) {
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

void *MapRWAnonymousFixedRaw(void *mem, size_t size, bool force_poison)
{
#if defined(PANDA_ASAN_ON)
    // If this assert fails, please decrease the size of the memory for your program
    // or don't run it with ASAN.
    if (!((reinterpret_cast<uintptr_t>(mem) > MMAP_FIXED_MAGIC_ADDR_FOR_ASAN) ||
          ((reinterpret_cast<uintptr_t>(mem) + size) < MMAP_FIXED_MAGIC_ADDR_FOR_ASAN))) {
        ASSERT((reinterpret_cast<uintptr_t>(mem) > MMAP_FIXED_MAGIC_ADDR_FOR_ASAN) ||
               ((reinterpret_cast<uintptr_t>(mem) + size) < MMAP_FIXED_MAGIC_ADDR_FOR_ASAN));
        std::abort();
    }
#endif
    ASSERT(size % GetPageSize() == 0);
    void *result =  // NOLINTNEXTLINE(hicpp-signed-bitwise)
        mmap(mem, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (result == reinterpret_cast<void *>(-1)) {
        result = nullptr;
    }
    if ((result != nullptr) && force_poison) {
        // If you have such an error here:
        // ==4120==AddressSanitizer CHECK failed:
        // ../../../../src/libsanitizer/asan/asan_mapping.h:303 "((AddrIsInMem(p))) != (0)" (0x0, 0x0)
        // Look at the big comment at the start of the method.
        ASAN_POISON_MEMORY_REGION(result, size);
    }

    return result;
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

#ifdef PANDA_TARGET_MOBILE
#include <sys/prctl.h>

#ifndef PR_SET_VMA
constexpr int PR_SET_VMA = 0x53564d41;
#endif

#ifndef PR_SET_VMA_ANON_NAME
constexpr unsigned long PR_SET_VMA_ANON_NAME = 0;
#endif
#endif  // PANDA_TARGET_MOBILE

std::optional<Error> TagAnonymousMemory([[maybe_unused]] const void *mem, [[maybe_unused]] size_t size,
                                        [[maybe_unused]] const char *tag)
{
#ifdef PANDA_TARGET_MOBILE
    ASSERT(size % GetPageSize() == 0);
    ASSERT(reinterpret_cast<uintptr_t>(mem) % GetPageSize() == 0);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    int res = prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME,
                    // NOLINTNEXTLINE(google-runtime-int)
                    static_cast<unsigned long>(ToUintPtr(mem)), size,
                    // NOLINTNEXTLINE(google-runtime-int)
                    static_cast<unsigned long>(ToUintPtr(tag)));
    if (UNLIKELY(res == -1)) {
        return Error(errno);
    }
#endif  // PANDA_TARGET_MOBILE
    return {};
}

size_t GetNativeBytesFromMallinfo()
{
    size_t mallinfo_bytes;
#if defined(PANDA_ASAN_ON) || defined(PANDA_TSAN_ON)
    mallinfo_bytes = DEFAULT_NATIVE_BYTES_FROM_MALLINFO;
    LOG(INFO, RUNTIME) << "Get native bytes from mallinfo with ASAN or TSAN. Return default value";
#else
#if defined(__GLIBC__) || defined(PANDA_TARGET_MOBILE)

    // For GLIBC, uordblks is total size of space which is allocated by malloc
    // For MOBILE_LIBC, uordblks is total size of space which is allocated by malloc or mmap called by malloc for
    // non-small allocations
#if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 33
    struct mallinfo2 info = mallinfo2();
    mallinfo_bytes = info.uordblks;
#else
    struct mallinfo info = mallinfo();
    mallinfo_bytes = static_cast<unsigned int>(info.uordblks);
#endif  // __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 33

#if defined(__GLIBC__)

    // For GLIBC, hblkhd is total size of space which is allocated by mmap called by malloc for non-small allocations
#if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 33
    mallinfo_bytes += info.hblkhd;
#else
    mallinfo_bytes += static_cast<unsigned int>(info.hblkhd);
#endif  // __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 33

#endif  // __GLIBC__
#else
    mallinfo_bytes = DEFAULT_NATIVE_BYTES_FROM_MALLINFO;
    LOG(INFO, RUNTIME) << "Get native bytes from mallinfo without GLIBC or MOBILE_LIBC. Return default value";
#endif  // __GLIBC__ || PANDA_TARGET_MOBILE
#endif  // PANDA_ASAN_ON || PANDA_TSAN_ON
    // For ASAN or TSAN, return default value. For GLIBC, return uordblks + hblkhd. For MOBILE_LIBC, return uordblks.
    // For others, return default value.
    return mallinfo_bytes;
}

}  // namespace panda::os::mem
