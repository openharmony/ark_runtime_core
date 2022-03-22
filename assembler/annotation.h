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

#ifndef PANDA_ASSEMBLER_ANNOTATION_H_
#define PANDA_ASSEMBLER_ANNOTATION_H_

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include "assembly-type.h"
#include "macros.h"

namespace panda::pandasm {

class AnnotationElement;

class AnnotationData {
public:
    AnnotationData(std::string_view record_name, std::vector<AnnotationElement> elements)
        : record_name_(record_name), elements_(std::move(elements))
    {
    }

    explicit AnnotationData(std::string_view record_name) : record_name_(record_name) {}

    DEFAULT_MOVE_SEMANTIC(AnnotationData);
    DEFAULT_COPY_SEMANTIC(AnnotationData);

    ~AnnotationData() = default;

    std::string GetName() const
    {
        return record_name_;
    }

    const std::vector<AnnotationElement> &GetElements() const
    {
        return elements_;
    }

    void AddElement(AnnotationElement &&element)
    {
        elements_.push_back(std::forward<AnnotationElement>(element));
    }

private:
    std::string record_name_;
    std::vector<AnnotationElement> elements_;
};

class ScalarValue;
class ArrayValue;

class Value {
public:
    enum class Type {
        U1,
        I8,
        U8,
        I16,
        U16,
        I32,
        U32,
        I64,
        U64,
        F32,
        F64,
        STRING,
        STRING_NULLPTR,
        RECORD,
        METHOD,
        ENUM,
        ANNOTATION,
        ARRAY,
        VOID,
        METHOD_HANDLE,
        UNKNOWN
    };

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_SIZE)
    static constexpr char GetTypeAsChar(Type t)
    {
        char type = '0';
        switch (t) {
            case Type::U1:
                type = '1';
                break;
            case Type::I8:
                type = '2';
                break;
            case Type::U8:
                type = '3';
                break;
            case Type::I16:
                type = '4';
                break;
            case Type::U16:
                type = '5';
                break;
            case Type::I32:
                type = '6';
                break;
            case Type::U32:
                type = '7';
                break;
            case Type::I64:
                type = '8';
                break;
            case Type::U64:
                type = '9';
                break;
            case Type::F32:
                type = 'A';
                break;
            case Type::F64:
                type = 'B';
                break;
            case Type::STRING:
                type = 'C';
                break;
            case Type::RECORD:
                type = 'D';
                break;
            case Type::METHOD:
                type = 'E';
                break;
            case Type::ENUM:
                type = 'F';
                break;
            case Type::ANNOTATION:
                type = 'G';
                break;
            case Type::ARRAY:
                type = 'H';
                break;
            case Type::VOID:
                type = 'I';
                break;
            case Type::METHOD_HANDLE:
                type = 'J';
                break;
            case Type::STRING_NULLPTR:
                type = '*';
                break;
            case Type::UNKNOWN:
            default:
                type = '0';
                break;
        }
        return type;
    }

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_SIZE)
    static constexpr char GetArrayTypeAsChar(Type t)
    {
        char type = '0';
        switch (t) {
            case Type::U1:
                type = 'K';
                break;
            case Type::I8:
                type = 'L';
                break;
            case Type::U8:
                type = 'M';
                break;
            case Type::I16:
                type = 'N';
                break;
            case Type::U16:
                type = 'O';
                break;
            case Type::I32:
                type = 'P';
                break;
            case Type::U32:
                type = 'Q';
                break;
            case Type::I64:
                type = 'R';
                break;
            case Type::U64:
                type = 'S';
                break;
            case Type::F32:
                type = 'T';
                break;
            case Type::F64:
                type = 'U';
                break;
            case Type::STRING:
                type = 'V';
                break;
            case Type::RECORD:
                type = 'W';
                break;
            case Type::METHOD:
                type = 'X';
                break;
            case Type::ENUM:
                type = 'Y';
                break;
            case Type::ANNOTATION:
                type = 'Z';
                break;
            case Type::METHOD_HANDLE:
                type = '@';
                break;
            case Type::UNKNOWN:
            default:
                type = '0';
                break;
        }
        return type;
    }

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_SIZE)
    static constexpr Type GetCharAsType(char c)
    {
        Type type = Type::UNKNOWN;
        switch (c) {
            case '1':
                type = Type::U1;
                break;
            case '2':
                type = Type::I8;
                break;
            case '3':
                type = Type::U8;
                break;
            case '4':
                type = Type::I16;
                break;
            case '5':
                type = Type::U16;
                break;
            case '6':
                type = Type::I32;
                break;
            case '7':
                type = Type::U32;
                break;
            case '8':
                type = Type::I64;
                break;
            case '9':
                type = Type::U64;
                break;
            case 'A':
                type = Type::F32;
                break;
            case 'B':
                type = Type::F64;
                break;
            case 'C':
                type = Type::STRING;
                break;
            case 'D':
                type = Type::RECORD;
                break;
            case 'E':
                type = Type::METHOD;
                break;
            case 'F':
                type = Type::ENUM;
                break;
            case 'G':
                type = Type::ANNOTATION;
                break;
            case 'H':
                type = Type::ARRAY;
                break;
            case 'I':
                type = Type::VOID;
                break;
            case 'J':
                type = Type::METHOD_HANDLE;
                break;
            case '*':
                type = Type::STRING_NULLPTR;
                break;
            case '0':
            default:
                type = Type::UNKNOWN;
                break;
        }
        return type;
    }

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_SIZE)
    static constexpr Type GetCharAsArrayType(char c)
    {
        Type type = Type::UNKNOWN;
        switch (c) {
            case 'K':
                type = Type::U1;
                break;
            case 'L':
                type = Type::I8;
                break;
            case 'M':
                type = Type::U8;
                break;
            case 'N':
                type = Type::I16;
                break;
            case 'O':
                type = Type::U16;
                break;
            case 'P':
                type = Type::I32;
                break;
            case 'Q':
                type = Type::U32;
                break;
            case 'R':
                type = Type::I64;
                break;
            case 'S':
                type = Type::U64;
                break;
            case 'T':
                type = Type::F32;
                break;
            case 'U':
                type = Type::F64;
                break;
            case 'V':
                type = Type::STRING;
                break;
            case 'W':
                type = Type::RECORD;
                break;
            case 'X':
                type = Type::METHOD;
                break;
            case 'Y':
                type = Type::ENUM;
                break;
            case 'Z':
                type = Type::ANNOTATION;
                break;
            case '@':
                type = Type::METHOD_HANDLE;
                break;
            case '0':
            default:
                type = Type::UNKNOWN;
                break;
        }
        return type;
    }

    Type GetType() const
    {
        return type_;
    }

    bool IsArray() const
    {
        return type_ == Type::ARRAY;
    }

    ScalarValue *GetAsScalar();

    const ScalarValue *GetAsScalar() const;

    ArrayValue *GetAsArray();

    const ArrayValue *GetAsArray() const;

    virtual ~Value() = default;

    DEFAULT_COPY_SEMANTIC(Value);
    DEFAULT_MOVE_SEMANTIC(Value);

