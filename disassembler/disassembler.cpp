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

#include "disassembler.h"
#include "mangling.h"
#include "utils/logger.h"

#include <iomanip>

namespace panda::disasm {

Disassembler::Disassembler(Disassembler &&that)
{
    this->file_ = std::move(that.file_);

    this->prog_ = std::move(that.prog_);

    this->file_language_ = std::move(that.file_language_);

    this->record_name_to_id_ = std::move(that.record_name_to_id_);
    this->method_name_to_id_ = std::move(that.method_name_to_id_);

    this->skip_strings_ = std::move(that.skip_strings_);
    this->quiet_ = std::move(that.quiet_);

    this->prog_info_ = std::move(that.prog_info_);
    this->prog_j_ann_ = std::move(that.prog_j_ann_);
}

void Disassembler::Disassemble(const std::string &filename_in, bool quiet, bool skip_strings)
{
    auto file_new = panda_file::File::Open(filename_in);
    file_.swap(file_new);

    if (file_ != nullptr) {
        prog_ = pandasm::Program {};

        record_name_to_id_.clear();
        method_name_to_id_.clear();

        skip_strings_ = skip_strings;
        quiet_ = quiet;

        prog_info_ = ProgInfo {};
        prog_j_ann_ = ProgJavaAnnotations {};

        GetLiteralArrays();
        GetRecords();

        GetLanguageSpecificMetadata();
    } else {
        LOG(ERROR, DISASSEMBLER) << "> Failed to open the specified pandafile: <" << filename_in << ">";
    }
}

void Disassembler::CollectInfo()
{
    LOG(DEBUG, DISASSEMBLER) << "\n[getting program info]\n";

    for (const auto &pair : record_name_to_id_) {
        GetRecordInfo(pair.second, &prog_info_.records_info[pair.first]);
    }

    for (const auto &pair : method_name_to_id_) {
        GetMethodInfo(pair.second, &prog_info_.methods_info[pair.first]);
    }
}

void Disassembler::Serialize(std::ostream &os, bool add_separators, bool print_information) const
{
    if (os.bad()) {
        LOG(DEBUG, DISASSEMBLER) << "> serialization failed. os bad\n";
        return;
    }

    if (file_ != nullptr) {
        os << "#\n# source binary: " << file_->GetFilename() << "\n#\n\n";
    }

    SerializeLanguage(os);

    if (add_separators) {
        os << "# ====================\n"
              "# LITERALS\n\n";
    }

    LOG(DEBUG, DISASSEMBLER) << "[serializing literals]";

    size_t index = 0;
    for (const auto &pair : prog_.literalarray_table) {
        Serialize(index++, pair.second, os);
    }

    os << "\n";

    if (add_separators) {
        os << "# ====================\n"
              "# RECORDS\n\n";
    }

    LOG(DEBUG, DISASSEMBLER) << "[serializing records]";

    for (const auto &r : prog_.record_table) {
        Serialize(r.second, os, print_information);
    }

    if (add_separators) {
        os << "# ====================\n"
              "# METHODS\n\n";
    }

    LOG(DEBUG, DISASSEMBLER) << "[serializing methods]";

    for (const auto &m : prog_.function_table) {
        Serialize(m.second, os, print_information);
    }
}

inline bool Disassembler::IsPandasmFriendly(const char c)
{
    return isalnum(c) || c == '_';
}

inline bool Disassembler::IsSystemType(const std::string &type_name)
{
    bool is_array_type = (type_name.find('[') != std::string::npos);
    bool is_global = (type_name == "_GLOBAL");

    return is_array_type || is_global;
}

std::string Disassembler::MakePandasmFriendly(const std::string &str)
{
    auto str_new = str;
    std::replace_if(
        str_new.begin(), str_new.end(), [](const char c) { return !IsPandasmFriendly(c); }, '_');

    return str_new;
}

void Disassembler::GetRecord(pandasm::Record *record, const panda_file::File::EntityId &record_id)
{
    LOG(DEBUG, DISASSEMBLER) << "\n[getting record]\nid: " << record_id.GetOffset();

    if (record == nullptr) {
        LOG(ERROR, DISASSEMBLER) << "> nullptr received!";
        return;
    }

    auto language = GetClassLanguage(record_id);
    record->name = GetFullRecordName(record_id, language);

    LOG(DEBUG, DISASSEMBLER) << "name: " << record->name;

    GetMetaData(record, record_id);

    if (!file_->IsExternal(record_id)) {
        GetMethods(record_id);
        GetFields(record, record_id);
    }
}

void Disassembler::GetMethod(pandasm::Function *method, const panda_file::File::EntityId &method_id)
{
    LOG(DEBUG, DISASSEMBLER) << "\n[getting method]\nid: " << method_id.GetOffset();

    if (method == nullptr) {
        LOG(ERROR, DISASSEMBLER) << "> nullptr received!";
        return;
    }

    panda_file::MethodDataAccessor method_accessor(*file_, method_id);
    pandasm::extensions::Language language = PFLangToPandasmLang(method_accessor.GetSourceLang());

    method->name = GetFullMethodName(method_id, language);

    LOG(DEBUG, DISASSEMBLER) << "name: " << method->name;

    GetParams(method, method_accessor.GetProtoId());
    GetMetaData(method, method_id);

    if (method->HasImplementation()) {
        if (method_accessor.GetCodeId().has_value()) {
            const IdList id_list = GetInstructions(method, method_id, method_accessor.GetCodeId().value());

            for (const auto &id : id_list) {
                pandasm::Function new_method("", language);
                GetMethod(&new_method, id);

                method_name_to_id_.emplace(new_method.name, id);
                prog_.function_table.emplace(new_method.name, std::move(new_method));
            }
        } else {
            LOG(ERROR, DISASSEMBLER) << "> error encountered at " << std::dec << method_id << " ("
                                     << "0x" << std::hex << method_id
                                     << "). Implementation of method is expected, but no \'CODE\' tag was found";
        }
    }
}

template <typename T>
void Disassembler::FillLiteralArrayData(pandasm::LiteralArray *lit_array, const panda_file::LiteralTag &tag,
                                        const panda_file::LiteralDataAccessor::LiteralValue &value) const
{
    panda_file::File::EntityId id(std::get<uint32_t>(value));
    auto sp = file_->GetSpanFromId(id);
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE)
    auto len = panda_file::helpers::Read<sizeof(uint32_t)>(&sp);
    if (tag != panda_file::LiteralTag::ARRAY_STRING) {
        for (size_t i = 0; i < len; i++) {
            pandasm::LiteralArray::Literal lit;
            lit.tag_ = tag;
            lit.value_ = bit_cast<T>(panda_file::helpers::Read<sizeof(T)>(&sp));
            lit_array->literals_.push_back(lit);
        }
        return;
    }
    for (size_t i = 0; i < len; i++) {
        // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE)
        auto str_id = panda_file::helpers::Read<sizeof(T)>(&sp);
        pandasm::LiteralArray::Literal lit;
        lit.tag_ = tag;
        lit.value_ = StringDataToString(file_->GetStringData(panda_file::File::EntityId(str_id)));
        lit_array->literals_.push_back(lit);
    }
}

