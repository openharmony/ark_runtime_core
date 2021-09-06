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

#ifndef PANDA_ASSEMBLER_LEXER_H_
#define PANDA_ASSEMBLER_LEXER_H_

#include <array>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "define.h"
#include "error.h"
#include "isa.h"
#include "utils/logger.h"

namespace panda::pandasm {

struct Token {
    enum class Type {
        ID_BAD = 0,
        /* delimiters */
        DEL_COMMA,                                                                          /* , */
        DEL_COLON,                                                                          /* : */
        DEL_BRACE_L,                                                                        /* { */
        DEL_BRACE_R,                                                                        /* } */
        DEL_BRACKET_L,                                                                      /* ( */
        DEL_BRACKET_R,                                                                      /* ) */
        DEL_SQUARE_BRACKET_L,                                                               /* [ */
        DEL_SQUARE_BRACKET_R,                                                               /* ] */
        DEL_GT,                                                                             /* > */
        DEL_LT,                                                                             /* < */
        DEL_EQ,                                                                             /* = */
        DEL_DOT,                                                                            /* . */
        ID,                                                                                 /* other */
        ID_STRING,                                                                          /* string literal */
        OPERATION,                                                                          /* special */
#define OPLIST(inst_code, name, optype, width, flags, dst_idx, src_idxs) ID_OP_##inst_code, /* command type list */
        PANDA_INSTRUCTION_LIST(OPLIST)
#undef OPLIST
            KEYWORD,                              /* special */
#define KEYWORDS(name, inst_code) ID_##inst_code, /* keyword type list */
        KEYWORDS_LIST(KEYWORDS)
#undef KEYWORDS
    };

    std::string whole_line;
    size_t bound_left; /* right and left bounds of tokens */
    size_t bound_right;
    Type type;

    Token() : Token(0, 0, Type::ID_BAD, "") {}

    Token(size_t b_l, size_t b_r, Type t, std::string beg_of_line)
        : whole_line(std::move(beg_of_line)), bound_left(b_l), bound_right(b_r), type(t)
    {
    }
};

using Tokens = std::pair<std::vector<Token>, Error>;

using TokenSet = const std::vector<std::vector<Token>>;

struct Line {
    std::vector<Token> tokens;
    std::string buffer; /* raw line, as read from the file */
    size_t pos;         /* current line position */
    size_t end;

    explicit Line(std::string str) : buffer(std::move(str)), pos(0), end(buffer.size()) {}
};

class Lexer {
public:
    Lexer();
    ~Lexer();
    NO_MOVE_SEMANTIC(Lexer);
    NO_COPY_SEMANTIC(Lexer);

    /*
     * The main function of Tokenizing, which takes a string.
     * Returns a vector of tokens.
     */
    Tokens TokenizeString(const std::string &);

private:
    std::vector<Line> lines_;
    Line *curr_line_;
    Error err_;

    bool Eol() const; /* end of line */
    bool LexString();
    void LexTokens();
    void LexPreprocess();
    void SkipSpace();
    void AnalyzeLine();
    Token::Type LexGetType(size_t, size_t) const;
};

/*
 * Returns a string representation of a token type.
 */
std::string_view TokenTypeWhat(Token::Type);

}  // namespace panda::pandasm

#endif  // PANDA_ASSEMBLER_LEXER_H_
