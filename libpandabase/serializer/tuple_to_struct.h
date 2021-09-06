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

#ifndef PANDA_LIBPANDABASE_SERIALIZER_TUPLE_TO_STRUCT_H_
#define PANDA_LIBPANDABASE_SERIALIZER_TUPLE_TO_STRUCT_H_

#include <tuple>

namespace panda::serializer::internal {

template <typename Struct, size_t... Is, typename Tuple>
Struct TupleToStructImpl([[maybe_unused]] std::index_sequence<Is...> is, Tuple &&tup)
{
    return {std::get<Is>(std::forward<Tuple>(tup))...};
}

template <typename Struct, typename Tuple>
Struct TupleToStruct(Tuple &&tup)
{
    using T = std::remove_reference_t<Tuple>;
    auto sequence = std::make_index_sequence<std::tuple_size_v<T>> {};

    return TupleToStructImpl<Struct>(sequence, std::forward<Tuple>(tup));
}

}  // namespace panda::serializer::internal

#endif  // PANDA_LIBPANDABASE_SERIALIZER_TUPLE_TO_STRUCT_H_
