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

#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "assembly-emitter.h"
#include "assembly-parser.h"
#include "error.h"
#include "lexer.h"
#include "utils/expected.h"
#include "utils/logger.h"
#include "utils/pandargs.h"

namespace panda::pandasm {

void PrintError(const panda::pandasm::Error &e, const std::string &msg)
{
    std::stringstream sos;
    std::cerr << msg << ": " << e.message << std::endl;
    sos << "      Line " << e.line_number << ", Column " << e.pos + 1 << ": ";
    std::cerr << sos.str() << e.whole_line << std::endl;
    std::cerr << std::setw(static_cast<int>(e.pos + sos.str().size()) + 1) << "^" << std::endl;
}

void PrintErrors(const panda::pandasm::ErrorList &warnings, const std::string &msg)
{
    for (const auto &iter : warnings) {
        PrintError(iter, msg);
    }
}

bool PrepareArgs(panda::PandArgParser &pa_parser, const panda::PandArg<std::string> &input_file,
                 const panda::PandArg<std::string> &output_file, const panda::PandArg<std::string> &log_file,
                 const panda::PandArg<bool> &help, const panda::PandArg<bool> &verbose, std::ifstream &inputfile,
                 int argc, char **argv)
{
    if (!pa_parser.Parse(argc, const_cast<const char **>(argv)) || input_file.GetValue().empty() ||
        output_file.GetValue().empty() || help.GetValue()) {
        std::cerr << "Usage:" << std::endl;
        std::cerr << "ark_asm [OPTIONS] INPUT_FILE OUTPUT_FILE" << std::endl << std::endl;
        std::cerr << "Supported options:" << std::endl << std::endl;
        std::cerr << pa_parser.GetHelpString() << std::endl;
        return false;
    }

    if (verbose.GetValue()) {
        if (log_file.GetValue().empty()) {
            panda::Logger::ComponentMask component_mask;
            component_mask.set(panda::Logger::Component::ASSEMBLER);
            panda::Logger::InitializeStdLogging(panda::Logger::Level::DEBUG, component_mask);
        } else {
            panda::Logger::ComponentMask component_mask;
            component_mask.set(panda::Logger::Component::ASSEMBLER);
            panda::Logger::InitializeFileLogging(log_file.GetValue(), panda::Logger::Level::DEBUG, component_mask);
        }
    }

    inputfile.open(input_file.GetValue(), std::ifstream::in);

    if (!inputfile) {
        std::cerr << "The input file does not exist." << std::endl;
        return false;
    }

    return true;
}

bool Tokenize(panda::pandasm::Lexer &lexer, std::vector<std::vector<panda::pandasm::Token>> &tokens,
              std::ifstream &inputfile)
{
    std::string s;

    while (getline(inputfile, s)) {
        panda::pandasm::Tokens q = lexer.TokenizeString(s);

        auto e = q.second;
        if (e.err != panda::pandasm::Error::ErrorType::ERR_NONE) {
            e.line_number = tokens.size() + 1;
            PrintError(e, "ERROR");
            return false;
        }

        tokens.push_back(q.first);
    }

    return true;
}

bool ParseProgram(panda::pandasm::Parser &parser, std::vector<std::vector<panda::pandasm::Token>> &tokens,
                  const panda::PandArg<std::string> &input_file,
                  panda::Expected<panda::pandasm::Program, panda::pandasm::Error> &res)
{
    res = parser.Parse(tokens, input_file.GetValue());
    if (!res) {
        PrintError(res.Error(), "ERROR");
        return false;
    }

    return true;
}

bool DumpProgramInJson(panda::pandasm::Program &program, const panda::PandArg<std::string> &scopes_file)
{
    if (!scopes_file.GetValue().empty()) {
        std::ofstream dump_file;
        dump_file.open(scopes_file.GetValue());

        if (!dump_file) {
            std::cerr << "Failed to write scopes into the given file." << std::endl;
            return false;
        }
        dump_file << program.JsonDump();
    }

    return true;
}

bool EmitProgramInBinary(panda::pandasm::Program &program, panda::PandArgParser &pa_parser,
                         const panda::PandArg<std::string> &output_file, panda::PandArg<bool> &optimize,
                         const panda::PandArg<bool> &size_stat)
{
    auto emit_debug_info = !optimize.GetValue();
    std::map<std::string, size_t> stat;
    std::map<std::string, size_t> *statp = size_stat.GetValue() ? &stat : nullptr;
    panda::pandasm::AsmEmitter::PandaFileToPandaAsmMaps maps {};
    panda::pandasm::AsmEmitter::PandaFileToPandaAsmMaps *mapsp = optimize.GetValue() ? &maps : nullptr;

    if (!panda::pandasm::AsmEmitter::Emit(output_file.GetValue(), program, statp, mapsp, emit_debug_info)) {
        std::cerr << "Failed to emit binary data: " << panda::pandasm::AsmEmitter::GetLastError() << std::endl;
        return false;
    }

    if (size_stat.GetValue()) {
        size_t total_size = 0;
        std::cout << "Panda file size statistic:" << std::endl;

        for (auto [name, size] : stat) {
            std::cout << name << " section: " << size << std::endl;
            total_size += size;
        }

        std::cout << "total: " << total_size << std::endl;
    }

    pa_parser.DisableTail();

    return true;
}

bool BuildFiles(panda::pandasm::Program &program, panda::PandArgParser &pa_parser,
                const panda::PandArg<std::string> &output_file, panda::PandArg<bool> &optimize,
                panda::PandArg<bool> &size_stat, panda::PandArg<std::string> &scopes_file)
{
    if (!DumpProgramInJson(program, scopes_file)) {
        return false;
    }

    if (!EmitProgramInBinary(program, pa_parser, output_file, optimize, size_stat)) {
        return false;
    }

    return true;
}

}  // namespace panda::pandasm

