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

#ifndef PANDA_RUNTIME_INCLUDE_MEM_PANDA_SMART_POINTERS_H_
#define PANDA_RUNTIME_INCLUDE_MEM_PANDA_SMART_POINTERS_H_

#include <memory>

#include "libpandabase/concepts.h"
#include "runtime/include/mem/allocator.h"

namespace panda {

template <class T>
struct DefaultPandaDelete {
    constexpr DefaultPandaDelete() noexcept = default;

    template <class U, std::enable_if_t<std::is_convertible_v<U *, T *>, void *> = nullptr>
    // NOLINTNEXTLINE(google-explicit-constructor, readability-named-parameter)
    DefaultPandaDelete(const DefaultPandaDelete<U> &) noexcept
    {
    }

    void operator()(T *ptr) const noexcept
    {
        static_assert(!std::is_void_v<T>, "Incorrect (void) type for DefaultPandaDelete");
        static_assert(sizeof(T) > 0, "Incorrect (incomplete) type for DefaultPandaDelete");
        mem::InternalAllocator<>::GetInternalAllocatorFromRuntime()->Delete(ptr);
    }
};

template <class T>
// NOLINTNEXTLINE(modernize-avoid-c-arrays)
struct DefaultPandaDelete<T[]> {
    constexpr DefaultPandaDelete() noexcept = default;

    // NOLINTNEXTLINE(modernize-avoid-c-arrays)
    template <class U, std::enable_if_t<std::is_convertible_v<U (*)[], T (*)[]>, void *> = nullptr>
    // NOLINTNEXTLINE(google-explicit-constructor, readability-named-parameter, modernize-avoid-c-arrays)
    DefaultPandaDelete(const DefaultPandaDelete<U[]> &) noexcept
    {
    }

    template <class U>
    // NOLINTNEXTLINE(google-explicit-constructor, readability-named-parameter, modernize-avoid-c-arrays)
    std::enable_if_t<std::is_convertible_v<U (*)[], T (*)[]>> operator()(U *ptr) const noexcept
    {
        static_assert(!std::is_void_v<T>, "Incorrect (void) type for DefaultPandaDelete");
        static_assert(sizeof(T) > 0, "Incorrect (incomplete) type for DefaultPandaDelete");
        mem::InternalAllocator<>::GetInternalAllocatorFromRuntime()->DeleteArray(ptr);
    }
};

template <typename T, typename Deleter = DefaultPandaDelete<T>>
using PandaUniquePtr = std::unique_ptr<T, Deleter>;

template <class T, bool cond>
using PandaUniquePtrIf = std::enable_if_t<cond, PandaUniquePtr<T>>;

template <class T, class... Args>
inline PandaUniquePtrIf<T, !std::is_array_v<T>> MakePandaUnique(Args &&... args)
{
    return PandaUniquePtr<T> {
        mem::InternalAllocator<>::GetInternalAllocatorFromRuntime()->New<T>(std::forward<Args>(args)...)};
}

template <class T>
inline PandaUniquePtrIf<T, is_unbounded_array_v<T>> MakePandaUnique(size_t size)
{
    return PandaUniquePtr<T> {mem::InternalAllocator<>::GetInternalAllocatorFromRuntime()->New<T>(size)};
}

template <class T, class... Args>
inline PandaUniquePtrIf<T, is_bounded_array_v<T>> MakePandaUnique(Args &&... args) = delete;

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_MEM_PANDA_SMART_POINTERS_H_
