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

#ifndef PANDA_ASSEMBLER_ASSEMBLY_CONTEXT_H_
#define PANDA_ASSEMBLER_ASSEMBLY_CONTEXT_H_

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "assembly-ins.h"
#include "assembly-type.h"
#include "error.h"
#include "lexer.h"

namespace panda::pandasm {

/*
 * Used to move around tokens.
 * *context :
 * returns current value of a token
 * ++context :
 * sets the next token value
 * returns current value of a token
 * context++ :
 * sets the next token value
 * returns the next value of a token
 * similarly --context and context--
 */
struct Context {
    std::string_view token;                    /* current token */
    std::vector<panda::pandasm::Token> tokens; /* token list */
    size_t number = 0;                         /* line number */
    bool end = false;                          /* end of line flag */
    Token::Type id = Token::Type::ID_BAD;      /* current token type */
    Token::Type signop = Token::Type::ID_BAD;  /* current token operand type (if it is an operation) */
    panda::pandasm::Error err;                 /* current error */
    int64_t *max_value_of_reg = nullptr;
    size_t ins_number = 0;
    Type curr_func_return_type;
    std::vector<std::pair<size_t, size_t>> *function_arguments_list = nullptr;
    std::unordered_map<std::string, std::vector<std::pair<size_t, size_t>>> function_arguments_lists;

    void Make(const std::vector<panda::pandasm::Token> &t);
    void UpSignOperation();
    bool ValidateRegisterName(char c, size_t n = 0) const;
    bool ValidateParameterName(size_t number_of_params_already_is) const;
    bool ValidateLabel();
    bool Mask();
    bool NextMask();
    size_t Len() const;
    std::string_view GiveToken();
    Token::Type WaitFor();
    Token::Type Next();

    Token::Type operator++(int);
    Token::Type operator--(int);
    Token::Type operator++();
    Token::Type operator--();
    Token::Type operator*();
};

}  // namespace panda::pandasm

#endif  // PANDA_ASSEMBLER_ASSEMBLY_CONTEXT_H_