void Disassembler::GetLiteralArray(pandasm::LiteralArray *lit_array, const size_t index) const
{
    LOG(DEBUG, DISASSEMBLER) << "\n[getting literal array]\nindex: " << index;

    panda_file::LiteralDataAccessor lit_array_accessor(*file_, file_->GetLiteralArraysId());

    lit_array_accessor.EnumerateLiteralVals(
        index, [this, lit_array](const panda_file::LiteralDataAccessor::LiteralValue &value,
                                 const panda_file::LiteralTag &tag) {
            switch (tag) {
                case panda_file::LiteralTag::ARRAY_I8: {
                    FillLiteralArrayData<uint8_t>(lit_array, tag, value);
                    break;
                }
                case panda_file::LiteralTag::ARRAY_I16: {
                    FillLiteralArrayData<uint16_t>(lit_array, tag, value);
                    break;
                }
                case panda_file::LiteralTag::ARRAY_I32: {
                    FillLiteralArrayData<uint32_t>(lit_array, tag, value);
                    break;
                }
                case panda_file::LiteralTag::ARRAY_I64: {
                    FillLiteralArrayData<uint64_t>(lit_array, tag, value);
                    break;
                }
                case panda_file::LiteralTag::ARRAY_F32: {
                    FillLiteralArrayData<float>(lit_array, tag, value);
                    break;
                }
                case panda_file::LiteralTag::ARRAY_F64: {
                    FillLiteralArrayData<double>(lit_array, tag, value);
                    break;
                }
                case panda_file::LiteralTag::ARRAY_STRING: {
                    FillLiteralArrayData<uint32_t>(lit_array, tag, value);
                    break;
                }
                case panda_file::LiteralTag::TAGVALUE:
                case panda_file::LiteralTag::ACCESSOR:
                case panda_file::LiteralTag::NULLVALUE: {
                    break;
                }
                default: {
                    UNREACHABLE();
                    break;
                }
            }
        });
}

void Disassembler::GetLiteralArrays()
{
    const auto lit_arrays_id = file_->GetLiteralArraysId();

    LOG(DEBUG, DISASSEMBLER) << "\n[getting literal arrays]\nid: " << lit_arrays_id.GetOffset() << "\n";

    panda_file::LiteralDataAccessor lit_array_accessor(*file_, lit_arrays_id);
    size_t num_litarrays = lit_array_accessor.GetLiteralNum();
    for (size_t index = 0; index < num_litarrays; index++) {
        panda::pandasm::LiteralArray lit_ar;
        GetLiteralArray(&lit_ar, index);
        prog_.literalarray_table.emplace(std::to_string(index), lit_ar);
    }
}

void Disassembler::GetRecords()
{
    LOG(DEBUG, DISASSEMBLER) << "\n[getting records]\n";

    const auto class_idx = file_->GetClasses();
    for (size_t i = 0; i < class_idx.size(); i++) {
        uint32_t id = class_idx[i];

        if (id > file_->GetHeader()->file_size) {
            LOG(ERROR, DISASSEMBLER) << "> error encountered at " << std::dec
                                     << file_->GetHeader()->class_idx_off + sizeof(uint32_t) * i << " ("
                                     << "0x" << std::hex << file_->GetHeader()->class_idx_off + sizeof(uint32_t) * i
                                     << "). binary file corrupted. record offset (" << id << ") out of bounds ("
                                     << file_->GetHeader()->file_size << ")!";
            break;
        }

        const panda_file::File::EntityId record_id {id};
        auto language = GetClassLanguage(record_id);
        if (language != file_language_) {
            if (file_language_ == pandasm::extensions::Language::PANDA_ASSEMBLY) {
                file_language_ = language;
            } else {
                LOG(ERROR, DISASSEMBLER) << "> possible error encountered at " << std::dec
                                         << file_->GetHeader()->class_idx_off + sizeof(uint32_t) * i << " ("
                                         << "0x" << std::hex << file_->GetHeader()->class_idx_off + sizeof(uint32_t) * i
                                         << "). record's language differs from file's language (or is default)!";
            }
        }

        pandasm::Record record("", language);
        GetRecord(&record, record_id);

        if (prog_.record_table.find(record.name) == prog_.record_table.end()) {
            record_name_to_id_.emplace(record.name, record_id);
            prog_.record_table.emplace(record.name, std::move(record));
        }
    }
}

void Disassembler::GetFields(pandasm::Record *record, const panda_file::File::EntityId &record_id)
{
    panda_file::ClassDataAccessor class_accessor {*file_, record_id};

    class_accessor.EnumerateFields([&](panda_file::FieldDataAccessor &field_accessor) -> void {
        pandasm::Field field(record->language);

        panda_file::File::EntityId field_name_id = field_accessor.GetNameId();
        field.name = StringDataToString(file_->GetStringData(field_name_id));

        uint32_t field_type = field_accessor.GetType();
        field.type = FieldTypeToPandasmType(field_type);

        GetMetaData(&field, field_accessor.GetFieldId());

        record->field_list.push_back(std::move(field));
    });
}

void Disassembler::GetMethods(const panda_file::File::EntityId &record_id)
{
    panda_file::ClassDataAccessor class_accessor {*file_, record_id};

    pandasm::extensions::Language language = PFLangToPandasmLang(class_accessor.GetSourceLang());

    class_accessor.EnumerateMethods([&](panda_file::MethodDataAccessor &method_accessor) -> void {
        const auto method_id = method_accessor.GetMethodId();

        pandasm::Function method("", language);
        GetMethod(&method, method_id);

        if (prog_.function_table.find(method.name) == prog_.function_table.end()) {
            method_name_to_id_.emplace(method.name, method_id);
            prog_.function_table.emplace(method.name, std::move(method));
        }
    });
}

