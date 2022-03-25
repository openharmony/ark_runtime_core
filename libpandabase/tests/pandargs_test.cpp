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

#include "utils/pandargs.h"

#include <gtest/gtest.h>

namespace panda::test {
TEST(libpandargs, TestAPI)
{
    static const bool ref_def_bool = false;
    static const int ref_def_int = 0;
    static const double ref_def_double = 1.0;
    static const std::string ref_def_string = "noarg";
    static const uint32_t ref_def_uint32 = 0;
    static const uint64_t ref_def_uint64 = 0;
    static const arg_list_t ref_def_dlist = arg_list_t();
    static const arg_list_t ref_def_list = arg_list_t();

    PandArg<bool> pab("bool", ref_def_bool, "Sample boolean argument");
    PandArg<int> pai("int", ref_def_int, "Sample integer argument");
    PandArg<double> pad("double", ref_def_double, "Sample rational argument");
    PandArg<std::string> pas("string", ref_def_string, "Sample string argument");
    PandArg<uint32_t> pau32("uint32", ref_def_uint32, "Sample uint32 argument");
    PandArg<uint64_t> pau64("uint64", ref_def_uint64, "Sample uint64 argument");
    PandArg<arg_list_t> pald("dlist", ref_def_dlist, "Sample delimiter list argument", ":");
    PandArg<arg_list_t> pal("list", ref_def_list, "Sample list argument");
    PandArg<int> pair("rint", ref_def_int, "Integer argument with range", -100, 100);
    PandArg<uint32_t> paur32("ruint32", ref_def_uint64, "uint32 argument with range", 0, 1000000000);
    PandArg<uint64_t> paur64("ruint64", ref_def_uint64, "uint64 argument with range", 0, 100000000000);

    PandArgParser pa_parser;
    EXPECT_TRUE(pa_parser.Add(&pab));
    EXPECT_TRUE(pa_parser.Add(&pai));
    EXPECT_TRUE(pa_parser.Add(&pad));
    EXPECT_TRUE(pa_parser.Add(&pas));
    EXPECT_TRUE(pa_parser.Add(&pau32));
    EXPECT_TRUE(pa_parser.Add(&pau64));
    EXPECT_TRUE(pa_parser.Add(&pald));
    EXPECT_TRUE(pa_parser.Add(&pal));
    EXPECT_TRUE(pa_parser.Add(&pair));
    EXPECT_TRUE(pa_parser.Add(&paur32));
    EXPECT_TRUE(pa_parser.Add(&paur64));

    PandArg<bool> t_pab("tail_bool", ref_def_bool, "Sample tail boolean argument");
    PandArg<int> t_pai("tail_int", ref_def_int, "Sample tail integer argument");
    PandArg<double> t_pad("tail_double", ref_def_double, "Sample tail rational argument");
    PandArg<std::string> t_pas("tail_string", ref_def_string, "Sample tail string argument");
    PandArg<uint32_t> t_pau32("tail_uint32", ref_def_uint32, "Sample tail uint32 argument");
    PandArg<uint64_t> t_pau64("tail_uint64", ref_def_uint64, "Sample tail uint64 argument");

    // expect all arguments are set in parser
    {
        EXPECT_TRUE(pa_parser.IsArgSet(&pab));
        EXPECT_TRUE(pa_parser.IsArgSet(pab.GetName()));
        EXPECT_TRUE(pa_parser.IsArgSet(&pai));
        EXPECT_TRUE(pa_parser.IsArgSet(pai.GetName()));
        EXPECT_TRUE(pa_parser.IsArgSet(&pad));
        EXPECT_TRUE(pa_parser.IsArgSet(pad.GetName()));
        EXPECT_TRUE(pa_parser.IsArgSet(&pas));
        EXPECT_TRUE(pa_parser.IsArgSet(pas.GetName()));
        EXPECT_TRUE(pa_parser.IsArgSet(&pau32));
        EXPECT_TRUE(pa_parser.IsArgSet(pau32.GetName()));
        EXPECT_TRUE(pa_parser.IsArgSet(&pau64));
        EXPECT_TRUE(pa_parser.IsArgSet(pau64.GetName()));
        EXPECT_TRUE(pa_parser.IsArgSet(&pald));
        EXPECT_TRUE(pa_parser.IsArgSet(pald.GetName()));
        EXPECT_TRUE(pa_parser.IsArgSet(&pal));
        EXPECT_TRUE(pa_parser.IsArgSet(pal.GetName()));
        EXPECT_TRUE(pa_parser.IsArgSet(&pair));
        EXPECT_TRUE(pa_parser.IsArgSet(pair.GetName()));
        EXPECT_TRUE(pa_parser.IsArgSet(&paur32));
        EXPECT_TRUE(pa_parser.IsArgSet(paur32.GetName()));
        EXPECT_TRUE(pa_parser.IsArgSet(&paur64));
        EXPECT_TRUE(pa_parser.IsArgSet(paur64.GetName()));
    }

    // expect default values and types are consistent
    {
        EXPECT_EQ(pab.GetDefaultValue(), ref_def_bool);
        EXPECT_EQ(pab.GetDefaultValue(), pab.GetValue());
        EXPECT_EQ(pab.GetType(), PandArgType::BOOL);

        EXPECT_EQ(pai.GetDefaultValue(), ref_def_int);
        EXPECT_EQ(pai.GetDefaultValue(), pai.GetValue());
        EXPECT_EQ(pai.GetType(), PandArgType::INTEGER);

        EXPECT_DOUBLE_EQ(pad.GetValue(), ref_def_double);
        EXPECT_DOUBLE_EQ(pad.GetDefaultValue(), pad.GetValue());
        EXPECT_EQ(pad.GetType(), PandArgType::DOUBLE);

        EXPECT_EQ(pas.GetDefaultValue(), ref_def_string);
        EXPECT_EQ(pas.GetDefaultValue(), pas.GetValue());
        EXPECT_EQ(pas.GetType(), PandArgType::STRING);

        EXPECT_EQ(pau32.GetDefaultValue(), ref_def_uint32);
        EXPECT_EQ(pau32.GetDefaultValue(), pau32.GetValue());
        EXPECT_EQ(pau32.GetType(), PandArgType::UINT32);

        EXPECT_EQ(pau64.GetDefaultValue(), ref_def_uint64);
        EXPECT_EQ(pau64.GetDefaultValue(), pau64.GetValue());
        EXPECT_EQ(pau64.GetType(), PandArgType::UINT64);

        EXPECT_TRUE(pald.GetValue().empty());
        EXPECT_EQ(pald.GetDefaultValue(), pald.GetValue());
        EXPECT_EQ(pald.GetType(), PandArgType::LIST);

        EXPECT_TRUE(pal.GetValue().empty());
        EXPECT_EQ(pal.GetDefaultValue(), pal.GetValue());
        EXPECT_EQ(pal.GetType(), PandArgType::LIST);

        EXPECT_EQ(pair.GetDefaultValue(), ref_def_int);
        EXPECT_EQ(pair.GetDefaultValue(), pair.GetValue());
        EXPECT_EQ(pair.GetType(), PandArgType::INTEGER);

        EXPECT_EQ(paur32.GetDefaultValue(), ref_def_uint64);
        EXPECT_EQ(paur32.GetDefaultValue(), paur32.GetValue());
        EXPECT_EQ(paur32.GetType(), PandArgType::UINT32);

        EXPECT_EQ(paur64.GetDefaultValue(), ref_def_uint64);
        EXPECT_EQ(paur64.GetDefaultValue(), paur64.GetValue());
        EXPECT_EQ(paur64.GetType(), PandArgType::UINT64);
    }

    // expect false on duplicate argument
    {
        PandArg<int> pai_dup("int", 0, "Integer number 0");
        EXPECT_TRUE(pa_parser.IsArgSet(pai_dup.GetName()));
        EXPECT_FALSE(pa_parser.Add(&pai_dup));
    }

    // add tail argument, expect false on duplicate arguments
    // erase tail, expect 0 tail size
    {
        EXPECT_EQ(pa_parser.GetTailSize(), 0U);
        EXPECT_TRUE(pa_parser.PushBackTail(&t_pai));
        EXPECT_EQ(pa_parser.GetTailSize(), 1U);
        EXPECT_FALSE(pa_parser.PushBackTail(&t_pai));
        pa_parser.PopBackTail();
        EXPECT_EQ(pa_parser.GetTailSize(), 0U);
    }

    // expect help string is correct
    {
        std::string ref_string = "--" + pab.GetName() + ": " + pab.GetDesc() + "\n";
        ref_string += "--" + pald.GetName() + ": " + pald.GetDesc() + "\n";
        ref_string += "--" + pad.GetName() + ": " + pad.GetDesc() + "\n";
        ref_string += "--" + pai.GetName() + ": " + pai.GetDesc() + "\n";
        ref_string += "--" + pal.GetName() + ": " + pal.GetDesc() + "\n";
        ref_string += "--" + pair.GetName() + ": " + pair.GetDesc() + "\n";
        ref_string += "--" + paur32.GetName() + ": " + paur32.GetDesc() + "\n";
        ref_string += "--" + paur64.GetName() + ": " + paur64.GetDesc() + "\n";
        ref_string += "--" + pas.GetName() + ": " + pas.GetDesc() + "\n";
        ref_string += "--" + pau32.GetName() + ": " + pau32.GetDesc() + "\n";
        ref_string += "--" + pau64.GetName() + ": " + pau64.GetDesc() + "\n";
        EXPECT_EQ(pa_parser.GetHelpString(), ref_string);
    }

    // expect regular args list is correct
    {
        arg_list_t ref_arg_dlist = pald.GetValue();
        arg_list_t ref_arg_list = pal.GetValue();
        std::string ref_string = "--" + pab.GetName() + "=" + std::to_string(pab.GetValue()) + "\n";
        ref_string += "--" + pald.GetName() + "=";
        for (const auto &i : ref_arg_dlist) {
            ref_string += i + ", ";
        }
        ref_string += "\n";
        ref_string += "--" + pad.GetName() + "=" + std::to_string(pad.GetValue()) + "\n";
        ref_string += "--" + pai.GetName() + "=" + std::to_string(pai.GetValue()) + "\n";
        ref_string += "--" + pal.GetName() + "=";
        for (const auto &i : ref_arg_list) {
            ref_string += i + ", ";
        }
        ref_string += "\n";
        ref_string += "--" + pair.GetName() + "=" + std::to_string(pair.GetValue()) + "\n";
        ref_string += "--" + paur32.GetName() + "=" + std::to_string(paur32.GetValue()) + "\n";
        ref_string += "--" + paur64.GetName() + "=" + std::to_string(paur64.GetValue()) + "\n";
        ref_string += "--" + pas.GetName() + "=" + pas.GetValue() + "\n";
        ref_string += "--" + pau32.GetName() + "=" + std::to_string(pau32.GetValue()) + "\n";
        ref_string += "--" + pau64.GetName() + "=" + std::to_string(pau64.GetValue()) + "\n";
        EXPECT_EQ(pa_parser.GetRegularArgs(), ref_string);
    }

    // expect all boolean values processed right
    {
        static const char *true_values[] = {"true", "on", "1"};
        static const char *false_values[] = {"false", "off", "0"};
        static const int argc_bool_only = 3;
        static const char *argv_bool_only[argc_bool_only];
        argv_bool_only[0] = "gtest_app";
        std::string s = "--" + pab.GetName();
        argv_bool_only[1] = s.c_str();

        for (int i = 0; i < 3; i++) {
            argv_bool_only[2] = true_values[i];
            EXPECT_TRUE(pa_parser.Parse(argc_bool_only, argv_bool_only));
            EXPECT_TRUE(pab.GetValue());
        }
        for (int i = 0; i < 3; i++) {
            argv_bool_only[2] = false_values[i];
            EXPECT_TRUE(pa_parser.Parse(argc_bool_only, argv_bool_only));
            EXPECT_FALSE(pab.GetValue());
        }
    }

    // expect wrong boolean arguments with "=" processed right
    {
        static const int argc_bool_only = 2;
        static const char *argv_bool_only[argc_bool_only];
        argv_bool_only[0] = "gtest_app";
        std::string s = "--" + pab.GetName() + "=";
        argv_bool_only[1] = s.c_str();
        EXPECT_FALSE(pa_parser.Parse(argc_bool_only, argv_bool_only));
    }

    // expect boolean at the end of arguments line is true
    {
        static const int argc_bool_only = 2;
        static const char *argv_bool_only[argc_bool_only];
        argv_bool_only[0] = "gtest_app";
        std::string s = "--" + pab.GetName();
        argv_bool_only[1] = s.c_str();
        EXPECT_TRUE(pa_parser.Parse(argc_bool_only, argv_bool_only));
        EXPECT_TRUE(pab.GetValue());
    }

    // expect positive and negative integer values processed right
    {
        static const int ref_int_pos = 42422424;
        static const int ref_int_neg = -42422424;
        static const int argc_int_only = 3;
        static const char *argv_int_only[argc_int_only];
        argv_int_only[0] = "gtest_app";
        std::string s = "--" + pai.GetName();
        argv_int_only[1] = s.c_str();
        argv_int_only[2] = "42422424";
        EXPECT_TRUE(pa_parser.Parse(argc_int_only, argv_int_only));
        EXPECT_EQ(pai.GetValue(), ref_int_pos);
        argv_int_only[2] = "-42422424";
        EXPECT_TRUE(pa_parser.Parse(argc_int_only, argv_int_only));
        EXPECT_EQ(pai.GetValue(), ref_int_neg);
    }

    // expect positive and negative double values processed right
    {
        static const double ref_double_pos = 4242.2424;
        static const double ref_double_neg = -4242.2424;
        static const int argc_double_only = 3;
        static const char *argv_double_only[argc_double_only];
        argv_double_only[0] = "gtest_app";
        std::string s = "--" + pad.GetName();
        argv_double_only[1] = s.c_str();
        argv_double_only[2] = "4242.2424";
        EXPECT_TRUE(pa_parser.Parse(argc_double_only, argv_double_only));
        EXPECT_EQ(pad.GetValue(), ref_double_pos);
        argv_double_only[2] = "-4242.2424";
        EXPECT_TRUE(pa_parser.Parse(argc_double_only, argv_double_only));
        EXPECT_EQ(pad.GetValue(), ref_double_neg);
    }

    // expect uint32_t values processed right
    {
        static const uint32_t ref_uint32_pos = 4242422424;
        static const int argc_uint32_only = 3;
        static const char *argv_uint32_only[argc_uint32_only];
        argv_uint32_only[0] = "gtest_app";
        std::string s = "--" + pau32.GetName();
        argv_uint32_only[1] = s.c_str();
        argv_uint32_only[2] = "4242422424";
        EXPECT_TRUE(pa_parser.Parse(argc_uint32_only, argv_uint32_only));
        EXPECT_EQ(pau32.GetValue(), ref_uint32_pos);
    }

    // expect uint64_t values processed right
    {
        static const uint64_t ref_uint64_pos = 424242422424;
        static const int argc_uint64_only = 3;
        static const char *argv_uint64_only[argc_uint64_only];
        argv_uint64_only[0] = "gtest_app";
        std::string s = "--" + pau64.GetName();
        argv_uint64_only[1] = s.c_str();
        argv_uint64_only[2] = "424242422424";
        EXPECT_TRUE(pa_parser.Parse(argc_uint64_only, argv_uint64_only));
        EXPECT_EQ(pau64.GetValue(), ref_uint64_pos);
    }

    // expect hex values processed right
    {
        static const uint64_t ref_uint64 = 274877906944;
        static const int ref_int = 64;
        static const int argc_uint64_int = 3;
        static const char *argv_uint64_int[argc_uint64_int];
        argv_uint64_int[0] = "gtest_app";
        std::string s = "--" + pau64.GetName();
        argv_uint64_int[1] = s.c_str();
        argv_uint64_int[2] = "0x4000000000";
        EXPECT_TRUE(pa_parser.Parse(argc_uint64_int, argv_uint64_int));
        EXPECT_EQ(pau64.GetValue(), ref_uint64);
        argv_uint64_int[2] = "0x40";
        EXPECT_TRUE(pa_parser.Parse(argc_uint64_int, argv_uint64_int));
        EXPECT_EQ(pau64.GetValue(), ref_int);
    }

    // expect out of range uint32_t values processed right
    {
        static const int argc_uint32_only = 3;
        static const char *argv_uint32_only[argc_uint32_only];
        argv_uint32_only[0] = "gtest_app";
        std::string s = "--" + pau32.GetName();
        argv_uint32_only[1] = s.c_str();
        argv_uint32_only[2] = "424224244242242442422424";
        EXPECT_FALSE(pa_parser.Parse(argc_uint32_only, argv_uint32_only));
        argv_uint32_only[2] = "0xffffffffffffffffffffffffff";
        EXPECT_FALSE(pa_parser.Parse(argc_uint32_only, argv_uint32_only));
    }

    // expect out of range uint64_t values processed right
    {
        static const int argc_uint64_only = 3;
        static const char *argv_uint64_only[argc_uint64_only];
        argv_uint64_only[0] = "gtest_app";
        std::string s = "--" + pau64.GetName();
        argv_uint64_only[1] = s.c_str();
        argv_uint64_only[2] = "424224244242242442422424";
        EXPECT_FALSE(pa_parser.Parse(argc_uint64_only, argv_uint64_only));
        argv_uint64_only[2] = "0xffffffffffffffffffffffffff";
        EXPECT_FALSE(pa_parser.Parse(argc_uint64_only, argv_uint64_only));
    }

    // expect string argument of one word and multiple word processed right
    {
        static const std::string ref_one_string = "string";
        static const std::string ref_multiple_string = "this is a string";
        static const char *str_argname = "--string";
        static const int argc_one_string = 3;
        static const char *argv_one_string[argc_one_string] = {"gtest_app", str_argname, "string"};
        static const int argc_multiple_string = 3;
        static const char *argv_multiple_string[argc_multiple_string] = {"gtest_app", str_argname, "this is a string"};
        EXPECT_TRUE(pa_parser.Parse(argc_multiple_string, argv_multiple_string));
        EXPECT_EQ(pas.GetValue(), ref_multiple_string);
        EXPECT_TRUE(pa_parser.Parse(argc_one_string, argv_one_string));
        EXPECT_EQ(pas.GetValue(), ref_one_string);
    }

    // expect string at the end of line is an empty string
    {
        static const int argc_string_only = 2;
        static const char *argv_string_only[argc_string_only];
        argv_string_only[0] = "gtest_app";
        std::string s = "--" + pas.GetName();
        argv_string_only[1] = s.c_str();
        EXPECT_TRUE(pa_parser.Parse(argc_string_only, argv_string_only));
        EXPECT_EQ(pas.GetValue(), "");
    }

    // expect list argument processed right
    {
        pald.ResetDefaultValue();
        static const arg_list_t ref_list = {"list1", "list2", "list3"};
        std::string s = "--" + pald.GetName();
        static const char *list_argname = s.c_str();
        static const int argc_list_only = 7;
        static const char *argv_list_only[argc_list_only] = {"gtest_app", list_argname, "list1", list_argname,
                                                             "list2",     list_argname, "list3"};
        EXPECT_TRUE(pa_parser.Parse(argc_list_only, argv_list_only));
        ASSERT_EQ(pald.GetValue().size(), ref_list.size());
        for (std::size_t i = 0; i < ref_list.size(); ++i) {
            EXPECT_EQ(pald.GetValue()[i], ref_list[i]);
        }
    }

    // expect list argument without delimiter processed right
    {
        pal.ResetDefaultValue();
        static const arg_list_t ref_list = {"list1", "list2", "list3", "list4"};
        std::string s = "--" + pal.GetName();
        static const char *list_argname = s.c_str();
        static const int argc_list_only = 9;
        static const char *argv_list_only[argc_list_only] = {
            "gtest_app", list_argname, "list1", list_argname, "list2", list_argname, "list3", list_argname, "list4"};
        EXPECT_TRUE(pa_parser.Parse(argc_list_only, argv_list_only));
        ASSERT_EQ(pal.GetValue().size(), ref_list.size());
        for (std::size_t i = 0; i < ref_list.size(); ++i) {
            EXPECT_EQ(pal.GetValue()[i], ref_list[i]);
        }
    }

    // expect delimiter list argument processed right
    {
        pald.ResetDefaultValue();
        static const arg_list_t ref_dlist = {"dlist1", "dlist2", "dlist3"};
        std::string s = "--" + pald.GetName();
        static const char *list_argname = s.c_str();
        static const int argc_dlist_only = 3;
        static const char *argv_dlist_only[argc_dlist_only] = {"gtest_app", list_argname, "dlist1:dlist2:dlist3"};
        EXPECT_TRUE(pa_parser.Parse(argc_dlist_only, argv_dlist_only));
        ASSERT_EQ(pald.GetValue().size(), ref_dlist.size());
        for (std::size_t i = 0; i < ref_dlist.size(); ++i) {
            EXPECT_EQ(pald.GetValue()[i], ref_dlist[i]);
        }
    }

    // expect delimiter and multiple list argument processed right
    {
        pald.ResetDefaultValue();
        static const arg_list_t ref_list = {"dlist1", "dlist2", "list1", "list2", "dlist3", "dlist4"};
        std::string s = "--" + pald.GetName();
        static const char *list_argname = s.c_str();
        static const int argc_list = 9;
        static const char *argv_list[argc_list] = {"gtest_app",  list_argname, "dlist1:dlist2", list_argname,   "list1",
                                                   list_argname, "list2",      list_argname,    "dlist3:dlist4"};
        EXPECT_TRUE(pa_parser.Parse(argc_list, argv_list));
        ASSERT_EQ(pald.GetValue().size(), ref_list.size());
        for (std::size_t i = 0; i < ref_list.size(); ++i) {
            EXPECT_EQ(pald.GetValue()[i], ref_list[i]);
        }
    }

    // expect positive and negative integer values with range processed right
    {
        static const int ref_int_pos = 99;
        static const int ref_int_neg = -99;
        static const int argc_int_only = 3;
        static const char *argv_int_only[argc_int_only];
        argv_int_only[0] = "gtest_app";
        std::string s = "--" + pair.GetName();
        argv_int_only[1] = s.c_str();
        argv_int_only[2] = "99";
        EXPECT_TRUE(pa_parser.Parse(argc_int_only, argv_int_only));
        EXPECT_EQ(pair.GetValue(), ref_int_pos);
        argv_int_only[2] = "-99";
        EXPECT_TRUE(pa_parser.Parse(argc_int_only, argv_int_only));
        EXPECT_EQ(pair.GetValue(), ref_int_neg);
    }

    // expect wrong positive and negative integer values with range processed right
    {
        static const int ref_int_pos = 101;
        static const int ref_int_neg = -101;
        static const int argc_int_only = 3;
        static const char *argv_int_only[argc_int_only];
        argv_int_only[0] = "gtest_app";
        std::string s = "--" + pair.GetName();
        argv_int_only[1] = s.c_str();
        argv_int_only[2] = "101";
        EXPECT_FALSE(pa_parser.Parse(argc_int_only, argv_int_only));
        argv_int_only[2] = "-101";
        EXPECT_FALSE(pa_parser.Parse(argc_int_only, argv_int_only));
    }

    // expect uint32_t values with range processed right
    {
        static const uint32_t ref_int_min = 1;
        static const uint32_t ref_int_max = 990000000;
        static const int argc_int_only = 3;
        static const char *argv_int_only[argc_int_only];
        argv_int_only[0] = "gtest_app";
        std::string s = "--" + paur32.GetName();
        argv_int_only[1] = s.c_str();
        argv_int_only[2] = "1";
        EXPECT_TRUE(pa_parser.Parse(argc_int_only, argv_int_only));
        EXPECT_EQ(paur32.GetValue(), ref_int_min);
        argv_int_only[2] = "990000000";
        EXPECT_TRUE(pa_parser.Parse(argc_int_only, argv_int_only));
        EXPECT_EQ(paur32.GetValue(), ref_int_max);
    }

    // expect wrong uint32_t values with range processed right
    {
        static const uint32_t ref_int_min = -1;
        static const uint32_t ref_int_max = 1000000001;
        static const int argc_int_only = 3;
        static const char *argv_int_only[argc_int_only];
        argv_int_only[0] = "gtest_app";
        std::string s = "--" + paur32.GetName();
        argv_int_only[1] = s.c_str();
        argv_int_only[2] = "-1";
        EXPECT_FALSE(pa_parser.Parse(argc_int_only, argv_int_only));
        argv_int_only[2] = "1000000001";
        EXPECT_FALSE(pa_parser.Parse(argc_int_only, argv_int_only));
    }

    // expect uint64_t values with range processed right
    {
        static const uint64_t ref_int_min = 1;
        static const uint64_t ref_int_max = 99000000000;
        static const int argc_int_only = 3;
        static const char *argv_int_only[argc_int_only];
        argv_int_only[0] = "gtest_app";
        std::string s = "--" + paur64.GetName();
        argv_int_only[1] = s.c_str();
        argv_int_only[2] = "1";
        EXPECT_TRUE(pa_parser.Parse(argc_int_only, argv_int_only));
        EXPECT_EQ(paur64.GetValue(), ref_int_min);
        argv_int_only[2] = "99000000000";
        EXPECT_TRUE(pa_parser.Parse(argc_int_only, argv_int_only));
        EXPECT_EQ(paur64.GetValue(), ref_int_max);
    }

    // expect wrong uint64_t values with range processed right
    {
        static const uint64_t ref_int_min = -1;
        static const uint64_t ref_int_max = 100000000001;
        static const int argc_int_only = 3;
        static const char *argv_int_only[argc_int_only];
        argv_int_only[0] = "gtest_app";
        std::string s = "--" + paur64.GetName();
        argv_int_only[1] = s.c_str();
        argv_int_only[2] = "-1";
        EXPECT_FALSE(pa_parser.Parse(argc_int_only, argv_int_only));
        argv_int_only[2] = "100000000001";
        EXPECT_FALSE(pa_parser.Parse(argc_int_only, argv_int_only));
    }

    // expect list at the end of line is a list with empty string
    {
        pald.ResetDefaultValue();
        static const arg_list_t ref_list = {""};
        static const int argc_list_only = 2;
        static const char *argv_list_only[argc_list_only];
        argv_list_only[0] = "gtest_app";
        std::string s = "--" + pald.GetName();
        argv_list_only[1] = s.c_str();
        EXPECT_TRUE(pa_parser.Parse(argc_list_only, argv_list_only));
        EXPECT_EQ(pald.GetValue(), ref_list);
    }

    // expect true on IsTailEnabled when tail is enabled, false otherwise
    {
        pa_parser.EnableTail();
        EXPECT_TRUE(pa_parser.IsTailEnabled());
        pa_parser.DisableTail();
        EXPECT_FALSE(pa_parser.IsTailEnabled());
    }

    // expect tail only argument is consistent
    {
        static const int argc_tail_only = 2;
        static const char *argv_tail_only[] = {"gtest_app", "tail1"};
        static const std::string ref_str_tail = "tail1";
        pa_parser.EnableTail();
        pa_parser.PushBackTail(&t_pas);
        EXPECT_TRUE(pa_parser.Parse(argc_tail_only, argv_tail_only));
        ASSERT_EQ(t_pas.GetValue(), ref_str_tail);
        pa_parser.DisableTail();
        pa_parser.EraseTail();
    }

    // expect multiple tail only argument is consistent
    {
        static const int argc_tail_only = 7;
        static const char *argv_tail_only[] = {"gtest_app", "str_tail", "off", "-4", "3.14", "2", "4"};
        static const std::string str_ref = "str_tail";
        static const bool bool_ref = false;
        static const int int_ref = -4;
        static const double double_ref = 3.14;
        static const uint32_t uint32_ref = 2;
        static const uint64_t uint64_ref = 4;
        pa_parser.EnableTail();
        pa_parser.PushBackTail(&t_pas);
        pa_parser.PushBackTail(&t_pab);
        pa_parser.PushBackTail(&t_pai);
        pa_parser.PushBackTail(&t_pad);
        pa_parser.PushBackTail(&t_pau32);
        pa_parser.PushBackTail(&t_pau64);
        EXPECT_EQ(pa_parser.GetTailSize(), 6U);
        EXPECT_TRUE(pa_parser.Parse(argc_tail_only, argv_tail_only));
        EXPECT_EQ(t_pas.GetValue(), str_ref);
        EXPECT_EQ(t_pab.GetValue(), bool_ref);
        EXPECT_EQ(t_pai.GetValue(), int_ref);
        EXPECT_DOUBLE_EQ(t_pad.GetValue(), double_ref);
        EXPECT_EQ(t_pau32.GetValue(), uint32_ref);
        EXPECT_EQ(t_pau64.GetValue(), uint64_ref);
        pa_parser.DisableTail();
        pa_parser.EraseTail();
        EXPECT_EQ(pa_parser.GetTailSize(), 0U);
    }

    // expect parse fail on wrong tail argument type
    {
        pa_parser.EnableTail();
        static const int argc_tail_only = 3;
        // boolean value instead of integer
        static const char *argv_tail_only[] = {"gtest_app", "str_tail", "off"};
        static const std::string str_ref = "str_tail";
        pa_parser.PushBackTail(&t_pas);
        pa_parser.PushBackTail(&t_pai);
        EXPECT_EQ(pa_parser.GetTailSize(), 2U);
        EXPECT_FALSE(pa_parser.Parse(argc_tail_only, argv_tail_only));
        EXPECT_EQ(t_pas.GetValue(), str_ref);
        pa_parser.DisableTail();
        pa_parser.EraseTail();
        EXPECT_EQ(pa_parser.GetTailSize(), 0U);
    }

    // expect right tail argument processing after preceding string arguments
    {
        pa_parser.EnableTail();
        static const char *str_argname = "--string";
        static const std::string ref_string = "this is a reference string";
        static const std::string ref_t_str = "string";
        static const double ref_t_double = 0.1;
        static const bool ref_t_bool = true;
        static const uint32_t ref_t_uint32 = 32;
        static const uint64_t ref_t_uint64 = 64;
        static const int argc_tail_string = 8;
        static const char *argv_tail_string[] = {
            "gtest_app", str_argname, "this is a reference string", "string", ".1", "on", "32", "64"};
        pa_parser.PushBackTail(&t_pas);
        pa_parser.PushBackTail(&t_pad);
        pa_parser.PushBackTail(&t_pab);
        pa_parser.PushBackTail(&t_pau32);
        pa_parser.PushBackTail(&t_pau64);
        EXPECT_TRUE(pa_parser.Parse(argc_tail_string, argv_tail_string));
        EXPECT_EQ(pas.GetValue(), ref_string);
        EXPECT_EQ(t_pas.GetValue(), ref_t_str);
        EXPECT_EQ(t_pad.GetValue(), ref_t_double);
        EXPECT_EQ(t_pab.GetValue(), ref_t_bool);
        EXPECT_EQ(t_pau32.GetValue(), ref_t_uint32);
        EXPECT_EQ(t_pau64.GetValue(), ref_t_uint64);
        pa_parser.DisableTail();
        pa_parser.EraseTail();
    }

    // expect right tail argument processing after preceding list argument
    {
        pald.ResetDefaultValue();
        pa_parser.EnableTail();
        static const char *list_argname = "--dlist";
        static const arg_list_t ref_list = {"list1", "list2", "list3", "list4", "list5"};
        static const double ref_t_double = -7;
        static const bool ref_t_bool = true;
        static const int ref_t_int = 255;
        static const uint32_t ref_t_uint32 = 32;
        static const uint64_t ref_t_uint64 = 64;
        static const int argc_tail_list = 16;
        static const char *argv_tail_list[] = {"gtest_app", list_argname, "list1", list_argname, "list2", list_argname,
                                               "list3",     list_argname, "list4", list_argname, "list5", "true",
                                               "255",       "-7",         "32",    "64"};
        pa_parser.PushBackTail(&t_pab);
        pa_parser.PushBackTail(&t_pai);
        pa_parser.PushBackTail(&t_pad);
        pa_parser.PushBackTail(&t_pau32);
        pa_parser.PushBackTail(&t_pau64);
        EXPECT_TRUE(pa_parser.Parse(argc_tail_list, argv_tail_list));
        ASSERT_EQ(pald.GetValue().size(), ref_list.size());
        for (std::size_t i = 0; i < ref_list.size(); i++) {
            EXPECT_EQ(pald.GetValue()[i], ref_list[i]);
        }
        EXPECT_EQ(t_pab.GetValue(), ref_t_bool);
        EXPECT_EQ(t_pai.GetValue(), ref_t_int);
        EXPECT_DOUBLE_EQ(t_pad.GetValue(), ref_t_double);
        EXPECT_EQ(t_pau32.GetValue(), ref_t_uint32);
        EXPECT_EQ(t_pau64.GetValue(), ref_t_uint64);

        pa_parser.DisableTail();
        pa_parser.EraseTail();
    }

    // expected result: tail arguments are processed properly after noparam boolean arguments
    {
        pa_parser.EnableTail();
        PandArg<std::string> t_pas0("tail_string0", ref_def_string, "Sample tail string argument 0");
        PandArg<std::string> t_pas1("tail_string1", ref_def_string, "Sample tail string argument 1");
        static const std::string ref_t_str1 = "offtail1";
        static const std::string ref_t_str2 = "offtail2";
        static const std::string ref_t_str3 = "offtail3";
        static const int argc_tail_bool = 5;
        static const char *argv_tail_bool[] = {"gtest_app", "--bool", "offtail1", "offtail2", "offtail3"};
        pa_parser.PushBackTail(&t_pas);
        pa_parser.PushBackTail(&t_pas0);
        pa_parser.PushBackTail(&t_pas1);
        EXPECT_TRUE(pa_parser.Parse(argc_tail_bool, argv_tail_bool));
        EXPECT_TRUE(pab.GetValue());
        EXPECT_EQ(t_pas.GetValue(), ref_t_str1);
        EXPECT_EQ(t_pas0.GetValue(), ref_t_str2);
        EXPECT_EQ(t_pas1.GetValue(), ref_t_str3);
        pa_parser.DisableTail();
        pa_parser.EraseTail();
    }

    // expect fail on amount of tail arguments more than pa_parser may have
    {
        pa_parser.EnableTail();
        static const int argc_tail = 5;
        static const char *argv_tail[] = {"gtest_app", "gdb", "--args", "file.bin", "entry"};

        PandArg<std::string> t_pas1("tail_string1", ref_def_string, "Sample tail string argument 1");
        pa_parser.PushBackTail(&t_pas);
        pa_parser.PushBackTail(&t_pas1);

        EXPECT_EQ(pa_parser.GetTailSize(), 2U);
        EXPECT_FALSE(pa_parser.Parse(argc_tail, argv_tail));
        pa_parser.DisableTail();
        pa_parser.EraseTail();
    }

    // expect remainder arguments only parsed as expected
    {
        pa_parser.EnableRemainder();
        static const arg_list_t ref_rem = {"rem1", "rem2", "rem3"};
        static int argc_rem = 5;
        static const char *argv_rem[] = {"gtest_app", "--", "rem1", "rem2", "rem3"};
        pa_parser.Parse(argc_rem, argv_rem);
        arg_list_t remainder = pa_parser.GetRemainder();
        EXPECT_EQ(remainder.size(), ref_rem.size());
        for (std::size_t i = 0; i < remainder.size(); i++) {
            EXPECT_EQ(remainder[i], ref_rem[i]);
        }
        pa_parser.DisableRemainder();
    }

    // expect regular argument before remainder parsed right
    {
        pa_parser.EnableRemainder();
        static const arg_list_t ref_rem = {"rem1", "rem2", "rem3"};
        std::string bool_name = "--" + pab.GetName();
        static int argc_rem = 6;
        static const char *argv_rem[] = {"gtest_app", bool_name.c_str(), "--", "rem1", "rem2", "rem3"};
        pa_parser.Parse(argc_rem, argv_rem);
        EXPECT_TRUE(pab.GetValue());
        arg_list_t remainder = pa_parser.GetRemainder();
        EXPECT_EQ(remainder.size(), ref_rem.size());
        for (std::size_t i = 0; i < remainder.size(); i++) {
            EXPECT_EQ(remainder[i], ref_rem[i]);
        }
        pa_parser.DisableRemainder();
    }

    // expect that all arguments parsed as expected
    {
        pald.ResetDefaultValue();
        pa_parser.EnableTail();
        pa_parser.EnableRemainder();
        static const arg_list_t ref_rem = {"rem1", "rem2", "rem3"};
        PandArg<std::string> t_pas0("tail_string0", ref_def_string, "Sample tail string argument 0");
        PandArg<std::string> t_pas1("tail_string1", ref_def_string, "Sample tail string argument 1");
        static const bool ref_bool = true;
        static const int ref_int = 42;
        static const arg_list_t ref_dlist = {"dlist1", "dlist2", "dlist3", "dlist4"};
        static const std::string ref_t_str1 = "tail1";
        static const std::string ref_t_str2 = "tail2 tail3";
        static const std::string ref_t_str3 = "tail4";
        static const std::string ref_str = "this is a string";
        static const double ref_dbl = 0.42;
        static const uint32_t ref_uint32 = std::numeric_limits<std::uint32_t>::max();
        static const uint32_t ref_uint32r = 990000000;
        static const uint64_t ref_uint64 = std::numeric_limits<std::uint64_t>::max();
        static const uint64_t ref_uint64r = 99000000000;
        static int argc_consistent = 21;
        static const char *argv_consistent[] = {"gtest_app",
                                                "--bool",
                                                "on",
                                                "--int=42",
                                                "--string",
                                                "this is a string",
                                                "--double",
                                                ".42",
                                                "--uint32=4294967295",
                                                "--uint64=18446744073709551615",
                                                "--dlist=dlist1:dlist2:dlist3:dlist4",
                                                "--rint=42",
                                                "--ruint32=990000000",
                                                "--ruint64=99000000000",
                                                "tail1",
                                                "tail2 tail3",
                                                "tail4",
                                                "--",
                                                "rem1",
                                                "rem2",
                                                "rem3"};
        pa_parser.PushBackTail(&t_pas);
        pa_parser.PushBackTail(&t_pas0);
        pa_parser.PushBackTail(&t_pas1);
        EXPECT_TRUE(pa_parser.Parse(argc_consistent, argv_consistent));
        EXPECT_EQ(pab.GetValue(), ref_bool);
        EXPECT_EQ(pai.GetValue(), ref_int);
        EXPECT_EQ(pas.GetValue(), ref_str);
        EXPECT_DOUBLE_EQ(pad.GetValue(), ref_dbl);
        EXPECT_EQ(pau32.GetValue(), ref_uint32);
        EXPECT_EQ(pau64.GetValue(), ref_uint64);
        ASSERT_EQ(pald.GetValue().size(), ref_dlist.size());
        for (std::size_t i = 0; i < ref_dlist.size(); ++i) {
            EXPECT_EQ(pald.GetValue()[i], ref_dlist[i]);
        }
        EXPECT_EQ(pair.GetValue(), ref_int);
        EXPECT_EQ(paur32.GetValue(), ref_uint32r);
        EXPECT_EQ(paur64.GetValue(), ref_uint64r);
        EXPECT_EQ(t_pas.GetValue(), ref_t_str1);
        EXPECT_EQ(t_pas0.GetValue(), ref_t_str2);
        EXPECT_EQ(t_pas1.GetValue(), ref_t_str3);
        arg_list_t remainder = pa_parser.GetRemainder();
        EXPECT_EQ(remainder.size(), ref_rem.size());
        for (std::size_t i = 0; i < remainder.size(); i++) {
            EXPECT_EQ(remainder[i], ref_rem[i]);
        }
        pa_parser.DisableRemainder();
        pa_parser.DisableTail();
        pa_parser.EraseTail();
    }
}
}  // namespace panda::test
