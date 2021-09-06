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

#ifndef PANDA_LIBPANDABASE_SERIALIZER_STRUCT_TO_TUPLE_H_
#define PANDA_LIBPANDABASE_SERIALIZER_STRUCT_TO_TUPLE_H_

#include <tuple>

namespace panda::serializer::internal {

template <size_t N>
struct StructToTupleImpl;

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage), CODECHECK-NOLINTNEXTLINE(C_RULE_ID_DEFINE_MULTILINE)
#define SERIALIZE_INTERNAL_STRUCT_TO_TUPLE_IMPL(N, ...)     \
    template <>                                             \
    struct StructToTupleImpl<N> {                           \
        template <typename Struct>                          \
        auto operator()(Struct &&str) const                 \
        {                                                   \
            auto [__VA_ARGS__] = std::forward<Struct>(str); \
            return std::make_tuple(__VA_ARGS__);            \
        }                                                   \
    }

SERIALIZE_INTERNAL_STRUCT_TO_TUPLE_IMPL(1, e0);
SERIALIZE_INTERNAL_STRUCT_TO_TUPLE_IMPL(2, e0, e1);
SERIALIZE_INTERNAL_STRUCT_TO_TUPLE_IMPL(3, e0, e1, e2);
SERIALIZE_INTERNAL_STRUCT_TO_TUPLE_IMPL(4, e0, e1, e2, e3);
SERIALIZE_INTERNAL_STRUCT_TO_TUPLE_IMPL(5, e0, e1, e2, e3, e4);
SERIALIZE_INTERNAL_STRUCT_TO_TUPLE_IMPL(6, e0, e1, e2, e3, e4, e5);
SERIALIZE_INTERNAL_STRUCT_TO_TUPLE_IMPL(7, e0, e1, e2, e3, e4, e5, e6);
SERIALIZE_INTERNAL_STRUCT_TO_TUPLE_IMPL(8, e0, e1, e2, e3, e4, e5, e6, e7);
SERIALIZE_INTERNAL_STRUCT_TO_TUPLE_IMPL(9, e0, e1, e2, e3, e4, e5, e6, e7, e8);
SERIALIZE_INTERNAL_STRUCT_TO_TUPLE_IMPL(10, e0, e1, e2, e3, e4, e5, e6, e7, e8, e9);
SERIALIZE_INTERNAL_STRUCT_TO_TUPLE_IMPL(11, e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10);
SERIALIZE_INTERNAL_STRUCT_TO_TUPLE_IMPL(12, e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11);
SERIALIZE_INTERNAL_STRUCT_TO_TUPLE_IMPL(13, e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12);
SERIALIZE_INTERNAL_STRUCT_TO_TUPLE_IMPL(14, e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13);
SERIALIZE_INTERNAL_STRUCT_TO_TUPLE_IMPL(15, e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14);
SERIALIZE_INTERNAL_STRUCT_TO_TUPLE_IMPL(16, e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15);

#undef SERIALIZE_INTERNAL_STRUCT_TO_TUPLE_IMPL

template <std::size_t N, class Struct>
auto StructToTuple(Struct &&str)
{
    return StructToTupleImpl<N> {}(std::forward<Struct>(str));
}

}  // namespace panda::serializer::internal

#endif  // PANDA_LIBPANDABASE_SERIALIZER_STRUCT_TO_TUPLE_H_
