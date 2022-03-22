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

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include <malloc.h>

#include "mem_hooks.h"

namespace panda::mem {

size_t PandaHooks::alloc_via_standard = 0;
void *(*volatile PandaHooks::old_malloc_hook)(size_t, const void *) = nullptr;
void *(*volatile PandaHooks::old_memalign_hook)(size_t, size_t, const void *) = nullptr;
void (*volatile PandaHooks::old_free_hook)(void *, const void *) = nullptr;

/* static */
void PandaHooks::SaveMemHooks()
{
#ifndef __MUSL__
    old_malloc_hook = __malloc_hook;
    old_memalign_hook = __memalign_hook;
    old_free_hook = __free_hook;
#endif  // __MUSL__
}

/* static */
void PandaHooks::SetMemHooks()
{
#ifndef __MUSL__
    __malloc_hook = MallocHook;
    __memalign_hook = MemalignHook;
    __free_hook = FreeHook;
#endif  // __MUSL__
}

/* static */
void *PandaHooks::MallocHook(size_t size, [[maybe_unused]] const void *caller)
{
    alloc_via_standard += size;
    // tracking internal allocator is implemented by malloc, we would fail here with this option
#ifndef TRACK_INTERNAL_ALLOCATIONS
    if (alloc_via_standard > MAX_ALLOC_VIA_STANDARD) {
        std::cerr << "Too many usage of standard allocations" << std::endl;
        abort();
    }
#endif  // TRACK_INTERNAL_ALLOCATIONS
    Disable();
    void *result = malloc(size);  // NOLINT(cppcoreguidelines-no-malloc)
    if (UNLIKELY(result == nullptr)) {
        std::cerr << "Malloc error" << std::endl;
        abort();
    }
    SetMemHooks();
    return result;
}

/* static */
void *PandaHooks::MemalignHook(size_t alignment, size_t size, [[maybe_unused]] const void *caller)
{
    alloc_via_standard += size;
    // tracking internal allocator is implemented by malloc, we would fail here with this option
#ifndef TRACK_INTERNAL_ALLOCATIONS
    if (alloc_via_standard > MAX_ALLOC_VIA_STANDARD) {
        std::cerr << "Too many usage of standard allocations" << std::endl;
        abort();
    }
#endif  // TRACK_INTERNAL_ALLOCATIONS
    Disable();
    void *result = memalign(alignment, size);
    if (UNLIKELY(result == nullptr)) {
        std::cerr << "Align error" << std::endl;
        abort();
    }
    SetMemHooks();
    return result;
}

/* static */
void PandaHooks::FreeHook(void *ptr, [[maybe_unused]] const void *caller)
{
    Disable();
    free(ptr);  // NOLINT(cppcoreguidelines-no-malloc)
    ptr = nullptr;
    SetMemHooks();
}

/* static */
void PandaHooks::Enable()
{
    SaveMemHooks();
    SetMemHooks();
}

/* static */
void PandaHooks::Disable()
{
#ifndef __MUSL__
    __malloc_hook = old_malloc_hook;
    __memalign_hook = old_memalign_hook;
    __free_hook = old_free_hook;
#endif  // __MUSL__
}

}  // namespace panda::mem
