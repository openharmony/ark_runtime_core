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

#ifndef PANDA_ASSEMBLER_ASSEMBLY_PARSER_H_
#define PANDA_ASSEMBLER_ASSEMBLY_PARSER_H_

#include <iostream>
#include <memory>
#include <string>
#include <string_view>

#include "assembly-context.h"
#include "assembly-emitter.h"
#include "assembly-field.h"
#include "assembly-function.h"
#include "assembly-ins.h"
#include "assembly-label.h"
#include "assembly-program.h"
#include "assembly-record.h"
#include "assembly-type.h"
#include "define.h"
#include "error.h"
#include "ide_helpers.h"
#include "lexer.h"
#include "meta.h"
#include "utils/expected.h"

namespace panda::pandasm {

using Instructions = std::pair<std::vector<Ins>, Error>;

using Functions = std::pair<std::unordered_map<std::string, Function>, std::unordered_map<std::string, Record>>;

class Parser {
public:
    Parser() = default;

    NO_MOVE_SEMANTIC(Parser);
    NO_COPY_SEMANTIC(Parser);

    ~Parser() = default;

    /*
     * The main function of parsing, which takes a vector of token vectors and a name of the source file.
     * Returns a program or an error value: Expected<Program, Error>
     * This function analyzes code containing several functions:
     *   - Each function used must be declared.
     *   - The correct function declaration looks like this: .function ret_type fun_name([param_type aN,]) [<metadata>]
     *     ([data] shows that this 'data' is optional).
     *   - N in function parameters must increase when number of parameters increases
     *     (Possible: a0, a1,..., aN. Impossible: a1, a10, a13).
     *   - Each function has its own label table.
     */
    Expected<Program, Error> Parse(TokenSet &vectors_tokens, const std::string &file_name = "");

    /*
     * The main function of parsing, which takes a string with source and a name of the source file.
     * Returns a program or an error value: Expected<Program, Error>
     */
    Expected<Program, Error> Parse(const std::string &source, const std::string &file_name = "");

    /*
     * Returns a set error
     */
    Error ShowError() const
    {
        return err_;
    }

    ErrorList ShowWarnings() const
    {
        return war_;
    }

private:
    panda::pandasm::Program program_;
    std::unordered_map<std::string, panda::pandasm::Label> *label_table_ = nullptr;
    Metadata *metadata_ = nullptr;
    Context context_; /* token iterator */
    panda::pandasm::Record *curr_record_ = nullptr;
    panda::pandasm::Function *curr_func_ = nullptr;
    panda::pandasm::Ins *curr_ins_ = nullptr;
    panda::pandasm::Field *curr_fld_ = nullptr;
    size_t line_stric_ = 0;
    panda::pandasm::Error err_;
    panda::pandasm::ErrorList war_;
    bool open_ = false; /* flag of being in a code section */
    bool record_def_ = false;
    bool func_def_ = false;

    inline Error GetError(const std::string &mess = "", Error::ErrorType err = Error::ErrorType::ERR_NONE,
                          int8_t shift = 0, int token_shift = 0, const std::string &add_mess = "") const
    {
        return Error(mess, line_stric_, err, add_mess,
                     context_.tokens[context_.number + token_shift - 1].bound_left + shift,
                     context_.tokens[context_.number + token_shift - 1].bound_right,
                     context_.tokens[context_.number + token_shift - 1].whole_line);
    }

    inline void GetWarning(const std::string &mess = "", Error::ErrorType err = Error::ErrorType::ERR_NONE,
                           int8_t shift = 0, const std::string &add_mess = "")
    {
        war_.emplace_back(mess, line_stric_, err, add_mess, context_.tokens[context_.number - 1].bound_left + shift,
                          context_.tokens[context_.number - 1].bound_right,
                          context_.tokens[context_.number - 1].whole_line, Error::ErrorClass::WARNING);
    }

    SourcePosition GetCurrentPosition(bool left_bound) const
    {
        if (left_bound) {
            return SourcePosition {line_stric_, context_.tokens[context_.number - 1].bound_left};
        }
        return SourcePosition {line_stric_, context_.tokens[context_.number - 1].bound_right};
    }

