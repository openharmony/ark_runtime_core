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

#include "debug_info_extractor.h"
#include "line_program_state.h"
#include "class_data_accessor-inl.h"
#include "debug_data_accessor-inl.h"
#include "utils/utf.h"

namespace panda::panda_file {

static const char *GetStringFromConstantPool(const File &pf, uint32_t offset)
{
    return utf::Mutf8AsCString(pf.GetStringData(File::EntityId(offset)).data);
}

DebugInfoExtractor::DebugInfoExtractor(const File *pf)
{
    Extract(pf);
}

class LineNumberProgramProcessor {
public:
    LineNumberProgramProcessor(LineProgramState state, const uint8_t *program) : state_(state), program_(program) {}

    ~LineNumberProgramProcessor() = default;

    NO_COPY_SEMANTIC(LineNumberProgramProcessor);
    NO_MOVE_SEMANTIC(LineNumberProgramProcessor);

    void Process()
    {
        auto opcode = ReadOpcode();
        lnt_.push_back({state_.GetAddress(), state_.GetLine()});
        while (opcode != Opcode::END_SEQUENCE) {
            switch (opcode) {
                case Opcode::ADVANCE_LINE: {
                    HandleAdvanceLine();
                    break;
                }
                case Opcode::ADVANCE_PC: {
                    HandleAdvancePc();
                    break;
                }
                case Opcode::SET_FILE: {
                    HandleSetFile();
                    break;
                }
                case Opcode::SET_SOURCE_CODE: {
                    HandleSetSourceCode();
                    break;
                }
                case Opcode::SET_PROLOGUE_END:
                case Opcode::SET_EPILOGUE_BEGIN:
                    break;
                case Opcode::START_LOCAL: {
                    HandleStartLocal();
                    break;
                }
                case Opcode::START_LOCAL_EXTENDED: {
                    HandleStartLocalExtended();
                    break;
                }
                case Opcode::RESTART_LOCAL: {
                    LOG(FATAL, PANDAFILE) << "Opcode RESTART_LOCAL is not supported";
                    break;
                }
                case Opcode::END_LOCAL: {
                    HandleEndLocal();
                    break;
                }
                default: {
                    HandleSpecialOpcode(opcode);
                    break;
                }
            }

            opcode = ReadOpcode();
        }

        ProcessVars();
    }

    LineNumberTable GetLineNumberTable() const
    {
        return lnt_;
    }

    LocalVariableTable GetLocalVariableTable() const
    {
        return lvt_;
    }

    const uint8_t *GetFile() const
    {
        return state_.GetFile();
    }

    const uint8_t *GetSourceCode() const
    {
        return state_.GetSourceCode();
    }

private:
    using Opcode = LineNumberProgramItem::Opcode;

    void ProcessVars()
    {
        for (auto &var : lvt_) {
            if (var.end_offset == 0) {
                var.end_offset = state_.GetAddress();
            }
        }
    }

    Opcode ReadOpcode()
    {
        auto opcode = static_cast<Opcode>(*program_);
        ++program_;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return opcode;
    }

    int32_t ReadRegisterNumber()
    {
        auto [regiser_number, n, is_full] = leb128::DecodeSigned<int32_t>(program_);
        LOG_IF(!is_full, FATAL, COMMON) << "Cannot read a register number";
        program_ += n;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return regiser_number;
    }

    void HandleAdvanceLine()
    {
        auto line_diff = state_.ReadSLeb128();
        state_.AdvanceLine(line_diff);
    }

    void HandleAdvancePc()
    {
        auto pc_diff = state_.ReadULeb128();
        state_.AdvancePc(pc_diff);
    }

    void HandleSetFile()
    {
        state_.SetFile(state_.ReadULeb128());
    }

    void HandleSetSourceCode()
    {
        state_.SetSourceCode(state_.ReadULeb128());
    }

    void HandleSetPrologueEnd() {}

    void HandleSetEpilogueBegin() {}

    void HandleStartLocal()
    {
        auto reg_number = ReadRegisterNumber();
        auto name_index = state_.ReadULeb128();
        auto type_index = state_.ReadULeb128();
        const char *name = GetStringFromConstantPool(state_.GetPandaFile(), name_index);
        const char *type = GetStringFromConstantPool(state_.GetPandaFile(), type_index);
        lvt_.push_back({name, type, type, reg_number, state_.GetAddress(), 0});
    }

    void HandleStartLocalExtended()
    {
        auto reg_number = ReadRegisterNumber();
        auto name_index = state_.ReadULeb128();
        auto type_index = state_.ReadULeb128();
        auto type_signature_index = state_.ReadULeb128();
        const char *name = GetStringFromConstantPool(state_.GetPandaFile(), name_index);
        const char *type = GetStringFromConstantPool(state_.GetPandaFile(), type_index);
        const char *type_sign = GetStringFromConstantPool(state_.GetPandaFile(), type_signature_index);
        lvt_.push_back({name, type, type_sign, reg_number, state_.GetAddress(), 0});
    }