void Disassembler::GetParams(pandasm::Function *method, const panda_file::File::EntityId &proto_id) const
{
    /**
     * frame size - 2^16 - 1
     */
    static const uint32_t MAX_ARG_NUM = 0xFFFF;

    LOG(DEBUG, DISASSEMBLER) << "[getting params]\nproto id: " << proto_id.GetOffset();

    if (method == nullptr) {
        LOG(ERROR, DISASSEMBLER) << "> nullptr received!";
        return;
    }

    panda_file::ProtoDataAccessor proto_accessor(*file_, proto_id);

    auto params_num = proto_accessor.GetNumArgs();
    if (params_num > MAX_ARG_NUM) {
        LOG(ERROR, DISASSEMBLER) << "> error encountered at " << std::dec << proto_id.GetOffset() << " ("
                                 << "0x" << std::hex << proto_id.GetOffset() << "). number of function's arguments ("
                                 << params_num << ") exceeds MAX_ARG_NUM (" << MAX_ARG_NUM << ") !";

        return;
    }

    size_t ref_idx = 0;
    method->return_type = PFTypeToPandasmType(proto_accessor.GetReturnType(), proto_accessor, ref_idx);

    for (uint8_t i = 0; i < params_num; i++) {
        auto arg_type = PFTypeToPandasmType(proto_accessor.GetArgType(i), proto_accessor, ref_idx);
        method->params.push_back(pandasm::Function::Parameter(arg_type, method->language));
    }
}

LabelTable Disassembler::GetExceptions(pandasm::Function *method, panda_file::File::EntityId method_id,
                                       panda_file::File::EntityId code_id) const
{
    LOG(DEBUG, DISASSEMBLER) << "[getting exceptions]\ncode id: " << code_id.GetOffset();
    if (method == nullptr) {
        LOG(DEBUG, DISASSEMBLER) << "> nullptr received!\n";
        return LabelTable {};
    }
    panda_file::CodeDataAccessor code_accessor(*file_, code_id);

    const auto bc_ins = BytecodeInstruction(code_accessor.GetInstructions());
    const auto bc_ins_last = bc_ins.JumpTo(code_accessor.GetCodeSize());

    size_t try_idx = 0;
    LabelTable label_table {};
    code_accessor.EnumerateTryBlocks([&](panda_file::CodeDataAccessor::TryBlock &try_block) {
        pandasm::Function::CatchBlock catch_block_pa {};
        if (!LocateTryBlock(bc_ins, bc_ins_last, try_block, &catch_block_pa, &label_table, try_idx)) {
            return false;
        }
        size_t catch_idx = 0;
        try_block.EnumerateCatchBlocks([&](panda_file::CodeDataAccessor::CatchBlock &catch_block) {
            auto class_idx = catch_block.GetTypeIdx();
            if (class_idx == panda_file::INVALID_INDEX) {
                catch_block_pa.exception_record = "";
            } else {
                const auto class_id = file_->ResolveClassIndex(method_id, class_idx);
                auto language = GetClassLanguage(class_id);
                catch_block_pa.exception_record = GetFullRecordName(class_id, language);
            }
            if (!LocateCatchBlock(bc_ins, bc_ins_last, catch_block, &catch_block_pa, &label_table, try_idx,
                                  catch_idx)) {
                return false;
            }

            method->catch_blocks.push_back(catch_block_pa);
            catch_block_pa.catch_begin_label = "";
            catch_block_pa.catch_end_label = "";
            catch_idx++;

            return true;
        });
        try_idx++;

        return true;
    });

    return label_table;
}

bool Disassembler::LocateTryBlock(const BytecodeInstruction &bc_ins, const BytecodeInstruction &bc_ins_last,
                                  const panda_file::CodeDataAccessor::TryBlock &try_block,
                                  pandasm::Function::CatchBlock *catch_block_pa, LabelTable *label_table,
                                  size_t try_idx) const
{
    const auto try_begin_bc_ins = bc_ins.JumpTo(try_block.GetStartPc());
    const auto try_end_bc_ins = bc_ins.JumpTo(try_block.GetStartPc() + try_block.GetLength());

    const size_t try_begin_idx = GetBytecodeInstructionNumber(bc_ins, try_begin_bc_ins);
    const size_t try_end_idx = GetBytecodeInstructionNumber(bc_ins, try_end_bc_ins);

    const bool try_begin_offset_in_range = bc_ins_last.GetAddress() > try_begin_bc_ins.GetAddress();
    const bool try_end_offset_in_range = bc_ins_last.GetAddress() >= try_end_bc_ins.GetAddress();
    const bool try_begin_offset_valid = try_begin_idx != std::numeric_limits<size_t>::max();
    const bool try_end_offset_valid = try_end_idx != std::numeric_limits<size_t>::max();

    if (!try_begin_offset_in_range || !try_begin_offset_valid) {
        LOG(ERROR, DISASSEMBLER) << "> invalid try block begin offset! addr is: 0x" << std::hex
                                 << try_begin_bc_ins.GetAddress();
        return false;
    } else {
        std::stringstream ss {};
        ss << "try_begin_label_" << try_idx;

        LabelTable::iterator it = label_table->find(try_begin_idx);
        if (it == label_table->end()) {
            catch_block_pa->try_begin_label = ss.str();
            label_table->insert(std::pair<size_t, std::string>(try_begin_idx, ss.str()));
        } else {
            catch_block_pa->try_begin_label = it->second;
        }
    }

    if (!try_end_offset_in_range || !try_end_offset_valid) {
        LOG(ERROR, DISASSEMBLER) << "> invalid try block end offset! addr is: 0x" << std::hex
                                 << try_end_bc_ins.GetAddress();
        return false;
    } else {
        std::stringstream ss {};
        ss << "try_end_label_" << try_idx;

        LabelTable::iterator it = label_table->find(try_end_idx);
        if (it == label_table->end()) {
            catch_block_pa->try_end_label = ss.str();
            label_table->insert(std::pair<size_t, std::string>(try_end_idx, ss.str()));
        } else {
            catch_block_pa->try_end_label = it->second;
        }
    }

    return true;
}

