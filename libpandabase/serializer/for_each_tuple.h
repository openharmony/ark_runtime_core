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

#ifndef PANDA_LIBPANDABASE_SERIALIZER_FOR_EACH_TUPLE_H_
#define PANDA_LIBPANDABASE_SERIALIZER_FOR_EACH_TUPLE_H_

#include <tuple>

namespace panda::serializer::internal {

template <typename Tuple, typename F, size_t... Index>
void ForEachTupleImpl(Tuple &&tuple, F &&f, std::index_sequence<Index...> /* unused */)
{
    [[maybe_unused]] auto unused = {true, (f(std::get<Index>(std::forward<Tuple>(tuple))), void(), true)...};
}

template <typename Tuple, typename F>
void ForEachTuple(Tuple &&tuple, F &&f)
{
    using T = std::remove_reference_t<Tuple>;
    auto sequence = std::make_index_sequence<std::tuple_size_v<T>> {};

    ForEachTupleImpl(std::forward<Tuple>(tuple), std::forward<F>(f), sequence);
}

}  // namespace panda::serializer::internal

#endif  // PANDA_LIBPANDABASE_SERIALIZER_FOR_EACH_TUPLE_H_
