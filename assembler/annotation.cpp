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

#include "annotation.h"

namespace panda::pandasm {

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_SIZE)
std::unique_ptr<ScalarValue> InitScalarValue(const ScalarValue &sc_val)
{
    std::unique_ptr<ScalarValue> copy_val;
    switch (sc_val.GetType()) {
        case Value::Type::U1: {
            copy_val = std::make_unique<ScalarValue>(ScalarValue::Create<Value::Type::U1>(sc_val.GetValue<uint8_t>()));
            break;
        }
        case Value::Type::U8: {
            copy_val = std::make_unique<ScalarValue>(ScalarValue::Create<Value::Type::U8>(sc_val.GetValue<uint8_t>()));
            break;
        }
        case Value::Type::U16: {
            copy_val =
                std::make_unique<ScalarValue>(ScalarValue::Create<Value::Type::U16>(sc_val.GetValue<uint16_t>()));
            break;
        }
        case Value::Type::U32: {
            copy_val =
                std::make_unique<ScalarValue>(ScalarValue::Create<Value::Type::U32>(sc_val.GetValue<uint32_t>()));
            break;
        }
        case Value::Type::U64: {
            copy_val =
                std::make_unique<ScalarValue>(ScalarValue::Create<Value::Type::U64>(sc_val.GetValue<uint64_t>()));
            break;
        }
        case Value::Type::I8: {
            copy_val = std::make_unique<ScalarValue>(ScalarValue::Create<Value::Type::I8>(sc_val.GetValue<int8_t>()));
            break;
        }
        case Value::Type::I16: {
            copy_val = std::make_unique<ScalarValue>(ScalarValue::Create<Value::Type::I16>(sc_val.GetValue<int16_t>()));
            break;
        }
        case Value::Type::I32: {
            copy_val = std::make_unique<ScalarValue>(ScalarValue::Create<Value::Type::I32>(sc_val.GetValue<int32_t>()));
            break;
        }
        case Value::Type::I64: {
            copy_val = std::make_unique<ScalarValue>(ScalarValue::Create<Value::Type::I64>(sc_val.GetValue<int64_t>()));
            break;
        }
        case Value::Type::F32: {
            copy_val = std::make_unique<ScalarValue>(ScalarValue::Create<Value::Type::F32>(sc_val.GetValue<float>()));
            break;
        }
        case Value::Type::F64: {
            copy_val = std::make_unique<ScalarValue>(ScalarValue::Create<Value::Type::F64>(sc_val.GetValue<double>()));
            break;
        }
        case Value::Type::STRING: {
            copy_val =
                std::make_unique<ScalarValue>(ScalarValue::Create<Value::Type::STRING>(sc_val.GetValue<std::string>()));
            break;
        }
        case Value::Type::STRING_NULLPTR: {
            copy_val = std::make_unique<ScalarValue>(
                ScalarValue::Create<Value::Type::STRING_NULLPTR>(sc_val.GetValue<int32_t>()));
            break;
        }
        case Value::Type::RECORD: {
            copy_val = std::make_unique<ScalarValue>(ScalarValue::Create<Value::Type::RECORD>(sc_val.GetValue<Type>()));
            break;
        }
        case Value::Type::METHOD: {
            copy_val =
                std::make_unique<ScalarValue>(ScalarValue::Create<Value::Type::METHOD>(sc_val.GetValue<std::string>()));
            break;
        }
        case Value::Type::ENUM: {
            copy_val =
                std::make_unique<ScalarValue>(ScalarValue::Create<Value::Type::ENUM>(sc_val.GetValue<std::string>()));
            break;
        }
        case Value::Type::ANNOTATION: {
            copy_val = std::make_unique<ScalarValue>(
                ScalarValue::Create<Value::Type::ANNOTATION>(sc_val.GetValue<AnnotationData>()));
            break;
        }
        default: {
            UNREACHABLE();
            copy_val = nullptr;
            break;
        }
    }
    return copy_val;
}

std::unique_ptr<Value> making_value(const AnnotationElement &ann_elem)
{
    std::unique_ptr<Value> copy_val;
    switch (ann_elem.GetValue()->GetType()) {
        case Value::Type::U1:
        case Value::Type::U8:
        case Value::Type::U16:
        case Value::Type::U32:
        case Value::Type::U64:
        case Value::Type::I8:
        case Value::Type::I16:
        case Value::Type::I32:
        case Value::Type::I64:
        case Value::Type::F32:
        case Value::Type::F64:
        case Value::Type::STRING:
        case Value::Type::STRING_NULLPTR:
        case Value::Type::RECORD:
        case Value::Type::METHOD:
        case Value::Type::ENUM:
        case Value::Type::ANNOTATION: {
            copy_val = InitScalarValue(*static_cast<ScalarValue *>(ann_elem.GetValue()));
            break;
        }
        case Value::Type::ARRAY: {
            Value::Type c_type;
            auto *elem_arr = static_cast<ArrayValue *>(ann_elem.GetValue());
            if (elem_arr->GetValues().size() == 0) {
                c_type = Value::Type::VOID;
            } else {
                c_type = elem_arr->GetValues().front().GetType();
            }
            std::vector<ScalarValue> sc_vals;
            for (const auto &sc_val : elem_arr->GetValues()) {
                sc_vals.push_back(*InitScalarValue(sc_val));
            }
            copy_val = std::make_unique<ArrayValue>(c_type, std::move(sc_vals));
            break;
        }
        default: {
            UNREACHABLE();
            copy_val = nullptr;
            break;
        }
    }
    return copy_val;
}

AnnotationElement::AnnotationElement(const AnnotationElement &ann_elem)
{
    this->value_ = making_value(ann_elem);
    this->name_ = ann_elem.GetName();
}

AnnotationElement &AnnotationElement::operator=(const AnnotationElement &ann_elem)
{
    if (this == &ann_elem) {
        return *this;
    }

    this->value_ = making_value(ann_elem);
    this->name_ = ann_elem.GetName();
    return *this;
}

ScalarValue *Value::GetAsScalar()
{
    ASSERT(!IsArray());
    return static_cast<ScalarValue *>(this);
}

const ScalarValue *Value::GetAsScalar() const
{
    ASSERT(!IsArray());
    return static_cast<const ScalarValue *>(this);
}

ArrayValue *Value::GetAsArray()
{
    ASSERT(IsArray());
    return static_cast<ArrayValue *>(this);
}

const ArrayValue *Value::GetAsArray() const
{
    ASSERT(IsArray());
    return static_cast<const ArrayValue *>(this);
}

/* static */
std::string AnnotationElement::TypeToString(Value::Type type)
{
    switch (type) {
        case Value::Type::U1:
            return "u1";
        case Value::Type::I8:
            return "i8";
        case Value::Type::U8:
            return "u8";
        case Value::Type::I16:
            return "i16";
        case Value::Type::U16:
            return "u16";
        case Value::Type::I32:
            return "i32";
        case Value::Type::U32:
            return "u32";
        case Value::Type::I64:
            return "i64";
        case Value::Type::U64:
            return "u64";
        case Value::Type::F32:
            return "f32";
        case Value::Type::F64:
            return "f64";
        case Value::Type::STRING:
            return "string";
        case Value::Type::RECORD:
            return "class";
        case Value::Type::METHOD:
            return "method";
        case Value::Type::ENUM:
            return "enum";
        case Value::Type::ANNOTATION:
            return "annotation";
        case Value::Type::ARRAY:
            return "array";
        case Value::Type::VOID:
            return "void";
        default: {
            UNREACHABLE();
            return "unknown";
        }
    }
}

}  // namespace panda::pandasm
