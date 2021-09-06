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

#ifndef PANDA_VERIFICATION_UTIL_CALLABLE_H_
#define PANDA_VERIFICATION_UTIL_CALLABLE_H_

#include <utility>
#include <tuple>

namespace panda::verifier {
/*

this is lightweight analogue of std::function.
Type-erased holder of function, closure or object with operator().

NB! it does not keep object contents, only pointers, so
a closure must be accessible during lifetime of
the callable<...> object.

Motivation: in many cases using type-erased callable object
based on std::function is very heavy-weight: extra allocations,
data copying, vtbls  and so on.
So here is lightweight counterpart, using static type-erasing
without extra storage other than two explicit private pointers.
*/

template <typename Signature>
class callable;

template <typename R, typename... Args>
class callable<R(Args...)> {
    struct callable_type {
        R operator()(Args...);
    };
    typedef R (callable_type::*method_type)(Args...);
    typedef R (*function_type)(Args...);
    callable_type *object {nullptr};
    union method_union {
        method_type m;
        function_type f;
        method_union(method_type method) : m(method) {}
        method_union(function_type function) : f(function) {}
        method_union() : m {nullptr} {}
        ~method_union() = default;
    } method;

public:
    using Result = R;
    using Arguments = std::tuple<Args...>;

    callable() = default;
    callable(const callable &) = default;
    callable(callable &&) = default;
    callable &operator=(const callable &) = default;
    callable &operator=(callable &&) = default;
    ~callable() = default;

    template <typename T, typename = decltype(static_cast<R (T::*)(Args...) const>(&T::operator()))>
    constexpr callable(const T &obj)
        : object {reinterpret_cast<callable_type *>(&const_cast<T &>(obj))},
          method {reinterpret_cast<method_type>(static_cast<R (T::*)(Args...) const>(&T::operator()))}
    {
    }

    template <typename T, typename = decltype(static_cast<R (T::*)(Args...)>(&T::operator()))>
    constexpr callable(T &obj)
        : object {reinterpret_cast<callable_type *>(&const_cast<T &>(obj))},
          method {reinterpret_cast<method_type>(static_cast<R (T::*)(Args...)>(&T::operator()))}
    {
    }

    template <typename T>
    constexpr callable(const T &obj, R (T::*param_method)(Args...) const)
        : object {reinterpret_cast<callable_type *>(&const_cast<T &>(obj))},
          method {reinterpret_cast<method_type>(param_method)}
    {
    }

    template <typename T>
    constexpr callable(T &obj, R (T::*param_method)(Args...))
        : object {reinterpret_cast<callable_type *>(&const_cast<T &>(obj))},
          method {reinterpret_cast<method_type>(param_method)}
    {
    }

    constexpr callable(function_type func) : object {nullptr}, method {func} {}

    constexpr R operator()(Args... args) const
    {
        if (object == nullptr) {
            return (method.f)(args...);
        }
        return (object->*(method.m))(args...);
    }

    operator bool() const
    {
        return method != nullptr;
    }
};
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_UTIL_CALLABLE_H_
