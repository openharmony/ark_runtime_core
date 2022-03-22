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

#include <iostream>
#include <string>

#include <gtest/gtest.h>
#include "../define.h"
#include "../lexer.h"

namespace panda::test {

using namespace panda::pandasm;

TEST(lexertests, test1)
{
    Lexer l;
    std::string s = "mov v1, v2";
    Tokens tok = l.TokenizeString(s);
    ASSERT_EQ(TokenTypeWhat(tok.first[0].type), "OPERATION") << "OPERATION expected";
    ASSERT_EQ(TokenTypeWhat(tok.first[1].type), "ID") << "ID expected";
    ASSERT_EQ(TokenTypeWhat(tok.first[2].type), "DEL_COMMA") << "DEL_COMMA expected";
    ASSERT_EQ(TokenTypeWhat(tok.first[3].type), "ID") << "ID expected";
    ASSERT_EQ(tok.second.err, Error::ErrorType::ERR_NONE) << "ERR_NONE expected";
}

TEST(lexertests, test2)
{
    Lexer l;
    std::string s = "ldai 1";
    Tokens tok = l.TokenizeString(s);
    ASSERT_EQ(TokenTypeWhat(tok.first[0].type), "OPERATION") << "OPERATION expected";
    ASSERT_EQ(TokenTypeWhat(tok.first[1].type), "ID") << "ID expected";
    ASSERT_EQ(tok.second.err, Error::ErrorType::ERR_NONE) << "ERR_NONE expected";
}

TEST(lexertests, test3)
{
    Lexer l;
    std::string s = "movi\nlda v2 v10 mov v2";
    Tokens tok = l.TokenizeString(s);
    ASSERT_EQ(TokenTypeWhat(tok.first[0].type), "OPERATION") << "OPERATION expected";
    ASSERT_EQ(TokenTypeWhat(tok.first[1].type), "OPERATION") << "OPERATION expected";
    ASSERT_EQ(TokenTypeWhat(tok.first[2].type), "ID") << "ID expected";
    ASSERT_EQ(TokenTypeWhat(tok.first[3].type), "ID") << "ID expected";
    ASSERT_EQ(TokenTypeWhat(tok.first[4].type), "OPERATION") << "OPERATION expected";
    ASSERT_EQ(TokenTypeWhat(tok.first[5].type), "ID") << "ID expected";
    ASSERT_EQ(tok.second.err, Error::ErrorType::ERR_NONE) << "ERR_NONE expected";
}

TEST(lexertests, test4)
{
    Lexer l;
    std::string s = "jmp Iasdfsadkfjhasifhsaiuhdacoisjdaociewhasdasdfkjasdfhjksadhfkhsakdfjhksajhdkfjhskhdfkjahhjdskaj";
    Tokens tok = l.TokenizeString(s);
    ASSERT_EQ(TokenTypeWhat(tok.first[0].type), "OPERATION") << "OPERATION expected";
    ASSERT_EQ(TokenTypeWhat(tok.first[1].type), "ID") << "ID expected";
    ASSERT_EQ(tok.second.err, Error::ErrorType::ERR_NONE) << "ERR_NONE expected";
}

TEST(lexertests, test5)
{
    Lexer l;
    std::string s = "call.short 1111, 1";
    Tokens tok = l.TokenizeString(s);
    ASSERT_EQ(TokenTypeWhat(tok.first[0].type), "OPERATION") << "OPERATION expected";
    ASSERT_EQ(TokenTypeWhat(tok.first[1].type), "ID") << "ID expected";
    ASSERT_EQ(TokenTypeWhat(tok.first[2].type), "DEL_COMMA") << "DEL_COMMA expected";
    ASSERT_EQ(TokenTypeWhat(tok.first[3].type), "ID") << "ID expected";
    ASSERT_EQ(tok.second.err, Error::ErrorType::ERR_NONE) << "ERR_NONE expected";
}

TEST(lexertests, test6)
{
    Lexer l;
    std::string s = "jle v1 met";
    Tokens tok = l.TokenizeString(s);
    ASSERT_EQ(TokenTypeWhat(tok.first[0].type), "OPERATION") << "OPERATION expected";
    ASSERT_EQ(TokenTypeWhat(tok.first[1].type), "ID") << "ID expected";
    ASSERT_EQ(TokenTypeWhat(tok.first[2].type), "ID") << "ID expected";
    ASSERT_EQ(tok.second.err, Error::ErrorType::ERR_NONE) << "ERR_NONE expected";
}

TEST(lexertests, test7)
{
    Lexer l;
    std::string s = "label:";
    Tokens tok = l.TokenizeString(s);
    ASSERT_EQ(TokenTypeWhat(tok.first[0].type), "ID") << "ID expected";
    ASSERT_EQ(tok.second.err, Error::ErrorType::ERR_NONE) << "ERR_NONE expected";
}

TEST(lexertests, test8)
{
    Lexer l;
    std::string s = ",";
    Tokens tok = l.TokenizeString(s);
    ASSERT_EQ(TokenTypeWhat(tok.first[0].type), "DEL_COMMA") << "DEL_COMMA expected";
    ASSERT_EQ(tok.second.err, Error::ErrorType::ERR_NONE) << "ERR_NONE expected";
}

TEST(lexertests, test9)
{
    Lexer l;
    std::string s = ",:{}()<>=";
    Tokens tok = l.TokenizeString(s);
    ASSERT_EQ(TokenTypeWhat(tok.first[0].type), "DEL_COMMA") << "DEL_COMMA expected";
    ASSERT_EQ(TokenTypeWhat(tok.first[1].type), "DEL_COLON") << "DEL_COMMA expected";
    ASSERT_EQ(TokenTypeWhat(tok.first[2].type), "DEL_BRACE_L") << "DEL_COMMA expected";
    ASSERT_EQ(TokenTypeWhat(tok.first[3].type), "DEL_BRACE_R") << "DEL_COMMA expected";
    ASSERT_EQ(TokenTypeWhat(tok.first[4].type), "DEL_BRACKET_L") << "DEL_BRACKET_L expected";
    ASSERT_EQ(TokenTypeWhat(tok.first[5].type), "DEL_BRACKET_R") << "DEL_BRACKET_R expected";
    ASSERT_EQ(TokenTypeWhat(tok.first[6].type), "DEL_LT") << "DEL_LT expected";
    ASSERT_EQ(TokenTypeWhat(tok.first[7].type), "DEL_GT") << "DEL_GT expected";
    ASSERT_EQ(TokenTypeWhat(tok.first[8].type), "DEL_EQ") << "DEL_EQ expected";
    ASSERT_EQ(tok.second.err, Error::ErrorType::ERR_NONE) << "ERR_NONE expected";
}

TEST(lexertests, test11)
{
    Lexer l;
    std::string s =
        "i64.to.f32 alsdhashdjskhfka "
        "shdkfhkasdhfkhsakdhfkshkfhskahlfkjsdfkjadskhfkshadkhfsdakhfksahdkfaksdfkhaskldhkfashdlfkjhasdkjfhklasjhdfklhsa"
        "fhska";
    Tokens tok = l.TokenizeString(s);
    ASSERT_EQ(tok.second.err, Error::ErrorType::ERR_NONE) << "ERR_NONE expected";
}

TEST(lexertests, test12)
{
    Lexer l;
    std::string s = ".function asd(u32){}";
    Tokens tok = l.TokenizeString(s);
    ASSERT_EQ(TokenTypeWhat(tok.first[0].type), "KEYWORD") << "KEYWORD expected";
    ASSERT_EQ(TokenTypeWhat(tok.first[1].type), "ID") << "ID expected";
    ASSERT_EQ(tok.second.err, Error::ErrorType::ERR_NONE) << "ERR_NONE expected";
}

TEST(lexertests, string_literal)
{
    {
        Lexer l;
        std::string s = "\"123";
        Tokens tok = l.TokenizeString(s);

        Error e = tok.second;

        ASSERT_EQ(e.err, Error::ErrorType::ERR_STRING_MISSING_TERMINATING_CHARACTER);
    }

    {
        Lexer l;
        std::string s = "\"123\\\"";
        Tokens tok = l.TokenizeString(s);

        Error e = tok.second;

        ASSERT_EQ(e.err, Error::ErrorType::ERR_STRING_MISSING_TERMINATING_CHARACTER);
    }

    {
        Lexer l;
        std::string s = "\" a b \\ c d \"";
        Tokens tok = l.TokenizeString(s);

        Error e = tok.second;

        ASSERT_EQ(e.err, Error::ErrorType::ERR_NONE);
        ASSERT_EQ(tok.first.size(), 1U);
        ASSERT_EQ(tok.first[0].type, Token::Type::ID_STRING);
        ASSERT_EQ(tok.first[0].bound_left, 0U);
        ASSERT_EQ(tok.first[0].bound_right, s.length());
    }

    {
        Lexer l;
        std::string s = "\"abcd\"1234";
        Tokens tok = l.TokenizeString(s);

        Error e = tok.second;

        ASSERT_EQ(e.err, Error::ErrorType::ERR_NONE);
        ASSERT_EQ(tok.first.size(), 2U);
        ASSERT_EQ(tok.first[0].type, Token::Type::ID_STRING);
        ASSERT_EQ(tok.first[0].bound_left, 0U);
        ASSERT_EQ(tok.first[0].bound_right, s.find('1'));
    }
}

TEST(lexertests, array_type)
{
    Lexer l;
    std::string s = "i32[]";
    Tokens tok = l.TokenizeString(s);

    Error e = tok.second;
    ASSERT_EQ(e.err, Error::ErrorType::ERR_NONE);
    ASSERT_EQ(tok.first.size(), 3U);
    ASSERT_EQ(tok.first[0].type, Token::Type::ID);
    ASSERT_EQ(tok.first[1].type, Token::Type::DEL_SQUARE_BRACKET_L);
    ASSERT_EQ(tok.first[2].type, Token::Type::DEL_SQUARE_BRACKET_R);
}

}  // namespace panda::test
