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

#ifndef PANDA_ASSEMBLER_ASSEMBLY_TYPE_H_
#define PANDA_ASSEMBLER_ASSEMBLY_TYPE_H_

#include "define.h"
#include "file_items.h"
#include "isa.h"

namespace panda::pandasm {

class Type {
public:
    enum VerificationType {
        TYPE_ID_OBJECT,
        TYPE_ID_ARRAY,
        TYPE_ID_ANY_OBJECT,
    };

    Type() = default;
    DEFAULT_MOVE_SEMANTIC(Type);
    DEFAULT_COPY_SEMANTIC(Type);
    ~Type() = default;

    Type(std::string_view component_name, size_t rank, bool ignore_primitive = false)
        : component_name_(component_name), rank_(rank)
    {
        name_ = GetName(component_name_, rank_);
        type_id_ = GetId(name_, ignore_primitive);
    }

    Type(const Type &component_type, size_t rank)
        : Type(component_type.GetComponentName(), component_type.GetRank() + rank)
    {
    }

    std::string GetDescriptor(bool ignore_primitive = false) const;

    std::string GetName() const
    {
        return name_;
    }

    std::string GetComponentName() const
    {
        return component_name_;
    }

    size_t GetRank() const
    {
        return rank_;
    }

    Type GetComponentType() const
    {
        return Type(component_name_, rank_ > 0 ? rank_ - 1 : 0);
    }

    panda_file::Type::TypeId GetId() const
    {
        return type_id_;
    }

    bool IsArrayContainsPrimTypes() const
    {
        auto elem = GetId(component_name_);
        return elem != panda_file::Type::TypeId::REFERENCE;
    }

    bool IsValid() const
    {
        return !component_name_.empty();
    }

    bool IsArray() const
    {
        return rank_ > 0;
    }

    bool IsObject() const
    {
        return type_id_ == panda_file::Type::TypeId::REFERENCE;
    }

    bool IsTagged() const
    {
        return type_id_ == panda_file::Type::TypeId::TAGGED;
    }

    bool IsIntegral() const
    {
        return type_id_ == panda_file::Type::TypeId::U1 || type_id_ == panda_file::Type::TypeId::U8 ||
               type_id_ == panda_file::Type::TypeId::I8 || type_id_ == panda_file::Type::TypeId::U16 ||
               type_id_ == panda_file::Type::TypeId::I16 || type_id_ == panda_file::Type::TypeId::U32 ||
               type_id_ == panda_file::Type::TypeId::I32 || type_id_ == panda_file::Type::TypeId::U64 ||
               type_id_ == panda_file::Type::TypeId::I64;
    }

    bool FitsInto32() const
    {
        return type_id_ == panda_file::Type::TypeId::U1 || type_id_ == panda_file::Type::TypeId::U8 ||
               type_id_ == panda_file::Type::TypeId::I8 || type_id_ == panda_file::Type::TypeId::U16 ||
               type_id_ == panda_file::Type::TypeId::I16 || type_id_ == panda_file::Type::TypeId::U32 ||
               type_id_ == panda_file::Type::TypeId::I32;
    }

    bool IsFloat32() const
    {
        return type_id_ == panda_file::Type::TypeId::F32;
    }

    bool IsFloat64() const
    {
        return type_id_ == panda_file::Type::TypeId::F64;
    }

    bool IsPrim32() const
    {
        return (IsIntegral() && FitsInto32()) || IsFloat32();
    }

    bool IsPrim64() const
    {
        return (IsIntegral() && !FitsInto32()) || IsFloat64();
    }

    bool IsPrimitive() const
    {
        return IsPrim64() || IsPrim32();
    }

    bool IsVoid() const
    {
        return type_id_ == panda_file::Type::TypeId::VOID;
    }

    static panda_file::Type::TypeId GetId(std::string_view name, bool ignore_primitive = false);

    bool operator==(const Type &type) const
    {
        return name_ == type.name_;
    }

    static Type FromDescriptor(std::string_view descriptor);

    static Type FromName(std::string_view name, bool ignore_primitive = false);

    static bool IsPandaPrimitiveType(const std::string &name);

private:
    static std::string GetName(std::string_view component_name, size_t rank);

    std::string component_name_;
    size_t rank_ {0};
    std::string name_;
    panda_file::Type::TypeId type_id_ {panda_file::Type::TypeId::VOID};
};

}  // namespace panda::pandasm

namespace std {

template <>
class hash<panda::pandasm::Type> {
public:
    size_t operator()(const panda::pandasm::Type &type) const
    {
        return std::hash<std::string>()(type.GetName());
    }
};

}  // namespace std

#endif  // PANDA_ASSEMBLER_ASSEMBLY_TYPE_H_