bool Disassembler::LocateCatchBlock(const BytecodeInstruction &bc_ins, const BytecodeInstruction &bc_ins_last,
                                    const panda_file::CodeDataAccessor::CatchBlock &catch_block,
                                    pandasm::Function::CatchBlock *catch_block_pa, LabelTable *label_table,
                                    size_t try_idx, size_t catch_idx) const
{
    const auto handler_begin_offset = catch_block.GetHandlerPc();
    const auto handler_end_offset = handler_begin_offset + catch_block.GetCodeSize();

    const auto handler_begin_bc_ins = bc_ins.JumpTo(handler_begin_offset);
    const auto handler_end_bc_ins = bc_ins.JumpTo(handler_end_offset);

    const size_t handler_begin_idx = GetBytecodeInstructionNumber(bc_ins, handler_begin_bc_ins);
    const size_t handler_end_idx = GetBytecodeInstructionNumber(bc_ins, handler_end_bc_ins);

    const bool handler_begin_offset_in_range = bc_ins_last.GetAddress() > handler_begin_bc_ins.GetAddress();
    const bool handler_end_offset_in_range = bc_ins_last.GetAddress() > handler_end_bc_ins.GetAddress();
    const bool handler_end_present = catch_block.GetCodeSize() != 0;
    const bool handler_begin_offset_valid = handler_begin_idx != std::numeric_limits<size_t>::max();
    const bool handler_end_offset_valid = handler_end_idx != std::numeric_limits<size_t>::max();

    if (!handler_begin_offset_in_range || !handler_begin_offset_valid) {
        LOG(ERROR, DISASSEMBLER) << "> invalid catch block begin offset! addr is: 0x" << std::hex
                                 << handler_begin_bc_ins.GetAddress();
        return false;
    } else {
        std::stringstream ss {};
        ss << "handler_begin_label_" << try_idx << "_" << catch_idx;

        LabelTable::iterator it = label_table->find(handler_begin_idx);
        if (it == label_table->end()) {
            catch_block_pa->catch_begin_label = ss.str();
            label_table->insert(std::pair<size_t, std::string>(handler_begin_idx, ss.str()));
        } else {
            catch_block_pa->catch_begin_label = it->second;
        }
    }

    if (!handler_end_offset_in_range || !handler_end_offset_valid) {
        LOG(ERROR, DISASSEMBLER) << "> invalid catch block end offset! addr is: 0x" << std::hex
                                 << handler_end_bc_ins.GetAddress();
        return false;
    } else if (handler_end_present) {
        std::stringstream ss {};
        ss << "handler_end_label_" << try_idx << "_" << catch_idx;

        LabelTable::iterator it = label_table->find(handler_end_idx);
        if (it == label_table->end()) {
            catch_block_pa->catch_end_label = ss.str();
            label_table->insert(std::pair<size_t, std::string>(handler_end_idx, ss.str()));
        } else {
            catch_block_pa->catch_end_label = it->second;
        }
    }

    return true;
}

void Disassembler::GetMetaData(pandasm::Function *method, const panda_file::File::EntityId &method_id) const
{
    LOG(DEBUG, DISASSEMBLER) << "[getting metadata]\nmethod id: " << method_id;

    if (method == nullptr) {
        LOG(ERROR, DISASSEMBLER) << "> nullptr received!";
        return;
    }

    panda_file::MethodDataAccessor method_accessor(*file_, method_id);

    const auto method_name_raw = StringDataToString(file_->GetStringData(method_accessor.GetNameId()));

    if (!method_accessor.IsStatic()) {
        const auto class_name = StringDataToString(file_->GetStringData(method_accessor.GetClassId()));
        auto this_type = pandasm::Type::FromDescriptor(class_name);

        this_type = pandasm::Type(MakePandasmFriendly(this_type.GetComponentName()), this_type.GetRank());

        LOG(DEBUG, DISASSEMBLER) << "method is not static. emplacing self-argument of type " << this_type.GetName();

        method->params.insert(method->params.begin(), pandasm::Function::Parameter(this_type, method->language));
    } else {
        method->metadata->SetAttribute("static");
    }

    if (file_->IsExternal(method_accessor.GetMethodId())) {
        method->metadata->SetAttribute("external");
    }

    if (method_accessor.IsNative()) {
        method->metadata->SetAttribute("native");
    }

    if (method_accessor.IsAbstract()) {
        method->metadata->SetAttribute("noimpl");
    }

    // no language data for external methods
    const bool is_ctor_js =
        method_name_raw == pandasm::extensions::GetCtorName(pandasm::extensions::Language::ECMASCRIPT);
    const bool is_cctor_js =
        method_name_raw == pandasm::extensions::GetCctorName(pandasm::extensions::Language::ECMASCRIPT);
    const bool is_ctor_panda =
        method_name_raw == pandasm::extensions::GetCtorName(pandasm::extensions::Language::PANDA_ASSEMBLY);
    const bool is_cctor_panda =
        method_name_raw == pandasm::extensions::GetCctorName(pandasm::extensions::Language::PANDA_ASSEMBLY);

    const bool is_ctor = is_ctor_js || is_ctor_panda;
    const bool is_cctor = is_cctor_js || is_cctor_panda;

    if (is_ctor) {
        method->metadata->SetAttribute("ctor");
    } else if (is_cctor) {
        method->metadata->SetAttribute("cctor");
    }
}

void Disassembler::GetMetaData(pandasm::Record *record, const panda_file::File::EntityId &record_id) const
{
    LOG(DEBUG, DISASSEMBLER) << "[getting metadata]\nrecord id: " << record_id;

    if (record == nullptr) {
        LOG(ERROR, DISASSEMBLER) << "> nullptr received!";
        return;
    }

    if (file_->IsExternal(record_id)) {
        record->metadata->SetAttribute("external");
    }
}

void Disassembler::GetMetaData(pandasm::Field *field, const panda_file::File::EntityId &field_id) const
{
    LOG(DEBUG, DISASSEMBLER) << "[getting metadata]\nfield id: " << field_id;

    if (field == nullptr) {
        LOG(ERROR, DISASSEMBLER) << "> nullptr received!";
        return;
    }

    panda_file::FieldDataAccessor field_accessor(*file_, field_id);

    if (field_accessor.IsExternal()) {
        field->metadata->SetAttribute("external");
    }

    if (field_accessor.IsStatic()) {
        field->metadata->SetAttribute("static");
    }
}