protected:
    explicit Value(Type type) : type_(type) {}

private:
    Type type_;
};

// clang-format off

template <Value::Type value_type>
struct ValueTypeHelper {
    // Disable checks due to clang-tidy bug https://bugs.llvm.org/show_bug.cgi?id=40640
    // NOLINTNEXTLINE(readability-magic-numbers)
    using type = std::conditional_t<value_type == Value::Type::U1, uint8_t,
                // NOLINTNEXTLINE(readability-magic-numbers)
                std::conditional_t<value_type == Value::Type::I8, int8_t,
                // NOLINTNEXTLINE(readability-magic-numbers)
                std::conditional_t<value_type == Value::Type::U8, uint8_t,
                // NOLINTNEXTLINE(readability-magic-numbers)
                std::conditional_t<value_type == Value::Type::I16, int16_t,
                // NOLINTNEXTLINE(readability-magic-numbers)
                std::conditional_t<value_type == Value::Type::U16, uint16_t,
                // NOLINTNEXTLINE(readability-magic-numbers)
                std::conditional_t<value_type == Value::Type::I32, int32_t,
                // NOLINTNEXTLINE(readability-magic-numbers)
                std::conditional_t<value_type == Value::Type::U32, uint32_t,
                // NOLINTNEXTLINE(readability-magic-numbers)
                std::conditional_t<value_type == Value::Type::I64, int64_t,
                // NOLINTNEXTLINE(readability-magic-numbers)
                std::conditional_t<value_type == Value::Type::U64, uint64_t,
                // NOLINTNEXTLINE(readability-magic-numbers)
                std::conditional_t<value_type == Value::Type::F32, float,
                // NOLINTNEXTLINE(readability-magic-numbers)
                std::conditional_t<value_type == Value::Type::F64, double,
                // NOLINTNEXTLINE(readability-magic-numbers)
                std::conditional_t<value_type == Value::Type::STRING, std::string_view,
                // NOLINTNEXTLINE(readability-magic-numbers)
                std::conditional_t<value_type == Value::Type::STRING_NULLPTR, uint32_t,
                // NOLINTNEXTLINE(readability-magic-numbers)
                std::conditional_t<value_type == Value::Type::RECORD, pandasm::Type,
                // NOLINTNEXTLINE(readability-magic-numbers)
                std::conditional_t<value_type == Value::Type::METHOD, std::string_view,
                // NOLINTNEXTLINE(readability-magic-numbers)
                std::conditional_t<value_type == Value::Type::ENUM, std::string_view,
                // NOLINTNEXTLINE(readability-magic-numbers)
                std::conditional_t<value_type == Value::Type::ANNOTATION, AnnotationData,
                void>>>>>>>>>>>>>>>>>;
};

