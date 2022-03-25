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

#ifndef PANDA_VERIFICATION_UTIL_REF_WRAPPER_H_
#define PANDA_VERIFICATION_UTIL_REF_WRAPPER_H_

#include <functional>
#include <type_traits>

#include "invalid_ref.h"

#include "macros.h"

namespace panda::verifier {

template <typename T>
class Ref : public std::reference_wrapper<T> {
    using Base = std::reference_wrapper<T>;

public:
    using type = T;
    Ref() : Base(Invalid<T>()) {}
    Ref(const Ref &) = default;
    Ref &operator=(const Ref &) = default;
    Ref(const std::reference_wrapper<T> &ref) : Base(ref) {}
    Ref &operator=(const std::reference_wrapper<T> &ref)
    {
        Base::operator=(ref);
        return *this;
    }
    ~Ref() = default;
    T &get() const noexcept
    {
        T &result = Base::get();
        ASSERT(Valid(result));
        return result;
    }
    operator T &() const noexcept
    {
        return get();
    }
    template <typename... Args>
    std::invoke_result_t<T &, Args...> operator()(Args &&... args) const
    {
#ifndef NDEBUG
        T &result = Base::get();
        ASSERT(Valid(result));
#endif  // NDEBUG

        return Base::operator()(std::forward<Args>(args)...);
    }
};

}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_UTIL_REF_WRAPPER_H_
