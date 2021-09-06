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

#include "assembly-emitter.h"

#include <cctype>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>

#include "bytecode_instruction-inl.h"
#include "file_items.h"
#include "file_writer.h"
#include "mangling.h"
#include "os/file.h"

namespace {

using panda::os::file::Mode;
using panda::os::file::Open;
using panda::panda_file::AnnotationItem;
using panda::panda_file::ArrayValueItem;
using panda::panda_file::BaseClassItem;
using panda::panda_file::BaseFieldItem;
using panda::panda_file::BaseMethodItem;
using panda::panda_file::ClassItem;
using panda::panda_file::CodeItem;
using panda::panda_file::DebugInfoItem;
using panda::panda_file::FieldItem;
using panda::panda_file::FileWriter;
using panda::panda_file::ForeignClassItem;
using panda::panda_file::ForeignFieldItem;
using panda::panda_file::ForeignMethodItem;
using panda::panda_file::ItemContainer;
using panda::panda_file::LineNumberProgramItem;
using panda::panda_file::MemoryBufferWriter;
using panda::panda_file::MethodHandleItem;
using panda::panda_file::MethodItem;
using panda::panda_file::MethodParamItem;
using panda::panda_file::ParamAnnotationsItem;
using panda::panda_file::PrimitiveTypeItem;
using panda::panda_file::ProtoItem;
using panda::panda_file::ScalarValueItem;
using panda::panda_file::StringItem;
using panda::panda_file::Type;
using panda::panda_file::TypeItem;
using panda::panda_file::ValueItem;
using panda::panda_file::Writer;

std::unordered_map<Type::TypeId, PrimitiveTypeItem *> CreatePrimitiveTypes(ItemContainer *container)
{
    auto res = std::unordered_map<Type::TypeId, PrimitiveTypeItem *> {};
    res.insert({Type::TypeId::VOID, container->CreateItem<PrimitiveTypeItem>(Type::TypeId::VOID)});
    res.insert({Type::TypeId::U1, container->CreateItem<PrimitiveTypeItem>(Type::TypeId::U1)});
    res.insert({Type::TypeId::I8, container->CreateItem<PrimitiveTypeItem>(Type::TypeId::I8)});
    res.insert({Type::TypeId::U8, container->CreateItem<PrimitiveTypeItem>(Type::TypeId::U8)});
    res.insert({Type::TypeId::I16, container->CreateItem<PrimitiveTypeItem>(Type::TypeId::I16)});
    res.insert({Type::TypeId::U16, container->CreateItem<PrimitiveTypeItem>(Type::TypeId::U16)});
    res.insert({Type::TypeId::I32, container->CreateItem<PrimitiveTypeItem>(Type::TypeId::I32)});
    res.insert({Type::TypeId::U32, container->CreateItem<PrimitiveTypeItem>(Type::TypeId::U32)});
    res.insert({Type::TypeId::I64, container->CreateItem<PrimitiveTypeItem>(Type::TypeId::I64)});
    res.insert({Type::TypeId::U64, container->CreateItem<PrimitiveTypeItem>(Type::TypeId::U64)});
    res.insert({Type::TypeId::F32, container->CreateItem<PrimitiveTypeItem>(Type::TypeId::F32)});
    res.insert({Type::TypeId::F64, container->CreateItem<PrimitiveTypeItem>(Type::TypeId::F64)});
    res.insert({Type::TypeId::TAGGED, container->CreateItem<PrimitiveTypeItem>(Type::TypeId::TAGGED)});
    return res;
}

template <class T>
typename T::mapped_type Find(const T &map, typename T::key_type key)
{
    auto res = map.find(key);
    ASSERT(res != map.end());
    return res->second;
}

}  // anonymous namespace

