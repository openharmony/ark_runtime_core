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

#include <unordered_map>

#include "assembly-type.h"

namespace panda::pandasm {

static std::unordered_map<std::string_view, std::string_view> primitive_types = {
    {"u1", "Z"},  {"i8", "B"},  {"u8", "H"},  {"i16", "S"}, {"u16", "C"},  {"i32", "I"}, {"u32", "U"},
    {"f32", "F"}, {"f64", "D"}, {"i64", "J"}, {"u64", "Q"}, {"void", "V"}, {"any", "A"}};

std::string Type::GetDescriptor(bool ignore_primitive) const
{
    if (!ignore_primitive) {
        auto it = primitive_types.find(component_name_);
        if (it != primitive_types.cend()) {
            return std::string(rank_, '[') + it->second.data();
        }
    }

    std::string res = std::string(rank_, '[') + "L" + component_name_ + ";";
    std::replace(res.begin(), res.end(), '.', '/');
    return res;
}

/* static */
panda_file::Type::TypeId Type::GetId(std::string_view name, bool ignore_primitive)
{
    static std::unordered_map<std::string_view, panda_file::Type::TypeId> panda_types = {
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define PANDATYPE(name, inst_code) {std::string_view(name), panda_file::Type::TypeId::inst_code},
        PANDA_ASSEMBLER_TYPES(PANDATYPE)
#undef PANDATYPE
    };

    if (!ignore_primitive) {
        auto iter = panda_types.find(name);
        if (iter == panda_types.end()) {
            return panda_file::Type::TypeId::REFERENCE;
        }
        return iter->second;
    }
    return panda_file::Type::TypeId::REFERENCE;
}

/* static */
std::string Type::GetName(std::string_view component_name, size_t rank)
{
    std::string name(component_name);
    while (rank-- > 0) {
        name += "[]";
    }
    return name;
}

/* static */
Type Type::FromDescriptor(std::string_view descriptor)
{
    static std::unordered_map<std::string_view, std::string_view> reverse_primitive_types = {
        {"Z", "u1"},  {"B", "i8"},  {"H", "u8"},  {"S", "i16"}, {"C", "u16"},  {"I", "i32"}, {"U", "u32"},
        {"F", "f32"}, {"D", "f64"}, {"J", "i64"}, {"Q", "u64"}, {"V", "void"}, {"A", "any"}};

    size_t i = 0;
    while (descriptor[i] == '[') {
        ++i;
    }

    size_t rank = i;
    bool is_ref_type = descriptor[i] == 'L';
    if (is_ref_type) {
        descriptor.remove_suffix(1); /* Remove semicolon */
        ++i;
    }

    descriptor.remove_prefix(i);

    if (is_ref_type) {
        return Type(descriptor, rank);
    }
    return Type(reverse_primitive_types[descriptor], rank);
}

/* static */
Type Type::FromName(std::string_view name, bool ignore_primitive)
{
    constexpr size_t STEP = 2;

    size_t size = name.size();
    size_t i = 0;

    while (name[size - i - 1] == ']') {
        i += STEP;
    }

    name.remove_suffix(i);
    return Type(name, i / STEP, ignore_primitive);
}

/* static */
bool Type::IsPandaPrimitiveType(const std::string &name)
{
    auto it = primitive_types.find(name);
    if (it != primitive_types.cend()) {
        return true;
    } else {
        return false;
    }
}

}  // namespace panda::pandasm
