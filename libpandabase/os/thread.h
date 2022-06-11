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

#ifndef PANDA_LIBPANDABASE_OS_THREAD_H_
#define PANDA_LIBPANDABASE_OS_THREAD_H_

#include "os/error.h"
#include "utils/expected.h"

#include <cstdint>
#include <memory>
#include <thread>
#include <pthread.h>
#ifdef PANDA_TARGET_UNIX
#include "os/unix/thread.h"
#elif PANDA_TARGET_WINDOWS
#include "os/windows/thread.h"
#else
#error "Unsupported platform"
#endif

namespace panda::os::thread {
using ThreadId = uint32_t;
using native_handle_type = std::thread::native_handle_type;

ThreadId GetCurrentThreadId();
int GetPid();
int SetThreadName(native_handle_type pthread_handle, const char *name);
native_handle_type GetNativeHandle();
void Yield();
void NativeSleep(unsigned int ms);
void ThreadDetach(native_handle_type pthread_handle);
void ThreadExit(void *ret);
void ThreadJoin(native_handle_type pthread_handle, void **ret);

// Templated functions need to be defined here to be accessible everywhere

namespace internal {
template <typename T>
struct SharedPtrStruct;

template <typename T>
// CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_NO_USE_SHAREDPTR)
using SharedPtrToSharedPtrStruct = std::shared_ptr<SharedPtrStruct<T>>;

template <typename T>
struct SharedPtrStruct {
    SharedPtrToSharedPtrStruct<T> this_ptr;  // NOLINT(misc-non-private-member-variables-in-classes)
    T data;                                  // NOLINT(misc-non-private-member-variables-in-classes)
    SharedPtrStruct(SharedPtrToSharedPtrStruct<T> ptr_in, T data_in)
        : this_ptr(std::move(ptr_in)), data(std::move(data_in))
    {
    }
};

template <size_t... Is>
struct Seq {
};

template <size_t N, size_t... Is>
struct GenArgSeq : GenArgSeq<N - 1, N - 1, Is...> {
};

template <size_t... Is>
struct GenArgSeq<1, Is...> : Seq<Is...> {
};

template <class Func, typename Tuple, size_t... I>
static void CallFunc(Func &func, Tuple &args, Seq<I...> /* unused */)
{
    func(std::get<I>(args)...);
}

template <class Func, typename Tuple, size_t N>
static void CallFunc(Func &func, Tuple &args)
{
    CallFunc(func, args, GenArgSeq<N>());
}

template <typename Func, typename Tuple, size_t N>
static void *ProxyFunc(void *args)
{
    // Parse pointer and move args to local tuple.
    // We need this pointer to be destroyed by the time function starts to avoid memleak on thread termination.
    Tuple args_tuple;
    {
        auto args_ptr = static_cast<SharedPtrStruct<Tuple> *>(args);
        SharedPtrToSharedPtrStruct<Tuple> local;
        // This breaks shared pointer loop
        local.swap(args_ptr->this_ptr);
        // This moves tuple data to local variable
        args_tuple = args_ptr->data;
    }
    Func *func = std::get<0>(args_tuple);
    CallFunc<Func, Tuple, N>(*func, args_tuple);
    return nullptr;
}
}  // namespace internal

template <typename Func, typename... Args>
native_handle_type ThreadStart(Func *func, Args... args)
{
#ifdef PANDA_TARGET_UNIX
    native_handle_type tid;
#else
    pthread_t tid;
#endif
    auto args_tuple = std::make_tuple(func, std::move(args)...);
    internal::SharedPtrStruct<decltype(args_tuple)> *ptr = nullptr;
    {
        // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_NO_USE_SHAREDPTR)
        auto shared_ptr = std::make_shared<internal::SharedPtrStruct<decltype(args_tuple)>>(nullptr, args_tuple);
        // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_NO_USE_SHAREDPTR)
        ptr = shared_ptr.get();
        // Make recursive ref to prevent shared pointer from being destroyed until child thread acquires it.
        // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_NO_USE_SHAREDPTR)
        ptr->this_ptr = shared_ptr;
        // Leave scope to make sure that local shared_ptr is destroyed before thread creation.
    }
    pthread_create(&tid, nullptr,
                   &internal::ProxyFunc<Func, decltype(args_tuple), std::tuple_size<decltype(args_tuple)>::value>,
                   static_cast<void *>(ptr));
#ifdef PANDA_TARGET_UNIX
    return tid;
#else
    return reinterpret_cast<native_handle_type>(tid);
#endif
}
}  // namespace panda::os::thread

#endif  // PANDA_LIBPANDABASE_OS_THREAD_H_
