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

#ifndef PANDA_LIBPANDABASE_MACROS_H_
#define PANDA_LIBPANDABASE_MACROS_H_

#include <cassert>
#include <iostream>
#include "os/stacktrace.h"
#include "utils/debug.h"

// Inline (disabled for DEBUG)
#ifndef NDEBUG
#define ALWAYS_INLINE // NOLINT(cppcoreguidelines-macro-usage)
#else  // NDEBUG
#define ALWAYS_INLINE __attribute__((always_inline)) // NOLINT(cppcoreguidelines-macro-usage)
#endif  // !NDEBUG

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define NO_INLINE __attribute__((noinline))

#ifdef __clang__
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define NO_OPTIMIZE [[clang::optnone]]
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define NO_OPTIMIZE __attribute__((optimize("O0")))
#endif

#ifdef __clang__
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define FIELD_UNUSED __attribute__((__unused__))
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define FIELD_UNUSED
#endif

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define PANDA_PUBLIC_API __attribute__((visibility ("default")))

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define MEMBER_OFFSET(T, F) offsetof(T, F)

#if defined(__cplusplus)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define NO_COPY_CTOR(TypeName) \
    TypeName(const TypeName&) = delete;

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define NO_COPY_OPERATOR(TypeName) \
    void operator=(const TypeName&) = delete

// Disabling copy ctor and copy assignment operator.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define NO_COPY_SEMANTIC(TypeName) \
    NO_COPY_CTOR(TypeName)         \
    NO_COPY_OPERATOR(TypeName)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define NO_MOVE_CTOR(TypeName)                   \
    /* NOLINTNEXTLINE(misc-macro-parentheses) */ \
    TypeName(TypeName&&) = delete;

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define NO_MOVE_OPERATOR(TypeName)               \
    /* NOLINTNEXTLINE(misc-macro-parentheses) */ \
    TypeName& operator=(TypeName&&) = delete

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define NO_MOVE_SEMANTIC(TypeName) \
    NO_MOVE_CTOR(TypeName)         \
    NO_MOVE_OPERATOR(TypeName)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DEFAULT_MOVE_CTOR(TypeName)              \
    /* NOLINTNEXTLINE(misc-macro-parentheses) */ \
    TypeName(TypeName&&) = default;

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DEFAULT_MOVE_OPERATOR(TypeName)          \
    /* NOLINTNEXTLINE(misc-macro-parentheses) */ \
    TypeName& operator=(TypeName&&) = default

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DEFAULT_MOVE_SEMANTIC(TypeName) \
    DEFAULT_MOVE_CTOR(TypeName)         \
    DEFAULT_MOVE_OPERATOR(TypeName)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DEFAULT_COPY_CTOR(TypeName)      \
    TypeName(const TypeName&) = default; \

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DEFAULT_COPY_OPERATOR(TypeName)          \
    /* NOLINTNEXTLINE(misc-macro-parentheses) */ \
    TypeName& operator=(const TypeName&) = default

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DEFAULT_COPY_SEMANTIC(TypeName) \
    DEFAULT_COPY_CTOR(TypeName)         \
    DEFAULT_COPY_OPERATOR(TypeName)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DEFAULT_NOEXCEPT_MOVE_CTOR(TypeName)     \
    /* NOLINTNEXTLINE(misc-macro-parentheses) */ \
    TypeName(TypeName&&) noexcept = default;

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DEFAULT_NOEXCEPT_MOVE_OPERATOR(TypeName) \
    /* NOLINTNEXTLINE(misc-macro-parentheses) */ \
    TypeName& operator=(TypeName&&) noexcept = default

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DEFAULT_NOEXCEPT_MOVE_SEMANTIC(TypeName) \
    DEFAULT_NOEXCEPT_MOVE_CTOR(TypeName)         \
    DEFAULT_NOEXCEPT_MOVE_OPERATOR(TypeName)

#endif  // defined(__cplusplus)

#define LIKELY(exp) (__builtin_expect((exp) != 0, true))  // NOLINT(cppcoreguidelines-macro-usage)
#define UNLIKELY(exp) (__builtin_expect((exp) != 0, false))  // NOLINT(cppcoreguidelines-macro-usage)