    void HandleEndLocal()
    {
        auto reg_number = ReadRegisterNumber();
        bool found = false;
        for (auto it = lvt_.rbegin(); it != lvt_.rend(); ++it) {
            if (it->reg_number == reg_number) {
                it->end_offset = state_.GetAddress();
                found = true;
                break;
            }
        }
        if (!found) {
            LOG(FATAL, PANDAFILE) << "Unknown variable";
        }
    }

    void HandleSpecialOpcode(LineNumberProgramItem::Opcode opcode)
    {
        ASSERT(static_cast<uint8_t>(opcode) >= LineNumberProgramItem::OPCODE_BASE);

        auto adjust_opcode = static_cast<uint8_t>(opcode) - LineNumberProgramItem::OPCODE_BASE;
        uint32_t pc_offset = adjust_opcode / LineNumberProgramItem::LINE_RANGE;
        int32_t line_offset = adjust_opcode % LineNumberProgramItem::LINE_RANGE + LineNumberProgramItem::LINE_BASE;
        state_.AdvancePc(pc_offset);
        state_.AdvanceLine(line_offset);
        lnt_.push_back({state_.GetAddress(), state_.GetLine()});
    }

    LineProgramState state_;
    const uint8_t *program_;
    LineNumberTable lnt_;
    LocalVariableTable lvt_;
};

void DebugInfoExtractor::Extract(const File *pf)
{
    ASSERT(pf != nullptr);
    const auto &panda_file = *pf;
    auto classes = pf->GetClasses();
    for (size_t i = 0; i < classes.Size(); i++) {
        File::EntityId id(classes[i]);
        if (panda_file.IsExternal(id)) {
            continue;
        }

        ClassDataAccessor cda(panda_file, id);

        auto source_file_id = cda.GetSourceFileId();

        cda.EnumerateMethods([&](MethodDataAccessor &mda) {
            auto debug_info_id = mda.GetDebugInfoId();

            if (!debug_info_id) {
                return;
            }

            DebugInfoDataAccessor dda(panda_file, debug_info_id.value());
            ProtoDataAccessor pda(panda_file, mda.GetProtoId());

            std::vector<std::string> param_names;

            dda.EnumerateParameters([&](File::EntityId &param_id) {
                if (param_id.IsValid()) {
                    param_names.emplace_back(utf::Mutf8AsCString(pf->GetStringData(param_id).data));
                } else {
                    param_names.emplace_back();
                }
            });

            const uint8_t *program = dda.GetLineNumberProgram();

            LineProgramState state(panda_file, source_file_id.value_or(File::EntityId(0)), dda.GetLineStart(),
                                   dda.GetConstantPool());

            LineNumberProgramProcessor program_processor(state, program);
            program_processor.Process();

            File::EntityId method_id = mda.GetMethodId();
            const char *source_file = utf::Mutf8AsCString(program_processor.GetFile());
            const char *source_code = utf::Mutf8AsCString(program_processor.GetSourceCode());
            methods_.push_back({source_file, source_code, method_id, program_processor.GetLineNumberTable(),
                                program_processor.GetLocalVariableTable(), std::move(param_names)});
        });
    }
}

const LineNumberTable &DebugInfoExtractor::GetLineNumberTable(File::EntityId method_id) const
{
    for (const auto &method : methods_) {
        if (method.method_id == method_id) {
            return method.line_number_table;
        }
    }

    static const LineNumberTable EMPTY_LINE_TABLE {};  // NOLINT(fuchsia-statically-constructed-objects)
    return EMPTY_LINE_TABLE;
}

const LocalVariableTable &DebugInfoExtractor::GetLocalVariableTable(File::EntityId method_id) const
{
    for (const auto &method : methods_) {
        if (method.method_id == method_id) {
            return method.local_variable_table;
        }
    }

    static const LocalVariableTable EMPTY_VARIABLE_TABLE {};  // NOLINT(fuchsia-statically-constructed-objects)
    return EMPTY_VARIABLE_TABLE;
}

const std::vector<std::string> &DebugInfoExtractor::GetParameterNames(File::EntityId method_id) const
{
    for (const auto &method : methods_) {
        if (method.method_id == method_id) {
            return method.param_names;
        }
    }

    static const std::vector<std::string> EMPTY_PARAM_LIST {};  // NOLINT(fuchsia-statically-constructed-objects)
    return EMPTY_PARAM_LIST;
}

const char *DebugInfoExtractor::GetSourceFile(File::EntityId method_id) const
{
    for (const auto &method : methods_) {
        if (method.method_id == method_id) {
            return method.source_file.c_str();
        }
    }
    return "";
}

const char *DebugInfoExtractor::GetSourceCode(File::EntityId method_id) const
{
    for (const auto &method : methods_) {
        if (method.method_id == method_id) {
            return method.source_code.c_str();
        }
    }
    return "";
}

std::vector<File::EntityId> DebugInfoExtractor::GetMethodIdList() const
{
    std::vector<File::EntityId> list;

    for (const auto &method : methods_) {
        list.push_back(method.method_id);
    }
    return list;
}

}  // namespace panda::panda_file