int main(int argc, char *argv[])
{
    panda::PandArg<bool> verbose("verbose", false, "Enable verbose output (will be printed to standard output)");
    panda::PandArg<std::string> log_file("log-file", "", "(--log-file FILENAME) Set log file name");
    panda::PandArg<std::string> scopes_file("dump-scopes", "",
                                            "(--dump-scopes FILENAME) Enable dump of scopes to file");
    panda::PandArg<bool> help("help", false, "Print this message and exit");
    panda::PandArg<bool> size_stat("size-stat", false, "Print panda file size statistic");
    panda::PandArg<bool> optimize("optimize", false, "Run the bytecode optimization");
    // tail arguments
    panda::PandArg<std::string> input_file("INPUT_FILE", "", "Path to the source assembly code");
    panda::PandArg<std::string> output_file("OUTPUT_FILE", "", "Path to the generated binary code");
    panda::PandArgParser pa_parser;
    pa_parser.Add(&verbose);
    pa_parser.Add(&help);
    pa_parser.Add(&log_file);
    pa_parser.Add(&scopes_file);
    pa_parser.Add(&size_stat);
    pa_parser.Add(&optimize);
    pa_parser.PushBackTail(&input_file);
    pa_parser.PushBackTail(&output_file);
    pa_parser.EnableTail();

    std::ifstream inputfile;

    if (!panda::pandasm::PrepareArgs(pa_parser, input_file, output_file, log_file, help, verbose, inputfile, argc,
                                     argv)) {
        return 1;
    }

    LOG(DEBUG, ASSEMBLER) << "Lexical analysis:";

    panda::pandasm::Lexer lexer;

    std::vector<std::vector<panda::pandasm::Token>> tokens;

    if (!Tokenize(lexer, tokens, inputfile)) {
        return 1;
    }

    LOG(DEBUG, ASSEMBLER) << "parsing:";

    panda::pandasm::Parser parser;

    panda::Expected<panda::pandasm::Program, panda::pandasm::Error> res;
    if (!panda::pandasm::ParseProgram(parser, tokens, input_file, res)) {
        return 1;
    }

    auto &program = res.Value();

    auto w = parser.ShowWarnings();
    if (!w.empty()) {
        panda::pandasm::PrintErrors(w, "WARNING");
    }

    if (!panda::pandasm::BuildFiles(program, pa_parser, output_file, optimize, size_stat, scopes_file)) {
        return 1;
    }

    return 0;
}