#if !defined(NDEBUG)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define ASSERT_FAIL(expr) panda::debug::AssertionFail(expr, __FILE__, __LINE__, __FUNCTION__)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define ASSERT_OP(lhs, op, rhs) do { \
        auto __lhs = lhs; \
        auto __rhs = rhs; \
        if (UNLIKELY(!(__lhs op __rhs))) { \
            std::cerr << "CHECK FAILED: " << #lhs << " " #op " " #rhs << std::endl; \
            std::cerr << "      VALUES: " << __lhs << " " #op " " << __rhs << std::endl; \
            panda::PrintStack(std::cerr); \
            std::abort(); \
        } \
    } while (0)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CHECK_LE(lhs, rhs) ASSERT_OP(lhs, <=, rhs)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CHECK_LT(lhs, rhs) ASSERT_OP(lhs, <, rhs)  // CODECHECK-NOLINT(C_RULE_ID_HORIZON_SPACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CHECK_GE(lhs, rhs) ASSERT_OP(lhs, >=, rhs)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CHECK_GT(lhs, rhs) ASSERT_OP(lhs, >, rhs)  // CODECHECK-NOLINT(C_RULE_ID_HORIZON_SPACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CHECK_EQ(lhs, rhs) ASSERT_OP(lhs, ==, rhs)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CHECK_NE(lhs, rhs) ASSERT_OP(lhs, !=, rhs)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define ASSERT(cond) \
    if (UNLIKELY(!(cond))) { \
        ASSERT_FAIL(#cond); \
    }

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define ASSERT_DO(cond, func) \
    if (auto cond_val = cond; UNLIKELY(!(cond_val))) {  \
        func;                                           \
        ASSERT_FAIL(#cond);                      \
    }
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define ASSERT_PRINT(cond, message)   \
    if (auto cond_val = cond; UNLIKELY(!(cond_val))) {  \
        std::cerr << message << std::endl;              \
        ASSERT_FAIL(#cond);                      \
    }
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define ASSERT_RETURN(cond) assert(cond)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define UNREACHABLE()                                                                            \
    do {                                                                                         \
        ASSERT_PRINT(false, "This line should be unreachable"); /* NOLINT(misc-static-assert) */ \
        __builtin_unreachable();                                                                 \
    } while (0)
#else  // NDEBUG
#define ASSERT(cond) static_cast<void>(0) // NOLINT(cppcoreguidelines-macro-usage)
#define ASSERT_DO(cond, func) static_cast<void>(0) // NOLINT(cppcoreguidelines-macro-usage)
#define ASSERT_PRINT(cond, message) static_cast<void>(0) // NOLINT(cppcoreguidelines-macro-usage)
#define ASSERT_RETURN(cond) static_cast<void>(cond) // NOLINT(cppcoreguidelines-macro-usage)
#define UNREACHABLE __builtin_unreachable // NOLINT(cppcoreguidelines-macro-usage)
#define ASSERT_OP(lhs, op, rhs)  // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CHECK_LE(lhs, rhs)  // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CHECK_LT(lhs, rhs)  // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CHECK_GE(lhs, rhs)  // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CHECK_GT(lhs, rhs)  // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CHECK_EQ(lhs, rhs)  // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CHECK_NE(lhs, rhs)  // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#endif  // !NDEBUG

// Due to the impossibility of using asserts in constexpr methods
// we need an extra version of UNREACHABLE macro that can be used in such situations
#define UNREACHABLE_CONSTEXPR __builtin_unreachable  // NOLINT(cppcoreguidelines-macro-usage)

#define MERGE_WORDS_X(A, B) A ## B  // NOLINT(cppcoreguidelines-macro-usage)
#define MERGE_WORDS(A, B) MERGE_WORDS_X(A, B)  // NOLINT(cppcoreguidelines-macro-usage)

#if defined(__has_feature)
#  if __has_feature(thread_sanitizer)
#define NO_THREAD_SANITIZE __attribute__((no_sanitize("thread"))) // NOLINT(cppcoreguidelines-macro-usage)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define TSAN_ANNOTATE_HAPPENS_BEFORE(addr) \
    AnnotateHappensBefore(__FILE__, __LINE__, reinterpret_cast<void *>(addr))
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define TSAN_ANNOTATE_HAPPENS_AFTER(addr) \
    AnnotateHappensAfter(__FILE__, __LINE__, reinterpret_cast<void *>(addr))
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define TSAN_ANNOTATE_IGNORE_WRITES_BEGIN() \
    AnnotateIgnoreWritesBegin(__FILE__, __LINE__)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define TSAN_ANNOTATE_IGNORE_WRITES_END() \
    AnnotateIgnoreWritesEnd(__FILE__, __LINE__)
extern "C" void AnnotateHappensBefore(const char* f, int l, void* addr);
extern "C" void AnnotateHappensAfter(const char* f, int l, void* addr);
extern "C" void AnnotateIgnoreWritesBegin(const char* f, int l);
extern "C" void AnnotateIgnoreWritesEnd(const char* f, int l);
#  else
#define NO_THREAD_SANITIZE
#define TSAN_ANNOTATE_HAPPENS_BEFORE(addr)
#define TSAN_ANNOTATE_HAPPENS_AFTER(addr)
#define TSAN_ANNOTATE_IGNORE_WRITES_BEGIN()
#define TSAN_ANNOTATE_IGNORE_WRITES_END()
#  endif
#else
#   ifdef __SANITIZE_THREAD__
#define NO_THREAD_SANITIZE __attribute__((no_sanitize("thread"))) // NOLINT(cppcoreguidelines-macro-usage)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define TSAN_ANNOTATE_HAPPENS_BEFORE(addr) \
    AnnotateHappensBefore(__FILE__, __LINE__, reinterpret_cast<void *>(addr))
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define TSAN_ANNOTATE_HAPPENS_AFTER(addr) \
    AnnotateHappensAfter(__FILE__, __LINE__, reinterpret_cast<void *>(addr))
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define TSAN_ANNOTATE_IGNORE_WRITES_BEGIN() \
    AnnotateIgnoreWritesBegin(__FILE__, __LINE__)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define TSAN_ANNOTATE_IGNORE_WRITES_END() \
    AnnotateIgnoreWritesEnd(__FILE__, __LINE__)
extern "C" void AnnotateHappensBefore(const char* f, int l, void* addr);
extern "C" void AnnotateHappensAfter(const char* f, int l, void* addr);
extern "C" void AnnotateIgnoreWritesBegin(const char* f, int l);
extern "C" void AnnotateIgnoreWritesEnd(const char* f, int l);
#  else
#define NO_THREAD_SANITIZE
#define TSAN_ANNOTATE_HAPPENS_BEFORE(addr)
#define TSAN_ANNOTATE_HAPPENS_AFTER(addr)
#define TSAN_ANNOTATE_IGNORE_WRITES_BEGIN()
#define TSAN_ANNOTATE_IGNORE_WRITES_END()
#  endif
#endif

#if defined(__has_feature)
#  if __has_feature(undefined_behavior_sanitizer)
#define NO_UB_SANITIZE __attribute__((no_sanitize("undefined"))) // NOLINT(cppcoreguidelines-macro-usage)
#  else
#define NO_UB_SANITIZE
#  endif
#else
#   ifdef __SANITIZE_UNDEFINED__
#define NO_UB_SANITIZE __attribute__((no_sanitize("undefined"))) // NOLINT(cppcoreguidelines-macro-usage)
#  else
#define NO_UB_SANITIZE
#  endif
#endif

#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
#define USE_ADDRESS_SANITIZER
#  endif
#elif defined(__SANITIZE_ADDRESS__)
#define USE_ADDRESS_SANITIZER
#endif

#ifdef USE_ADDRESS_SANITIZER
#define NO_ADDRESS_SANITIZE __attribute__((no_sanitize("address"))) // NOLINT(cppcoreguidelines-macro-usage)
#else
#define NO_ADDRESS_SANITIZE
#endif  // USE_ADDRESS_SANITIZER

#if defined(__has_include)
#if __has_include(<filesystem>)
#define USE_STD_FILESYSTEM
#endif
#endif

#endif  // PANDA_LIBPANDABASE_MACROS_H_