namespace panda::pandasm {

/* static */
std::string AsmEmitter::last_error("");

static panda_file::Type::TypeId GetTypeId(Value::Type type)
{
    switch (type) {
        case Value::Type::U1:
            return panda_file::Type::TypeId::U1;
        case Value::Type::I8:
            return panda_file::Type::TypeId::I8;
        case Value::Type::U8:
            return panda_file::Type::TypeId::U8;
        case Value::Type::I16:
            return panda_file::Type::TypeId::I16;
        case Value::Type::U16:
            return panda_file::Type::TypeId::U16;
        case Value::Type::I32:
            return panda_file::Type::TypeId::I32;
        case Value::Type::U32:
            return panda_file::Type::TypeId::U32;
        case Value::Type::I64:
            return panda_file::Type::TypeId::I64;
        case Value::Type::U64:
            return panda_file::Type::TypeId::U64;
        case Value::Type::F32:
            return panda_file::Type::TypeId::F32;
        case Value::Type::F64:
            return panda_file::Type::TypeId::F64;
        case Value::Type::VOID:
            return panda_file::Type::TypeId::VOID;
        default:
            return panda_file::Type::TypeId::REFERENCE;
    }
}

/* static */
bool AsmEmitter::CheckValueType(Value::Type value_type, Type type, const Program &program)
{
    auto value_type_id = GetTypeId(value_type);
    if (value_type_id != type.GetId()) {
        SetLastError("Inconsistent element (" + AnnotationElement::TypeToString(value_type) +
                     ") and function's return type (" + type.GetName() + ")");
        return false;
    }

    switch (value_type) {
        case Value::Type::STRING:
        case Value::Type::RECORD:
        case Value::Type::ANNOTATION:
        case Value::Type::ENUM: {
            auto it = program.record_table.find(type.GetName());
            if (it == program.record_table.cend()) {
                SetLastError("Record " + type.GetName() + " not found");
                return false;
            }

            auto &record = it->second;
            if (value_type == Value::Type::ANNOTATION && !record.metadata->IsAnnotation() &&
                !record.metadata->IsRuntimeAnnotation() && !record.metadata->IsRuntimeTypeAnnotation() &&
                !record.metadata->IsTypeAnnotation()) {
                SetLastError("Record " + type.GetName() + " isn't annotation");
                return false;
            }

            if (value_type == Value::Type::ENUM && (record.metadata->GetAccessFlags() & ACC_ENUM) == 0) {
                SetLastError("Record " + type.GetName() + " isn't enum");
                return false;
            }

            break;
        }
        case Value::Type::ARRAY: {
            if (!type.IsArray()) {
                SetLastError("Inconsistent element (" + AnnotationElement::TypeToString(value_type) +
                             ") and function's return type (" + type.GetName() + ")");
                return false;
            }

            break;
        }
        default: {
            break;
        }
    }

    return true;
}

/* static */
// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_SIZE)
panda_file::LiteralItem *AsmEmitter::CreateLiteralItem(
    ItemContainer *container, const Value *value, std::vector<panda_file::LiteralItem> *out,
    const std::unordered_map<std::string, panda_file::BaseMethodItem *> &methods)
{
    auto value_type = value->GetType();

    switch (value_type) {
        case Value::Type::U1:
        case Value::Type::I8:
        case Value::Type::U8: {
            auto v = value->GetAsScalar()->GetValue<uint8_t>();
            out->emplace_back(v);
            return &out->back();
        }
        case Value::Type::I16:
        case Value::Type::U16: {
            auto v = value->GetAsScalar()->GetValue<uint16_t>();
            out->emplace_back(v);
            return &out->back();
        }
        case Value::Type::I32:
        case Value::Type::U32:
        case Value::Type::STRING_NULLPTR: {
            auto v = value->GetAsScalar()->GetValue<uint32_t>();
            out->emplace_back(v);
            return &out->back();
        }
        case Value::Type::I64:
        case Value::Type::U64: {
            auto v = value->GetAsScalar()->GetValue<uint64_t>();
            out->emplace_back(v);
            return &out->back();
        }
        case Value::Type::F32: {
            auto v = bit_cast<uint32_t>(value->GetAsScalar()->GetValue<float>());
            out->emplace_back(v);
            return &out->back();
        }
        case Value::Type::F64: {
            auto v = bit_cast<uint64_t>(value->GetAsScalar()->GetValue<double>());
            out->emplace_back(v);
            return &out->back();
        }
        case Value::Type::STRING: {
            auto *string_item = container->GetOrCreateStringItem(value->GetAsScalar()->GetValue<std::string>());
            out->emplace_back(string_item);
            return &out->back();
        }
        case Value::Type::METHOD: {
            auto name = value->GetAsScalar()->GetValue<std::string>();
            auto method_item = static_cast<panda::panda_file::MethodItem *>(Find(methods, name));
            out->emplace_back(method_item);
            return &out->back();
        }
        default:
            return nullptr;
    }
}

/* static */
bool AsmEmitter::CheckValueRecordCase(const Value *value, const Program &program)
{
    auto t = value->GetAsScalar()->GetValue<Type>();
    if (!t.IsObject()) {
        return true;
    }

    auto record_name = t.GetName();
    bool is_found = true;
    if (t.IsArray()) {
        auto it = program.array_types.find(t);
        is_found = it != program.array_types.cend();
    } else {
        auto it = program.record_table.find(record_name);
        is_found = it != program.record_table.cend();
    }

    if (!is_found) {
        SetLastError("Incorrect value: record " + record_name + " not found");
        return false;
    }

    return true;
}

/* static */
bool AsmEmitter::CheckValueMethodCase(const Value *value, const Program &program)
{
    auto function_name = value->GetAsScalar()->GetValue<std::string>();
    auto it = program.function_table.find(function_name);
    if (it == program.function_table.cend()) {
        SetLastError("Incorrect value: function " + function_name + " not found");
        return false;
    }

    return true;
}

/* static */
bool AsmEmitter::CheckValueEnumCase(const Value *value, Type type, const Program &program)
{
    auto enum_value = value->GetAsScalar()->GetValue<std::string>();
    auto record_name = GetOwnerName(enum_value);
    auto field_name = GetItemName(enum_value);

    if (record_name != type.GetName()) {
        SetLastError("Incorrect value: Expected " + type.GetName() + " enum record");
        return false;
    }

    const auto &record = program.record_table.find(record_name)->second;
    auto it = std::find_if(record.field_list.cbegin(), record.field_list.cend(),
                           [&field_name](const Field &field) { return field.name == field_name; });
    if (it == record.field_list.cend()) {
        SetLastError("Incorrect value: Enum field " + enum_value + " not found");
        return false;
    }

    const auto &field = *it;
    if ((field.metadata->GetAccessFlags() & ACC_ENUM) == 0) {
        SetLastError("Incorrect value: Field " + enum_value + " isn't enum");
        return false;
    }

    return true;
}

/* static */
bool AsmEmitter::CheckValueArrayCase(const Value *value, Type type, const Program &program)
{
    auto component_type = type.GetComponentType();
    auto value_component_type = value->GetAsArray()->GetComponentType();

    if (value_component_type == Value::Type::VOID && value->GetAsArray()->GetValues().empty()) {
        return true;
    }

    if (!CheckValueType(value_component_type, component_type, program)) {
        SetLastError("Incorrect array's component type: " + GetLastError());
        return false;
    }

    for (auto &elem_value : value->GetAsArray()->GetValues()) {
        if (!CheckValue(&elem_value, component_type, program)) {
            SetLastError("Incorrect array's element: " + GetLastError());
            return false;
        }
    }

    return true;
}

/* static */
bool AsmEmitter::CheckValue(const Value *value, Type type, const Program &program)
{
    auto value_type = value->GetType();

    if (!CheckValueType(value_type, type, program)) {
        SetLastError("Incorrect type: " + GetLastError());
        return false;
    }

    switch (value_type) {
        case Value::Type::RECORD: {
            if (!CheckValueRecordCase(value, program)) {
                return false;
            }

            break;
        }
        case Value::Type::METHOD: {
            if (!CheckValueMethodCase(value, program)) {
                return false;
            }

            break;
        }
        case Value::Type::ENUM: {
            if (!CheckValueEnumCase(value, type, program)) {
                return false;
            }

            break;
        }
        case Value::Type::ARRAY: {
            if (!CheckValueArrayCase(value, type, program)) {
                return false;
            }

            break;
        }
        default: {
            break;
        }
    }

    return true;
}

/* static */
ScalarValueItem *AsmEmitter::CreateScalarStringValueItem(ItemContainer *container, const Value *value,
                                                         std::vector<ScalarValueItem> *out)
{
    auto *string_item = container->GetOrCreateStringItem(value->GetAsScalar()->GetValue<std::string>());
    if (out != nullptr) {
        out->emplace_back(string_item);
        return &out->back();
    }

    return container->CreateItem<ScalarValueItem>(string_item);
}

/* static */
ScalarValueItem *AsmEmitter::CreateScalarRecordValueItem(
    ItemContainer *container, const Value *value, std::vector<ScalarValueItem> *out,
    const std::unordered_map<std::string, BaseClassItem *> &classes)
{
    auto type = value->GetAsScalar()->GetValue<Type>();
    BaseClassItem *class_item;
    if (type.IsObject()) {
        auto name = type.GetName();
        auto it = classes.find(name);
        if (it == classes.cend()) {
            return nullptr;
        }

        class_item = it->second;
    } else {
        class_item = container->GetOrCreateForeignClassItem(type.GetDescriptor());
    }

    if (out != nullptr) {
        out->emplace_back(class_item);
        return &out->back();
    }

    return container->CreateItem<ScalarValueItem>(class_item);
}

/* static */
ScalarValueItem *AsmEmitter::CreateScalarMethodValueItem(
    ItemContainer *container, const Value *value, std::vector<ScalarValueItem> *out,
    const std::unordered_map<std::string, BaseMethodItem *> &methods)
{
    auto name = value->GetAsScalar()->GetValue<std::string>();
    auto it = methods.find(name);
    if (it == methods.cend()) {
        return nullptr;
    }

    auto *method_item = it->second;
    if (out != nullptr) {
        out->emplace_back(method_item);
        return &out->back();
    }

    return container->CreateItem<ScalarValueItem>(method_item);
}

/* static */
ScalarValueItem *AsmEmitter::CreateScalarEnumValueItem(ItemContainer *container, const Value *value,
                                                       std::vector<ScalarValueItem> *out,
                                                       const std::unordered_map<std::string, BaseFieldItem *> &fields)
{
    auto name = value->GetAsScalar()->GetValue<std::string>();
    auto it = fields.find(name);
    if (it == fields.cend()) {
        return nullptr;
    }

    auto *field_item = it->second;
    if (out != nullptr) {
        out->emplace_back(field_item);
        return &out->back();
    }

    return container->CreateItem<ScalarValueItem>(field_item);
}

/* static */
ScalarValueItem *AsmEmitter::CreateScalarAnnotationValueItem(
    ItemContainer *container, const Value *value, std::vector<ScalarValueItem> *out, const Program &program,
    const std::unordered_map<std::string, BaseClassItem *> &classes,
    const std::unordered_map<std::string, BaseFieldItem *> &fields,
    const std::unordered_map<std::string, BaseMethodItem *> &methods)
{
    auto annotation = value->GetAsScalar()->GetValue<AnnotationData>();
    auto *annotation_item = CreateAnnotationItem(container, annotation, program, classes, fields, methods);
    if (annotation_item == nullptr) {
        return nullptr;
    }

    if (out != nullptr) {
        out->emplace_back(annotation_item);
        return &out->back();
    }

    return container->CreateItem<ScalarValueItem>(annotation_item);
}

/* static */
ScalarValueItem *AsmEmitter::CreateScalarValueItem(ItemContainer *container, const Value *value,
                                                   std::vector<ScalarValueItem> *out, const Program &program,
                                                   const std::unordered_map<std::string, BaseClassItem *> &classes,
                                                   const std::unordered_map<std::string, BaseFieldItem *> &fields,
                                                   const std::unordered_map<std::string, BaseMethodItem *> &methods)
{
    auto value_type = value->GetType();
    switch (value_type) {
        case Value::Type::U1:
        case Value::Type::I8:
        case Value::Type::U8:
        case Value::Type::I16:
        case Value::Type::U16:
        case Value::Type::I32:
        case Value::Type::U32:
        case Value::Type::STRING_NULLPTR: {
            return CreateScalarPrimValueItem<uint32_t>(container, value, out);
        }
        case Value::Type::I64:
        case Value::Type::U64: {
            return CreateScalarPrimValueItem<uint64_t>(container, value, out);
        }
        case Value::Type::F32: {
            return CreateScalarPrimValueItem<float>(container, value, out);
        }
        case Value::Type::F64: {
            return CreateScalarPrimValueItem<double>(container, value, out);
        }
        case Value::Type::STRING: {
            return CreateScalarStringValueItem(container, value, out);
        }
        case Value::Type::RECORD: {
            return CreateScalarRecordValueItem(container, value, out, classes);
        }
        case Value::Type::METHOD: {
            return CreateScalarMethodValueItem(container, value, out, methods);
        }
        case Value::Type::ENUM: {
            return CreateScalarEnumValueItem(container, value, out, fields);
        }
        case Value::Type::ANNOTATION: {
            return CreateScalarAnnotationValueItem(container, value, out, program, classes, fields, methods);
        }
        default: {
            UNREACHABLE();
            return nullptr;
        }
    }
}

/* static */
ValueItem *AsmEmitter::CreateValueItem(ItemContainer *container, const Value *value, const Program &program,
                                       const std::unordered_map<std::string, BaseClassItem *> &classes,
                                       const std::unordered_map<std::string, BaseFieldItem *> &fields,
                                       const std::unordered_map<std::string, BaseMethodItem *> &methods)
{
    switch (value->GetType()) {
        case Value::Type::ARRAY: {
            std::vector<ScalarValueItem> elements;
            for (const auto &elem_value : value->GetAsArray()->GetValues()) {
                auto *item =
                    CreateScalarValueItem(container, &elem_value, &elements, program, classes, fields, methods);
                if (item == nullptr) {
                    return nullptr;
                }
            }

            auto component_type = value->GetAsArray()->GetComponentType();
            return container->CreateItem<ArrayValueItem>(panda_file::Type(GetTypeId(component_type)),
                                                         std::move(elements));
        }
        default: {
            return CreateScalarValueItem(container, value, nullptr, program, classes, fields, methods);
        }
    }
}

/* static */
AnnotationItem *AsmEmitter::CreateAnnotationItem(ItemContainer *container, const AnnotationData &annotation,
                                                 const Program &program,
                                                 const std::unordered_map<std::string, BaseClassItem *> &classes,
                                                 const std::unordered_map<std::string, BaseFieldItem *> &fields,
                                                 const std::unordered_map<std::string, BaseMethodItem *> &methods)
{
    auto record_name = annotation.GetName();
    auto it = program.record_table.find(record_name);
    if (it == program.record_table.cend()) {
        SetLastError("Record " + record_name + " not found");
        return nullptr;
    }

    auto &record = it->second;
    if (!record.metadata->IsAnnotation()) {
        SetLastError("Record " + record_name + " isn't annotation");
        return nullptr;
    }

    std::vector<AnnotationItem::Elem> item_elements;
    std::vector<AnnotationItem::Tag> tag_elements;

    for (const auto &element : annotation.GetElements()) {
        auto name = element.GetName();
        auto *value = element.GetValue();

        auto value_type = value->GetType();

        uint8_t tag_type;

        if (value_type == Value::Type::ARRAY && !value->GetAsArray()->GetValues().empty()) {
            auto array_element_type = value->GetAsArray()->GetComponentType();
            tag_type = Value::GetArrayTypeAsChar(array_element_type);
        } else {
            tag_type = Value::GetTypeAsChar(value_type);
        }

        ASSERT(tag_type != '0');

        auto function_name = record.name + "." + name;

        if (record.HasImplementation()) {
            auto func_it = program.function_table.find(function_name);
            if (func_it == program.function_table.cend()) {
                // Definitions of the system annotations in the libcore haven't values.
                // So print message and continue if corresponding function isn't found.
                LOG(INFO, ASSEMBLER) << "Function " << function_name << " not found";
            } else {
                auto &function = func_it->second;
                if (!CheckValue(value, function.return_type, program)) {
                    SetLastError("Incorrect annotation element " + function_name + ": " + GetLastError());
                    return nullptr;
                }
            }
        }

        auto *item = CreateValueItem(container, value, program, classes, fields, methods);
        if (item == nullptr) {
            SetLastError("Cannot create value item for annotation element " + function_name + ": " + GetLastError());
            return nullptr;
        }

        item_elements.emplace_back(container->GetOrCreateStringItem(name), item);
        tag_elements.emplace_back(tag_type);
    }

    auto *cls = classes.find(record_name)->second;
    return container->CreateItem<AnnotationItem>(cls, std::move(item_elements), std::move(tag_elements));
}

MethodHandleItem *AsmEmitter::CreateMethodHandleItem(ItemContainer *container, const MethodHandle &mh,
                                                     const std::unordered_map<std::string, BaseFieldItem *> &fields,
                                                     const std::unordered_map<std::string, BaseMethodItem *> &methods)
{
    MethodHandleItem *item = nullptr;
    switch (mh.type) {
        case panda_file::MethodHandleType::PUT_STATIC:
        case panda_file::MethodHandleType::GET_STATIC:
        case panda_file::MethodHandleType::PUT_INSTANCE:
        case panda_file::MethodHandleType::GET_INSTANCE: {
            item = container->CreateItem<MethodHandleItem>(mh.type, fields.at(mh.item_name));
            break;
        }
        case panda_file::MethodHandleType::INVOKE_STATIC:
        case panda_file::MethodHandleType::INVOKE_INSTANCE:
        case panda_file::MethodHandleType::INVOKE_CONSTRUCTOR:
        case panda_file::MethodHandleType::INVOKE_DIRECT:
        case panda_file::MethodHandleType::INVOKE_INTERFACE: {
            item = container->CreateItem<MethodHandleItem>(mh.type, methods.at(mh.item_name));
            break;
        }
        default:
            UNREACHABLE();
            break;
    }
    return item;
}

/* static */
template <class T>
bool AsmEmitter::AddAnnotations(T *item, ItemContainer *container, const AnnotationMetadata &metadata,
                                const Program &program, const std::unordered_map<std::string, BaseClassItem *> &classes,
                                const std::unordered_map<std::string, BaseFieldItem *> &fields,
                                const std::unordered_map<std::string, BaseMethodItem *> &methods)
{
    for (const auto &annotation : metadata.GetAnnotations()) {
        auto *annotation_item = CreateAnnotationItem(container, annotation, program, classes, fields, methods);
        if (annotation_item == nullptr) {
            return false;
        }

        auto &record = program.record_table.find(annotation.GetName())->second;
        if (record.metadata->IsRuntimeAnnotation()) {
            item->AddRuntimeAnnotation(annotation_item);
        } else if (record.metadata->IsAnnotation()) {
            item->AddAnnotation(annotation_item);
        } else if (record.metadata->IsRuntimeTypeAnnotation()) {
            item->AddRuntimeTypeAnnotation(annotation_item);
        } else if (record.metadata->IsTypeAnnotation()) {
            item->AddTypeAnnotation(annotation_item);
        }
    }

    return true;
}

template <class T>
void SetSourceLang(T *item, extensions::Language lang)
{
    switch (lang) {
        case extensions::Language::ECMASCRIPT: {
            item->SetSourceLang(panda_file::SourceLang::ECMASCRIPT);
            break;
        }
        case extensions::Language::PANDA_ASSEMBLY: {
            item->SetSourceLang(panda_file::SourceLang::PANDA_ASSEMBLY);
            break;
        }
        default: {
            UNREACHABLE();
            break;
        }
    }
}

template <class T>
static void AddBytecodeIndexDependencies(MethodItem *method, const Ins &insn,
                                         const std::unordered_map<std::string, T *> &items)
{
    ASSERT(!insn.ids.empty());

    for (const auto &id : insn.ids) {
        auto it = items.find(id);
        ASSERT(it != items.cend());

        auto *item = it->second;
        ASSERT(item->GetIndexType() != panda_file::IndexType::NONE);
        method->AddIndexDependency(item);
    }
}

static void AddBytecodeIndexDependencies(MethodItem *method, const Function &func,
                                         const AsmEmitter::AsmEntityCollections &entities)
{
    for (const auto &insn : func.ins) {
        if (insn.opcode == Opcode::INVALID) {
            continue;
        }

        if (insn.HasFlag(InstFlags::METHOD_ID)) {
            AddBytecodeIndexDependencies(method, insn, entities.method_items);
            continue;
        }

        if (insn.HasFlag(InstFlags::FIELD_ID)) {
            AddBytecodeIndexDependencies(method, insn, entities.field_items);
            continue;
        }

        if (insn.HasFlag(InstFlags::TYPE_ID)) {
            AddBytecodeIndexDependencies(method, insn, entities.class_items);
            continue;
        }
    }

    for (const auto &catch_block : func.catch_blocks) {
        if (catch_block.exception_record.empty()) {
            continue;
        }

        auto it = entities.class_items.find(catch_block.exception_record);
        ASSERT(it != entities.class_items.cend());
        auto *item = it->second;
        ASSERT(item->GetIndexType() != panda_file::IndexType::NONE);
        method->AddIndexDependency(item);
    }
}

/* static */
void AsmEmitter::MakeStringItems(ItemContainer *items, const Program &program,
                                 AsmEmitter::AsmEntityCollections &entities)
{
    for (const auto &s : program.strings) {
        auto *item = items->GetOrCreateStringItem(s);
        entities.string_items.insert({s, item});
    }
}

static std::unique_ptr<ScalarValue> CreateValue(const panda::pandasm::LiteralArray::Literal &literal)
{
    switch (literal.tag_) {
        case panda_file::LiteralTag::TAGVALUE:
            return std::make_unique<ScalarValue>(
                ScalarValue::Create<Value::Type::U8>(std::get<uint8_t>(literal.value_)));
        case panda_file::LiteralTag::BOOL:
            return std::make_unique<ScalarValue>(
                ScalarValue::Create<Value::Type::U8>(static_cast<uint8_t>(std::get<bool>(literal.value_))));
        case panda_file::LiteralTag::ARRAY_I8:
            return std::make_unique<ScalarValue>(
                ScalarValue::Create<Value::Type::I8>(std::get<uint8_t>(literal.value_)));
        case panda_file::LiteralTag::ARRAY_I16:
            return std::make_unique<ScalarValue>(
                ScalarValue::Create<Value::Type::I16>(std::get<uint16_t>(literal.value_)));
        case panda_file::LiteralTag::INTEGER:
            [[fallthrough]];
        case panda_file::LiteralTag::ARRAY_I32:
            return std::make_unique<ScalarValue>(
                ScalarValue::Create<Value::Type::I32>(std::get<uint32_t>(literal.value_)));
        case panda_file::LiteralTag::ARRAY_I64:
            return std::make_unique<ScalarValue>(
                ScalarValue::Create<Value::Type::I64>(std::get<uint64_t>(literal.value_)));
        case panda_file::LiteralTag::FLOAT:
            [[fallthrough]];
        case panda_file::LiteralTag::ARRAY_F32:
            return std::make_unique<ScalarValue>(
                ScalarValue::Create<Value::Type::F32>(std::get<float>(literal.value_)));
        case panda_file::LiteralTag::DOUBLE:
            [[fallthrough]];
        case panda_file::LiteralTag::ARRAY_F64:
            return std::make_unique<ScalarValue>(
                ScalarValue::Create<Value::Type::F64>(std::get<double>(literal.value_)));
        case panda_file::LiteralTag::STRING:
            [[fallthrough]];
        case panda_file::LiteralTag::ARRAY_STRING:
            return std::make_unique<ScalarValue>(
                ScalarValue::Create<Value::Type::STRING>(std::string_view(std::get<std::string>(literal.value_))));
        case panda_file::LiteralTag::METHOD:
            return std::make_unique<ScalarValue>(
                ScalarValue::Create<Value::Type::METHOD>(std::string_view(std::get<std::string>(literal.value_))));
        case panda_file::LiteralTag::GENERATORMETHOD:
            return std::make_unique<ScalarValue>(
                ScalarValue::Create<Value::Type::METHOD>(std::string_view(std::get<std::string>(literal.value_))));
        case panda_file::LiteralTag::ACCESSOR:
            [[fallthrough]];
        case panda_file::LiteralTag::NULLVALUE:
            return std::make_unique<ScalarValue>(
                ScalarValue::Create<Value::Type::U8>(std::get<uint8_t>(literal.value_)));
        default:
            UNREACHABLE();
            break;
    }

    return nullptr;
}

/* static */
void AsmEmitter::MakeLiteralItems(ItemContainer *items, const Program &program,
                                  AsmEmitter::AsmEntityCollections &entities)
{
    for (const auto &[id, l] : program.literalarray_table) {
        auto literal_array_item = items->GetOrCreateLiteralArrayItem(id);
        std::vector<panda_file::LiteralItem> literal_array;

        for (auto &literal : l.literals_) {
            auto value = CreateValue(literal);
            // the return pointer of vector element should not be rewrited
            CreateLiteralItem(items, value.get(), &literal_array, entities.method_items);
        }

        literal_array_item->AddItems(literal_array);
        entities.literalarray_items.insert({id, literal_array_item});
    }
}

/* static */
void AsmEmitter::MakeArrayTypeItems(ItemContainer *items, const Program &program,
                                    AsmEmitter::AsmEntityCollections &entities)
{
    for (const auto &t : program.array_types) {
        auto *foreign_record = items->GetOrCreateForeignClassItem(t.GetDescriptor());
        entities.class_items.insert({t.GetName(), foreign_record});
    }
}

/* static */
bool AsmEmitter::HandleRecordAsForeign(
    ItemContainer *items, const Program &program, AsmEmitter::AsmEntityCollections &entities,
    const std::unordered_map<panda_file::Type::TypeId, PrimitiveTypeItem *> &primitive_types, const std::string &name,
    const Record &rec)
{
    Type record_type = Type::FromName(name);
    auto *foreign_record = items->GetOrCreateForeignClassItem(record_type.GetDescriptor(rec.conflict));
    entities.class_items.insert({name, foreign_record});
    for (const auto &f : rec.field_list) {
        ASSERT(f.metadata->IsForeign());
        auto *field_name = items->GetOrCreateStringItem(pandasm::DeMangleName(f.name));
        std::string full_field_name = name + "." + f.name;
        if (!f.metadata->IsForeign()) {
            SetLastError("External record " + name + " has a non-external field " + f.name);
            return false;
        }
        auto *type_item = GetTypeItem(items, primitive_types, f.type, program);
        if (type_item == nullptr) {
            SetLastError("Field " + full_field_name + " has undefined type");
            return false;
        }
        auto *field = items->CreateItem<ForeignFieldItem>(foreign_record, field_name, type_item);
        entities.field_items.insert({full_field_name, field});
    }
    return true;
}

/* static */
bool AsmEmitter::HandleBaseRecord(ItemContainer *items, const Program &program, const std::string &name,
                                  const Record &base_rec, ClassItem *record)
{
    auto base_name = base_rec.metadata->GetBase();
    if (!base_name.empty()) {
        auto it = program.record_table.find(base_name);
        if (it == program.record_table.cend()) {
            SetLastError("Base record " + base_name + " is not defined for record " + name);
            return false;
        }
        auto &rec = it->second;
        Type base_type(base_name, 0);
        if (rec.metadata->IsForeign()) {
            record->SetSuperClass(items->GetOrCreateForeignClassItem(base_type.GetDescriptor(rec.conflict)));
        } else {
            record->SetSuperClass(items->GetOrCreateClassItem(base_type.GetDescriptor(rec.conflict)));
        }
    }
    return true;
}

/* static */
bool AsmEmitter::HandleInterfaces(ItemContainer *items, const Program &program, const std::string &name,
                                  const Record &rec, ClassItem *record)
{
    auto ifaces = rec.metadata->GetInterfaces();
    for (const auto &item : ifaces) {
        auto it = program.record_table.find(item);
        if (it == program.record_table.cend()) {
            SetLastError("Interface record " + item + " is not defined for record " + name);
            return false;
        }
        auto &iface = it->second;
        Type iface_type(item, 0);
        if (iface.metadata->IsForeign()) {
            record->AddInterface(items->GetOrCreateForeignClassItem(iface_type.GetDescriptor(iface.conflict)));
        } else {
            record->AddInterface(items->GetOrCreateClassItem(iface_type.GetDescriptor(iface.conflict)));
        }
    }
    return true;
}

/* static */
bool AsmEmitter::HandleFields(ItemContainer *items, const Program &program, AsmEmitter::AsmEntityCollections &entities,
                              const std::unordered_map<panda_file::Type::TypeId, PrimitiveTypeItem *> &primitive_types,
                              const std::string &name, const Record &rec, ClassItem *record)
{
    for (const auto &f : rec.field_list) {
        auto *field_name = items->GetOrCreateStringItem(pandasm::DeMangleName(f.name));
        std::string full_field_name = name + "." + f.name;
        auto *type_item = GetTypeItem(items, primitive_types, f.type, program);
        if (type_item == nullptr) {
            SetLastError("Field " + full_field_name + " has undefined type");
            return false;
        }
        BaseFieldItem *field;
        if (f.metadata->IsForeign()) {
            field = items->CreateItem<ForeignFieldItem>(record, field_name, type_item);
        } else {
            field = record->AddField(field_name, type_item, f.metadata->GetAccessFlags());
        }
        entities.field_items.insert({full_field_name, field});
    }
    return true;
}

/* static */
bool AsmEmitter::HandleRecord(ItemContainer *items, const Program &program, AsmEmitter::AsmEntityCollections &entities,
                              const std::unordered_map<panda_file::Type::TypeId, PrimitiveTypeItem *> &primitive_types,
                              const std::string &name, const Record &rec)
{
    Type record_type = Type::FromName(name);
    auto *record = items->GetOrCreateClassItem(record_type.GetDescriptor(rec.conflict));
    entities.class_items.insert({name, record});

    record->SetAccessFlags(rec.metadata->GetAccessFlags());

    SetSourceLang(record, rec.language);

    if (!rec.source_file.empty()) {
        auto *source_file_item = items->GetOrCreateStringItem(rec.source_file);
        record->SetSourceFile(source_file_item);
    }

    if (!HandleBaseRecord(items, program, name, rec, record)) {
        return false;
    }

    if (!HandleInterfaces(items, program, name, rec, record)) {
        return false;
    }

    if (!HandleFields(items, program, entities, primitive_types, name, rec, record)) {
        return false;
    }

    return true;
}

/* static */
bool AsmEmitter::MakeRecordItems(
    ItemContainer *items, const Program &program, AsmEmitter::AsmEntityCollections &entities,
    const std::unordered_map<panda_file::Type::TypeId, PrimitiveTypeItem *> &primitive_types)
{
    for (const auto &[name, rec] : program.record_table) {
        if (rec.metadata->IsForeign()) {
            if (!HandleRecordAsForeign(items, program, entities, primitive_types, name, rec)) {
                return false;
            }
        } else {
            if (!HandleRecord(items, program, entities, primitive_types, name, rec)) {
                return false;
            }
        }
    }
    return true;
}

/* static */
StringItem *AsmEmitter::GetMethodName(ItemContainer *items, const Function &func, const std::string &name)
{
    if (func.metadata->IsCtor()) {
        return items->GetOrCreateStringItem(extensions::GetCtorName(func.language));
    }

    if (func.metadata->IsCctor()) {
        return items->GetOrCreateStringItem(extensions::GetCctorName(func.language));
    }

    return items->GetOrCreateStringItem(GetItemName(name));
}

/* static */
bool AsmEmitter::HandleAreaForInner(ItemContainer *items, const Program &program, ClassItem **area,
                                    ForeignClassItem **foreign_area, const std::string &name,
                                    const std::string &record_owner_name)
{
    auto iter = program.record_table.find(record_owner_name);
    if (iter != program.record_table.end()) {
        auto &rec = iter->second;
        Type record_owner_type = Type::FromName(record_owner_name);
        auto descriptor = record_owner_type.GetDescriptor(rec.conflict);
        if (rec.metadata->IsForeign()) {
            *foreign_area = items->GetOrCreateForeignClassItem(descriptor);
            if (*foreign_area == nullptr) {
                SetLastError("Unable to create external record " + iter->first);
                return false;
            }
        } else {
            *area = items->GetOrCreateClassItem(descriptor);
            (*area)->SetAccessFlags(rec.metadata->GetAccessFlags());
        }
    } else {
        SetLastError("Function " + name + " is bound to undefined record " + record_owner_name);
        return false;
    }
    return true;
}

/* static */
bool AsmEmitter::HandleRecordOnwer(ItemContainer *items, const Program &program, ClassItem **area,
                                   ForeignClassItem **foreign_area, const std::string &name,
                                   const std::string &record_owner_name)
{
    if (record_owner_name.empty()) {
        *area = items->GetOrCreateGlobalClassItem();
        (*area)->SetAccessFlags(ACC_PUBLIC);
        SetSourceLang(*area, program.lang);
    } else {
        if (!HandleAreaForInner(items, program, area, foreign_area, name, record_owner_name)) {
            return false;
        }
    }
    return true;
}

/* static */
bool AsmEmitter::HandleFunctionParams(
    ItemContainer *items, const Program &program, size_t idx, const std::string &name, const Function &func,
    const std::unordered_map<panda_file::Type::TypeId, PrimitiveTypeItem *> &primitive_types,
    std::vector<MethodParamItem> &params)
{
    for (size_t i = idx; i < func.params.size(); i++) {
        const auto &p = func.params[i].type;
        auto *type_item = GetTypeItem(items, primitive_types, p, program);
        if (type_item == nullptr) {
            SetLastError("Argument " + std::to_string(i) + " of function " + name + " has undefined type");
            return false;
        }
        params.emplace_back(type_item);
    }
    return true;
}

/* static */
bool AsmEmitter::HandleFunctionLocalVariables(ItemContainer *items, const Function &func, const std::string &name)
{
    for (const auto &v : func.local_variable_debug) {
        if (v.name.empty()) {
            SetLastError("Function '" + name + "' has an empty local variable name");
            return false;
        }
        if (v.signature.empty()) {
            SetLastError("Function '" + name + "' has an empty local variable signature");
            return false;
        }
        items->GetOrCreateStringItem(v.name);
        // Skip signature and signature type for parameters
        ASSERT(v.reg >= 0);
        if (func.IsParameter(v.reg)) {
            continue;
        }
        items->GetOrCreateStringItem(v.signature);
        if (!v.signature_type.empty()) {
            items->GetOrCreateStringItem(v.signature_type);
        }
    }
    return true;
}

/* static */
bool AsmEmitter::CreateMethodItem(ItemContainer *items, AsmEmitter::AsmEntityCollections &entities,
                                  const Function &func, TypeItem *type_item, ClassItem *area,
                                  ForeignClassItem *foreign_area, uint32_t access_flags, StringItem *method_name,
                                  const std::string &mangled_name, const std::string &name,
                                  std::vector<MethodParamItem> &params)
{
    auto *proto = items->GetOrCreateProtoItem(type_item, params);
    BaseMethodItem *method;
    if (foreign_area == nullptr) {
        if (func.metadata->IsForeign()) {
            method = items->CreateItem<ForeignMethodItem>(area, method_name, proto, access_flags);
        } else {
            method = area->AddMethod(method_name, proto, access_flags, params);
        }
    } else {
        if (!func.metadata->IsForeign()) {
            SetLastError("Non-external function " + name + " is bound to external record");
            return false;
        }
        method = items->CreateItem<ForeignMethodItem>(foreign_area, method_name, proto, access_flags);
    }
    entities.method_items.insert({mangled_name, method});
    if (!func.metadata->IsForeign() && func.metadata->HasImplementation()) {
        if (!func.source_file.empty()) {
            items->GetOrCreateStringItem(func.source_file);
        }
        if (!func.source_code.empty()) {
            items->GetOrCreateStringItem(func.source_code);
        }
    }
    return true;
}

/* static */
bool AsmEmitter::MakeFunctionItems(
    ItemContainer *items, const Program &program, AsmEmitter::AsmEntityCollections &entities,
    const std::unordered_map<panda_file::Type::TypeId, PrimitiveTypeItem *> &primitive_types, bool emit_debug_info)
{
    for (const auto &f : program.function_table) {
        const auto &[mangled_name, func] = f;

        auto name = pandasm::DeMangleName(mangled_name);
        StringItem *method_name = GetMethodName(items, func, name);

        ClassItem *area = nullptr;
        ForeignClassItem *foreign_area = nullptr;

        std::string record_owner_name = GetOwnerName(name);
        if (!HandleRecordOnwer(items, program, &area, &foreign_area, name, record_owner_name)) {
            return false;
        }

        auto params = std::vector<MethodParamItem> {};
        uint32_t access_flags = func.metadata->GetAccessFlags();
        if (func.params.empty() || func.params[0].type.GetName() != record_owner_name) {
            access_flags |= ACC_STATIC;
        }

        bool is_static = (access_flags & ACC_STATIC) != 0;
        size_t idx = is_static ? 0 : 1;
        if (!HandleFunctionParams(items, program, idx, name, func, primitive_types, params)) {
            return false;
        }

        if (emit_debug_info && !HandleFunctionLocalVariables(items, func, name)) {
            return false;
        }

        auto *type_item = GetTypeItem(items, primitive_types, func.return_type, program);
        if (type_item == nullptr) {
            SetLastError("Function " + name + " has undefined return type");
            return false;
        }

        if (!CreateMethodItem(items, entities, func, type_item, area, foreign_area, access_flags, method_name,
                              mangled_name, name, params)) {
            return false;
        }
    }
    return true;
}

/* static */
bool AsmEmitter::MakeRecordAnnotations(ItemContainer *items, const Program &program,
                                       const AsmEmitter::AsmEntityCollections &entities)
{
    for (const auto &[name, record] : program.record_table) {
        if (record.metadata->IsForeign()) {
            continue;
        }

        auto *class_item = static_cast<ClassItem *>(Find(entities.class_items, name));
        if (!AddAnnotations(class_item, items, *record.metadata, program, entities.class_items, entities.field_items,
                            entities.method_items)) {
            SetLastError("Cannot emit annotations for record " + record.name + ": " + GetLastError());
            return false;
        }

        for (const auto &field : record.field_list) {
            auto field_name = record.name + "." + field.name;
            auto *field_item = static_cast<FieldItem *>(Find(entities.field_items, field_name));
            if (!AddAnnotations(field_item, items, *field.metadata, program, entities.class_items, entities.field_items,
                                entities.method_items)) {
                SetLastError("Cannot emit annotations for field " + field_name + ": " + GetLastError());
                return false;
            }

            auto res = field.metadata->GetValue();
            if (res) {
                auto value = res.value();
                auto *item = CreateValueItem(items, &value, program, entities.class_items, entities.field_items,
                                             entities.method_items);
                field_item->SetValue(item);
            }
        }
    }
    return true;
}

/* static */
void AsmEmitter::SetCodeAndDebugInfo(ItemContainer *items, MethodItem *method, const Function &func,
                                     bool emit_debug_info)
{
    auto *code = items->CreateItem<CodeItem>();
    method->SetCode(code);

    if (!emit_debug_info && !func.CanThrow()) {
        return;
    }

    auto *line_number_program = items->CreateLineNumberProgramItem();
    auto *debug_info = items->CreateItem<DebugInfoItem>(line_number_program);
    if (emit_debug_info) {
        for (const auto &v : func.local_variable_debug) {
            ASSERT(v.reg >= 0);
            if (func.IsParameter(v.reg)) {
                debug_info->AddParameter(items->GetOrCreateStringItem(v.name));
            }
        }
    } else {
        auto nparams = method->GetParams().size();
        for (size_t i = 0; i < nparams; i++) {
            debug_info->AddParameter(nullptr);
        }
    }
    method->SetDebugInfo(debug_info);
}

/* static */
void AsmEmitter::SetMethodSourceLang(const Program &program, MethodItem *method, const Function &func,
                                     const std::string &name)
{
    std::string record_name = GetOwnerName(name);
    if (!record_name.empty()) {
        auto &rec = program.record_table.find(record_name)->second;
        if (rec.language != func.language) {
            SetSourceLang(method, func.language);
        }
    } else {
        SetSourceLang(method, func.language);
    }
}

/* static */
bool AsmEmitter::AddMethodAndParamsAnnotations(ItemContainer *items, const Program &program,
                                               const AsmEmitter::AsmEntityCollections &entities, MethodItem *method,
                                               const Function &func)
{
    if (!AddAnnotations(method, items, *func.metadata, program, entities.class_items, entities.field_items,
                        entities.method_items)) {
        SetLastError("Cannot emit annotations for function " + func.name + ": " + GetLastError());
        return false;
    }

    auto &param_items = method->GetParams();
    for (size_t proto_idx = 0; proto_idx < param_items.size(); proto_idx++) {
        size_t param_idx = method->IsStatic() ? proto_idx : proto_idx + 1;
        auto &param = func.params[param_idx];
        auto &param_item = param_items[proto_idx];
        if (!AddAnnotations(&param_item, items, *param.metadata, program, entities.class_items, entities.field_items,
                            entities.method_items)) {
            SetLastError("Cannot emit annotations for parameter a" + std::to_string(param_idx) + "of function " +
                         func.name + ": " + GetLastError());
            return false;
        }
    }

    if (method->HasRuntimeParamAnnotations()) {
        items->CreateItem<ParamAnnotationsItem>(method, true);
    }

    if (method->HasParamAnnotations()) {
        items->CreateItem<ParamAnnotationsItem>(method, false);
    }

    return true;
}

/* static */
bool AsmEmitter::MakeFunctionDebugInfoAndAnnotations(ItemContainer *items, const Program &program,
                                                     const AsmEmitter::AsmEntityCollections &entities,
                                                     bool emit_debug_info)
{
    for (const auto &[name, func] : program.function_table) {
        if (func.metadata->IsForeign()) {
            continue;
        }
        auto *method = static_cast<MethodItem *>(Find(entities.method_items, name));

        if (func.metadata->HasImplementation()) {
            SetCodeAndDebugInfo(items, method, func, emit_debug_info);
            AddBytecodeIndexDependencies(method, func, entities);
        }

        SetMethodSourceLang(program, method, func, name);

        if (!AddMethodAndParamsAnnotations(items, program, entities, method, func)) {
            return false;
        }
    }
    return true;
}

/* static */
void AsmEmitter::FillMap(PandaFileToPandaAsmMaps *maps, const AsmEmitter::AsmEntityCollections &entities)
{
    for (const auto &[name, method] : entities.method_items) {
        maps->methods.insert({method->GetFileId().GetOffset(), std::string(name)});
    }

    for (const auto &[name, field] : entities.field_items) {
        maps->fields.insert({field->GetFileId().GetOffset(), std::string(name)});
    }

    for (const auto &[name, cls] : entities.class_items) {
        maps->classes.insert({cls->GetFileId().GetOffset(), std::string(name)});
    }

    for (const auto &[name, str] : entities.string_items) {
        maps->strings.insert({str->GetFileId().GetOffset(), std::string(name)});
    }

    for (const auto &[name, arr] : entities.literalarray_items) {
        maps->literalarrays.emplace(arr->GetFileId().GetOffset(), name);
    }
}

/* static */
void AsmEmitter::EmitDebugInfo(ItemContainer *items, const Program &program, const std::vector<uint8_t> *bytes,
                               const MethodItem *method, const Function &func, const std::string &name,
                               bool emit_debug_info)
{
    auto *debug_info = method->GetDebugInfo();
    if (debug_info == nullptr) {
        return;
    }

    auto *line_number_program = debug_info->GetLineNumberProgram();
    auto *constant_pool = debug_info->GetConstantPool();

    std::string record_name = GetOwnerName(name);
    std::string record_source_file;
    if (!record_name.empty()) {
        auto &rec = program.record_table.find(record_name)->second;
        record_source_file = rec.source_file;
    }

    if (!func.source_file.empty() && func.source_file != record_source_file) {
        if (!func.source_code.empty()) {
            auto *source_code_item = items->GetOrCreateStringItem(func.source_code);
            ASSERT(source_code_item->GetOffset() != 0);
            line_number_program->EmitSetSourceCode(constant_pool, source_code_item);
        }
        auto *source_file_item = items->GetOrCreateStringItem(func.source_file);
        ASSERT(source_file_item->GetOffset() != 0);
        line_number_program->EmitSetFile(constant_pool, source_file_item);
    }
    func.BuildLineNumberProgram(debug_info, *bytes, items, constant_pool, emit_debug_info);
}

/* static */
bool AsmEmitter::EmitFunctions(ItemContainer *items, const Program &program,
                               const AsmEmitter::AsmEntityCollections &entities, bool emit_debug_info)
{
    for (const auto &f : program.function_table) {
        const auto &[name, func] = f;

        if (func.metadata->IsForeign() || !func.metadata->HasImplementation()) {
            continue;
        }

        auto emitter = BytecodeEmitter {};
        auto *method = static_cast<MethodItem *>(Find(entities.method_items, name));
        if (!func.Emit(emitter, method, entities.method_items, entities.field_items, entities.class_items,
                       entities.string_items, entities.literalarray_items)) {
            SetLastError("Internal error during emitting function: " + func.name);
            return false;
        }

        auto *code = method->GetCode();
        code->SetNumVregs(func.regs_num);
        code->SetNumArgs(func.GetParamsNum());

        size_t num_ins =
            std::count_if(func.ins.begin(), func.ins.end(), [](auto it) { return it.opcode != Opcode::INVALID; });
        code->SetNumInstructions(num_ins);

        auto *bytes = code->GetInstructions();
        auto status = emitter.Build(static_cast<std::vector<unsigned char> *>(bytes));
        if (status != BytecodeEmitter::ErrorCode::SUCCESS) {
            SetLastError("Internal error during emitting binary code, status=" +
                         std::to_string(static_cast<int>(status)));
            return false;
        }
        auto try_blocks = func.BuildTryBlocks(method, entities.class_items, *bytes);
        for (auto &try_block : try_blocks) {
            code->AddTryBlock(try_block);
        }

        EmitDebugInfo(items, program, bytes, method, func, name, emit_debug_info);
    }
    return true;
}

/* static */
bool AsmEmitter::Emit(ItemContainer *items, const Program &program, PandaFileToPandaAsmMaps *maps, bool emit_debug_info)
{
    auto primitive_types = CreatePrimitiveTypes(items);

    auto entities = AsmEmitter::AsmEntityCollections {};

    SetLastError("");

    MakeStringItems(items, program, entities);

    MakeArrayTypeItems(items, program, entities);

    if (!MakeRecordItems(items, program, entities, primitive_types)) {
        return false;
    }

    if (!MakeFunctionItems(items, program, entities, primitive_types, emit_debug_info)) {
        return false;
    }

    MakeLiteralItems(items, program, entities);

    // Add annotations for records and fields
    if (!MakeRecordAnnotations(items, program, entities)) {
        return false;
    }

    // Add Code and DebugInfo items last due to that they have variable size that depends on bytecode
    if (!MakeFunctionDebugInfoAndAnnotations(items, program, entities, emit_debug_info)) {
        return false;
    }

    items->ComputeLayout();

    if (maps != nullptr) {
        FillMap(maps, entities);
    }

    if (!EmitFunctions(items, program, entities, emit_debug_info)) {
        return false;
    }

    return true;
}

bool AsmEmitter::Emit(Writer *writer, const Program &program, std::map<std::string, size_t> *stat,
                      PandaFileToPandaAsmMaps *maps, bool debug_info)
{
    auto items = ItemContainer {};
    if (!Emit(&items, program, maps, debug_info)) {
        return false;
    }

    if (stat != nullptr) {
        *stat = items.GetStat();
    }

    return items.Write(writer);
}

bool AsmEmitter::Emit(const std::string &filename, const Program &program, std::map<std::string, size_t> *stat,
                      PandaFileToPandaAsmMaps *maps, bool debug_info)
{
    auto writer = FileWriter(filename);
    if (!writer) {
        SetLastError("Unable to open" + filename + " for writing");
        return false;
    }
    return Emit(&writer, program, stat, maps, debug_info);
}

std::unique_ptr<const panda_file::File> AsmEmitter::Emit(const Program &program, PandaFileToPandaAsmMaps *maps)
{
    auto items = ItemContainer {};
    if (!Emit(&items, program, maps)) {
        return nullptr;
    }

    size_t size = items.ComputeLayout();
    // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
    auto *buffer = new (std::nothrow) std::byte[size];
    if (buffer == nullptr) {
        LOG(ERROR, ASSEMBLER) << "Invalid items size " << size;
        return nullptr;
    }

    auto writer = MemoryBufferWriter(reinterpret_cast<uint8_t *>(buffer), size);
    if (!items.Write(&writer)) {
        return nullptr;
    }

    os::mem::ConstBytePtr ptr(
        buffer, size, [](std::byte *buffer_ptr, [[maybe_unused]] size_t param_size) noexcept { delete[] buffer_ptr; });
    return panda_file::File::OpenFromMemory(std::move(ptr));
}

TypeItem *AsmEmitter::GetTypeItem(
    ItemContainer *items, const std::unordered_map<panda_file::Type::TypeId, PrimitiveTypeItem *> &primitive_types,
    const Type &type, const Program &program)
{
    if (!type.IsObject()) {
        return Find(primitive_types, type.GetId());
    }

    if (type.IsArray()) {
        return items->GetOrCreateForeignClassItem(type.GetDescriptor());
    }

    const auto &name = type.GetName();
    auto iter = program.record_table.find(name);
    if (iter == program.record_table.end()) {
        return nullptr;
    }

    auto &rec = iter->second;
    if (rec.metadata->IsForeign()) {
        return items->GetOrCreateForeignClassItem(type.GetDescriptor());
    }

    return items->GetOrCreateClassItem(type.GetDescriptor());
}

bool Function::Emit(BytecodeEmitter &emitter, panda_file::MethodItem *method,
                    const std::unordered_map<std::string, panda_file::BaseMethodItem *> &methods,
                    const std::unordered_map<std::string, panda_file::BaseFieldItem *> &fields,
                    const std::unordered_map<std::string, panda_file::BaseClassItem *> &classes,
                    const std::unordered_map<std::string_view, panda_file::StringItem *> &strings,
                    const std::unordered_map<std::string, panda_file::LiteralArrayItem *> &literalarrays) const
{
    auto labels = std::unordered_map<std::string_view, panda::Label> {};

    for (const auto &insn : ins) {
        if (insn.set_label) {
            labels.insert_or_assign(insn.label, emitter.CreateLabel());
        }
    }

    for (const auto &insn : ins) {
        if (insn.set_label) {
            auto search = labels.find(insn.label);
            ASSERT(search != labels.end());
            emitter.Bind(search->second);
        }

        if (insn.opcode != Opcode::INVALID) {
            if (!insn.Emit(emitter, method, methods, fields, classes, strings, literalarrays, labels)) {
                return false;
            }
        }
    }

    return true;
}

void Function::EmitLocalVariable(panda_file::LineNumberProgramItem *program, ItemContainer *container,
                                 std::vector<uint8_t> *constant_pool, uint32_t &pc_inc, size_t instruction_number) const
{
    auto try_emit_pc = [program, constant_pool, &pc_inc]() -> void {
        if (pc_inc) {
            program->EmitAdvancePc(constant_pool, pc_inc);
            pc_inc = 0;
        }
    };
    for (auto &v : local_variable_debug) {
        if (IsParameter(v.reg)) {
            continue;
        }
        if (instruction_number == v.start) {
            try_emit_pc();
            StringItem *variable_name = container->GetOrCreateStringItem(v.name);
            StringItem *variable_type = container->GetOrCreateStringItem(v.signature);
            if (v.signature_type.empty()) {
                program->EmitStartLocal(constant_pool, v.reg, variable_name, variable_type);
            } else {
                StringItem *type_signature = container->GetOrCreateStringItem(v.signature_type);
                program->EmitStartLocalExtended(constant_pool, v.reg, variable_name, variable_type, type_signature);
            }
        }
        if (instruction_number == (v.start + v.length)) {
            try_emit_pc();
            program->EmitEndLocal(v.reg);
        }
    }
}

size_t Function::GetLineNumber(size_t i) const
{
    return static_cast<int32_t>(ins[i].ins_debug.line_number);
}

void Function::EmitNumber(panda_file::LineNumberProgramItem *program, std::vector<uint8_t> *constant_pool,
                          uint32_t pc_inc, int32_t line_inc) const
{
    if (!program->EmitSpecialOpcode(pc_inc, line_inc)) {
        if (pc_inc) {
            program->EmitAdvancePc(constant_pool, pc_inc);
            if (!program->EmitSpecialOpcode(0, line_inc)) {
                program->EmitAdvanceLine(constant_pool, line_inc);
                program->EmitSpecialOpcode(0, 0);
            }
        } else {
            program->EmitAdvanceLine(constant_pool, line_inc);
            program->EmitSpecialOpcode(0, 0);
        }
    }
}

void Function::EmitLineNumber(panda_file::LineNumberProgramItem *program, std::vector<uint8_t> *constant_pool,
                              int32_t &prev_line_number, uint32_t &pc_inc, size_t instruction_number) const
{
    int32_t line_inc = GetLineNumber(instruction_number) - prev_line_number;
    if (line_inc) {
        prev_line_number = GetLineNumber(instruction_number);
        EmitNumber(program, constant_pool, pc_inc, line_inc);
        pc_inc = 0;
    }
}

void Function::BuildLineNumberProgram(panda_file::DebugInfoItem *debug_item, const std::vector<uint8_t> &bytecode,
                                      ItemContainer *container, std::vector<uint8_t> *constant_pool,
                                      bool emit_debug_info) const
{
    auto *program = debug_item->GetLineNumberProgram();

    if (ins.empty()) {
        program->EmitEnd();
        return;
    }

    uint32_t pc_inc = 0;
    int32_t prev_line_number = GetLineNumber(0);
    BytecodeInstruction bi(bytecode.data());
    debug_item->SetLineNumber(static_cast<uint32_t>(prev_line_number));

    for (size_t i = 0; i < ins.size(); i++) {
        if (emit_debug_info) {
            EmitLocalVariable(program, container, constant_pool, pc_inc, i);
        }
        if (ins[i].opcode == Opcode::INVALID) {
            continue;
        }

        if (emit_debug_info || ins[i].CanThrow()) {
            EmitLineNumber(program, constant_pool, prev_line_number, pc_inc, i);
        }

        pc_inc += bi.GetSize();
        bi = bi.GetNext();
    }

    program->EmitEnd();
}

Function::TryCatchInfo Function::MakeOrderAndOffsets(const std::vector<uint8_t> &bytecode) const
{
    std::unordered_map<std::string_view, size_t> try_catch_labels;
    std::unordered_map<std::string, std::vector<const CatchBlock *>> try_catch_map;
    std::vector<std::string> try_catch_order;

    for (auto &catch_block : catch_blocks) {
        try_catch_labels.insert_or_assign(catch_block.try_begin_label, 0);
        try_catch_labels.insert_or_assign(catch_block.try_end_label, 0);
        try_catch_labels.insert_or_assign(catch_block.catch_begin_label, 0);
        try_catch_labels.insert_or_assign(catch_block.catch_end_label, 0);

        std::string try_key = catch_block.try_begin_label + ":" + catch_block.try_end_label;
        auto it = try_catch_map.find(try_key);
        if (it == try_catch_map.cend()) {
            std::tie(it, std::ignore) = try_catch_map.try_emplace(try_key);
            try_catch_order.push_back(try_key);
        }
        it->second.push_back(&catch_block);
    }

    BytecodeInstruction bi(bytecode.data());
    size_t pc_offset = 0;

    for (size_t i = 0; i < ins.size(); i++) {
        if (ins[i].set_label) {
            auto it = try_catch_labels.find(ins[i].label);
            if (it != try_catch_labels.cend()) {
                try_catch_labels[ins[i].label] = pc_offset;
            }
        }
        if (ins[i].opcode == Opcode::INVALID) {
            continue;
        }

        pc_offset += bi.GetSize();
        bi = bi.GetNext();
    }

    return Function::TryCatchInfo {try_catch_labels, try_catch_map, try_catch_order};
}

std::vector<CodeItem::TryBlock> Function::BuildTryBlocks(
    MethodItem *method, const std::unordered_map<std::string, BaseClassItem *> &class_items,
    const std::vector<uint8_t> &bytecode) const
{
    std::vector<CodeItem::TryBlock> try_blocks;

    if (ins.empty()) {
        return try_blocks;
    }

    Function::TryCatchInfo tcs = MakeOrderAndOffsets(bytecode);

    for (const auto &t_key : tcs.try_catch_order) {
        auto kv = tcs.try_catch_map.find(t_key);
        ASSERT(kv != tcs.try_catch_map.cend());
        auto &try_catch_blocks = kv->second;

        ASSERT(!try_catch_blocks.empty());

        std::vector<CodeItem::CatchBlock> catch_block_items;

        for (auto *catch_block : try_catch_blocks) {
            auto class_name = catch_block->exception_record;

            BaseClassItem *class_item = nullptr;
            if (!class_name.empty()) {
                auto it = class_items.find(class_name);
                ASSERT(it != class_items.cend());
                class_item = it->second;
            }

            auto handler_pc_offset = tcs.try_catch_labels[catch_block->catch_begin_label];
            auto handler_code_size = tcs.try_catch_labels[catch_block->catch_end_label] - handler_pc_offset;
            catch_block_items.emplace_back(method, class_item, handler_pc_offset, handler_code_size);
        }

        auto try_start_pc_offset = tcs.try_catch_labels[try_catch_blocks[0]->try_begin_label];
        auto try_end_pc_offset = tcs.try_catch_labels[try_catch_blocks[0]->try_end_label];
        ASSERT(try_end_pc_offset >= try_start_pc_offset);
        try_blocks.emplace_back(try_start_pc_offset, try_end_pc_offset - try_start_pc_offset, catch_block_items);
    }

    return try_blocks;
}

void Function::DebugDump() const
{
    std::cerr << "name: " << name << std::endl;
    for (const auto &i : ins) {
        std::cerr << i.ToString("\n", true, regs_num);
    }
}

std::string GetOwnerName(const std::string &name)
{
    auto super_pos = name.find_last_of(PARSE_AREA_MARKER);
    if (super_pos == std::string::npos) {
        return "";
    }
    return name.substr(0, super_pos);
}

std::string GetItemName(const std::string &name)
{
    auto super_pos = name.find_last_of(PARSE_AREA_MARKER);
    if (super_pos == std::string::npos) {
        return name;
    }
    return name.substr(super_pos + 1);
}

}  // namespace panda::pandasm
