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

#include <string_view>
#include "assembly-parser.h"
#include "utils/number-utils.h"

namespace panda::pandasm {

void Context::Make(const std::vector<panda::pandasm::Token> &t)
{
    err = {};
    ins_number = 0;
    tokens = t;
    number = 1;
    end = false;

    token = std::string_view(&*(tokens[number - 1].whole_line.begin() + tokens[number - 1].bound_left),
                             tokens[number - 1].bound_right - tokens[number - 1].bound_left);

    id = this->tokens[number - 1].type;
}

size_t Context::Len() const
{
    return token.size();
}

bool Context::ValidateRegisterName(char c, size_t n) const
{
    if (token[0] == c) {
        std::string_view p = token;
        p.remove_prefix(1);
        if (p.empty() || (p.size() > 1 && p[0] == '0')) {
            return false;
        }

        if (c != 'a') {
            for (const auto &ch : p) {
                if (std::isdigit(ch) == 0) {
                    return false;
                }
            }
        } else {
            if (ToNumber(p) > n) {
                return false;
            }
        }

        return true;
    }

    return false;
}

bool Context::ValidateParameterName(size_t number_of_params_already_is) const
{
    if (number_of_params_already_is >= MAX_DWORD) {
        return false;
    }

    if (token[0] == 'a') {
        std::string_view p = token;
        p.remove_prefix(1);
        if (ToNumber(p) == number_of_params_already_is) {
            return true;
        }
    }

    return false;
}

std::string_view Context::GiveToken()
{
    return token;
}

Token::Type Context::Next()
{
    if (this->tokens.size() > number) {
        return this->tokens[number].type;
    }

    return this->tokens[number - 1].type;
}

void Context::UpSignOperation()
{
    signop = id;
}

Token::Type Context::WaitFor()
{
    return signop;
}

bool Context::Mask()
{
    return end;
}

bool Context::NextMask()
{
    if (end) {
        return true;
    }

    return this->tokens.size() < number + 1;
}

// NOLINTNEXTLINE(cert-dcl21-cpp)
Token::Type Context::operator++(int)
{
    Token::Type last_id = id;

    if (this->tokens.size() > number) {
        ++number;
        id = this->tokens[number - 1].type;

        token = std::string_view(&*(tokens[number - 1].whole_line.begin() + tokens[number - 1].bound_left),
                                 tokens[number - 1].bound_right - tokens[number - 1].bound_left);
    } else {
        end = true;
    }

    return last_id;
}

Token::Type Context::operator++()
{
    if (this->tokens.size() > number) {
        ++number;
        id = this->tokens[number - 1].type;

        token = std::string_view(&*(tokens[number - 1].whole_line.begin() + tokens[number - 1].bound_left),
                                 tokens[number - 1].bound_right - tokens[number - 1].bound_left);
    } else {
        end = true;
    }

    return id;
}

// NOLINTNEXTLINE(cert-dcl21-cpp)
Token::Type Context::operator--(int)
{
    Token::Type last_id = id;

    if (number > 1) {
        end = false;
        --number;
        id = this->tokens[number - 1].type;

        token = std::string_view(&*(tokens[number - 1].whole_line.begin() + tokens[number - 1].bound_left),
                                 tokens[number - 1].bound_right - tokens[number - 1].bound_left);
    } else {
        end = false;
    }

    return last_id;
}

Token::Type Context::operator--()
{
    if (number > 1) {
        end = false;
        --number;
        id = this->tokens[number - 1].type;

        token = std::string_view(&*(tokens[number - 1].whole_line.begin() + tokens[number - 1].bound_left),
                                 tokens[number - 1].bound_right - tokens[number - 1].bound_left);
    } else {
        end = false;
    }

    return id;
}

Token::Type Context::operator*()
{
    return id;
}

}  // namespace panda::pandasm