void Disassembler::GetLanguageSpecificMetadata() const
{
    LOG(DEBUG, DISASSEMBLER) << "\n[getting language-specific annotations]\n";
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_SIZE)
std::string Disassembler::AnnotationTagToString(const char tag) const
{
    switch (tag) {
        case '1':
            return "u1";
        case '2':
            return "i8";
        case '3':
            return "u8";
        case '4':
            return "i16";
        case '5':
            return "u16";
        case '6':
            return "i32";
        case '7':
            return "u32";
        case '8':
            return "i64";
        case '9':
            return "u64";
        case 'A':
            return "f32";
        case 'B':
            return "f64";
        case 'C':
            return "string";
        case 'D':
            return "record";
        case 'E':
            return "method";
        case 'F':
            return "enum";
        case 'G':
            return "annotation";
        case 'I':
            return "void";
        case 'J':
            return "method_handle";
        case 'K':
            return "u1[]";
        case 'L':
            return "i8[]";
        case 'M':
            return "u8[]";
        case 'N':
            return "i16[]";
        case 'O':
            return "u16[]";
        case 'P':
            return "i32[]";
        case 'Q':
            return "u32[]";
        case 'R':
            return "i64[]";
        case 'S':
            return "u64[]";
        case 'T':
            return "f32[]";
        case 'U':
            return "f64[]";
        case 'V':
            return "string[]";
        case 'W':
            return "record[]";
        case 'X':
            return "method[]";
        case 'Y':
            return "enum[]";
        case 'Z':
            return "annotation[]";
        case '@':
            return "method_handle[]";
        case '*':
            return "nullptr string";
        default:
            return std::string();
    }
}

std::string Disassembler::ScalarValueToString(const panda_file::ScalarValue &value, const std::string &type) const
{
    std::stringstream ss;

    if (type == "i8") {
        int8_t res = value.Get<int8_t>();
        ss << static_cast<int>(res);
    } else if (type == "u1" || type == "u8") {
        uint8_t res = value.Get<uint8_t>();
        ss << static_cast<unsigned int>(res);
    } else if (type == "i16") {
        ss << value.Get<int16_t>();
    } else if (type == "u16") {
        ss << value.Get<uint16_t>();
    } else if (type == "i32") {
        ss << value.Get<int32_t>();
    } else if (type == "u32") {
        ss << value.Get<uint32_t>();
    } else if (type == "i64") {
        ss << value.Get<int64_t>();
    } else if (type == "u64") {
        ss << value.Get<uint64_t>();
    } else if (type == "f32") {
        ss << value.Get<float>();
    } else if (type == "f64") {
        ss << value.Get<double>();
    } else if (type == "string") {
        const auto id = value.Get<panda_file::File::EntityId>();
        ss << "\"" << StringDataToString(file_->GetStringData(id)) << "\"";
    } else if (type == "record") {
        const auto id = value.Get<panda_file::File::EntityId>();
        auto language = GetClassLanguage(id);
        ss << GetFullRecordName(id, language);
    } else if (type == "method") {
        const auto id = value.Get<panda_file::File::EntityId>();
        auto language = GetClassLanguage(id);
        ss << GetFullMethodName(id, language);
    } else if (type == "enum") {
        const auto id = value.Get<panda_file::File::EntityId>();
        panda_file::FieldDataAccessor field_accessor(*file_, id);
        ss << GetFullRecordName(field_accessor.GetClassId(), pandasm::extensions::Language::PANDA_ASSEMBLY) << "."
           << StringDataToString(file_->GetStringData(field_accessor.GetNameId()));
    } else if (type == "annotation") {
        const auto id = value.Get<panda_file::File::EntityId>();
        ss << "id_" << id.GetOffset();
    } else if (type == "void") {
        return std::string();
    } else if (type == "method_handle") {
    }

    return ss.str();
}

std::string Disassembler::ArrayValueToString(const panda_file::ArrayValue &value, const std::string &type,
                                             const size_t idx) const
{
    std::stringstream ss;

    if (type == "i8") {
        int8_t res = value.Get<int8_t>(idx);
        ss << static_cast<int>(res);
    } else if (type == "u1" || type == "u8") {
        uint8_t res = value.Get<uint8_t>(idx);
        ss << static_cast<unsigned int>(res);
    } else if (type == "i16") {
        ss << value.Get<int16_t>(idx);
    } else if (type == "u16") {
        ss << value.Get<uint16_t>(idx);
    } else if (type == "i32") {
        ss << value.Get<int32_t>(idx);
    } else if (type == "u32") {
        ss << value.Get<uint32_t>(idx);
    } else if (type == "i64") {
        ss << value.Get<int64_t>(idx);
    } else if (type == "u64") {
        ss << value.Get<uint64_t>(idx);
    } else if (type == "f32") {
        ss << value.Get<float>(idx);
    } else if (type == "f64") {
        ss << value.Get<double>(idx);
    } else if (type == "string") {
        const auto id = value.Get<panda_file::File::EntityId>(idx);
        ss << '\"' << StringDataToString(file_->GetStringData(id)) << '\"';
    } else if (type == "record") {
        const auto id = value.Get<panda_file::File::EntityId>(idx);
        auto language = GetClassLanguage(id);
        ss << GetFullRecordName(id, language);
    } else if (type == "method") {
        const auto id = value.Get<panda_file::File::EntityId>(idx);
        panda_file::ClassDataAccessor method_accessor {*file_, id};
        pandasm::extensions::Language language = PFLangToPandasmLang(method_accessor.GetSourceLang());
        ss << GetFullMethodName(id, language);
    } else if (type == "enum") {
        const auto id = value.Get<panda_file::File::EntityId>(idx);
        panda_file::FieldDataAccessor field_accessor(*file_, id);
        ss << GetFullRecordName(field_accessor.GetClassId(), pandasm::extensions::Language::PANDA_ASSEMBLY) << "."
           << StringDataToString(file_->GetStringData(field_accessor.GetNameId()));
    } else if (type == "annotation") {
        const auto id = value.Get<panda_file::File::EntityId>(idx);
        ss << "id_" << id.GetOffset();
    } else if (type == "method_handle") {
    } else if (type == "nullptr string") {
    }

    return ss.str();
}

std::string Disassembler::GetFullMethodName(const panda_file::File::EntityId &method_id,
                                            pandasm::extensions::Language language) const
{
    panda::panda_file::MethodDataAccessor method_accessor(*file_, method_id);

    const auto method_name_raw = StringDataToString(file_->GetStringData(method_accessor.GetNameId()));

    pandasm::Function method(method_name_raw, language);
    GetParams(&method, method_accessor.GetProtoId());
    GetMetaData(&method, method_id);

    method.name = pandasm::MangleFunctionName(method.name, method.params, method.return_type);
    method.name = MakePandasmFriendly(method.name);
    std::string class_name = GetFullRecordName(method_accessor.GetClassId(), language);
    if (IsSystemType(class_name)) {
        class_name = "";
    } else {
        class_name += ".";
    }

    return class_name + method.name;
}

