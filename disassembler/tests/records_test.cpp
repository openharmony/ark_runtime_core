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

TEST(record_test, empty_record)
{
    Disassembler d {};

    std::stringstream ss {};
    d.Disassemble(g_bin_path_abs + "empty_record.bc");
    d.Serialize(ss, false);

    EXPECT_TRUE(ss.str().find(".record A {\n}") != std::string::npos) << "record translated incorrectly";
}

TEST(record_test, record_with_fields)
{
    Disassembler d {};

    std::stringstream ss {};
    d.Disassemble(g_bin_path_abs + "record_with_fields.bc");
    d.Serialize(ss, false);

    EXPECT_TRUE(ss.str().find("u1 a") != std::string::npos) << "u1 translated incorrectly";
    EXPECT_TRUE(ss.str().find("i8 b") != std::string::npos) << "i8 translated incorrectly";
    EXPECT_TRUE(ss.str().find("u8 c") != std::string::npos) << "u8 translated incorrectly";
    EXPECT_TRUE(ss.str().find("i16 d") != std::string::npos) << "i16 translated incorrectly";
    EXPECT_TRUE(ss.str().find("u16 e") != std::string::npos) << "u16 translated incorrectly";
    EXPECT_TRUE(ss.str().find("i32 f") != std::string::npos) << "i32 translated incorrectly";
    EXPECT_TRUE(ss.str().find("u32 g") != std::string::npos) << "u32 translated incorrectly";
    EXPECT_TRUE(ss.str().find("f32 h") != std::string::npos) << "f32 translated incorrectly";
    EXPECT_TRUE(ss.str().find("f64 i") != std::string::npos) << "f64 translated incorrectly";
    EXPECT_TRUE(ss.str().find("i64 j") != std::string::npos) << "i64 translated incorrectly";
    EXPECT_TRUE(ss.str().find("u64 k") != std::string::npos) << "u64 translated incorrectly";
}

TEST(record_test, record_with_record)
{
    Disassembler d {};

    std::stringstream ss {};
    d.Disassemble(g_bin_path_abs + "record_in_record.bc");
    d.Serialize(ss, false);

    size_t beg_a = ss.str().find(".record A");
    size_t end_a = ss.str().find('}', beg_a);
    size_t beg_b = ss.str().find(".record B");
    size_t end_b = ss.str().find("}", beg_b);

    ASSERT_TRUE(beg_a != std::string::npos && end_a != std::string::npos) << "record A not found";
    ASSERT_TRUE(beg_b != std::string::npos && end_b != std::string::npos) << "record B not found";

    std::string rec_a = ss.str().substr(beg_a, end_a + 1);
    std::string rec_b = ss.str().substr(beg_b, end_b + 1);

    EXPECT_TRUE(rec_a.find("i64 aw") != std::string::npos);

    EXPECT_TRUE(rec_b.find("A a") != std::string::npos);
    EXPECT_TRUE(rec_b.find("i32 c") != std::string::npos);
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