    bool LabelValidName();
    bool TypeValidName();
    bool RegValidName();
    bool ParamValidName();
    bool FunctionValidName();
    bool ParseFunctionName();
    bool ParseLabel();
    bool ParseOperation();
    bool ParseOperands();
    bool ParseFunctionCode();
    bool ParseFunctionInstruction();
    bool ParseFunctionFullSign();
    bool ParseFunctionReturn();
    bool ParseFunctionArg();
    bool ParseFunctionArgComma(bool &comma);
    bool ParseFunctionArgs();
    bool ParseType(Type *type);
    bool PrefixedValidName();
    bool ParseMetaListComma(bool &comma, bool eq);
    bool MeetExpMetaList(bool eq);
    bool BuildMetaListAttr(bool &eq, std::string &attribute_name, std::string &attribute_value);
    bool ParseMetaList(bool flag);
    bool ParseMetaDef();
    bool ParseRecordFullSign();
    bool ParseRecordFields();
    bool ParseRecordField();
    bool ParseRecordName();
    bool RecordValidName();
    bool ParseFieldName();
    bool ParseFieldType();
    std::optional<std::string> ParseStringLiteral();
    int64_t MnemonicToBuiltinId();

    bool ParseOperandVreg();
    bool ParseOperandComma();
    bool ParseOperandInteger();
    bool ParseOperandFloat();
    bool ParseOperandId();
    bool ParseOperandLabel();
    bool ParseOperandField();
    bool ParseOperandType(Type::VerificationType ver_type);
    bool ParseOperandNone();
    bool ParseOperandString();
    bool ParseOperandCall();
    bool ParseOperandBuiltinMnemonic();

    void SetFunctionInformation();
    void SetRecordInformation();
    void SetOperationInformation();
    void ParseAsCatchall(const std::vector<Token> &tokens);
    void ParseAsLanguage(const std::vector<Token> &tokens, bool &is_lang_parsed, bool &is_first_statement);
    void ParseAsRecord(const std::vector<Token> &tokens);
    void ParseAsFunction(const std::vector<Token> &tokens);
    void ParseAsBraceRight(const std::vector<Token> &tokens);
    bool ParseAfterLine(bool &is_first_statement);
    Expected<Program, Error> ParseAfterMainLoop(const std::string &file_name);
    void ParseResetFunctionLabelsAndParams();
    void ParseResetTables();
    void ParseResetFunctionTable();
    void ParseResetRecordTable();
    void ParseAsLanguageDirective();
    Function::CatchBlock PrepareCatchBlock(bool is_catchall, size_t size, size_t catchall_tokens_num,
                                           size_t catch_tokens_num);
    void ParseAsCatchDirective();
    void SetError();
    void SetMetadataContextError(const Metadata::Error &err, bool has_value);

    Expected<char, Error> ParseOctalEscapeSequence(std::string_view s, size_t *i);
    Expected<char, Error> ParseHexEscapeSequence(std::string_view s, size_t *i);
    Expected<char, Error> ParseEscapeSequence(std::string_view s, size_t *i);

    template <class T>
    auto TryEmplaceInTable(bool flag, T &item)
    {
        return item.try_emplace(std::string(context_.GiveToken().data(), context_.GiveToken().length()),
                                std::string(context_.GiveToken().data(), context_.GiveToken().length()), program_.lang,
                                context_.tokens[context_.number - 1].bound_left,
                                context_.tokens[context_.number - 1].bound_right,
                                context_.tokens[context_.number - 1].whole_line, flag, line_stric_);
    }

    template <class T>
    bool AddObjectInTable(bool flag, T &item)
    {
        auto [iter, is_inserted] = TryEmplaceInTable(flag, item);

        if (is_inserted) {
            return true;
        }

        if (iter->second.file_location->is_defined && flag) {
            return false;
        }

        if (!iter->second.file_location->is_defined && flag) {
            iter->second.file_location->is_defined = true;
            return true;
        }

        if (!iter->second.file_location->is_defined) {
            iter->second.file_location->bound_left = context_.tokens[context_.number - 1].bound_left;
            iter->second.file_location->bound_right = context_.tokens[context_.number - 1].bound_right;
            iter->second.file_location->whole_line = context_.tokens[context_.number - 1].whole_line;
            iter->second.file_location->line_number = line_stric_;
        }

        return true;
    }
};

template <>
inline auto Parser::TryEmplaceInTable(bool flag, std::unordered_map<std::string, panda::pandasm::Label> &item)
{
    return item.try_emplace(std::string(context_.GiveToken().data(), context_.GiveToken().length()),
                            std::string(context_.GiveToken().data(), context_.GiveToken().length()),
                            context_.tokens[context_.number - 1].bound_left,
                            context_.tokens[context_.number - 1].bound_right,
                            context_.tokens[context_.number - 1].whole_line, flag, line_stric_);
}

}  // namespace panda::pandasm

#endif  // PANDA_ASSEMBLER_ASSEMBLY_PARSER_H_