std::string Disassembler::GetFullRecordName(const panda_file::File::EntityId &class_id,
                                            [[maybe_unused]] pandasm::extensions::Language language) const
{
    std::string name = StringDataToString(file_->GetStringData(class_id));

    auto type = pandasm::Type::FromDescriptor(name);
    type = pandasm::Type(MakePandasmFriendly(type.GetComponentName()), type.GetRank());

    return type.GetName();
}

void Disassembler::GetRecordInfo(const panda_file::File::EntityId &record_id, RecordInfo *record_info) const
{
    constexpr size_t DEFAULT_OFFSET_WIDTH = 4;

    if (file_->IsExternal(record_id)) {
        return;
    }

    panda_file::ClassDataAccessor class_accessor {*file_, record_id};
    std::stringstream ss;

    ss << "offset: 0x" << std::setfill('0') << std::setw(DEFAULT_OFFSET_WIDTH) << std::hex
       << class_accessor.GetClassId().GetOffset() << ", size: 0x" << std::setfill('0')
       << std::setw(DEFAULT_OFFSET_WIDTH) << std::hex << class_accessor.GetSize() << " (" << std::dec
       << class_accessor.GetSize() << ")";

    record_info->record_info = ss.str();
    ss.str(std::string());

    class_accessor.EnumerateFields([&](panda_file::FieldDataAccessor &field_accessor) -> void {
        ss << "offset: 0x" << std::setfill('0') << std::setw(DEFAULT_OFFSET_WIDTH) << std::hex
           << field_accessor.GetFieldId().GetOffset() << ", type: 0x" << std::hex << field_accessor.GetType();

        record_info->fields_info.push_back(ss.str());

        ss.str(std::string());
    });
}

void Disassembler::GetMethodInfo(const panda_file::File::EntityId &method_id, MethodInfo *method_info) const
{
    constexpr size_t DEFAULT_OFFSET_WIDTH = 4;

    panda_file::MethodDataAccessor method_accessor {*file_, method_id};
    std::stringstream ss;

    ss << "offset: 0x" << std::setfill('0') << std::setw(DEFAULT_OFFSET_WIDTH) << std::hex
       << method_accessor.GetMethodId().GetOffset();

    if (method_accessor.GetCodeId().has_value()) {
        ss << ", code offset: 0x" << std::setfill('0') << std::setw(DEFAULT_OFFSET_WIDTH) << std::hex
           << method_accessor.GetCodeId().value().GetOffset();

        GetInsInfo(method_accessor.GetCodeId().value(), method_info);
    } else {
        ss << ", <no code>";
    }

    method_info->method_info = ss.str();
}

void Disassembler::Serialize(size_t index, const pandasm::LiteralArray &lit_array, std::ostream &os) const
{
    // remove once literals are supported in assembly_format

    if (lit_array.literals_.empty()) {
        return;
    }

    os << ".array array_" << index << " {\n";

    SerializeValues(lit_array, os);

    os << "}\n";
}

template <class T>
using make_storage = std::conditional_t<std::is_integral_v<T>, std::make_unsigned<T>, std::common_type<T>>;

template <class T>
static void SerializeArrayValues(const pandasm::LiteralArray &lit_array, std::ostream &os)
{
    using S = typename make_storage<T>::type;
    os << std::get<S>(lit_array.literals_[0].value_);

    for (size_t i = 1; i < lit_array.literals_.size(); i++) {
        os << ", " << bit_cast<T>(std::get<S>(lit_array.literals_[i].value_));
    }
}

void Disassembler::SerializeValues(const pandasm::LiteralArray &lit_array, std::ostream &os) const
{
    panda_file::LiteralTag tag = lit_array.literals_[0].tag_;
    switch (tag) {
        case panda_file::LiteralTag::ARRAY_I8: {
            for (size_t i = 0; i < lit_array.literals_.size(); i++) {
                os << "\t"
                   << "i8 " << static_cast<int16_t>(bit_cast<int8_t>(std::get<uint8_t>(lit_array.literals_[i].value_)))
                   << "\n";
            }
            break;
        }
        case panda_file::LiteralTag::ARRAY_I16: {
            for (size_t i = 0; i < lit_array.literals_.size(); i++) {
                os << "\t"
                   << "i16 " << bit_cast<int16_t>(std::get<uint16_t>(lit_array.literals_[i].value_)) << "\n";
            }
            break;
        }
        case panda_file::LiteralTag::ARRAY_I32: {
            for (size_t i = 0; i < lit_array.literals_.size(); i++) {
                os << "\t"
                   << "i32 " << bit_cast<int32_t>(std::get<uint32_t>(lit_array.literals_[i].value_)) << "\n";
            }
            break;
        }
        case panda_file::LiteralTag::ARRAY_I64: {
            for (size_t i = 0; i < lit_array.literals_.size(); i++) {
                os << "\t"
                   << "i64 " << bit_cast<int64_t>(std::get<uint64_t>(lit_array.literals_[i].value_)) << "\n";
            }
            break;
        }
        case panda_file::LiteralTag::ARRAY_F64: {
            for (size_t i = 0; i < lit_array.literals_.size(); i++) {
                os << "\t"
                   << "f64 " << std::get<double>(lit_array.literals_[i].value_) << "\n";
            }
            break;
        }
        case panda_file::LiteralTag::ARRAY_F32: {
            for (size_t i = 0; i < lit_array.literals_.size(); i++) {
                os << "\t"
                   << "f32 " << std::get<float>(lit_array.literals_[i].value_) << "\n";
            }
            break;
        }
        case panda_file::LiteralTag::ARRAY_STRING: {
            for (size_t i = 0; i < lit_array.literals_.size(); i++) {
                os << "\t"
                   << "string " << std::get<std::string>(lit_array.literals_[i].value_) << "\n";
            }
            break;
        }
        default:
            break;
    }
}

void Disassembler::Serialize(const pandasm::Record &record, std::ostream &os, bool print_information) const
{
    if (IsSystemType(record.name)) {
        return;
    }

    os << ".record " << record.name;

    const auto record_iter = prog_j_ann_.record_annotations.find(record.name);
    const bool record_in_table = record_iter != prog_j_ann_.record_annotations.end();
    if (record_in_table) {
        Serialize(*record.metadata, record_iter->second.ann_list, os);
    } else {
        Serialize(*record.metadata, {}, os);
    }

    if (record.metadata->IsForeign()) {
        os << "\n\n";
        return;
    }

    os << " {\n";

    if (print_information && prog_info_.records_info.find(record.name) != prog_info_.records_info.end()) {
        os << " # " << prog_info_.records_info.at(record.name).record_info << "\n";
        SerializeFields(record, os, true);
    } else {
        SerializeFields(record, os, false);
    }

    os << "}\n\n";
}

