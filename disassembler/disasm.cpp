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

#include "disassembler.h"

#include "utils/logger.h"
#include "utils/pandargs.h"

#include <iostream>

static void PrintHelp(const panda::PandArgParser &pa_parser)
{
    std::cerr << "Usage:" << std::endl;
    std::cerr << "ark_disasm [options] input_file output_file" << std::endl << std::endl;
    std::cerr << "Supported options:" << std::endl << std::endl;
    std::cerr << pa_parser.GetHelpString() << std::endl;
}

static void Disassemble(const std::string &input_file, const std::string &output_file, const bool verbose,
                        const bool quiet, const bool skip_strings)
{
    LOG(DEBUG, DISASSEMBLER) << "[initializing disassembler]\nfile: " << input_file << "\n";

    panda::disasm::Disassembler disasm {};
    disasm.Disassemble(input_file, quiet, skip_strings);
    if (verbose) {
        disasm.CollectInfo();
    }

    LOG(DEBUG, DISASSEMBLER) << "[serializing results]\n";

    std::ofstream res_pa;
    res_pa.open(output_file, std::ios::trunc | std::ios::out);
    disasm.Serialize(res_pa, true, verbose);
    res_pa.close();
}

int main(int argc, const char **argv)
{
    panda::PandArg<bool> help("help", false, "Print this message and exit");
    panda::PandArg<bool> verbose("verbose", false, "Enable informative code output");
    panda::PandArg<bool> quiet("quiet", false, "Enable all --skip-* flags");
    panda::PandArg<bool> skip_strings(
        "skip-string-literals", false,
        "Replace string literals with their respective IDs, thus reducing the emitted code size");
    panda::PandArg<bool> debug("debug", false,
                               "Enable output of debug messages, which will be printed to the standard output if no "
                               "--debug-file is specified");
    panda::PandArg<std::string> debug_file(
        "debug-file", "", "(--debug-file FILENAME) Set the debug file name, which is std::cout by default");
    panda::PandArg<std::string> input_file("input_file", "", "Path to the source binary code");
    panda::PandArg<std::string> output_file("output_file", "", "Path to the generated assembly code");

    panda::PandArgParser pa_parser;

    pa_parser.Add(&help);
    pa_parser.Add(&verbose);
    pa_parser.Add(&quiet);
    pa_parser.Add(&skip_strings);
    pa_parser.Add(&debug);
    pa_parser.Add(&debug_file);
    pa_parser.PushBackTail(&input_file);
    pa_parser.PushBackTail(&output_file);
    pa_parser.EnableTail();

    if (!pa_parser.Parse(argc, argv) || input_file.GetValue().empty() || output_file.GetValue().empty() ||
        help.GetValue()) {
        PrintHelp(pa_parser);
        return 1;
    }

    if (debug.GetValue()) {
        if (debug_file.GetValue().empty()) {
            panda::Logger::InitializeStdLogging(
                panda::Logger::Level::DEBUG,
                panda::Logger::ComponentMask().set(panda::Logger::Component::DISASSEMBLER));
        } else {
            panda::Logger::InitializeFileLogging(
                debug_file.GetValue(), panda::Logger::Level::DEBUG,
                panda::Logger::ComponentMask().set(panda::Logger::Component::DISASSEMBLER));
        }
    } else {
        panda::Logger::InitializeStdLogging(panda::Logger::Level::ERROR,
                                            panda::Logger::ComponentMask().set(panda::Logger::Component::DISASSEMBLER));
    }

    Disassemble(input_file.GetValue(), output_file.GetValue(), verbose.GetValue(), quiet.GetValue(),
                skip_strings.GetValue());

    pa_parser.DisableTail();

    return 0;
}
