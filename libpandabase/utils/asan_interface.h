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

#ifndef PANDA_LIBPANDABASE_UTILS_ASAN_INTERFACE_H_
#define PANDA_LIBPANDABASE_UTILS_ASAN_INTERFACE_H_

#if defined(__has_feature)
// for clang
#if __has_feature(address_sanitizer)
#define PANDA_ASAN_ON
#endif
#else
#if defined(__SANITIZE_ADDRESS__)
// for gnu compiler
#define PANDA_ASAN_ON
#endif
#endif

#ifdef PANDA_ASAN_ON
extern "C" {
// Marks memory region [addr, addr+size) as unaddressable.
// NOLINTNEXTLINE(readability-identifier-naming)
void __asan_poison_memory_region(void const volatile *addr, size_t size) __attribute__((visibility("default")));
// Marks memory region [addr, addr+size) as addressable.
// NOLINTNEXTLINE(readability-identifier-naming)
void __asan_unpoison_memory_region(void const volatile *addr, size_t size) __attribute__((visibility("default")));
}
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define ASAN_POISON_MEMORY_REGION(addr, size) __asan_poison_memory_region((addr), (size))
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define ASAN_UNPOISON_MEMORY_REGION(addr, size) __asan_unpoison_memory_region((addr), (size))

// Uses attribute ATTRIBUTE_NO_SANITIZE_ADDRESS to fix an issue with concurrent POISON/UNPOISON
// during accessing class fields from the class methods during MT ASAN runs.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define ATTRIBUTE_NO_SANITIZE_ADDRESS __attribute__((no_sanitize_address))
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define ASAN_POISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define ASAN_UNPOISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define ATTRIBUTE_NO_SANITIZE_ADDRESS
#endif

#endif  // PANDA_LIBPANDABASE_UTILS_ASAN_INTERFACE_H_
