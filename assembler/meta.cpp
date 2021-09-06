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

#include "meta.h"

#include <stdlib.h>

#include <algorithm>
#include <limits>

#include "utils/expected.h"

namespace panda::pandasm {

std::optional<Metadata::Error> Metadata::ValidateSize(std::string_view value) const
{
    constexpr size_t SIZE = 10;

    if (!std::all_of(value.cbegin(), value.cend(), ::isdigit)) {
        return Error("Unsigned interger value expected", Error::Type::INVALID_VALUE);
    }

    strtoul(value.data(), nullptr, SIZE);
    if (errno == ERANGE) {
        return Error("Value is out of range", Error::Type::INVALID_VALUE);
    }

    return {};
}

bool ItemMetadata::IsForeign() const
{
    return GetAttribute("external");
}

static panda::pandasm::Value::Type GetType(std::string_view value)
{
    using VType = panda::pandasm::Value::Type;
    static std::unordered_map<std::string_view, VType> types {
        {"u1", VType::U1},        {"i8", VType::I8},        {"u8", VType::U8},
        {"i16", VType::I16},      {"u16", VType::U16},      {"i32", VType::I32},
        {"u32", VType::U32},      {"i64", VType::I64},      {"u64", VType::U64},
        {"f32", VType::F32},      {"f64", VType::F64},      {"string", VType::STRING},
        {"class", VType::RECORD}, {"enum", VType::ENUM},    {"annotation", VType::ANNOTATION},
        {"array", VType::ARRAY},  {"method", VType::METHOD}};

    return types[value];
}

template <class T>
static T ConvertFromString(std::string_view value, char **end)
{
    static_assert(std::is_integral_v<T>, "T must be integral type");

    constexpr T MIN = std::numeric_limits<T>::min();
    constexpr T MAX = std::numeric_limits<T>::max();

    if constexpr (std::is_signed_v<T>) {
        auto v = ConvertFromString<int64_t>(value, end);
        if (v < MIN || v > MAX) {
            errno = ERANGE;
        }
        return static_cast<T>(v);
    }

    if constexpr (!std::is_signed_v<T>) {
        auto v = ConvertFromString<uint64_t>(value, end);
        if (v < MIN || v > MAX) {
            errno = ERANGE;
        }
        return static_cast<T>(v);
    }
}

template <>
int64_t ConvertFromString(std::string_view value, char **end)
{
    return static_cast<int64_t>(strtoll(value.data(), end, 0));
}

template <>
uint64_t ConvertFromString(std::string_view value, char **end)
{
    return static_cast<uint64_t>(strtoull(value.data(), end, 0));
}

template <>
float ConvertFromString(std::string_view value, char **end)
{
    return strtof(value.data(), end);
}

template <>
double ConvertFromString(std::string_view value, char **end)
{
    return strtod(value.data(), end);
}

template <class T>
static Expected<T, Metadata::Error> ConvertFromString(std::string_view value)
{
    static_assert(std::is_arithmetic_v<T>, "T must be arithmetic type");

    char *end = nullptr;
    auto v = ConvertFromString<T>(value, &end);

    if (end != value.data() + value.length()) {
        return Unexpected(Metadata::Error("Excepted integer literal", Metadata::Error::Type::INVALID_VALUE));
    }

    if (errno == ERANGE) {
        errno = 0;
        return Unexpected(Metadata::Error("Value is out of range", Metadata::Error::Type::INVALID_VALUE));
    }

    return static_cast<T>(v);
}

template <Value::Type type, class T = ValueTypeHelperT<type>>
static Expected<ScalarValue, Metadata::Error> CreatePrimitiveValue(std::string_view value,
                                                                   T max_value = std::numeric_limits<T>::max())
{
    auto res = ConvertFromString<T>(value);
    if (!res) {
        return Unexpected(res.Error());
    }

    auto converted = res.Value();
    if (converted > max_value) {
        return Unexpected(Metadata::Error("Value is out of range", Metadata::Error::Type::INVALID_VALUE));
    }

    return ScalarValue::Create<type>(converted);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_SIZE)
static Expected<ScalarValue, Metadata::Error> CreateValue(
    Value::Type type, std::string_view value,
    const std::unordered_map<std::string, std::unique_ptr<AnnotationData>> &annotation_id_map = {})
{
    switch (type) {
        case Value::Type::U1: {
            return CreatePrimitiveValue<Value::Type::U1>(value, 1);
        }
        case Value::Type::I8: {
            return CreatePrimitiveValue<Value::Type::I8>(value);
        }
        case Value::Type::U8: {
            return CreatePrimitiveValue<Value::Type::U8>(value);
        }
        case Value::Type::I16: {
            return CreatePrimitiveValue<Value::Type::I16>(value);
        }
        case Value::Type::U16: {
            return CreatePrimitiveValue<Value::Type::U16>(value);
        }
        case Value::Type::I32: {
            return CreatePrimitiveValue<Value::Type::I32>(value);
        }
        case Value::Type::U32: {
            return CreatePrimitiveValue<Value::Type::U32>(value);
        }
        case Value::Type::I64: {
            return CreatePrimitiveValue<Value::Type::I64>(value);
        }
        case Value::Type::U64: {
            return CreatePrimitiveValue<Value::Type::U64>(value);
        }
        case Value::Type::F32: {
            return CreatePrimitiveValue<Value::Type::F32>(value);
        }
        case Value::Type::F64: {
            return CreatePrimitiveValue<Value::Type::F64>(value);
        }
        case Value::Type::STRING: {
            return ScalarValue::Create<Value::Type::STRING>(value);
        }
        case Value::Type::RECORD: {
            return ScalarValue::Create<Value::Type::RECORD>(Type::FromName(value));
        }
        case Value::Type::METHOD: {
            return ScalarValue::Create<Value::Type::METHOD>(value);
        }
        case Value::Type::ENUM: {
            return ScalarValue::Create<Value::Type::ENUM>(value);
        }
        case Value::Type::ANNOTATION: {
            auto it = annotation_id_map.find(std::string(value));
            if (it == annotation_id_map.cend()) {
                return Unexpected(Metadata::Error("Unknown annotation id", Metadata::Error::Type::INVALID_VALUE));
            }

            auto annotation_value = it->second.get();
            return ScalarValue::Create<Value::Type::ANNOTATION>(*annotation_value);
        }
        default: {
            break;
        }
    }

    UNREACHABLE();
}

std::optional<Metadata::Error> AnnotationMetadata::AnnotationElementBuilder::AddValue(
    std::string_view value, const std::unordered_map<std::string, std::unique_ptr<AnnotationData>> &annotation_id_map)
{
    ASSERT(type_.has_value());

    auto type = type_.value();
    if (type == Value::Type::ARRAY) {
        ASSERT(component_type_.has_value());
        type = component_type_.value();
    }

    auto res = CreateValue(type, value, annotation_id_map);
    if (!res) {
        return res.Error();
    }

    values_.push_back(res.Value());

    return {};
}

std::optional<Metadata::Error> AnnotationMetadata::Store(std::string_view attribute)
{
    if (IsParseAnnotationElement() && !annotation_element_builder_.IsCompleted()) {
        return Error(std::string("Unexpected attribute '").append(attribute) +
                         "'. Annotation element isn't completely defined",
                     Error::Type::UNEXPECTED_ATTRIBUTE);
    }

    if (IsParseAnnotation()) {
        ResetAnnotationBuilder();
    }

    return Metadata::Store(attribute);
}

std::optional<Metadata::Error> AnnotationMetadata::MeetExpRecordAttribute(std::string_view attribute,
                                                                          std::string_view value)
{
    if (IsParseAnnotationElement() && !annotation_element_builder_.IsCompleted()) {
        return Error(std::string("Unexpected attribute '").append(attribute) +
                         "'. Annotation element isn't completely defined",
                     Error::Type::UNEXPECTED_ATTRIBUTE);
    }

    InitializeAnnotationBuilder(value);

    return {};
}

std::optional<Metadata::Error> AnnotationMetadata::MeetExpIdAttribute(std::string_view attribute,
                                                                      std::string_view value)
{
    if (!IsParseAnnotation() || IsParseAnnotationElement()) {
        return Error(std::string("Unexpected attribute '").append(attribute) +
                         "'. Annotation record attribute must be defined first",
                     Error::Type::UNEXPECTED_ATTRIBUTE);
    }

    if (annotation_builder_.HasId()) {
        return Error(std::string("Unexpected attribute '").append(attribute) +
                         "'. Annotation id attribute already defined",
                     Error::Type::UNEXPECTED_ATTRIBUTE);
    }

    annotation_builder_.SetId(value);

    return {};
}

std::optional<Metadata::Error> AnnotationMetadata::MeetExpElementNameAttribute(std::string_view attribute,
                                                                               std::string_view value)
{
    if (!IsParseAnnotation()) {
        return Error(std::string("Unexpected attribute '").append(attribute) +
                         "'. Annotation record attribute must be defined first",
                     Error::Type::UNEXPECTED_ATTRIBUTE);
    }

    if (IsParseAnnotationElement() && !annotation_element_builder_.IsCompleted()) {
        return Error(std::string("Unexpected attribute '").append(attribute) +
                         "'. Previous annotation element isn't defined completely",
                     Error::Type::UNEXPECTED_ATTRIBUTE);
    }

    InitializeAnnotationElementBuilder(value);

    return {};
}

std::optional<Metadata::Error> AnnotationMetadata::MeetExpElementTypeAttribute(std::string_view attribute,
                                                                               std::string_view value)
{
    if (!IsParseAnnotationElement()) {
        return Error(std::string("Unexpected attribute '").append(attribute) +
                         "'. Annotation element name attribute must be defined first",
                     Error::Type::UNEXPECTED_ATTRIBUTE);
    }

    if (annotation_element_builder_.IsTypeSet()) {
        return Error(std::string("Unexpected attribute '").append(attribute) +
                         "'. Annotation element type attribute already defined",
                     Error::Type::UNEXPECTED_ATTRIBUTE);
    }

    annotation_element_builder_.SetType(GetType(value));

    return {};
}

std::optional<Metadata::Error> AnnotationMetadata::MeetExpElementArrayComponentTypeAttribute(std::string_view attribute,
                                                                                             std::string_view value)
{
    if (!IsParseAnnotationElement()) {
        return Error(std::string("Unexpected attribute '").append(attribute) +
                         "'. Annotation element name attribute must be defined first",
                     Error::Type::UNEXPECTED_ATTRIBUTE);
    }

    if (!annotation_element_builder_.IsArray()) {
        return Error(std::string("Unexpected attribute '").append(attribute) + "'. Annotation element type isn't array",
                     Error::Type::UNEXPECTED_ATTRIBUTE);
    }

    if (annotation_element_builder_.IsComponentTypeSet()) {
        return Error(std::string("Unexpected attribute '").append(attribute) +
                         "'. Annotation element array component type attribute already defined",
                     Error::Type::UNEXPECTED_ATTRIBUTE);
    }

    annotation_element_builder_.SetComponentType(GetType(value));

    return {};
}

std::optional<Metadata::Error> AnnotationMetadata::MeetExpElementValueAttribute(std::string_view attribute,
                                                                                std::string_view value)
{
    if (!IsParseAnnotationElement()) {
        return Error(std::string("Unexpected attribute '").append(attribute) +
                         "'. Annotation element name attribute must be defined first",
                     Error::Type::UNEXPECTED_ATTRIBUTE);
    }

    if (!annotation_element_builder_.IsTypeSet()) {
        return Error(std::string("Unexpected attribute '").append(attribute) +
                         "'. Annotation element type attribute isn't defined",
                     Error::Type::UNEXPECTED_ATTRIBUTE);
    }

    if (annotation_element_builder_.IsArray() && !annotation_element_builder_.IsComponentTypeSet()) {
        return Error(std::string("Unexpected attribute '").append(attribute) +
                         "'. Annotation element array component type attribute isn't defined",
                     Error::Type::UNEXPECTED_ATTRIBUTE);
    }

    if (!annotation_element_builder_.IsArray() && annotation_element_builder_.IsCompleted()) {
        return Error(std::string("Unexpected attribute '").append(attribute) +
                         "'. Annotation element is completely defined",
                     Error::Type::UNEXPECTED_ATTRIBUTE);
    }

    return annotation_element_builder_.AddValue(value, id_map_);
}

std::optional<Metadata::Error> AnnotationMetadata::StoreValue(std::string_view attribute, std::string_view value)
{
    auto err = Metadata::StoreValue(attribute, value);
    if (err) {
        return err;
    }

    if (IsAnnotationRecordAttribute(attribute)) {
        return MeetExpRecordAttribute(attribute, value);
    }

    if (IsAnnotationIdAttribute(attribute)) {
        return MeetExpIdAttribute(attribute, value);
    }

    if (IsAnnotationElementNameAttribute(attribute)) {
        return MeetExpElementNameAttribute(attribute, value);
    }

    if (IsAnnotationElementTypeAttribute(attribute)) {
        return MeetExpElementTypeAttribute(attribute, value);
    }

    if (IsAnnotationElementArrayComponentTypeAttribute(attribute)) {
        return MeetExpElementArrayComponentTypeAttribute(attribute, value);
    }

    if (IsAnnotationElementValueAttribute(attribute)) {
        return MeetExpElementValueAttribute(attribute, value);
    }

    if (IsParseAnnotationElement() && !annotation_element_builder_.IsCompleted()) {
        return Error(std::string("Unexpected attribute '").append(attribute) +
                         "'. Annotation element isn't completely defined",
                     Error::Type::UNEXPECTED_ATTRIBUTE);
    }

    if (IsParseAnnotation()) {
        ResetAnnotationBuilder();
    }

    return {};
}

std::optional<Metadata::Error> AnnotationMetadata::ValidateData()
{
    if (IsParseAnnotationElement() && !annotation_element_builder_.IsCompleted()) {
        return Error("Annotation element isn't completely defined", Error::Type::MISSING_ATTRIBUTE);
    }

    if (IsParseAnnotation()) {
        ResetAnnotationBuilder();
    }

    return Metadata::ValidateData();
}

std::string RecordMetadata::GetBase() const
{
    return "";
}

std::vector<std::string> RecordMetadata::GetInterfaces() const
{
    return {};
}

bool RecordMetadata::IsAnnotation() const
{
    return false;
}

bool RecordMetadata::IsRuntimeAnnotation() const
{
    return false;
}

bool RecordMetadata::IsTypeAnnotation() const
{
    return false;
}

bool RecordMetadata::IsRuntimeTypeAnnotation() const
{
    return false;
}

bool FunctionMetadata::HasImplementation() const
{
    return !(ACC_ABSTRACT & GetAccessFlags()) && !(ACC_NATIVE & GetAccessFlags());
}

bool FunctionMetadata::IsCtor() const
{
    return GetAttribute("ctor");
}

bool FunctionMetadata::IsCctor() const
{
    return GetAttribute("cctor");
}

std::optional<Metadata::Error> FieldMetadata::StoreValue(std::string_view attribute, std::string_view value)
{
    auto err = ItemMetadata::StoreValue(attribute, value);
    if (err) {
        return err;
    }

    if (IsValueAttribute(attribute)) {
        Value::Type value_type;
        if (!field_type_.IsObject()) {
            value_type = GetType(field_type_.GetName());
        } else {
            value_type = Value::Type::STRING;
        }

        auto res = CreateValue(value_type, value);
        if (!res) {
            return res.Error();
        }

        value_ = res.Value();
    }

    return {};
}

#include <meta_gen.h>

}  // namespace panda::pandasm
