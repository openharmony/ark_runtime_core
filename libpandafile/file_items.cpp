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

#include "file_items.h"
#include "macros.h"
#include "utils/bit_utils.h"
#include "utils/leb128.h"
#include "utils/utf.h"

#include <iomanip>

namespace panda::panda_file {

template <class Tag, class Val>
static bool WriteUlebTaggedValue(Writer *writer, Tag tag, Val v)
{
    if (!writer->WriteByte(static_cast<uint8_t>(tag))) {
        return false;
    }

    if (!writer->WriteUleb128(v)) {
        return false;
    }

    return true;
}

template <class Tag, class Val>
static bool WriteSlebTaggedValue(Writer *writer, Tag tag, Val v)
{
    if (!writer->WriteByte(static_cast<uint8_t>(tag))) {
        return false;
    }

    if (!writer->WriteSleb128(v)) {
        return false;
    }

    return true;
}

template <class Tag, class Val>
static bool WriteTaggedValue(Writer *writer, Tag tag, Val v)
{
    if (!writer->WriteByte(static_cast<uint8_t>(tag))) {
        return false;
    }

    if (!writer->Write(v)) {
        return false;
    }

    return true;
}

template <class Tag>
static bool WriteIdTaggedValue(Writer *writer, Tag tag, BaseItem *item)
{
    ASSERT(item->GetOffset() != 0);
    return WriteTaggedValue(writer, tag, item->GetOffset());
}

StringItem::StringItem(std::string str) : str_(std::move(str))
{
    str_.push_back(0);
    utf16_length_ = utf::MUtf8ToUtf16Size(utf::CStringAsMutf8(str_.data()));
    is_ascii_ = 1;

    for (auto c : str_) {
        if (static_cast<uint8_t>(c) > utf::MUTF8_1B_MAX) {
            is_ascii_ = 0;
            break;
        }
    }
}

size_t StringItem::CalculateSize() const
{
    return leb128::UnsignedEncodingSize((utf16_length_ << 1U) | is_ascii_) + str_.size();
}

bool StringItem::Write(Writer *writer)
{
    ASSERT(GetOffset() == writer->GetOffset());

    constexpr size_t max_string_length = 0x7fffffffU;
    if (utf16_length_ > max_string_length) {
        LOG(ERROR, PANDAFILE) << "Writing StringItem with size greater than 0x7fffffffU is not supported!";
        return false;
    }

    if (!writer->WriteUleb128((utf16_length_ << 1U) | is_ascii_)) {
        return false;
    }

    for (auto c : str_) {
        if (!writer->WriteByte(static_cast<uint8_t>(c))) {
            return false;
        }
    }
    return true;
}

size_t BaseClassItem::CalculateSize() const
{
    return name_.GetSize();
}

void BaseClassItem::ComputeLayout()
{
    uint32_t offset = GetOffset();

    ASSERT(offset != 0);

    name_.SetOffset(offset);
}

bool BaseClassItem::Write(Writer *writer)
{
    ASSERT(GetOffset() == writer->GetOffset());
    return name_.Write(writer);
}

size_t ClassItem::CalculateSizeWithoutFieldsAndMethods() const
{
    size_t size = BaseClassItem::CalculateSize() + ID_SIZE + leb128::UnsignedEncodingSize(access_flags_);

    size += leb128::UnsignedEncodingSize(fields_.size());
    size += leb128::UnsignedEncodingSize(methods_.size());

    if (!ifaces_.empty()) {
        size += TAG_SIZE + leb128::UnsignedEncodingSize(ifaces_.size()) + IDX_SIZE * ifaces_.size();
    }

    if (source_lang_ != SourceLang::PANDA_ASSEMBLY) {
        size += TAG_SIZE + sizeof(SourceLang);
    }

    size += (TAG_SIZE + ID_SIZE) * runtime_annotations_.size();
    size += (TAG_SIZE + ID_SIZE) * annotations_.size();
    size += (TAG_SIZE + ID_SIZE) * runtime_type_annotations_.size();
    size += (TAG_SIZE + ID_SIZE) * type_annotations_.size();

    if (source_file_ != nullptr) {
        size += TAG_SIZE + ID_SIZE;
    }

    size += TAG_SIZE;  // null tag

    return size;
}

size_t ClassItem::CalculateSize() const
{
    size_t size = CalculateSizeWithoutFieldsAndMethods();

    for (auto &field : fields_) {
        size += field->GetSize();
    }

    for (auto &method : methods_) {
        size += method->GetSize();
    }

    return size;
}

void ClassItem::ComputeLayout()
{
    BaseClassItem::ComputeLayout();

    uint32_t offset = GetOffset();

    offset += CalculateSizeWithoutFieldsAndMethods();

    for (auto &field : fields_) {
        field->SetOffset(offset);
        field->ComputeLayout();
        offset += field->GetSize();
    }

    for (auto &method : methods_) {
        method->SetOffset(offset);
        method->ComputeLayout();
        offset += method->GetSize();
    }
}

bool ClassItem::WriteIfaces(Writer *writer)
{
    if (!ifaces_.empty()) {
        if (!writer->WriteByte(static_cast<uint8_t>(ClassTag::INTERFACES))) {
            return false;
        }

        if (!writer->WriteUleb128(ifaces_.size())) {
            return false;
        }

        for (auto iface : ifaces_) {
            ASSERT(iface->HasIndex(this));
            if (!writer->Write<uint16_t>(iface->GetIndex(this))) {
                return false;
            }
        }
    }

    return true;
}

bool ClassItem::WriteAnnotations(Writer *writer)
{
    for (auto runtime_annotation : runtime_annotations_) {
        if (!WriteIdTaggedValue(writer, ClassTag::RUNTIME_ANNOTATION, runtime_annotation)) {
            return false;
        }
    }

    for (auto annotation : annotations_) {
        if (!WriteIdTaggedValue(writer, ClassTag::ANNOTATION, annotation)) {
            return false;
        }
    }

    for (auto runtime_type_annotation : runtime_type_annotations_) {
        if (!WriteIdTaggedValue(writer, ClassTag::RUNTIME_TYPE_ANNOTATION, runtime_type_annotation)) {
            return false;
        }
    }

    for (auto type_annotation : type_annotations_) {
        if (!WriteIdTaggedValue(writer, ClassTag::TYPE_ANNOTATION, type_annotation)) {
            return false;
        }
    }

    return true;
}

bool ClassItem::WriteTaggedData(Writer *writer)
{
    if (!WriteIfaces(writer)) {
        return false;
    }

    if (source_lang_ != SourceLang::PANDA_ASSEMBLY) {
        if (!WriteTaggedValue(writer, ClassTag::SOURCE_LANG, static_cast<uint8_t>(source_lang_))) {
            return false;
        }
    }

    if (!WriteAnnotations(writer)) {
        return false;
    }

    if (source_file_ != nullptr) {
        if (!WriteIdTaggedValue(writer, ClassTag::SOURCE_FILE, source_file_)) {
            return false;
        }
    }

    return writer->WriteByte(static_cast<uint8_t>(ClassTag::NOTHING));
}

bool ClassItem::Write(Writer *writer)
{
    if (!BaseClassItem::Write(writer)) {
        return false;
    }

    uint32_t offset = super_class_ != nullptr ? super_class_->GetOffset() : 0;
    if (!writer->Write(offset)) {
        return false;
    }

    if (!writer->WriteUleb128(access_flags_)) {
        return false;
    }

    if (!writer->WriteUleb128(fields_.size())) {
        return false;
    }

    if (!writer->WriteUleb128(methods_.size())) {
        return false;
    }

    if (!WriteTaggedData(writer)) {
        return false;
    }

    for (auto &field : fields_) {
        if (!field->Write(writer)) {
            return false;
        }
    }

    for (auto &method : methods_) {
        if (!method->Write(writer)) {
            return false;
        }
    }

    return true;
}

ParamAnnotationsItem::ParamAnnotationsItem(MethodItem *method, bool is_runtime_annotations)
{
    for (const auto &param : method->GetParams()) {
        if (is_runtime_annotations) {
            annotations_.push_back(param.GetRuntimeAnnotations());
        } else {
            annotations_.push_back(param.GetAnnotations());
        }
    }

    if (is_runtime_annotations) {
        method->SetRuntimeParamAnnotationItem(this);
    } else {
        method->SetParamAnnotationItem(this);
    }
}

size_t ParamAnnotationsItem::CalculateSize() const
{
    size_t size = sizeof(uint32_t);  // size

    for (const auto &param_annotations : annotations_) {
        size += sizeof(uint32_t);  // count
        size += param_annotations.size() * ID_SIZE;
    }

    return size;
}

bool ParamAnnotationsItem::Write(Writer *writer)
{
    ASSERT(GetOffset() == writer->GetOffset());

    if (!writer->Write(static_cast<uint32_t>(annotations_.size()))) {
        return false;
    }

    for (const auto &param_annotations : annotations_) {
        if (!writer->Write(static_cast<uint32_t>(param_annotations.size()))) {
            return false;
        }

        for (auto *item : param_annotations) {
            ASSERT(item->GetOffset() != 0);

            if (!writer->Write(item->GetOffset())) {
                return false;
            }
        }
    }

    return true;
}

ProtoItem::ProtoItem(TypeItem *ret_type, const std::vector<MethodParamItem> &params)
{
    size_t n = 0;
    shorty_.push_back(0);
    AddType(ret_type, &n);
    for (auto &p : params) {
        AddType(p.GetType(), &n);
    }
}

void ProtoItem::AddType(TypeItem *type, size_t *n)
{
    constexpr size_t SHORTY_ELEMS_COUNT = std::numeric_limits<uint16_t>::digits / SHORTY_ELEM_SIZE;

    uint16_t v = shorty_.back();

    size_t shift = (*n % SHORTY_ELEMS_COUNT) * SHORTY_ELEM_SIZE;

    v |= static_cast<uint16_t>(static_cast<uint16_t>(type->GetType().GetEncoding()) << shift);
    shorty_.back() = v;

    if (!type->GetType().IsPrimitive()) {
        reference_types_.push_back(type);
        AddIndexDependency(type);
    }

    *n += 1;

    if (*n % SHORTY_ELEMS_COUNT == 0) {
        shorty_.push_back(0);
    }
}

bool ProtoItem::Write(Writer *writer)
{
    ASSERT(GetOffset() == writer->GetOffset());
    for (auto s : shorty_) {
        if (!writer->Write(s)) {
            return false;
        }
    }

    for (auto r : reference_types_) {
        ASSERT(r->HasIndex(this));
        if (!writer->Write<uint16_t>(r->GetIndex(this))) {
            return false;
        }
    }

    return true;
}

BaseMethodItem::BaseMethodItem(BaseClassItem *cls, StringItem *name, ProtoItem *proto, uint32_t access_flags)
    : class_(cls), name_(name), proto_(proto), access_flags_(access_flags)
{
    AddIndexDependency(cls);
    AddIndexDependency(proto);
}

size_t BaseMethodItem::CalculateSize() const
{
    // class id + proto id + name id + access flags
    return IDX_SIZE + IDX_SIZE + ID_SIZE + leb128::UnsignedEncodingSize(access_flags_);
}

bool BaseMethodItem::Write(Writer *writer)
{
    ASSERT(GetOffset() == writer->GetOffset());
    ASSERT(class_ != nullptr);
    ASSERT(class_->HasIndex(this));

    if (!writer->Write<uint16_t>(class_->GetIndex(this))) {
        return false;
    }

    ASSERT(proto_->HasIndex(this));

    if (!writer->Write<uint16_t>(proto_->GetIndex(this))) {
        return false;
    }

    ASSERT(name_->GetOffset() != 0);

    if (!writer->Write(name_->GetOffset())) {
        return false;
    }

    return writer->WriteUleb128(access_flags_);
}

MethodItem::MethodItem(ClassItem *cls, StringItem *name, ProtoItem *proto, uint32_t access_flags,
                       std::vector<MethodParamItem> params)
    : BaseMethodItem(cls, name, proto, access_flags),
      params_(std::move(params)),
      source_lang_(SourceLang::PANDA_ASSEMBLY),
      code_(nullptr),
      debug_info_(nullptr)
{
}

size_t MethodItem::CalculateSize() const
{
    size_t size = BaseMethodItem::CalculateSize();

    if (code_ != nullptr) {
        size += TAG_SIZE + ID_SIZE;
    }

    if (source_lang_ != SourceLang::PANDA_ASSEMBLY) {
        size += TAG_SIZE + sizeof(SourceLang);
    }

    size += (TAG_SIZE + ID_SIZE) * runtime_annotations_.size();

    if (runtime_param_annotations_ != nullptr) {
        size += TAG_SIZE + ID_SIZE;
    }

    size += (TAG_SIZE + ID_SIZE) * annotations_.size();

    if (param_annotations_ != nullptr) {
        size += TAG_SIZE + ID_SIZE;
    }

    size += (TAG_SIZE + ID_SIZE) * runtime_type_annotations_.size();
    size += (TAG_SIZE + ID_SIZE) * type_annotations_.size();

    if (debug_info_ != nullptr) {
        size += TAG_SIZE + ID_SIZE;
    }

    size += TAG_SIZE;  // null tag

    return size;
}

bool MethodItem::WriteRuntimeAnnotations(Writer *writer)
{
    for (auto runtime_annotation : runtime_annotations_) {
        if (!WriteIdTaggedValue(writer, MethodTag::RUNTIME_ANNOTATION, runtime_annotation)) {
            return false;
        }
    }

    if (runtime_param_annotations_ != nullptr) {
        if (!WriteIdTaggedValue(writer, MethodTag::RUNTIME_PARAM_ANNOTATION, runtime_param_annotations_)) {
            return false;
        }
    }

    return true;
}

bool MethodItem::WriteTypeAnnotations(Writer *writer)
{
    for (auto runtime_type_annotation : runtime_type_annotations_) {
        if (!WriteIdTaggedValue(writer, MethodTag::RUNTIME_TYPE_ANNOTATION, runtime_type_annotation)) {
            return false;
        }
    }

    for (auto type_annotation : type_annotations_) {
        if (!WriteIdTaggedValue(writer, MethodTag::TYPE_ANNOTATION, type_annotation)) {
            return false;
        }
    }

    return true;
}

bool MethodItem::WriteTaggedData(Writer *writer)
{
    if (code_ != nullptr) {
        if (!WriteIdTaggedValue(writer, MethodTag::CODE, code_)) {
            return false;
        }
    }

    if (source_lang_ != SourceLang::PANDA_ASSEMBLY) {
        if (!WriteTaggedValue(writer, MethodTag::SOURCE_LANG, static_cast<uint8_t>(source_lang_))) {
            return false;
        }
    }

    if (!WriteRuntimeAnnotations(writer)) {
        return false;
    }

    if (debug_info_ != nullptr) {
        if (!WriteIdTaggedValue(writer, MethodTag::DEBUG_INFO, debug_info_)) {
            return false;
        }
    }

    for (auto annotation : annotations_) {
        if (!WriteIdTaggedValue(writer, MethodTag::ANNOTATION, annotation)) {
            return false;
        }
    }

    if (!WriteTypeAnnotations(writer)) {
        return false;
    }

    if (param_annotations_ != nullptr) {
        if (!WriteIdTaggedValue(writer, MethodTag::PARAM_ANNOTATION, param_annotations_)) {
            return false;
        }
    }

    return writer->WriteByte(static_cast<uint8_t>(MethodTag::NOTHING));
}

bool MethodItem::Write(Writer *writer)
{
    if (!BaseMethodItem::Write(writer)) {
        return false;
    }

    return WriteTaggedData(writer);
}

size_t CodeItem::CatchBlock::CalculateSize() const
{
    ASSERT(type_ == nullptr || type_->HasIndex(method_));
    uint32_t type_off = type_ != nullptr ? type_->GetIndex(method_) + 1 : 0;
    return leb128::UnsignedEncodingSize(type_off) + leb128::UnsignedEncodingSize(handler_pc_) +
           leb128::UnsignedEncodingSize(code_size_);
}

bool CodeItem::CatchBlock::Write(Writer *writer)
{
    ASSERT(GetOffset() == writer->GetOffset());
    ASSERT(type_ == nullptr || type_->HasIndex(method_));

    uint32_t type_off = type_ != nullptr ? type_->GetIndex(method_) + 1 : 0;

    if (!writer->WriteUleb128(type_off)) {
        return false;
    }

    if (!writer->WriteUleb128(handler_pc_)) {
        return false;
    }

    if (!writer->WriteUleb128(code_size_)) {
        return false;
    }

    return true;
}

void CodeItem::TryBlock::ComputeLayout()
{
    size_t offset = GetOffset();
    offset += CalculateSizeWithoutCatchBlocks();

    for (auto &catch_block : catch_blocks_) {
        catch_block.SetOffset(offset);
        catch_block.ComputeLayout();
        offset += catch_block.GetSize();
    }
}

size_t CodeItem::TryBlock::CalculateSizeWithoutCatchBlocks() const
{
    return leb128::UnsignedEncodingSize(start_pc_) + leb128::UnsignedEncodingSize(length_) +
           leb128::UnsignedEncodingSize(catch_blocks_.size());
}

size_t CodeItem::TryBlock::CalculateSize() const
{
    size_t size = CalculateSizeWithoutCatchBlocks();

    for (auto &catch_block : catch_blocks_) {
        size += catch_block.GetSize();
    }

    return size;
}

bool CodeItem::TryBlock::Write(Writer *writer)
{
    ASSERT(GetOffset() == writer->GetOffset());

    if (!writer->WriteUleb128(start_pc_)) {
        return false;
    }

    if (!writer->WriteUleb128(length_)) {
        return false;
    }

    if (!writer->WriteUleb128(catch_blocks_.size())) {
        return false;
    }

    for (auto &catch_block : catch_blocks_) {
        if (!catch_block.Write(writer)) {
            return false;
        }
    }

    return true;
}

void CodeItem::ComputeLayout()
{
    uint32_t offset = GetOffset();

    offset += CalculateSizeWithoutTryBlocks();

    for (auto &try_block : try_blocks_) {
        try_block.SetOffset(offset);
        try_block.ComputeLayout();
        offset += try_block.GetSize();
    }
}

size_t CodeItem::CalculateSizeWithoutTryBlocks() const
{
    size_t size = leb128::UnsignedEncodingSize(num_vregs_) + leb128::UnsignedEncodingSize(num_args_) +
                  leb128::UnsignedEncodingSize(instructions_.size()) + leb128::UnsignedEncodingSize(try_blocks_.size());

    size += instructions_.size();

    return size;
}

size_t CodeItem::GetCodeSize() const
{
    return instructions_.size();
}

size_t CodeItem::CalculateSize() const
{
    size_t size = CalculateSizeWithoutTryBlocks();

    for (auto &try_block : try_blocks_) {
        size += try_block.GetSize();
    }

    return size;
}

bool CodeItem::Write(Writer *writer)
{
    ASSERT(GetOffset() == writer->GetOffset());

    if (!writer->WriteUleb128(num_vregs_)) {
        return false;
    }

    if (!writer->WriteUleb128(num_args_)) {
        return false;
    }

    if (!writer->WriteUleb128(instructions_.size())) {
        return false;
    }

    if (!writer->WriteUleb128(try_blocks_.size())) {
        return false;
    }

    if (!writer->WriteBytes(instructions_)) {
        return false;
    }

    for (auto &try_block : try_blocks_) {
        if (!try_block.Write(writer)) {
            return false;
        }
    }

    return true;
}

const ScalarValueItem *ValueItem::GetAsScalar() const
{
    ASSERT(!IsArray());
    return static_cast<const ScalarValueItem *>(this);
}

const ArrayValueItem *ValueItem::GetAsArray() const
{
    ASSERT(IsArray());
    return static_cast<const ArrayValueItem *>(this);
}

size_t ScalarValueItem::GetULeb128EncodedSize()
{
    switch (GetType()) {
        case Type::INTEGER:
            return leb128::UnsignedEncodingSize(GetValue<uint32_t>());

        case Type::LONG:
            return leb128::UnsignedEncodingSize(GetValue<uint64_t>());

        case Type::ID:
            return leb128::UnsignedEncodingSize(GetId().GetOffset());

        default:
            return 0;
    }
}

size_t ScalarValueItem::GetSLeb128EncodedSize() const
{
    switch (GetType()) {
        case Type::INTEGER:
            return leb128::SignedEncodingSize(static_cast<int32_t>(GetValue<uint32_t>()));

        case Type::LONG:
            return leb128::SignedEncodingSize(static_cast<int64_t>(GetValue<uint64_t>()));

        default:
            return 0;
    }
}

size_t ScalarValueItem::CalculateSize() const
{
    size_t size = 0;
    switch (GetType()) {
        case Type::INTEGER: {
            size = sizeof(uint32_t);
            break;
        }

        case Type::LONG: {
            size = sizeof(uint64_t);
            break;
        }

        case Type::FLOAT: {
            size = sizeof(float);
            break;
        }

        case Type::DOUBLE: {
            size = sizeof(double);
            break;
        }

        case Type::ID: {
            size = ID_SIZE;
            break;
        }
        default: {
            UNREACHABLE();
            break;
        }
    }

    return size;
}

size_t ScalarValueItem::Alignment()
{
    return GetSize();
}

bool ScalarValueItem::Write(Writer *writer)
{
    ASSERT(GetOffset() == writer->GetOffset());

    switch (GetType()) {
        case Type::INTEGER:
            return writer->Write(GetValue<uint32_t>());

        case Type::LONG:
            return writer->Write(GetValue<uint64_t>());

        case Type::FLOAT:
            return writer->Write(bit_cast<uint32_t>(GetValue<float>()));

        case Type::DOUBLE:
            return writer->Write(bit_cast<uint64_t>(GetValue<double>()));

        case Type::ID: {
            ASSERT(GetId().IsValid());
            return writer->Write(GetId().GetOffset());
        }
        default: {
            UNREACHABLE();
            break;
        }
    }

    return true;
}

bool ScalarValueItem::WriteAsUleb128(Writer *writer)
{
    ASSERT(GetOffset() == writer->GetOffset());

    switch (GetType()) {
        case Type::INTEGER:
            return writer->WriteUleb128(GetValue<uint32_t>());

        case Type::LONG:
            return writer->WriteUleb128(GetValue<uint64_t>());

        case Type::ID: {
            ASSERT(GetId().IsValid());
            return writer->WriteUleb128(GetId().GetOffset());
        }
        default:
            return false;
    }
}

size_t ArrayValueItem::CalculateSize() const
{
    return leb128::UnsignedEncodingSize(items_.size()) + items_.size() * GetComponentSize();
}

void ArrayValueItem::ComputeLayout()
{
    uint32_t offset = GetOffset();

    ASSERT(offset != 0);

    offset += leb128::UnsignedEncodingSize(items_.size());

    for (auto &item : items_) {
        item.SetOffset(offset);
        offset += GetComponentSize();
    }
}

bool ArrayValueItem::Write(Writer *writer)
{
    ASSERT(GetOffset() == writer->GetOffset());

    if (!writer->WriteUleb128(items_.size())) {
        return false;
    }

    switch (component_type_.GetId()) {
        case panda_file::Type::TypeId::U1:
        case panda_file::Type::TypeId::I8:
        case panda_file::Type::TypeId::U8: {
            for (auto &item : items_) {
                auto value = static_cast<uint8_t>(item.GetValue<uint32_t>());
                if (!writer->Write(value)) {
                    return false;
                }
            }
            break;
        }
        case panda_file::Type::TypeId::I16:
        case panda_file::Type::TypeId::U16: {
            for (auto &item : items_) {
                auto value = static_cast<uint16_t>(item.GetValue<uint32_t>());
                if (!writer->Write(value)) {
                    return false;
                }
            }
            break;
        }
        default: {
            for (auto &item : items_) {
                if (!item.Write(writer)) {
                    return false;
                }
            }
            break;
        }
    }

    return true;
}

size_t ArrayValueItem::GetComponentSize() const
{
    switch (component_type_.GetId()) {
        case panda_file::Type::TypeId::U1:
        case panda_file::Type::TypeId::I8:
        case panda_file::Type::TypeId::U8:
            return sizeof(uint8_t);
        case panda_file::Type::TypeId::I16:
        case panda_file::Type::TypeId::U16:
            return sizeof(uint16_t);
        case panda_file::Type::TypeId::I32:
        case panda_file::Type::TypeId::U32:
        case panda_file::Type::TypeId::F32:
            return sizeof(uint32_t);
        case panda_file::Type::TypeId::I64:
        case panda_file::Type::TypeId::U64:
        case panda_file::Type::TypeId::F64:
            return sizeof(uint64_t);
        case panda_file::Type::TypeId::REFERENCE:
            return ID_SIZE;
        case panda_file::Type::TypeId::VOID:
            return 0;
        default: {
            UNREACHABLE();
            return 0;
        }
    }
}

size_t LiteralItem::CalculateSize() const
{
    size_t size = 0;
    switch (GetType()) {
        case Type::B1: {
            size = sizeof(uint8_t);
            break;
        }

        case Type::B2: {
            size = sizeof(uint16_t);
            break;
        }

        case Type::B4: {
            size = sizeof(uint32_t);
            break;
        }

        case Type::B8: {
            size = sizeof(uint64_t);
            break;
        }

        case Type::STRING:
        case Type::METHOD: {
            size = ID_SIZE;
            break;
        }

        default: {
            UNREACHABLE();
            break;
        }
    }

    return size;
}

size_t LiteralItem::Alignment()
{
    return GetSize();
}

bool LiteralItem::Write(Writer *writer)
{
    ASSERT(GetOffset() == writer->GetOffset());

    switch (GetType()) {
        case Type::B1: {
            return writer->Write(GetValue<uint8_t>());
        }
        case Type::B2: {
            return writer->Write(GetValue<uint16_t>());
        }
        case Type::B4: {
            return writer->Write(GetValue<uint32_t>());
        }
        case Type::B8: {
            return writer->Write(GetValue<uint64_t>());
        }
        case Type::STRING: {
            ASSERT(GetId().IsValid());
            return writer->Write(GetId().GetOffset());
        }
        case Type::METHOD: {
            ASSERT(GetMethodId().IsValid());
            return writer->Write(GetMethodId().GetOffset());
        }
        default: {
            UNREACHABLE();
            break;
        }
    }

    return true;
}

void LiteralArrayItem::AddItems(const std::vector<LiteralItem> &item)
{
    items_.assign(item.begin(), item.end());
}

size_t LiteralArrayItem::CalculateSize() const
{
    size_t size = sizeof(uint32_t);
    for (auto &item : items_) {
        size += item.CalculateSize();
    }

    return size;
}

void LiteralArrayItem::ComputeLayout()
{
    uint32_t offset = GetOffset();

    ASSERT(offset != 0);

    offset += sizeof(uint32_t);

    for (auto &item : items_) {
        item.SetOffset(offset);
        offset += item.CalculateSize();
    }
}

bool LiteralArrayItem::Write(Writer *writer)
{
    ASSERT(GetOffset() == writer->GetOffset());

    if (!writer->Write(static_cast<uint32_t>(items_.size()))) {
        return false;
    }

    for (auto &item : items_) {
        if (!item.Write(writer)) {
            return false;
        }
    }

    return true;
}

BaseFieldItem::BaseFieldItem(BaseClassItem *cls, StringItem *name, TypeItem *type)
    : class_(cls), name_(name), type_(type)
{
    AddIndexDependency(cls);
    AddIndexDependency(type);
}

size_t BaseFieldItem::CalculateSize() const
{
    // class id + type id + name id
    return IDX_SIZE + IDX_SIZE + ID_SIZE;
}

bool BaseFieldItem::Write(Writer *writer)
{
    ASSERT(GetOffset() == writer->GetOffset());
    ASSERT(class_->HasIndex(this));
    ASSERT(type_->HasIndex(this));

    if (!writer->Write<uint16_t>(class_->GetIndex(this))) {
        return false;
    }

    if (!writer->Write<uint16_t>(type_->GetIndex(this))) {
        return false;
    }

    return writer->Write(name_->GetOffset());
}

FieldItem::FieldItem(ClassItem *cls, StringItem *name, TypeItem *type, uint32_t access_flags)
    : BaseFieldItem(cls, name, type), access_flags_(access_flags), value_(nullptr)
{
}

void FieldItem::SetValue(ValueItem *value)
{
    value_ = value;
    value_->SetNeedsEmit(!value_->Is32bit());
}

size_t FieldItem::CalculateSize() const
{
    size_t size = BaseFieldItem::CalculateSize() + leb128::UnsignedEncodingSize(access_flags_);

    if (value_ != nullptr) {
        if (value_->GetType() == ValueItem::Type::INTEGER) {
            size += TAG_SIZE + value_->GetAsScalar()->GetSLeb128EncodedSize();
        } else {
            size += TAG_SIZE + ID_SIZE;
        }
    }

    size += (TAG_SIZE + ID_SIZE) * runtime_annotations_.size();
    size += (TAG_SIZE + ID_SIZE) * annotations_.size();
    size += (TAG_SIZE + ID_SIZE) * runtime_type_annotations_.size();
    size += (TAG_SIZE + ID_SIZE) * type_annotations_.size();

    size += TAG_SIZE;  // null tag

    return size;
}

bool FieldItem::WriteValue(Writer *writer)
{
    if (value_ == nullptr) {
        return true;
    }

    if (value_->GetType() == ValueItem::Type::INTEGER) {
        auto v = static_cast<int32_t>(value_->GetAsScalar()->GetValue<uint32_t>());
        if (!WriteSlebTaggedValue(writer, FieldTag::INT_VALUE, v)) {
            return false;
        }
    } else if (value_->GetType() == ValueItem::Type::FLOAT) {
        auto v = bit_cast<uint32_t>(value_->GetAsScalar()->GetValue<float>());
        if (!WriteTaggedValue(writer, FieldTag::VALUE, v)) {
            return false;
        }
    } else if (value_->GetType() == ValueItem::Type::ID) {
        auto id = value_->GetAsScalar()->GetId();
        ASSERT(id.GetOffset() != 0);
        if (!WriteTaggedValue(writer, FieldTag::VALUE, id.GetOffset())) {
            return false;
        }
    } else {
        ASSERT(!value_->Is32bit());
        if (!WriteIdTaggedValue(writer, FieldTag::VALUE, value_)) {
            return false;
        }
    }

    return true;
}

bool FieldItem::WriteAnnotations(Writer *writer)
{
    for (auto runtime_annotation : runtime_annotations_) {
        if (!WriteIdTaggedValue(writer, FieldTag::RUNTIME_ANNOTATION, runtime_annotation)) {
            return false;
        }
    }

    for (auto annotation : annotations_) {
        if (!WriteIdTaggedValue(writer, FieldTag::ANNOTATION, annotation)) {
            return false;
        }
    }

    for (auto runtime_type_annotation : runtime_type_annotations_) {
        if (!WriteIdTaggedValue(writer, FieldTag::RUNTIME_TYPE_ANNOTATION, runtime_type_annotation)) {
            return false;
        }
    }

    for (auto type_annotation : type_annotations_) {
        if (!WriteIdTaggedValue(writer, FieldTag::TYPE_ANNOTATION, type_annotation)) {
            return false;
        }
    }

    return true;
}

bool FieldItem::WriteTaggedData(Writer *writer)
{
    if (!WriteValue(writer)) {
        return false;
    }

    if (!WriteAnnotations(writer)) {
        return false;
    }

    return writer->WriteByte(static_cast<uint8_t>(FieldTag::NOTHING));
}

bool FieldItem::Write(Writer *writer)
{
    if (!BaseFieldItem::Write(writer)) {
        return false;
    }

    if (!writer->WriteUleb128(access_flags_)) {
        return false;
    }

    return WriteTaggedData(writer);
}

size_t AnnotationItem::CalculateSize() const
{
    // class id + count + (name id + value id) * count + tag size * count
    return IDX_SIZE + sizeof(uint16_t) + (ID_SIZE + ID_SIZE) * elements_.size() + sizeof(uint8_t) * tags_.size();
}

bool AnnotationItem::Write(Writer *writer)
{
    ASSERT(GetOffset() == writer->GetOffset());
    ASSERT(class_->HasIndex(this));

    if (!writer->Write<uint16_t>(class_->GetIndex(this))) {
        return false;
    }

    if (!writer->Write(static_cast<uint16_t>(elements_.size()))) {
        return false;
    }

    for (auto elem : elements_) {
        ASSERT(elem.GetName()->GetOffset() != 0);
        if (!writer->Write(elem.GetName()->GetOffset())) {
            return false;
        }

        ValueItem *value_item = elem.GetValue();

        switch (value_item->GetType()) {
            case ValueItem::Type::INTEGER: {
                if (!writer->Write(value_item->GetAsScalar()->GetValue<uint32_t>())) {
                    return false;
                }
                break;
            }
            case ValueItem::Type::FLOAT: {
                if (!writer->Write(bit_cast<uint32_t>(value_item->GetAsScalar()->GetValue<float>()))) {
                    return false;
                }
                break;
            }
            case ValueItem::Type::ID: {
                if (!writer->Write(value_item->GetAsScalar()->GetId().GetOffset())) {
                    return false;
                }
                break;
            }
            default: {
                ASSERT(value_item->GetOffset() != 0);
                if (!writer->Write(value_item->GetOffset())) {
                    return false;
                }
                break;
            }
        }
    }

    for (auto tag : tags_) {
        if (!writer->Write(tag.GetItem())) {
            return false;
        }
    }

    return true;
}

void LineNumberProgramItem::EmitEnd()
{
    EmitOpcode(Opcode::END_SEQUENCE);
}

void LineNumberProgramItem::EmitAdvancePc(std::vector<uint8_t> *constant_pool, uint32_t value)
{
    EmitOpcode(Opcode::ADVANCE_PC);
    EmitUleb128(constant_pool, value);
}

void LineNumberProgramItem::EmitAdvanceLine(std::vector<uint8_t> *constant_pool, int32_t value)
{
    EmitOpcode(Opcode::ADVANCE_LINE);
    EmitSleb128(constant_pool, value);
}

void LineNumberProgramItem::EmitStartLocal(std::vector<uint8_t> *constant_pool, int32_t register_number,
                                           StringItem *name, StringItem *type)
{
    EmitStartLocalExtended(constant_pool, register_number, name, type, nullptr);
}

void LineNumberProgramItem::EmitStartLocalExtended(std::vector<uint8_t> *constant_pool, int32_t register_number,
                                                   StringItem *name, StringItem *type, StringItem *type_signature)
{
    if (type == nullptr) {
        return;
    }

    ASSERT(name->GetOffset() != 0);
    ASSERT(type->GetOffset() != 0);

    EmitOpcode(type_signature == nullptr ? Opcode::START_LOCAL : Opcode::START_LOCAL_EXTENDED);
    EmitRegister(register_number);
    EmitUleb128(constant_pool, name->GetOffset());
    EmitUleb128(constant_pool, type->GetOffset());

    if (type_signature != nullptr) {
        ASSERT(type_signature->GetOffset() != 0);
        EmitUleb128(constant_pool, type_signature->GetOffset());
    }
}

void LineNumberProgramItem::EmitEndLocal(int32_t register_number)
{
    EmitOpcode(Opcode::END_LOCAL);
    EmitRegister(register_number);
}

void LineNumberProgramItem::EmitRestartLocal(int32_t register_number)
{
    EmitOpcode(Opcode::RESTART_LOCAL);
    EmitRegister(register_number);
}

bool LineNumberProgramItem::EmitSpecialOpcode(uint32_t pc_inc, int32_t line_inc)
{
    if (line_inc < LINE_BASE || (line_inc - LINE_BASE) >= LINE_RANGE) {
        return false;
    }

    auto opcode = static_cast<size_t>(line_inc - LINE_BASE) + static_cast<size_t>(pc_inc * LINE_RANGE) + OPCODE_BASE;
    if (opcode > std::numeric_limits<uint8_t>::max()) {
        return false;
    }

    data_.push_back(static_cast<uint8_t>(opcode));
    return true;
}

void LineNumberProgramItem::EmitColumn(std::vector<uint8_t> *constant_pool, uint32_t pc_inc, int32_t column)
{
    if (pc_inc != 0U) {
        EmitAdvancePc(constant_pool, pc_inc);
    }
    EmitOpcode(Opcode::SET_COLUMN);
    EmitUleb128(constant_pool, column);
}

void LineNumberProgramItem::EmitPrologEnd()
{
    EmitOpcode(Opcode::SET_PROLOGUE_END);
}

void LineNumberProgramItem::EmitEpilogBegin()
{
    EmitOpcode(Opcode::SET_EPILOGUE_BEGIN);
}

void LineNumberProgramItem::EmitSetFile(std::vector<uint8_t> *constant_pool, StringItem *source_file)
{
    EmitOpcode(Opcode::SET_FILE);

    if (source_file == nullptr) {
        return;
    }

    ASSERT(source_file->GetOffset() != 0);
    EmitUleb128(constant_pool, source_file->GetOffset());
}

void LineNumberProgramItem::EmitSetSourceCode(std::vector<uint8_t> *constant_pool, StringItem *source_code)
{
    EmitOpcode(Opcode::SET_SOURCE_CODE);

    if (source_code == nullptr) {
        return;
    }

    ASSERT(source_code->GetOffset() != 0);
    EmitUleb128(constant_pool, source_code->GetOffset());
}

void LineNumberProgramItem::EmitOpcode(Opcode opcode)
{
    data_.push_back(static_cast<uint8_t>(opcode));
}

void LineNumberProgramItem::EmitRegister(int32_t register_number)
{
    EmitSleb128(&data_, register_number);
}

/* static */
void LineNumberProgramItem::EmitUleb128(std::vector<uint8_t> *data, uint32_t value)
{
    size_t n = leb128::UnsignedEncodingSize(value);
    std::vector<uint8_t> out(n);
    leb128::EncodeUnsigned(value, out.data());

    if (data == nullptr) {
        return;
    }

    data->insert(data->end(), out.cbegin(), out.cend());
}

/* static */
void LineNumberProgramItem::EmitSleb128(std::vector<uint8_t> *data, int32_t value)
{
    size_t n = leb128::SignedEncodingSize(value);
    std::vector<uint8_t> out(n);
    leb128::EncodeSigned(value, out.data());
    data->insert(data->end(), out.cbegin(), out.cend());
}

size_t LineNumberProgramItem::CalculateSize() const
{
    return data_.size();
}

bool LineNumberProgramItem::Write(Writer *writer)
{
    ASSERT(GetOffset() == writer->GetOffset());

    return writer->WriteBytes(data_);
}

size_t DebugInfoItem::CalculateSize() const
{
    size_t n = leb128::UnsignedEncodingSize(line_num_) + leb128::UnsignedEncodingSize(parameters_.size());

    for (auto *p : parameters_) {
        ASSERT(p == nullptr || p->GetOffset() != 0);
        n += leb128::UnsignedEncodingSize(p == nullptr ? 0 : p->GetOffset());
    }

    n += leb128::UnsignedEncodingSize(constant_pool_.size());
    n += constant_pool_.size();

    n += leb128::UnsignedEncodingSize(program_->GetIndex(this));

    return n;
}

bool DebugInfoItem::Write(Writer *writer)
{
    ASSERT(GetOffset() == writer->GetOffset());

    if (!writer->WriteUleb128(line_num_)) {
        return false;
    }

    if (!writer->WriteUleb128(parameters_.size())) {
        return false;
    }

    for (auto *p : parameters_) {
        ASSERT(p == nullptr || p->GetOffset() != 0);

        if (!writer->WriteUleb128(p == nullptr ? 0 : p->GetOffset())) {
            return false;
        }
    }

    if (!writer->WriteUleb128(constant_pool_.size())) {
        return false;
    }

    if (!writer->WriteBytes(constant_pool_)) {
        return false;
    }

    ASSERT(program_ != nullptr);
    ASSERT(program_->HasIndex(this));

    return writer->WriteUleb128(program_->GetIndex(this));
}

void DebugInfoItem::Dump(std::ostream &os) const
{
    os << "line_start = " << line_num_ << std::endl;

    os << "num_parameters = " << parameters_.size() << std::endl;
    for (auto *item : parameters_) {
        if (item != nullptr) {
            os << "  string_item[" << item->GetOffset() << "]" << std::endl;
        } else {
            os << "  string_item[INVALID_OFFSET]" << std::endl;
        }
    }

    os << "constant_pool = [";
    for (size_t i = 0; i < constant_pool_.size(); i++) {
        size_t b = constant_pool_[i];
        os << "0x" << std::setfill('0') << std::setw(2U) << std::right << std::hex << b << std::dec;
        if (i < constant_pool_.size() - 1) {
            os << ", ";
        }
    }
    os << "]" << std::endl;

    os << "line_number_program = line_number_program_idx[";
    if (program_ != nullptr && program_->HasIndex(this)) {
        os << program_->GetIndex(this);
    } else {
        os << "NO_INDEX";
    }
    os << "]";
}

bool MethodHandleItem::Write(Writer *writer)
{
    ASSERT(GetOffset() == writer->GetOffset());

    if (!writer->WriteByte(static_cast<uint8_t>(type_))) {
        return false;
    }

    return writer->WriteUleb128(entity_->GetOffset());
}

}  // namespace panda::panda_file
