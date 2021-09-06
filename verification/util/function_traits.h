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

#ifndef PANDA_VERIFICATION_UTIL_FUNCTION_TRAITS_H_
#define PANDA_VERIFICATION_UTIL_FUNCTION_TRAITS_H_

#include <type_traits>
#include <tuple>

namespace panda::verifier {
template <int...>
struct indices {
};

template <int N, int... S>
struct gen_indices : gen_indices<N - 1, N - 1, S...> {
};

template <int... S>
struct gen_indices<0, S...> {
    typedef indices<S...> type;
};

template <class F>
struct function_signature_helper;

template <typename R, typename... Args>
struct function_signature_helper<R (*)(Args...)> : public function_signature_helper<R(Args...)> {
};

template <typename R, typename F, typename... Args>
struct function_signature_helper<R (F::*)(Args...) const> : public function_signature_helper<R(Args...)> {
};

template <typename R, typename F, typename... Args>
struct function_signature_helper<R (F::*)(Args...)> : public function_signature_helper<R(Args...)> {
};

template <typename R, typename... Args>
struct function_signature_helper<R(Args...)> {
    using return_type = R;
    using args_tuple = std::tuple<Args...>;

    static constexpr std::size_t arity = sizeof...(Args);

    template <std::size_t N>
    struct argument {
        static_assert(N < arity, "invalid argument index");
        using type = std::tuple_element_t<N, args_tuple>;
    };

    template <std::size_t N>
    using arg_type = typename argument<N>::type;
};

template <typename F>
class function_signature : public function_signature_helper<decltype(&F::operator())> {
    using base = function_signature_helper<decltype(&F::operator())>;

public:
    template <typename L, typename... Args>
    static typename base::return_type call(L f, std::tuple<Args...> &&args)
    {
        return call_helper(f, std::forward(args), typename gen_indices<sizeof...(Args)>::type {});
    }

private:
    template <typename L, typename A, int... S>
    static typename base::return_type call_helper(L f, A &&a, indices<S...>)
    {
        return f(std::forward(std::get<S>(a))...);
    }
};

template <typename BinOp>
class n_ary {
public:
    using sig = function_signature<BinOp>;

    using ret_type = std::decay_t<typename sig::return_type>;
    using lhs_type = std::decay_t<typename sig::template argument<0>::type>;
    using rhs_type = std::decay_t<typename sig::template argument<1>::type>;
    static_assert(sig::arity == 0x2, "only binary operation is expected");
    static_assert(std::is_same<lhs_type, rhs_type>::value, "argument types should be the same");
    static_assert(std::is_same<ret_type, lhs_type>::value, "return value type should be the same as arguments one");

    using type = ret_type;

    n_ary(BinOp op) : binop {op} {}
    ~n_ary() = default;
    DEFAULT_MOVE_SEMANTIC(n_ary);
    DEFAULT_COPY_SEMANTIC(n_ary);

    auto operator()(type lhs, type rhs)
    {
        return binop(lhs, rhs);
    }

    template <typename... Args>
    auto operator()(type lhs, Args &&... args)
    {
        return binop(lhs, operator()(std::forward<Args>(args)...));
    }

    template <typename... Args>
    auto operator()(std::tuple<Args...> &&args)
    {
        return call_helper(std::forward<std::tuple<Args...>>(args), typename gen_indices<sizeof...(Args)>::type {});
    }

private:
    template <typename A, int... S>
    auto call_helper(A &&a, indices<S...>)
    {
        return operator()(std::forward<std::tuple_element_t<S, A>>(std::get<S>(a))...);
    }

    BinOp binop;
};
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_UTIL_FUNCTION_TRAITS_H_