// clang-format on

template <Value::Type type>
using ValueTypeHelperT = typename ValueTypeHelper<type>::type;

class ScalarValue : public Value {
public:
    template <Value::Type type>
    // Disable checks due to clang-tidy bug https://bugs.llvm.org/show_bug.cgi?id=40640
    // NOLINTNEXTLINE(readability-magic-numbers)
    static ScalarValue Create(ValueTypeHelperT<type> value)
    {
        // NOLINTNEXTLINE(readability-magic-numbers)
        using T = ValueTypeHelperT<type>;
        // Disable checks due to clang-tidy bug https://bugs.llvm.org/show_bug.cgi?id=32203
        // NOLINTNEXTLINE(readability-braces-around-statements, hicpp-braces-around-statements)
        if constexpr (std::is_integral_v<T>) {  // NOLINT(bugprone-suspicious-semicolon)
            // NOLINTNEXTLINE(readability-magic-numbers)
            return ScalarValue(type, static_cast<uint64_t>(value));
        }

        // NOLINTNEXTLINE(readability-braces-around-statements, hicpp-braces-around-statements)
        if constexpr (!std::is_integral_v<T>) {  // NOLINT(bugprone-suspicious-semicolon)
            // NOLINTNEXTLINE(readability-magic-numbers)
            return ScalarValue(type, value);
        }
    }

    template <class T>
    T GetValue() const
    {
        // Disable checks due to clang-tidy bug https://bugs.llvm.org/show_bug.cgi?id=32203
        // NOLINTNEXTLINE(readability-braces-around-statements, hicpp-braces-around-statements)
        if constexpr (std::is_integral_v<T>) {  // NOLINT(bugprone-suspicious-semicolon)
            return static_cast<T>(std::get<uint64_t>(value_));
        }

        // NOLINTNEXTLINE(readability-braces-around-statements, hicpp-braces-around-statements)
        if constexpr (!std::is_integral_v<T>) {  // NOLINT(bugprone-suspicious-semicolon)
            return std::get<T>(value_);
        }
    }

    DEFAULT_MOVE_SEMANTIC(ScalarValue);
    DEFAULT_COPY_SEMANTIC(ScalarValue);

    ~ScalarValue() override = default;

private:
    ScalarValue(Type type, uint64_t value) : Value(type), value_(value) {}

    ScalarValue(Type type, float value) : Value(type), value_(value) {}

    ScalarValue(Type type, double value) : Value(type), value_(value) {}

    ScalarValue(Type type, std::string_view value) : Value(type), value_(std::string(value)) {}

    ScalarValue(Type type, pandasm::Type value) : Value(type), value_(std::move(value)) {}

    ScalarValue(Type type, const AnnotationData &value) : Value(type), value_(value) {}

    std::variant<uint64_t, float, double, std::string, pandasm::Type, AnnotationData> value_;
};

class ArrayValue : public Value {
public:
    ArrayValue(Type component_type, std::vector<ScalarValue> values)
        : Value(Type::ARRAY), component_type_(component_type), values_(std::move(values))
    {
    }

    DEFAULT_MOVE_SEMANTIC(ArrayValue);
    DEFAULT_COPY_SEMANTIC(ArrayValue);

    ~ArrayValue() override = default;

    const std::vector<ScalarValue> &GetValues() const
    {
        return values_;
    }

    Type GetComponentType() const
    {
        return component_type_;
    }

private:
    Type component_type_;
    std::vector<ScalarValue> values_;
};

class AnnotationElement {
public:
    AnnotationElement(std::string_view name, std::unique_ptr<Value> value) : name_(name), value_(std::move(value)) {}

    AnnotationElement(const AnnotationElement &ann_elem);
    AnnotationElement &operator=(const AnnotationElement &ann_elem);
    DEFAULT_MOVE_SEMANTIC(AnnotationElement);
    ~AnnotationElement() = default;

    std::string GetName() const
    {
        return name_;
    }

    Value *GetValue() const
    {
        return value_.get();
    }

    static std::string TypeToString(Value::Type type);

private:
    std::string name_;
    std::unique_ptr<Value> value_;
};

}  // namespace panda::pandasm

#endif  // PANDA_ASSEMBLER_ANNOTATION_H_
