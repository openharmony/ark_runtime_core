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

#ifndef PANDA_LIBPANDABASE_CONCEPTS_H_
#define PANDA_LIBPANDABASE_CONCEPTS_H_

#include <iterator>

namespace panda {

// Iterable concept

template <typename T, typename = void>
struct is_iterable : public std::false_type {};  // NOLINT(readability-identifier-naming)

template <typename T>
struct is_iterable<  // NOLINT(readability-identifier-naming)
    T, std::void_t<typename T::iterator, decltype(std::declval<T>().begin()), decltype(std::declval<T>().end())>>
    : public std::true_type {};

template <typename T>
// NOLINTNEXTLINE(readability-identifier-naming, misc-definitions-in-headers)
constexpr bool is_iterable_v = is_iterable<T>::value;

// Random access iterable concept

template <typename T>
struct is_random_access_iterable  // NOLINT(readability-identifier-naming)
    : public std::bool_constant<is_iterable_v<T> &&
                                std::is_same_v<typename std::iterator_traits<typename T::iterator>::iterator_category,
                                               std::random_access_iterator_tag>> {};

template <typename T>
// NOLINTNEXTLINE(readability-identifier-naming, misc-definitions-in-headers)
constexpr bool is_random_access_iterable_v = is_random_access_iterable<T>::value;

// Forward iterable concept

template <typename T>
struct is_forward_iterable  // NOLINT(readability-identifier-naming)
    : public std::bool_constant<is_iterable_v<T> &&
                                std::is_same_v<typename std::iterator_traits<typename T::iterator>::iterator_category,
                                               std::forward_iterator_tag>> {};

template <typename T>
// NOLINTNEXTLINE(readability-identifier-naming, misc-definitions-in-headers)
constexpr bool is_forward_iterable_v = is_forward_iterable<T>::value;

// Vectorable concept

template <class V, typename = void>
struct is_vectorable : public std::false_type {};  // NOLINT(readability-identifier-naming)

template <class V>
struct is_vectorable<  // NOLINT(readability-identifier-naming)
    V, std::void_t<typename V::value_type, typename V::allocator_type, decltype(std::declval<V>().size()),
                   decltype(std::declval<V>().data())>> : public std::bool_constant<is_random_access_iterable_v<V>> {};

template <class V>
// NOLINTNEXTLINE(readability-identifier-naming, misc-definitions-in-headers)
constexpr bool is_vectorable_v = is_vectorable<V>::value;

// Stringable concept

template <class S, typename = void>
struct is_stringable : public std::false_type {};  // NOLINT(readability-identifier-naming)

template <class S>
struct is_stringable<  // NOLINT(readability-identifier-naming)
    S, std::void_t<typename S::value_type, typename S::allocator_type, typename S::traits_type,
                   decltype(std::declval<S>().length()), decltype(std::declval<S>().data())>>
    : public std::bool_constant<is_random_access_iterable_v<S>> {};

template <class S>
// NOLINTNEXTLINE(readability-identifier-naming, misc-definitions-in-headers)
constexpr bool is_stringable_v = is_stringable<S>::value;

// Hash mappable concept

template <class HM, typename = void>
struct is_hash_mappable : public std::false_type {};  // NOLINT(readability-identifier-naming)

template <class HM>
struct is_hash_mappable<  // NOLINT(readability-identifier-naming)
    HM, std::void_t<typename HM::key_type, typename HM::mapped_type, typename HM::value_type, typename HM::hasher,
                    typename HM::key_equal, typename HM::allocator_type, decltype(std::declval<HM>().size())>>
    : public std::bool_constant<is_forward_iterable_v<HM>> {};

template <class HM>
// NOLINTNEXTLINE(readability-identifier-naming, misc-definitions-in-headers)
constexpr bool is_hash_mappable_v = is_hash_mappable<HM>::value;

/**
 *  Added in C++20
 */

// Checks whether T is an array type of unknown bound

template <class T>
// NOLINTNEXTLINE(readability-identifier-naming)
struct is_unbounded_array : public std::false_type {};

template <class T>
// NOLINTNEXTLINE(readability-identifier-naming, modernize-avoid-c-arrays)
struct is_unbounded_array<T[]> : public std::true_type {};

template <class T>
// NOLINTNEXTLINE(readability-identifier-naming, misc-definitions-in-headers)
constexpr bool is_unbounded_array_v = is_unbounded_array<T>::value;

// Checks whether T is an array type of known bound

template <class T>
// NOLINTNEXTLINE(readability-identifier-naming)
struct is_bounded_array : public std::false_type {};

template <class T, size_t N>
// NOLINTNEXTLINE(readability-identifier-naming, modernize-avoid-c-arrays)
struct is_bounded_array<T[N]> : public std::true_type {};

template <class T>
// NOLINTNEXTLINE(readability-identifier-naming, misc-definitions-in-headers)
constexpr bool is_bounded_array_v = is_bounded_array<T>::value;

}  // namespace panda

#endif  // PANDA_LIBPANDABASE_CONCEPTS_H_
