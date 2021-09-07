
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

TEST(instructions_test, test_language_panda_assembly)
{
    Disassembler d {};

    std::stringstream ss {};
    d.Disassemble(g_bin_path_abs + "empty_record.bc");
    d.Serialize(ss);

    EXPECT_TRUE(ss.str().find(".language PandaAssembly") != std::string::npos);
}

TEST(instructions_test, test_ins)
{
    Disassembler d {};

    std::stringstream ss {};
    d.Disassemble(g_bin_path_abs + "instructions.bc");
    d.Serialize(ss);

    size_t beg_g = ss.str().find("g_u1_() <static> {");
    size_t end_g = ss.str().find('}', beg_g);

    ASSERT_TRUE(beg_g != std::string::npos && end_g != std::string::npos) << "function g not found";

    std::string body_g = ss.str().substr(beg_g + strlen("g() {"), end_g - (beg_g + strlen("g() {")));

    EXPECT_TRUE(body_g.find("\tmov v0, v1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tmov.64 v2, v3") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tmov.obj v4, v5") != std::string::npos);

    EXPECT_TRUE(body_g.find("\tmovi v0, 0xffffffffffffffff") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tmovi.64 v0, 0x2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tfmovi.64 v0, 0x4008147ae147ae14") != std::string::npos);

    EXPECT_TRUE(body_g.find("\tlda v1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tlda.64 v0") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tlda.obj v1") != std::string::npos);

    EXPECT_TRUE(body_g.find("\tldai 0x1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tldai.64 0x2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tfldai.64 0x4008147ae147ae14") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tlda.str \"kek\"") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tlda.type A") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tlda.null") != std::string::npos);

    EXPECT_TRUE(body_g.find("\tsta v0") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tsta.64 v1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tsta.obj v2") != std::string::npos);

    EXPECT_TRUE(body_g.find("\tjump_label_0: jmp jump_label_0") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tjeq v1, jump_label_1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tldai 0x1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tjmp jump_label_2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tjump_label_1: ldai 0x0") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tjump_label_2: cmp.64 v1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tucmp v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tucmp.64 v3") != std::string::npos);

    EXPECT_TRUE(body_g.find("\tfcmpl.64 v1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tfcmpg.64 v1") != std::string::npos);

    EXPECT_TRUE(body_g.find("\tjeqz jump_label_0") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tjnez jump_label_0") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tjltz jump_label_0") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tjgtz jump_label_0") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tjlez jump_label_0") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tjgez jump_label_0") != std::string::npos);

    EXPECT_TRUE(body_g.find("\tjeq v2, jump_label_0") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tjne v2, jump_label_0") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tjlt v2, jump_label_0") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tjgt v2, jump_label_0") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tjle v2, jump_label_0") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tjge v2, jump_label_0") != std::string::npos);

    EXPECT_TRUE(body_g.find("\tfadd2.64 v1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tfsub2.64 v1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tfmul2.64 v1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tfdiv2.64 v1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tfmod2.64 v1") != std::string::npos);

    EXPECT_TRUE(body_g.find("\tadd2 v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tadd2.64 v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tsub2 v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tsub2.64 v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tmul2 v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tmul2.64 v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tand2 v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tand2.64 v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tor2 v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tor2.64 v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\txor2 v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\txor2.64 v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tshl2 v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tshl2.64 v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tshr2 v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tshr2.64 v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tashr2 v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tashr2.64 v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tdiv2 v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tdiv2.64 v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tmod2 v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tmod2.64 v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tdivu2 v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tdivu2.64 v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tmodu2 v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tmodu2.64 v2") != std::string::npos);

    EXPECT_TRUE(body_g.find("\tadd v1, v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tsub v1, v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tmul v1, v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tand v1, v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tor v1, v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\txor v1, v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tshl v1, v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tshr v1, v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tashr v1, v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tdiv v1, v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tmod v1, v2") != std::string::npos);

    EXPECT_TRUE(body_g.find("\taddi 0x1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tsubi 0x1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tmuli 0x1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tandi 0x1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tori 0x1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\txori 0x1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tshli 0x1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tshri 0x1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tashri 0x1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tdivi 0x1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tmodi 0x1") != std::string::npos);

    EXPECT_TRUE(body_g.find("\tneg") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tneg.64") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tnot") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tnot.64") != std::string::npos);

    EXPECT_TRUE(body_g.find("\ti32tof64") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tu32tof64") != std::string::npos);
    EXPECT_TRUE(body_g.find("\ti64tof64") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tu64tof64") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tf64toi32") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tf64toi64") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tf64tou32") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tf64tou64") != std::string::npos);
    EXPECT_TRUE(body_g.find("\ti32toi64") != std::string::npos);
    EXPECT_TRUE(body_g.find("\ti64toi32") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tu32toi64") != std::string::npos);

    EXPECT_TRUE(body_g.find("\tldarr.8 v1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tldarru.8 v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tldarr.16 v1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tldarru.16 v1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tldarr v1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tldarr.64 v1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tfldarr.32 v1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tfldarr.64 v1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tldarr.obj v1") != std::string::npos);

    EXPECT_TRUE(body_g.find("\tstarr.8 v1, v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tstarr.16 v1, v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tstarr v1, v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tstarr.64 v1, v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tfstarr.32 v1, v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tfstarr.64 v1, v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tstarr.obj v1, v2") != std::string::npos);

    EXPECT_TRUE(body_g.find("\tnewobj v6, A") != std::string::npos);

    EXPECT_TRUE(body_g.find("\tinitobj A.init_") != std::string::npos);

    EXPECT_TRUE(body_g.find("\tldobj v0, A.kek") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tldobj.64 v0, A.kek") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tldobj.obj v0, A.kek") != std::string::npos);

    EXPECT_TRUE(body_g.find("\tstobj v1, A.kek") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tstobj.64 v1, A.kek") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tstobj.obj v1, A.kek") != std::string::npos);

    EXPECT_TRUE(body_g.find("\tldstatic A.kek") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tldstatic.64 A.kek") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tldstatic.obj A.kek") != std::string::npos);

    EXPECT_TRUE(body_g.find("\tststatic A.kek") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tststatic.64 A.kek") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tststatic.obj A.kek") != std::string::npos);

    EXPECT_TRUE(body_g.find("\tcheckcast A") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tisinstance A") != std::string::npos);
}

