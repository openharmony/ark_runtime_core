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

#ifndef PANDA_ASSEMBLER_PANDASM_H_
#define PANDA_ASSEMBLER_PANDASM_H_

#include "utils/pandargs.h"

namespace panda::pandasm {

void PrintError(const panda::pandasm::Error &e, const std::string &msg);

void PrintErrors(const panda::pandasm::ErrorList &warnings, const std::string &msg);

bool PrepareArgs(panda::PandArgParser &pa_parser, const panda::PandArg<std::string> &input_file,
                 const panda::PandArg<std::string> &output_file, const panda::PandArg<std::string> &log_file,
                 const panda::PandArg<bool> &help, const panda::PandArg<bool> &verbose, std::ifstream &inputfile,
                 int argc, char **argv);

bool Tokenize(panda::pandasm::Lexer &lexer, std::vector<std::vector<panda::pandasm::Token>> &tokens,
              std::ifstream &inputfile);

bool ParseProgram(panda::pandasm::Parser &parser, std::vector<std::vector<panda::pandasm::Token>> &tokens,
                  const panda::PandArg<std::string> &input_file,
                  panda::Expected<panda::pandasm::Program, panda::pandasm::Error> &res);

bool DumpProgramInJson(panda::pandasm::Program &program, const panda::PandArg<std::string> &scopes_file);

bool EmitProgramInBinary(panda::pandasm::Program &program, panda::PandArgParser &pa_parser,
                         const panda::PandArg<std::string> &output_file, panda::PandArg<bool> &optimize,
                         panda::PandArg<bool> &size_stat);

bool BuildFiles(panda::pandasm::Program &program, panda::PandArgParser &pa_parser,
                const panda::PandArg<std::string> &output_file, panda::PandArg<bool> &optimize,
                panda::PandArg<bool> &size_stat, panda::PandArg<std::string> &scopes_file);

}  // namespace panda::pandasm

#endif  // PANDA_ASSEMBLER_PANDASM_H_