void Disassembler::SerializeFields(const pandasm::Record &record, std::ostream &os, bool print_information) const
{
    constexpr size_t INFO_OFFSET = 80;

    const auto record_iter = prog_j_ann_.record_annotations.find(record.name);
    const bool record_in_table = record_iter != prog_j_ann_.record_annotations.end();
    const auto rec_inf = (print_information) ? (prog_info_.records_info.at(record.name)) : (RecordInfo {});
    size_t field_idx = 0;

    std::stringstream ss;
    for (const auto &f : record.field_list) {
        ss << "\t" << f.type.GetName() << " " << f.name;
        if (record_in_table) {
            const auto field_iter = record_iter->second.field_annotations.find(f.name);
            if (field_iter != record_iter->second.field_annotations.end()) {
                Serialize(*f.metadata, field_iter->second, ss);
            } else {
                Serialize(*f.metadata, {}, ss);
            }
        } else {
            Serialize(*f.metadata, {}, ss);
        }

        if (print_information) {
            os << std::setw(INFO_OFFSET) << std::left << ss.str() << " # " << rec_inf.fields_info.at(field_idx) << "\n";
        } else {
            os << ss.str() << "\n";
        }

        ss.str(std::string());
        ss.clear();

        field_idx++;
    }
}

void Disassembler::Serialize(const pandasm::Function &method, std::ostream &os, bool print_information) const
{
    os << ".function " << method.return_type.GetName() << " " << method.name << "(";

    if (method.params.size() > 0) {
        os << method.params[0].type.GetName() << " a0";

        for (uint8_t i = 1; i < method.params.size(); i++) {
            os << ", " << method.params[i].type.GetName() << " a" << (size_t)i;
        }
    }
    os << ")";

    const auto method_iter = prog_j_ann_.method_annotations.find(method.name);
    if (method_iter != prog_j_ann_.method_annotations.end()) {
        Serialize(*method.metadata, method_iter->second, os);
    } else {
        Serialize(*method.metadata, {}, os);
    }

    if (!method.HasImplementation()) {
        os << "\n\n";
        return;
    }

    if (print_information && prog_info_.methods_info.find(method.name) != prog_info_.methods_info.end()) {
        const auto method_info = prog_info_.methods_info.at(method.name);

        size_t width = 0;
        for (const auto &i : method.ins) {
            if (i.ToString().size() > width) {
                width = i.ToString().size();
            }
        }

        os << " { # " << method_info.method_info << "\n";

        for (size_t i = 0; i < method.ins.size(); i++) {
            os << "\t" << std::setw(width) << std::left << method.ins.at(i).ToString("", true, method.regs_num) << " # "
               << method_info.instructions_info.at(i) << "\n";
        }
    } else {
        os << " {\n";

        for (const auto &i : method.ins) {
            os << "\t" << i.ToString("", true, method.regs_num) << "\n";
        }
    }

    if (method.catch_blocks.size() != 0) {
        os << "\n";

        for (const auto &catch_block : method.catch_blocks) {
            Serialize(catch_block, os);

            os << "\n";
        }
    }

    os << "}\n\n";
}

void Disassembler::Serialize(const pandasm::Function::CatchBlock &catch_block, std::ostream &os) const
{
    if (catch_block.exception_record == "") {
        os << ".catchall ";
    } else {
        os << ".catch " << catch_block.exception_record << ", ";
    }

    os << catch_block.try_begin_label << ", " << catch_block.try_end_label << ", " << catch_block.catch_begin_label;

    if (catch_block.catch_end_label != "") {
        os << ", " << catch_block.catch_end_label;
    }
}

void Disassembler::Serialize(const pandasm::ItemMetadata &meta, const AnnotationList &ann_list, std::ostream &os) const
{
    auto bool_attributes = meta.GetBoolAttributes();
    auto attributes = meta.GetAttributes();

    if (bool_attributes.empty() && attributes.empty() && ann_list.empty()) {
        return;
    }

    os << " <";

    size_t size = bool_attributes.size();
    size_t idx = 0;
    for (const auto &attr : bool_attributes) {
        os << attr;
        ++idx;

        if (!attributes.empty() || !ann_list.empty() || idx < size) {
            os << ", ";
        }
    }

    size = attributes.size();
    idx = 0;
    for (const auto &[key, values] : attributes) {
        for (size_t i = 0; i < values.size(); i++) {
            os << key << "=" << values[i];

            if (i < values.size() - 1) {
                os << ", ";
            }
        }

        ++idx;

        if (!ann_list.empty() || idx < size) {
            os << ", ";
        }
    }

    size = ann_list.size();
    idx = 0;
    for (const auto &[key, value] : ann_list) {
        os << key << "=" << value;

        ++idx;

        if (idx < size) {
            os << ", ";
        }
    }

    os << ">";
}

void Disassembler::SerializeLanguage(std::ostream &os) const
{
    std::string lang = pandasm::extensions::LanguageToString(file_language_);
    if (!lang.empty()) {
        os << ".language " << lang << "\n\n";
    }
}

pandasm::extensions::Language Disassembler::PFLangToPandasmLang(
    const std::optional<panda_file::SourceLang> &language) const
{
    const auto lang = language.value_or(panda_file::SourceLang::PANDA_ASSEMBLY);
    switch (lang) {
        case panda_file::SourceLang::ECMASCRIPT:
            return pandasm::extensions::Language::ECMASCRIPT;
        case panda_file::SourceLang::PANDA_ASSEMBLY:
            [[fallthrough]];
        default:
            return pandasm::extensions::Language::PANDA_ASSEMBLY;
    }
}

std::string Disassembler::StringDataToString(panda_file::File::StringData sd) const
{
    std::string res(reinterpret_cast<char *>(const_cast<uint8_t *>(sd.data)));
    return res;
}

pandasm::Opcode Disassembler::BytecodeOpcodeToPandasmOpcode(uint8_t o) const
{
    return BytecodeOpcodeToPandasmOpcode(BytecodeInstruction::Opcode(o));
}

