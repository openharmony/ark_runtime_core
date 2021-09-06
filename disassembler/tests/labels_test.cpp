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
#include "disassembler.h"

using namespace panda::disasm;

std::string g_bin_path_abs {};

TEST(label_test, test1)
{
    Disassembler d {};

    std::stringstream ss {};
    d.Disassemble(g_bin_path_abs + "labels1.bc");
    d.Serialize(ss, false);

    size_t beg_g = ss.str().find("g_u1_() <static> {\n");
    size_t end_g = ss.str().find('}', beg_g);
    size_t beg_gg = ss.str().find("gg_u1_() <static> {\n");
    size_t end_gg = ss.str().find('}', beg_gg);

    ASSERT_TRUE(beg_g != std::string::npos && end_g != std::string::npos) << "function g not found";
    ASSERT_TRUE(beg_gg != std::string::npos && end_gg != std::string::npos) << "function gg not found";

    std::string body_g =
        ss.str().substr(beg_g + strlen("g_u1_() <static> {\n"), end_g - (beg_g + strlen("g_u1_() <static> {\n")));
    std::string body_gg =
        ss.str().substr(beg_gg + strlen("gg_u1_() <static> {\n"), end_gg - (beg_gg + strlen("gg_u1_() <static> {\n")));

    EXPECT_EQ(body_g, "\tjump_label_0: jmp jump_label_0\n\treturn\n");
    EXPECT_EQ(body_gg, "\tjmp jump_label_0\n\tjump_label_0: return\n");
}

TEST(label_test, test2)
{
    Disassembler d {};

    std::stringstream ss {};
    d.Disassemble(g_bin_path_abs + "labels2.bc");
    d.Serialize(ss);

    size_t beg_g = ss.str().find("g_u1_() <static> {");
    size_t end_g = ss.str().find('}', beg_g);

    ASSERT_TRUE(beg_g != std::string::npos && end_g != std::string::npos) << "function g not found";

    std::string body_g = ss.str().substr(beg_g + strlen("g_u1_() {"), end_g - (beg_g + strlen("g_u1_() {")));

    EXPECT_TRUE(body_g.find("jump_label_0: movi v0, 0x0") != std::string::npos) << "jump_label_0 not found";
    EXPECT_TRUE(body_g.find("jump_label_2: movi v0, 0x1") != std::string::npos) << "jump_label_1 not found";
    EXPECT_TRUE(body_g.find("jump_label_4: movi v0, 0x2") != std::string::npos) << "jump_label_2 not found";
    EXPECT_TRUE(body_g.find("jump_label_6: movi v0, 0x3") != std::string::npos) << "jump_label_3 not found";
    EXPECT_TRUE(body_g.find("jump_label_7: movi v0, 0x4") != std::string::npos) << "jump_label_4 not found";
    EXPECT_TRUE(body_g.find("jump_label_5: movi v0, 0x5") != std::string::npos) << "jump_label_5 not found";
    EXPECT_TRUE(body_g.find("jump_label_3: movi v0, 0x6") != std::string::npos) << "jump_label_6 not found";
    EXPECT_TRUE(body_g.find("jump_label_1: movi v0, 0x7") != std::string::npos) << "jump_label_7 not found";

    EXPECT_TRUE(body_g.find("\tjmp jump_label_0\n"
                            "\tjmp jump_label_1\n"
                            "\tjmp jump_label_2\n"
                            "\tjmp jump_label_3\n"
                            "\tjmp jump_label_4\n"
                            "\tjmp jump_label_5\n"
                            "\tjmp jump_label_6\n"
                            "\tjmp jump_label_7\n") != std::string::npos)
        << "label sequence is broken";
}

TEST(label_test, test_exceptions)
{
    Disassembler d {};

    std::stringstream ss {};
    d.Disassemble(g_bin_path_abs + "exceptions.bc");
    d.Serialize(ss);

    std::string res = ss.str();

    EXPECT_TRUE(res.find("try_begin_label_0: ldai 0x1") != std::string::npos);
    EXPECT_TRUE(res.find("try_end_label_0: ldai 0x3") != std::string::npos);
    EXPECT_TRUE(res.find("handler_begin_label_0_0: call.virt.short A_exception.getMessage_A_exception_A_, v0") !=
                std::string::npos);
    EXPECT_TRUE(res.find("handler_end_label_0_0: ldai 0x6") != std::string::npos);
    EXPECT_TRUE(res.find("handler_begin_label_0_1: ldai 0x7") != std::string::npos);
    EXPECT_TRUE(
        res.find(
            ".catch A_exception, try_begin_label_0, try_end_label_0, handler_begin_label_0_0, handler_end_label_0_0") !=
        std::string::npos);
    EXPECT_TRUE(res.find(".catchall try_begin_label_0, try_end_label_0, handler_begin_label_0_1") != std::string::npos);
}

int main(int argc, char **argv)
{
    std::string dir_basename {};
    std::string glob_argv0 = argv[0];

    size_t last_slash_idx = glob_argv0.rfind('/');

    if (std::string::npos != last_slash_idx) {
        dir_basename = glob_argv0.substr(0, last_slash_idx + 1);
    }

    g_bin_path_abs = dir_basename + "../disassembler/bin/";

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
