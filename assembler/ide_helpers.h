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

#ifndef PANDA_ASSEMBLER_IDE_HELPERS_H_
#define PANDA_ASSEMBLER_IDE_HELPERS_H_

#include <sstream>

namespace panda::pandasm {

struct SourcePosition {
    size_t line = 0;
    size_t column = 0;

    std::string JsonSerialize() const
    {
        std::stringstream ss;
        ss << "{ "
           << "\"line\": " << line << ", "
           << "\"column\": " << column << " }";
        return ss.str();
    }
};

struct SourceLocation {
    SourcePosition begin;
    SourcePosition end;

    std::string JsonSerialize() const
    {
        std::stringstream ss;
        ss << "{ "
           << "\"begin\": " << begin.JsonSerialize() << ", "
           << "\"end\": " << end.JsonSerialize() << " }";
        return ss.str();
    }
};

template <typename T>
std::string JsonSerializeItemBody(const T &item)
{
    std::stringstream ss;
    std::string quoted_name = "\"" + item.name + "\"";
    ss << "{ "
       << "\"name\": " << quoted_name;
    if (item.file_location->is_defined) {
        ss << ", "
           << "\"bodyLocation\": " << item.body_location.JsonSerialize() << " }";
    } else {
        ss << " }";
    }
    return ss.str();
}

template <typename T>
std::string JsonSerializeProgramItems(const T &item_table)
{
    std::stringstream ss;
    ss << "[ ";
    auto it = item_table.begin();
    if (it != item_table.end()) {
        ss << JsonSerializeItemBody(it->second);
        ++it;
    }
    while (it != item_table.end()) {
        ss << ", " << JsonSerializeItemBody(it->second);
        ++it;
    }
    ss << " ]";
    return ss.str();
}
}  // namespace panda::pandasm

#endif  // PANDA_ASSEMBLER_IDE_HELPERS_H_
