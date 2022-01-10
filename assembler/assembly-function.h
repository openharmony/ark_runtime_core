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

#ifndef PANDA_ASSEMBLER_ASSEMBLY_FUNCTION_H_
#define PANDA_ASSEMBLER_ASSEMBLY_FUNCTION_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "assembly-ins.h"
#include "assembly-label.h"
#include "assembly-type.h"
#include "assembly-debug.h"
#include "assembly-file-location.h"
#include "bytecode_emitter.h"
#include "extensions/extensions.h"
#include "file_items.h"
#include "file_item_container.h"
#include "ide_helpers.h"
#include "meta.h"

namespace panda::pandasm {

struct Function {
    struct CatchBlock {
        std::string whole_line;
        std::string exception_record;
        std::string try_begin_label;
        std::string try_end_label;
        std::string catch_begin_label;
        std::string catch_end_label;
    };

    struct TryCatchInfo {
        std::unordered_map<std::string_view, size_t> try_catch_labels;
        std::unordered_map<std::string, std::vector<const CatchBlock *>> try_catch_map;
        std::vector<std::string> try_catch_order;
        TryCatchInfo(std::unordered_map<std::string_view, size_t> &labels,
                     std::unordered_map<std::string, std::vector<const CatchBlock *>> &map,
                     std::vector<std::string> &param_try_catch_order)
            : try_catch_labels(labels), try_catch_map(map), try_catch_order(param_try_catch_order)
        {
        }
    };

    struct Parameter {
        Type type;
        std::unique_ptr<ParamMetadata> metadata;

        Parameter(Type t, extensions::Language lang)
            : type(std::move(t)), metadata(extensions::MetadataExtension::CreateParamMetadata(lang))
        {
        }
    };

    std::string name = "";
    extensions::Language language;
    std::unique_ptr<FunctionMetadata> metadata;

    std::unordered_map<std::string, panda::pandasm::Label> label_table;
    std::vector<panda::pandasm::Ins> ins; /* function instruction list */
    std::vector<panda::pandasm::debuginfo::LocalVariable> local_variable_debug;
    std::string source_file; /* The file in which the function is defined or empty */
    std::string source_code;
    std::vector<CatchBlock> catch_blocks;
    int64_t value_of_first_param = -1;
    size_t regs_num = 0;
    std::vector<Parameter> params;
    bool body_presence = false;
    Type return_type;
    SourceLocation body_location;
    std::optional<FileLocation> file_location;

    void SetInsDebug(const std::vector<debuginfo::Ins> &ins_debug)
    {
        ASSERT(ins_debug.size() == ins.size());
        for (std::size_t i = 0; i < ins.size(); i++) {
            ins[i].ins_debug = ins_debug[i];
        }
    }

    void AddInstruction(const panda::pandasm::Ins &instruction)
    {
        ins.emplace_back(instruction);
    }

    Function(std::string s, extensions::Language lang, size_t b_l, size_t b_r, std::string f_c, bool d, size_t l_n)
        : name(std::move(s)),
          language(lang),
          metadata(extensions::MetadataExtension::CreateFunctionMetadata(lang)),
          file_location({f_c, b_l, b_r, l_n, d})
    {
    }

    Function(std::string s, extensions::Language lang)
        : name(std::move(s)), language(lang), metadata(extensions::MetadataExtension::CreateFunctionMetadata(lang))
    {
    }

    std::size_t GetParamsNum() const
    {
        return params.size();
    }

    bool IsStatic() const
    {
        return (metadata->GetAccessFlags() & ACC_STATIC) != 0;
    }

    bool Emit(BytecodeEmitter &emitter, panda_file::MethodItem *method,
              const std::unordered_map<std::string, panda_file::BaseMethodItem *> &methods,
              const std::unordered_map<std::string, panda_file::BaseFieldItem *> &fields,
              const std::unordered_map<std::string, panda_file::BaseClassItem *> &classes,
              const std::unordered_map<std::string_view, panda_file::StringItem *> &strings,
              const std::unordered_map<std::string, panda_file::LiteralArrayItem *> &literalarrays) const;

    size_t GetLineNumber(size_t i) const;

    size_t GetColumnNumber(size_t i) const;

    void EmitLocalVariable(panda_file::LineNumberProgramItem *program, panda_file::ItemContainer *container,
                           std::vector<uint8_t> *constant_pool, uint32_t &pc_inc, size_t instruction_number) const;
    void EmitNumber(panda_file::LineNumberProgramItem *program, std::vector<uint8_t> *constant_pool, uint32_t pc_inc,
                    int32_t line_inc) const;
    void EmitLineNumber(panda_file::LineNumberProgramItem *program, std::vector<uint8_t> *constant_pool,
                        int32_t &prev_line_number, uint32_t &pc_inc, size_t instruction_number) const;
    // column number is only for javascript for now
    void EmitColumnNumber(panda_file::LineNumberProgramItem *program, std::vector<uint8_t> *constant_pool,
                        int32_t &prev_column_number, uint32_t &pc_inc, size_t instruction_number, bool emit_debug_info) const;

    void BuildLineNumberProgram(panda_file::DebugInfoItem *debug_item, const std::vector<uint8_t> &bytecode,
                                panda_file::ItemContainer *container, std::vector<uint8_t> *constant_pool,
                                bool emit_debug_info) const;

    Function::TryCatchInfo MakeOrderAndOffsets(const std::vector<uint8_t> &bytecode) const;

    std::vector<panda_file::CodeItem::TryBlock> BuildTryBlocks(
        panda_file::MethodItem *method, const std::unordered_map<std::string, panda_file::BaseClassItem *> &class_items,
        const std::vector<uint8_t> &bytecode) const;

    bool HasImplementation() const
    {
        return !metadata->IsForeign() && metadata->HasImplementation();
    }

    bool IsParameter(uint32_t reg_number) const
    {
        return reg_number >= regs_num;
    }

    bool CanThrow() const
    {
        return std::any_of(ins.cbegin(), ins.cend(), [](const Ins &insn) { return insn.CanThrow(); });
    }

    bool HasDebugInfo() const
    {
        return std::any_of(ins.cbegin(), ins.cend(), [](const Ins &insn) { return insn.HasDebugInfo(); });
    }

    void DebugDump() const;
};

}  // namespace panda::pandasm

#endif  // PANDA_ASSEMBLER_ASSEMBLY_FUNCTION_H_
