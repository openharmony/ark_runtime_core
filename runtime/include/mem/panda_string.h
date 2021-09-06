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

#ifndef PANDA_RUNTIME_INCLUDE_MEM_PANDA_STRING_H_
#define PANDA_RUNTIME_INCLUDE_MEM_PANDA_STRING_H_

#include <sstream>
#include <string>
#include <string_view>

#include "runtime/mem/allocator_adapter.h"

namespace panda {
namespace coretypes {
class String;
}  // namespace coretypes

using PandaString = std::basic_string<char, std::char_traits<char>, mem::AllocatorAdapter<char>>;
using PandaStringStream = std::basic_stringstream<char, std::char_traits<char>, mem::AllocatorAdapter<char>>;
using PandaIStringStream = std::basic_istringstream<char, std::char_traits<char>, mem::AllocatorAdapter<char>>;
using PandaOStringStream = std::basic_ostringstream<char, std::char_traits<char>, mem::AllocatorAdapter<char>>;

int64_t PandaStringToLL(const PandaString &str);
uint64_t PandaStringToULL(const PandaString &str);
float PandaStringToF(const PandaString &str);
double PandaStringToD(const PandaString &str);
PandaString ConvertToString(const std::string &str);
PandaString ConvertToString(coretypes::String *s);

template <class T>
std::enable_if_t<std::is_arithmetic_v<T>, PandaString> ToPandaString(T value)
{
    PandaStringStream str_stream;
    str_stream << value;
    return str_stream.str();
}

struct PandaStringHash {
    using argument_type = panda::PandaString;
    using result_type = std::size_t;

    size_t operator()(const PandaString &str) const noexcept
    {
        return std::hash<std::string_view>()(std::string_view(str.data(), str.size()));
    }
};

inline std::string PandaStringToStd(const PandaString &pandastr)
{
    // NOLINTNEXTLINE(readability-redundant-string-cstr)
    std::string str = pandastr.c_str();
    return str;
}

}  // namespace panda

namespace std {

template <>
struct hash<panda::PandaString> {
    using argument_type = panda::PandaStringHash::argument_type;
    using result_type = panda::PandaStringHash::result_type;

    size_t operator()(const panda::PandaString &str) const noexcept
    {
        return panda::PandaStringHash()(str);
    }
};

}  // namespace std

#endif  // PANDA_RUNTIME_INCLUDE_MEM_PANDA_STRING_H_
