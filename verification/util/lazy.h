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

#ifndef PANDA_VERIFICATION_UTIL_LAZY_H_
#define PANDA_VERIFICATION_UTIL_LAZY_H_

#include "callable.h"
#include "macros.h"

#include <optional>
#include <functional>
#include <tuple>

#include <type_traits>

namespace panda::verifier {

template <typename Value>
struct IsLazyStreamValue {
    using type = Value;
    template <typename U, decltype(static_cast<bool>(*(static_cast<U *>(nullptr)))) *>
    struct FS {
    };
    template <typename U>
    static char F(FS<U, nullptr> *);
    template <typename U>
    static int F(...);

    template <typename U, std::decay_t<decltype(*((*(static_cast<U *>(nullptr)))))> *>
    struct GS {
    };
    template <typename U>
    static char G(GS<U, nullptr> *);
    template <typename U>
    static int G(...);

    static constexpr bool value = sizeof(F<Value>(0)) == 1 && sizeof(G<Value>(0)) == 1;
};

template <typename Stream>
struct IsLazyStream {
    using type = Stream;
    static constexpr bool value = IsLazyStreamValue<decltype((*(static_cast<Stream *>(nullptr)))())>::value;
};

template <typename V>
struct LazyStreamValue {
    using type = V;
    using value_type = decltype(*(*(static_cast<V *>(nullptr))));
};

template <typename V>
using LazyStreamValueType = typename LazyStreamValue<V>::value_type;

template <typename C>
auto LazyFetch(C &c)
{
    return [end = c.end(), it = c.begin()]() mutable -> std::optional<std::decay_t<decltype(*(c.begin()))>> {
        if (it != end) {
            return *(it++);
        }
        return std::nullopt;
    };
}

template <typename C>
auto LazyFetch(const C &c)
{
    return [end = c.end(), it = c.begin()]() mutable -> std::optional<std::decay_t<decltype(*(c.begin()))>> {
        if (it != end) {
            return *(it++);
        }
        return std::nullopt;
    };
}

template <typename C>
auto ConstLazyFetch(const C &c)
{
    return [cend = c.cend(), it = c.cbegin()]() mutable -> std::optional<std::decay_t<decltype(*(c.cbegin()))>> {
        if (it != cend) {
            return *(it++);
        }
        return std::nullopt;
    };
}

template <typename C>
auto RefLazyFetch(C &c)
{
    return [end = c.end(), it = c.begin()]() mutable -> std::optional<decltype(std::ref(*(c.begin())))> {
        if (it != end) {
            return {std::ref(*(it++))};
        }
        return std::nullopt;
    };
}

template <typename C>
auto RefConstLazyFetch(const C &c)
{
    return [cend = c.cend(), it = c.cbegin()]() mutable -> std::optional<decltype(std::cref(*(c.cbegin())))> {
        if (it != cend) {
            return {std::cref(*(it++))};
        }
        return std::nullopt;
    };
}

template <typename F, typename L, std::enable_if_t<IsLazyStream<F>::value, int> = 0>
auto Transform(F fetcher, L converter)
{
    return [fetcher, converter]() mutable -> std::optional<decltype(converter(*fetcher()))> {
        if (auto val = fetcher()) {
            return {converter(*val)};
        }
        return std::nullopt;
    };
}

template <typename F, typename L, std::enable_if_t<IsLazyStream<F>::value, int> = 0>
auto Filter(F fetcher, L filter)
{
    return [fetcher, filter]() mutable -> decltype(fetcher()) {
        while (auto val = fetcher()) {
            if (filter(*val)) {
                return val;
            }
        }
        return std::nullopt;
    };
}

template <typename F, std::enable_if_t<IsLazyStream<F>::value, int> = 0>
auto Enumerate(F fetcher, size_t from = 0)
{
    return Transform(fetcher, [idx = from](auto v) mutable { return std::tuple {idx++, v}; });
}

template <typename C>
auto IndicesOf(const C &c)
{
    size_t from = 0;
    size_t to = c.size();
    return [from, to]() mutable -> std::optional<size_t> {
        if (from < to) {
            return {from++};
        }
        return std::nullopt;
    };
}

template <typename Fetcher, typename Func, std::enable_if_t<IsLazyStream<Fetcher>::value, int> = 0>
void ForEachCond(Fetcher fetcher, Func func)
{
    while (auto val = fetcher()) {
        if (!func(*val)) {
            return;
        }
    }
}

template <typename Fetcher, typename Func, std::enable_if_t<IsLazyStream<Fetcher>::value, int> = 0>
void ForEach(Fetcher fetcher, Func func)
{
    while (auto val = fetcher()) {
        func(*val);
    }
}

template <typename Fetcher, typename Accum, typename Func, std::enable_if_t<IsLazyStream<Fetcher>::value, int> = 0>
Accum FoldLeft(Fetcher fetcher, Accum accum, Func func)
{
    while (auto val = fetcher()) {
        accum = func(accum, *val);
    }
    return accum;
}

template <typename F, std::enable_if_t<IsLazyStream<F>::value, int> = 0>
auto Iterable(F fetcher)
{
    class SomeClass {
    public:
        explicit SomeClass(F f) : Fetcher_ {f} {};
        ~SomeClass() = default;
        DEFAULT_MOVE_SEMANTIC(SomeClass);
        DEFAULT_COPY_SEMANTIC(SomeClass);
        class Iterator {
        public:
            explicit Iterator(std::optional<F> f) : Fetcher_ {std::move(f)}
            {
                if (Fetcher_) {
                    val_ = (*Fetcher_)();
                }
            }
            ~Iterator() = default;
            DEFAULT_MOVE_SEMANTIC(Iterator);
            DEFAULT_COPY_SEMANTIC(Iterator);
            Iterator &operator++()
            {
                val_ = (*Fetcher_)();
                return *this;
            }
            bool operator==([[maybe_unused]] const Iterator &it)
            {
                return !static_cast<bool>(val_);
            }
            bool operator!=([[maybe_unused]] const Iterator &it)
            {
                return static_cast<bool>(val_);
            }
            auto operator*()
            {
                return *val_;
            }

        private:
            std::optional<F> Fetcher_;
            decltype((*Fetcher_)()) val_;
        };
        Iterator end()  // NOLINT(readability-identifier-naming)
        {
            return Iterator {{}};
        }
        Iterator begin()  // NOLINT(readability-identifier-naming)
        {
            return Iterator {Fetcher_};
        }

    private:
        F Fetcher_;
    };
    return SomeClass {fetcher};
}

template <typename Prev, typename Next,
          std::enable_if_t<std::is_same<decltype((*static_cast<Prev *>(nullptr))()),
                                        decltype((*static_cast<Next *>(nullptr))())>::value,
                           int> = 0,
          std::enable_if_t<IsLazyStream<Prev>::value, int> = 0>
auto operator+(Prev prev, Next next)
{
    return [prev, next, first = true]() mutable {
        if (first) {
            auto val = prev();
            if (val) {
                return val;
            }
            first = false;
        }
        auto val = next();
        return val;
    };
}

template <typename C, typename S>
C ContainerOf(S stream, typename std::decay<decltype(
                            (*static_cast<C *>(nullptr))
                                .push_back(*static_cast<std::decay_t<decltype(*((*(static_cast<S *>(nullptr)))()))> *>(
                                    nullptr)))>::type *tmp = nullptr)
{
    (void)tmp;
    C c;
    while (auto val = stream()) {
        c.push_back(*val);
    }
    return c;
}

template <typename C, typename S>
C ContainerOf(
    S stream,
    typename std::decay<decltype(
        (*static_cast<C *>(nullptr))
            .insert(*static_cast<std::decay_t<decltype(*((*(static_cast<S *>(nullptr)))()))> *>(nullptr)))>::type *tmp =
        nullptr)
{
    (void)tmp;
    C c;
    while (auto val = stream()) {
        c.insert(*val);
    }
    return c;
}

template <template <typename...> class UnorderedSet, typename Fetcher,
          std::enable_if_t<IsLazyStream<Fetcher>::value, int> = 0>
auto Uniq(Fetcher fetcher)
{
    auto handler = [set = UnorderedSet<std::decay_t<decltype(*fetcher())>> {}](const auto &val) mutable {  // NOLINT
        if (set.count(val) == 0) {
            set.insert(val);
            return true;
        }
        return false;
    };
    return Filter(std::move(fetcher), std::move(handler));
}

template <typename Stream, std::enable_if_t<IsLazyStream<Stream>::value, int> = 0>
auto FirstElement(Stream stream)
{
    return stream();
}

template <typename Stream, std::enable_if_t<IsLazyStream<Stream>::value, int> = 0>
bool IsLazyStreamEmpty(Stream stream)
{
    return !FirstElement(stream);
}

template <typename Stream, typename Pred, std::enable_if_t<IsLazyStream<Stream>::value, int> = 0>
auto Find(Stream stream, Pred pred)
{
    return FirstElement(Filter(stream, pred));
}

template <typename Stream, typename Pred, std::enable_if_t<IsLazyStream<Stream>::value, int> = 0>
bool IsPresent(Stream stream, Pred pred)
{
    return Find(stream, pred);
}

template <typename LHS, typename RHS, std::enable_if_t<IsLazyStream<LHS>::value, int> = 0,
          std::enable_if_t<IsLazyStream<RHS>::value, int> = 0>
auto JoinStreams(LHS lhs, RHS rhs)
{
    return [lhs, rhs]() mutable -> std::optional<std::tuple<decltype(*(lhs())), decltype(*(rhs()))>> {
        auto lv = lhs();
        if (!lv) {
            return std::nullopt;
        }
        auto rv = rhs();
        if (!rv) {
            return std::nullopt;
        }
        return std::make_tuple(*lv, *rv);
    };
}

template <typename LHS, typename MHS, typename RHS, std::enable_if_t<IsLazyStream<LHS>::value, int> = 0,
          std::enable_if_t<IsLazyStream<MHS>::value, int> = 0, std::enable_if_t<IsLazyStream<RHS>::value, int> = 0>
auto JoinStreams(LHS lhs, MHS mhs, RHS rhs)
{
    return [lhs, mhs, rhs]() mutable -> std::optional<std::tuple<std::remove_reference_t<decltype(*(lhs()))>,
                                                                 std::remove_reference_t<decltype(*(mhs()))>,
                                                                 std::remove_reference_t<decltype(*(rhs()))>>> {
        auto lv = lhs();
        if (!lv) {
            return std::nullopt;
        }
        auto mv = mhs();
        if (!mv) {
            return std::nullopt;
        }
        auto rv = rhs();
        if (!rv) {
            return std::nullopt;
        }
        return std::make_tuple(*lv, *mv, *rv);
    };
}
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_UTIL_LAZY_H_
