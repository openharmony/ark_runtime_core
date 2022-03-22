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

#ifndef PANDA_LIBPANDABASE_CLANG_H_
#define PANDA_LIBPANDABASE_CLANG_H_

// Based on Thread safety analysis annotations taken from https://clang.llvm.org/docs/ThreadSafetyAnalysis.html

// Enable thread safety attributes only with clang.
// The attributes can be safely erased when compiling with other compilers.
#if defined(__clang__) && (!defined(SWIG))
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define THREAD_ANNOTATION_ATTRIBUTE__(x) __attribute__((x))
#else
#define THREAD_ANNOTATION_ATTRIBUTE__(x)  // no-op
#endif

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(capability(x))

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define SHARED_CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(shared_capability(x))

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOCKABLE  THREAD_ANNOTATION_ATTRIBUTE__(lockable)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define SCOPED_CAPABILITY THREAD_ANNOTATION_ATTRIBUTE__(scoped_lockable)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define GUARDED_BY(x) THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(x))

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define PT_GUARDED_BY(x) THREAD_ANNOTATION_ATTRIBUTE__(pt_guarded_by(x))

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define ACQUIRED_BEFORE(...) THREAD_ANNOTATION_ATTRIBUTE__(acquired_before(__VA_ARGS__))

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define ACQUIRED_AFTER(...) THREAD_ANNOTATION_ATTRIBUTE__(acquired_after(__VA_ARGS__))

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define REQUIRES(...) THREAD_ANNOTATION_ATTRIBUTE__(requires_capability(__VA_ARGS__))

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define REQUIRES_SHARED(...) THREAD_ANNOTATION_ATTRIBUTE__(requires_shared_capability(__VA_ARGS__))

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define ACQUIRE(...) THREAD_ANNOTATION_ATTRIBUTE__(acquire_capability(__VA_ARGS__))

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define ACQUIRE_SHARED(...) THREAD_ANNOTATION_ATTRIBUTE__(acquire_shared_capability(__VA_ARGS__))

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define RELEASE(...) THREAD_ANNOTATION_ATTRIBUTE__(release_capability(__VA_ARGS__))

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define RELEASE_SHARED(...) THREAD_ANNOTATION_ATTRIBUTE__(release_shared_capability(__VA_ARGS__))

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define RELEASE_GENERIC(...) THREAD_ANNOTATION_ATTRIBUTE__(release_generic_capability(__VA_ARGS__))

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define TRY_ACQUIRE(...) THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_capability(__VA_ARGS__))

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define TRY_ACQUIRE_SHARED(...) THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_shared_capability(__VA_ARGS__))

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define EXCLUDES(...) THREAD_ANNOTATION_ATTRIBUTE__(locks_excluded(__VA_ARGS__))

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define ASSERT_CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(assert_capability(x))

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define ASSERT_SHARED_CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(assert_shared_capability(x))

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define RETURN_CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(lock_returned(x))

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define NO_THREAD_SAFETY_ANALYSIS THREAD_ANNOTATION_ATTRIBUTE__(no_thread_safety_analysis)

#endif  // PANDA_LIBPANDABASE_CLANG_H_
