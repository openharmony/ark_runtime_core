/**
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
#include "assembly-ins.h"

namespace panda::pandasm {

std::string panda::pandasm::Ins::RegsToString(bool &first, bool print_args, size_t first_arg_idx) const
{
    std::stringstream translator;
    for (const auto &reg : this->regs) {
        if (!first) {
            translator << ",";
        } else {
            first = false;
        }

        if (print_args && reg >= first_arg_idx) {
            translator << " a" << reg - first_arg_idx;
        } else {
            translator << " v" << reg;
        }
    }
    return translator.str();
}

std::string panda::pandasm::Ins::ImmsToString(bool &first) const
{
    std::stringstream translator;
    for (const auto &imm : this->imms) {
        if (!first) {
            translator << ",";
        } else {
            first = false;
        }

        auto *number = std::get_if<double>(&imm);
        if (number != nullptr) {
            translator << " " << std::scientific << *number;
        } else {
            translator << " 0x" << std::hex << std::get<int64_t>(imm);
        }
        translator.clear();
    }
    return translator.str();
}

std::string panda::pandasm::Ins::IdsToString(bool &first) const
{
    std::stringstream translator;
    for (const auto &id : this->ids) {
        if (!first) {
            translator << ",";
        } else {
            first = false;
        }

        translator << " " << id;
    }
    return translator.str();
}

std::string panda::pandasm::Ins::OperandsToString(PrintKind print_kind, bool print_args, size_t first_arg_idx) const
{
    bool first = true;
    std::stringstream ss {};

    switch (print_kind) {
        case PrintKind::CALL:
            ss << this->IdsToString(first) << this->RegsToString(first, print_args, first_arg_idx);
            if (!imms.empty()) {
                ss << ImmsToString(first);
            }
            break;
        case PrintKind::CALLI:
            ss << this->IdsToString(first) << this->ImmsToString(first)
               << this->RegsToString(first, print_args, first_arg_idx);
            break;
        case PrintKind::DEFAULT:
        default:
            ss << this->RegsToString(first, print_args, first_arg_idx) << this->ImmsToString(first)
               << this->IdsToString(first);
    }

    return ss.str();
}

}  // namespace panda::pandasm