TEST(instructions_test, test_calls)
{
    Disassembler d {};

    std::stringstream ss {};
    d.Disassemble(g_bin_path_abs + "calls.bc");
    d.Serialize(ss);

    size_t beg_g = ss.str().find("g_u1_u1_(u1 a0) <static> {");
    size_t end_g = ss.str().find('}', beg_g);

    ASSERT_TRUE(beg_g != std::string::npos && end_g != std::string::npos) << "function g not found";

    std::string body_g = ss.str().substr(beg_g + strlen("g_u1_u1_(u1 a0) <static> {"),
                                         end_g - (beg_g + strlen("g_u1_u1_(u1 a0) <static> {")));

    EXPECT_TRUE(body_g.find("\tcall.virt.short B.Bhandler_unspec_B_u8_, v4") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tcall.virt.short B.Bhandler_short_B_u1_u8_, v4, v1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tcall.virt B.Bhandler_short2_B_u1_i64_u8_, v4, v1, v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tcall.virt B.Bhandler_long_B_i8_i16_i32_u16_, v4, v0, v1, v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tcall.virt.range B.Bhandler_range_B_i8_i16_i32_i8_i16_i32_u16_, v4") !=
                std::string::npos);

    EXPECT_TRUE(body_g.find("\tcall.short handler_unspec_u8_") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tcall.short handler_short_u1_u8_, v1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tcall.short handler_short2_u1_i64_u8_, v1, v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tcall handler_long_i8_i16_i32_u16_, v0, v1, v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tcall handler_long2_i8_i16_i32_f64_u16_, v0, v1, v2, v3") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tcall.range handler_range_i8_i16_i32_i8_i16_i32_u16_, v0") != std::string::npos);

    EXPECT_TRUE(body_g.find("\tinitobj B.Bhandler_unspec_B_u8_") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tinitobj.short B.Bhandler_short_B_u1_u8_, v1") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tinitobj.short B.Bhandler_short2_B_u1_i64_u8_, v1, v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tinitobj B.Bhandler_long_B_i8_i16_i32_u16_, v0, v1, v2") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tinitobj B.Bhandler_long2_B_i8_i16_i32_i64_u16_, v0, v1, v2, v3") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tinitobj.range B.Bhandler_range_B_i8_i16_i32_i8_i16_i32_u16_, v0") != std::string::npos);

    EXPECT_TRUE(body_g.find("\tcall.acc.short handler_short_u1_u8_, v0, 0x0") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tcall.acc.short handler_short2_u1_i64_u8_, a0, 0x1") != std::string::npos);

    EXPECT_TRUE(ss.str().find(".function u16 long_function_i8_i16_i32_i8_i16_i32_i64_f32_u16_(i8 a0, i16 a1, i32 a2, "
                              "i8 a3, i16 a4, i32 a5, i64 a6, f32 a7)") != std::string::npos);

    EXPECT_TRUE(body_g.find("\tcalli.dyn.short 0x1, v0") != std::string::npos);
}

TEST(instructions_test, test_returns)
{
    Disassembler d {};

    std::stringstream ss {};
    d.Disassemble(g_bin_path_abs + "returns.bc");
    d.Serialize(ss);

    size_t beg_g = ss.str().find("g_u1_() <static> {");
    size_t end_g = ss.str().find('}', beg_g);

    ASSERT_TRUE(beg_g != std::string::npos && end_g != std::string::npos) << "function g not found";

    std::string body_g = ss.str().substr(beg_g + strlen("g() {"), end_g - (beg_g + strlen("g() {")));

    EXPECT_TRUE(body_g.find("\treturn") != std::string::npos);
    EXPECT_TRUE(body_g.find("\treturn.64") != std::string::npos);
    EXPECT_TRUE(body_g.find("\treturn.obj") != std::string::npos);
    EXPECT_TRUE(body_g.find("\treturn.void") != std::string::npos);
}

TEST(instructions_test, test_newarr)
{
    Disassembler d {};

    std::stringstream ss {};
    d.Disassemble(g_bin_path_abs + "newarrs.bc");
    d.Serialize(ss);

    size_t beg_g = ss.str().find("g_u1_u1_(u1 a0) <static> {");
    size_t end_g = ss.str().find('}', beg_g);

    ASSERT_TRUE(beg_g != std::string::npos && end_g != std::string::npos) << "function g not found";

    std::string body_g = ss.str().substr(beg_g + strlen("g() {"), end_g - (beg_g + strlen("g() {")));

    EXPECT_TRUE(body_g.find("\tnewarr v0, a0, u1[]") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tnewarr v0, a0, i8[]") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tnewarr v0, a0, u8[]") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tnewarr v0, a0, i16[]") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tnewarr v0, a0, u16[]") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tnewarr v0, a0, i32[]") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tnewarr v0, a0, u32[]") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tnewarr v0, a0, f32[]") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tnewarr v0, a0, f64[]") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tnewarr v0, a0, i64[]") != std::string::npos);
    EXPECT_TRUE(body_g.find("\tnewarr v0, a0, u64[]") != std::string::npos);
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
