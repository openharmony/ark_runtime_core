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

#include "lexer.h"

namespace panda::pandasm {

Token::Type FindDelim(char c)
{
    // The map of delimiters
    static const std::unordered_map<char, Token::Type> DELIM = {{',', Token::Type::DEL_COMMA},
                                                                {':', Token::Type::DEL_COLON},
                                                                {'{', Token::Type::DEL_BRACE_L},
                                                                {'}', Token::Type::DEL_BRACE_R},
                                                                {'(', Token::Type::DEL_BRACKET_L},
                                                                {')', Token::Type::DEL_BRACKET_R},
                                                                {'<', Token::Type::DEL_LT},
                                                                {'>', Token::Type::DEL_GT},
                                                                {'=', Token::Type::DEL_EQ},
                                                                {'[', Token::Type::DEL_SQUARE_BRACKET_L},
                                                                {']', Token::Type::DEL_SQUARE_BRACKET_R}};

    auto iter = DELIM.find(c);
    if (iter == DELIM.end()) {
        return Token::Type::ID_BAD;
    }

    return DELIM.at(c);
}

Token::Type FindOperation(std::string_view s)
{
    // Generate the map of OPERATIONS from ISA
    static const std::unordered_map<std::string_view, Token::Type> OPERATIONS = {
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define OPLIST(inst_code, name, optype, width, flags, dst_idx, use_idxs) \
    {std::string_view(name), Token::Type::ID_OP_##inst_code},
        PANDA_INSTRUCTION_LIST(OPLIST)
#undef OPLIST
    };

    auto iter = OPERATIONS.find(s);
    if (iter == OPERATIONS.end()) {
        return Token::Type::ID_BAD;
    }

    return OPERATIONS.at(s);
}

Token::Type Findkeyword(std::string_view s)
{
    // Generate the map of KEYWORDS
    static const std::unordered_map<std::string_view, Token::Type> KEYWORDS = {
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define KEYWORDS(name, inst_code) {std::string_view(name), Token::Type::ID_##inst_code},
        KEYWORDS_LIST(KEYWORDS)
#undef KEYWORDS
    };

    auto iter = KEYWORDS.find(s);
    if (iter == KEYWORDS.end()) {
        return Token::Type::ID_BAD;
    }

    return KEYWORDS.at(s);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_SIZE)
std::string_view TokenTypeWhat(Token::Type t)
{
    if (t >= Token::Type::OPERATION && t < Token::Type::KEYWORD) {
        return "OPERATION";
    }

    if (t >= Token::Type::KEYWORD) {
        return "KEYWORD";
    }

    switch (t) {
        case Token::Type::ID_BAD: {
            return "ID_BAD";
        }
        case Token::Type::DEL_COMMA: {
            return "DEL_COMMA";
        }
        case Token::Type::DEL_COLON: {
            return "DEL_COLON";
        }
        case Token::Type::DEL_BRACE_L: {
            return "DEL_BRACE_L";
        }
        case Token::Type::DEL_BRACE_R: {
            return "DEL_BRACE_R";
        }
        case Token::Type::DEL_BRACKET_L: {
            return "DEL_BRACKET_L";
        }
        case Token::Type::DEL_BRACKET_R: {
            return "DEL_BRACKET_R";
        }
        case Token::Type::DEL_SQUARE_BRACKET_L: {
            return "DEL_SQUARE_BRACKET_L";
        }
        case Token::Type::DEL_SQUARE_BRACKET_R: {
            return "DEL_SQUARE_BRACKET_R";
        }
        case Token::Type::DEL_GT: {
            return "DEL_GT";
        }
        case Token::Type::DEL_LT: {
            return "DEL_LT";
        }
        case Token::Type::DEL_EQ: {
            return "DEL_EQ";
        }
        case Token::Type::DEL_DOT: {
            return "DEL_DOT";
        }
        case Token::Type::ID: {
            return "ID";
        }
        case Token::Type::ID_STRING: {
            return "ID_STRING";
        }
        default:
            return "NONE";
    }
}

static bool IsQuote(char c)
{
    return c == '"';
}

Lexer::Lexer() : curr_line_(nullptr)
{
    LOG(DEBUG, ASSEMBLER) << "element of class Lexer initialized";
}

Lexer::~Lexer()
{
    LOG(DEBUG, ASSEMBLER) << "element of class Lexer destructed";
}

Tokens Lexer::TokenizeString(const std::string &source_str)
{
    LOG(DEBUG, ASSEMBLER) << "started tokenizing of line " << lines_.size() + 1 << ": ";

    lines_.emplace_back(source_str);

    curr_line_ = &lines_.back();

    LOG(DEBUG, ASSEMBLER) << std::string_view(&*(curr_line_->buffer.begin() + curr_line_->pos),
                                              curr_line_->end - curr_line_->pos);

    AnalyzeLine();

    LOG(DEBUG, ASSEMBLER) << "tokenization of line " << lines_.size() << " is successful";
    LOG(DEBUG, ASSEMBLER) << "         tokens identified: ";

    for (const auto &f_i : lines_.back().tokens) {
        LOG(DEBUG, ASSEMBLER) << "\n                           "
                              << std::string_view(&*(f_i.whole_line.begin() + f_i.bound_left),
                                                  f_i.bound_right - f_i.bound_left)
                              << " (type: " << TokenTypeWhat(f_i.type) << ")";

        LOG(DEBUG, ASSEMBLER);
        LOG(DEBUG, ASSEMBLER);
    }
    return std::pair<std::vector<Token>, Error>(lines_.back().tokens, err_);
}

// End of line
bool Lexer::Eol() const
{
    return curr_line_->pos == curr_line_->end;
}

// Return the type of token
Token::Type Lexer::LexGetType(size_t beg, size_t end) const
{
    if (FindDelim(curr_line_->buffer[beg]) != Token::Type::ID_BAD) { /* delimiter */
        return FindDelim(curr_line_->buffer[beg]);
    }

    std::string_view p(&*(curr_line_->buffer.begin() + beg), end - beg);
    Token::Type type = Findkeyword(p);
    if (type != Token::Type::ID_BAD) {
        return type;
    }

    type = FindOperation(p);
    if (type != Token::Type::ID_BAD) {
        return type;
    }

    if (IsQuote(curr_line_->buffer[beg])) {
        return Token::Type::ID_STRING;
    }

    return Token::Type::ID;  // other
}

// Handle string literal
bool Lexer::LexString()
{
    bool is_escape_seq = false;
    char quote = curr_line_->buffer[curr_line_->pos];
    size_t begin = curr_line_->pos;
    while (!Eol()) {
        ++(curr_line_->pos);

        char c = curr_line_->buffer[curr_line_->pos];

        if (is_escape_seq) {
            is_escape_seq = false;
            continue;
        }

        if (c == '\\') {
            is_escape_seq = true;
        }

        if (c == quote) {
            break;
        }
    }

    if (curr_line_->buffer[curr_line_->pos] != quote) {
        err_ = Error(std::string("Missing terminating ") + quote + " character", 0,
                     Error::ErrorType::ERR_STRING_MISSING_TERMINATING_CHARACTER, "", begin, curr_line_->pos,
                     curr_line_->buffer);
        return false;
    }

    ++(curr_line_->pos);

    return true;
}

/*
 * Tokens handling: set the corresponding
 * elements bound_left and bound_right of the array tokens
 * to the first and last characters of a corresponding token.
 *
 *                                                  bound_r1   bound_r2    bound_r3
 *                                                  |          |           |
 *                                                  v          v           v
 *       token1 token2 token3 ...             token1     token2      token3 ...
 *                                       =>   ^          ^           ^
 *                                            |          |           |
 *    bound1    bound2    bound3 ...          bound_l1   bound_l2    bound_l3 ...
 *
 */
void Lexer::LexTokens()
{
    if (Eol()) {
        return;
    }

    LOG(DEBUG, ASSEMBLER) << "token search started (line " << lines_.size() << "): "
                          << std::string_view(&*(curr_line_->buffer.begin() + curr_line_->pos),
                                              curr_line_->end - curr_line_->pos);

    while (curr_line_->end > curr_line_->pos && isspace(curr_line_->buffer[curr_line_->end - 1]) != 0) {
        --(curr_line_->end);
    }

    while (isspace(curr_line_->buffer[curr_line_->pos]) != 0 && !Eol()) {
        ++(curr_line_->pos);
    }

    size_t bound_right;
    size_t bound_left;

    for (int i = 0; !Eol(); ++i) {
        bound_left = curr_line_->pos;

        if (FindDelim(curr_line_->buffer[curr_line_->pos]) != Token::Type::ID_BAD) {
            ++(curr_line_->pos);
        } else if (IsQuote(curr_line_->buffer[curr_line_->pos])) {
            if (!LexString()) {
                return;
            }
        } else {
            while (!Eol() && FindDelim(curr_line_->buffer[curr_line_->pos]) == Token::Type::ID_BAD &&
                   isspace(curr_line_->buffer[curr_line_->pos]) == 0) {
                ++(curr_line_->pos);
            }
        }

        bound_right = curr_line_->pos;

        LOG(DEBUG, ASSEMBLER) << "token identified (line " << lines_.size() << ", "
                              << "token " << curr_line_->tokens.size() + 1 << "): "
                              << std::string_view(&*(curr_line_->buffer.begin() + bound_left), bound_right - bound_left)
                              << " ("
                              << "type: " << TokenTypeWhat(LexGetType(bound_left, bound_right)) << ")";

        curr_line_->tokens.emplace_back(bound_left, bound_right, LexGetType(bound_left, bound_right),
                                        curr_line_->buffer);

        while (isspace(curr_line_->buffer[curr_line_->pos]) != 0 && !Eol()) {
            ++(curr_line_->pos);
        }
    }

    LOG(DEBUG, ASSEMBLER) << "all tokens identified (line " << lines_.size() << ")";
}

/*
 * Ignore comments:
 * find PARSE_COMMENT_MARKER and move line->end to another position
 * next after the last character of the last significant (not a comment)
 * element in a current line: line->buffer.
 *
 * Ex:
 *   [Label:] operation operand[,operand] [# comment]
 *
 *   L1: mov v0, v1 # moving!        L1: mov v0, v1 # moving!
 *                          ^   =>                 ^
 *                          |                      |
 *                         end                    end
 */
void Lexer::LexPreprocess()
{
    LOG(DEBUG, ASSEMBLER) << "started removing comments (line " << lines_.size() << "): "
                          << std::string_view(&*(curr_line_->buffer.begin() + curr_line_->pos),
                                              curr_line_->end - curr_line_->pos);

    // Searching for comment marker located outside of the string literals.
    bool inside_str_lit = curr_line_->buffer.size() > 0 && curr_line_->buffer[0] == '\"';
    size_t cmt_pos = curr_line_->buffer.find_first_of("\"#", 0);
    if (cmt_pos != std::string::npos) {
        do {
            if (cmt_pos != 0 && curr_line_->buffer[cmt_pos - 1] != '\\' && curr_line_->buffer[cmt_pos] == '\"') {
                inside_str_lit = !inside_str_lit;
            } else if (curr_line_->buffer[cmt_pos] == PARSE_COMMENT_MARKER && !inside_str_lit) {
                break;
            }
        } while ((cmt_pos = curr_line_->buffer.find_first_of("\"#", cmt_pos + 1)) != std::string::npos);
    }

    if (cmt_pos != std::string::npos) {
        curr_line_->end = cmt_pos;
    }

    while (curr_line_->end > curr_line_->pos && isspace(curr_line_->buffer[curr_line_->end - 1]) != 0) {
        --(curr_line_->end);
    }

    LOG(DEBUG, ASSEMBLER) << "comments removed (line " << lines_.size() << "): "
                          << std::string_view(&*(curr_line_->buffer.begin() + curr_line_->pos),
                                              curr_line_->end - curr_line_->pos);
}

void Lexer::SkipSpace()
{
    while (!Eol() && isspace(curr_line_->buffer[curr_line_->pos]) != 0) {
        ++(curr_line_->pos);
    }
}

void Lexer::AnalyzeLine()
{
    LexPreprocess();

    SkipSpace();

    LexTokens();
}

}  // namespace panda::pandasm
