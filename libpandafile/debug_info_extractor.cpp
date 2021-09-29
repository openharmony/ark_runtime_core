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

#include "debug_info_extractor.h"
#include "line_number_program.h"
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

class LineNumberProgramHandler {
public:
    explicit LineNumberProgramHandler(LineProgramState *state) : state_(state) {}
    ~LineNumberProgramHandler() = default;

    NO_COPY_SEMANTIC(LineNumberProgramHandler);
    NO_MOVE_SEMANTIC(LineNumberProgramHandler);

    LineProgramState *GetState() const
    {
        return state_;
    }

    void ProcessBegin()
    {
        lnt_.push_back({state_->GetAddress(), state_->GetLine()});
    }

    void ProcessEnd()
    {
        ProcessVars();
    }

    bool HandleAdvanceLine(int32_t line_diff) const
    {
        state_->AdvanceLine(line_diff);
        return true;
    }

    bool HandleAdvancePc(uint32_t pc_diff) const
    {
        state_->AdvancePc(pc_diff);
        return true;
    }

    bool HandleSetFile(uint32_t source_file_id) const
    {
        state_->SetFile(source_file_id);
        return true;
    }

    bool HandleSetSourceCode(uint32_t source_code_id) const
    {
        state_->SetSourceCode(source_code_id);
        return true;
    }

    bool HandleSetPrologueEnd() const
    {
        return true;
    }

    bool HandleSetEpilogueBegin() const
    {
        return true;
    }

    bool HandleStartLocal(int32_t reg_number, uint32_t name_id, uint32_t type_id)
    {
        const char *name = GetStringFromConstantPool(state_->GetPandaFile(), name_id);
        const char *type = GetStringFromConstantPool(state_->GetPandaFile(), type_id);
        lvt_.push_back({name, type, type, reg_number, state_->GetAddress(), 0});
        return true;
    }

    bool HandleStartLocalExtended(int32_t reg_number, uint32_t name_id, uint32_t type_id, uint32_t type_signature_id)
    {
        const char *name = GetStringFromConstantPool(state_->GetPandaFile(), name_id);
        const char *type = GetStringFromConstantPool(state_->GetPandaFile(), type_id);
        const char *type_sign = GetStringFromConstantPool(state_->GetPandaFile(), type_signature_id);
        lvt_.push_back({name, type, type_sign, reg_number, state_->GetAddress(), 0});
        return true;
    }

    bool HandleEndLocal(int32_t reg_number)
    {
        bool found = false;
        for (auto it = lvt_.rbegin(); it != lvt_.rend(); ++it) {
            if (it->reg_number == reg_number) {
                it->end_offset = state_->GetAddress();
                found = true;
                break;
            }
        }
        if (!found) {
            LOG(FATAL, PANDAFILE) << "Unknown variable";
        }
        return true;
    }

    bool HandleSetColumn(int32_t column_number)
    {
        state_->SetColumn(column_number);
        cnt_.push_back({state_->GetAddress(), state_->GetColumn()});
        return true;
    }

    bool HandleSpecialOpcode(uint32_t pc_offset, int32_t line_offset)
    {
        state_->AdvancePc(pc_offset);
        state_->AdvanceLine(line_offset);
        lnt_.push_back({state_->GetAddress(), state_->GetLine()});
        return true;
    }

    LineNumberTable GetLineNumberTable() const
    {
        return lnt_;
    }

    LocalVariableTable GetLocalVariableTable() const
    {
        return lvt_;
    }

    ColumnNumberTable GetColumnNumberTable() const
    {
        return cnt_;
    }

    const uint8_t *GetFile() const
    {
        return state_->GetFile();
    }

    const uint8_t *GetSourceCode() const
    {
        return state_->GetSourceCode();
    }

private:
    using Opcode = LineNumberProgramItem::Opcode;

    void ProcessVars()
    {
        for (auto &var : lvt_) {
            if (var.end_offset == 0) {
                var.end_offset = state_->GetAddress();
            }
        }
    }

    LineProgramState *state_;
    LineNumberTable lnt_;
    LocalVariableTable lvt_;
    ColumnNumberTable cnt_;
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

            LineNumberProgramHandler handler(&state);
            LineNumberProgramProcessor<LineNumberProgramHandler> program_processor(program, &handler);
            program_processor.Process();

            File::EntityId method_id = mda.GetMethodId();
            const char *source_file = utf::Mutf8AsCString(handler.GetFile());
            const char *source_code = utf::Mutf8AsCString(handler.GetSourceCode());
            methods_.push_back({source_file, source_code, method_id, handler.GetLineNumberTable(),
                                handler.GetLocalVariableTable(), std::move(param_names),
                                handler.GetColumnNumberTable()});
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

const ColumnNumberTable &DebugInfoExtractor::GetColumnNumberTable(File::EntityId method_id) const
{
    for (const auto &method : methods_) {
        if (method.method_id == method_id) {
            return method.column_number_table;
        }
    }

    static const ColumnNumberTable EMPTY_COLUMN_TABLE {};  // NOLINT(fuchsia-statically-constructed-objects)
    return EMPTY_COLUMN_TABLE;
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