std::string Disassembler::IDToString(BytecodeInstruction bc_ins, panda_file::File::EntityId method_id,
                                     pandasm::extensions::Language language) const
{
    std::stringstream name;

    if (bc_ins.HasFlag(BytecodeInstruction::Flags::TYPE_ID)) {
        auto idx = bc_ins.GetId().AsIndex();
        auto id = file_->ResolveClassIndex(method_id, idx);
        name << StringDataToString(file_->GetStringData(id));

        auto type = pandasm::Type::FromDescriptor(name.str());
        type = pandasm::Type(MakePandasmFriendly(type.GetComponentName()), type.GetRank());

        name.str("");
        name << type.GetName();
    } else if (bc_ins.HasFlag(BytecodeInstruction::Flags::METHOD_ID)) {
        auto idx = bc_ins.GetId().AsIndex();
        auto id = file_->ResolveMethodIndex(method_id, idx);
        panda_file::MethodDataAccessor method_accessor(*file_, id);

        name << GetFullMethodName(method_accessor.GetMethodId(), language);
    } else if (bc_ins.HasFlag(BytecodeInstruction::Flags::STRING_ID)) {
        name << '\"';

        if (skip_strings_ || quiet_) {
            name << std::hex << "0x" << bc_ins.GetId().AsFileId();
        } else {
            name << StringDataToString(file_->GetStringData(bc_ins.GetId().AsFileId()));
        }

        name << '\"';
    } else if (bc_ins.HasFlag(BytecodeInstruction::Flags::FIELD_ID)) {
        auto idx = bc_ins.GetId().AsIndex();
        auto id = file_->ResolveFieldIndex(method_id, idx);
        panda_file::FieldDataAccessor field_accessor(*file_, id);

        name << GetFullRecordName(field_accessor.GetClassId(), language);
        name << '.';
        name << StringDataToString(file_->GetStringData(field_accessor.GetNameId()));
    } else if (bc_ins.HasFlag(BytecodeInstruction::Flags::LITERALARRAY_ID)) {
        panda_file::LiteralDataAccessor lit_array_accessor(*file_, file_->GetLiteralArraysId());
        auto idx = bc_ins.GetId().AsFileId().GetOffset();

        name << idx;
    }

    return name.str();
}

size_t Disassembler::GetBytecodeInstructionNumber(BytecodeInstruction bc_ins_first,
                                                  BytecodeInstruction bc_ins_cur) const
{
    size_t count = 0;

    while (bc_ins_first.GetAddress() != bc_ins_cur.GetAddress()) {
        count++;
        bc_ins_first = bc_ins_first.GetNext();
        if (bc_ins_first.GetAddress() > bc_ins_cur.GetAddress()) {
            return std::numeric_limits<size_t>::max();
        }
    }

    return count;
}

pandasm::extensions::Language Disassembler::GetClassLanguage(panda_file::File::EntityId class_id) const
{
    if (file_->IsExternal(class_id)) {
        return pandasm::extensions::Language::PANDA_ASSEMBLY;
    }

    panda_file::ClassDataAccessor cda(*file_, class_id);
    return PFLangToPandasmLang(cda.GetSourceLang());
}

IdList Disassembler::GetInstructions(pandasm::Function *method, panda_file::File::EntityId method_id,
                                     panda_file::File::EntityId code_id) const
{
    panda_file::CodeDataAccessor code_accessor(*file_, code_id);

    const auto ins_sz = code_accessor.GetCodeSize();
    const auto ins_arr = code_accessor.GetInstructions();

    method->regs_num = code_accessor.GetNumVregs();

    auto bc_ins = BytecodeInstruction(ins_arr);
    const auto bc_ins_last = bc_ins.JumpTo(ins_sz);

    LabelTable label_table = GetExceptions(method, method_id, code_id);

    IdList unknown_external_methods {};

    while (bc_ins.GetAddress() != bc_ins_last.GetAddress()) {
        if (bc_ins.GetAddress() > bc_ins_last.GetAddress()) {
            LOG(ERROR, DISASSEMBLER) << "> error encountered at " << std::dec << code_id.GetOffset() << " ("
                                     << "0x" << std::hex << code_id.GetOffset()
                                     << "). bytecode instructions sequence corrupted for method " << method->name
                                     << "! went out of bounds";

            break;
        }

        auto pa_ins = BytecodeInstructionToPandasmInstruction(bc_ins, method_id, method->language);
        // alter instructions operands depending on instruction type
        if (pa_ins.IsConditionalJump() || pa_ins.IsJump()) {
            const int32_t jmp_offset = std::get<int64_t>(pa_ins.imms.at(0));
            const auto bc_ins_dest = bc_ins.JumpTo(jmp_offset);
            if (bc_ins_last.GetAddress() > bc_ins_dest.GetAddress()) {
                size_t idx = GetBytecodeInstructionNumber(BytecodeInstruction(ins_arr), bc_ins_dest);

                if (idx != std::numeric_limits<size_t>::max()) {
                    if (label_table.find(idx) == label_table.end()) {
                        std::stringstream ss {};
                        ss << "jump_label_" << label_table.size();
                        label_table[idx] = ss.str();
                    }

                    pa_ins.imms.clear();
                    pa_ins.ids.push_back(label_table[idx]);
                } else {
                    LOG(ERROR, DISASSEMBLER)
                        << "> error encountered at " << std::dec << code_id.GetOffset() << " ("
                        << "0x" << std::hex << code_id.GetOffset() << "). incorrect instruction at offset "
                        << (bc_ins.GetAddress() - ins_arr) << ": invalid jump offset " << jmp_offset
                        << " - jumping in the middle of another instruction!";
                }
            } else {
                LOG(ERROR, DISASSEMBLER) << "> error encountered at " << std::dec << code_id.GetOffset() << " ("
                                         << "0x" << std::hex << code_id.GetOffset()
                                         << "). incorrect instruction at offset: " << (bc_ins.GetAddress() - ins_arr)
                                         << ": invalid jump offset " << jmp_offset << " - jumping out of bounds!";
            }
        }

        // check if method id is unknown external method. if so, emplace it in table
        if (bc_ins.HasFlag(BytecodeInstruction::Flags::METHOD_ID)) {
            const auto arg_method_idx = bc_ins.GetId().AsIndex();
            const auto arg_method_id = file_->ResolveMethodIndex(method_id, arg_method_idx);

            const auto arg_method_name = GetFullMethodName(arg_method_id, method->language);

            const bool is_present = prog_.function_table.find(arg_method_name) != prog_.function_table.cend();
            const bool is_external = file_->IsExternal(arg_method_id);
            if (is_external && !is_present) {
                unknown_external_methods.push_back(arg_method_id);
            }
        }

        method->ins.push_back(pa_ins);
        bc_ins = bc_ins.GetNext();
    }

    for (const auto &pair : label_table) {
        method->ins[pair.first].label = pair.second;
        method->ins[pair.first].set_label = true;
    }

    return unknown_external_methods;
}

}  // namespace panda::disasm
