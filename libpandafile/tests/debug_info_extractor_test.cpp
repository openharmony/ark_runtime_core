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

#include "debug_info_extractor.h"
#include "annotation_data_accessor.h"
#include "class_data_accessor-inl.h"
#include "code_data_accessor-inl.h"
#include "debug_data_accessor-inl.h"
#include "field_data_accessor-inl.h"
#include "file.h"
#include "file_item_container.h"
#include "file_writer.h"
#include "helpers.h"
#include "method_data_accessor-inl.h"
#include "modifiers.h"
#include "proto_data_accessor-inl.h"

#include <cstdio>

#include <memory>
#include <vector>

#include <gtest/gtest.h>

namespace panda::panda_file::test {

static const char G_SOURCE_FILE[] = "asm.pa";

void PreparePandaFile(ItemContainer *container)
{
    ClassItem *class_item = container->GetOrCreateClassItem("A");
    class_item->SetAccessFlags(ACC_PUBLIC);

    StringItem *method_name = container->GetOrCreateStringItem("foo");

    PrimitiveTypeItem *ret_type = container->CreateItem<PrimitiveTypeItem>(Type::TypeId::VOID);
    std::vector<MethodParamItem> params;
    params.emplace_back(container->CreateItem<PrimitiveTypeItem>(Type::TypeId::I32));
    ProtoItem *proto_item = container->GetOrCreateProtoItem(ret_type, params);
    MethodItem *method_item = class_item->AddMethod(method_name, proto_item, ACC_PUBLIC | ACC_STATIC, params);

    std::vector<uint8_t> instructions {1, 2, 3, 4};
    CodeItem *code_item = container->CreateItem<CodeItem>(4U, 1, instructions);

    method_item->SetCode(code_item);

    StringItem *source_file_item = container->GetOrCreateStringItem(G_SOURCE_FILE);
    StringItem *param_string_item = container->GetOrCreateStringItem("arg0");
    StringItem *local_variable_name_0 = container->GetOrCreateStringItem("local_0");
    StringItem *local_variable_name_1 = container->GetOrCreateStringItem("local_1");
    StringItem *local_variable_name_2 = container->GetOrCreateStringItem("local_2");
    StringItem *local_variable_type_i32 = container->GetOrCreateStringItem("I");
    StringItem *local_variable_sig_type_i32 = container->GetOrCreateStringItem("type_i32");

    LineNumberProgramItem *line_number_program_item = container->CreateLineNumberProgramItem();
    DebugInfoItem *debug_info_item = container->CreateItem<DebugInfoItem>(line_number_program_item);
    method_item->SetDebugInfo(debug_info_item);

    // Add static method with ref arg

    StringItem *method_name_bar = container->GetOrCreateStringItem("bar");

    PrimitiveTypeItem *ret_type_bar = container->CreateItem<PrimitiveTypeItem>(Type::TypeId::VOID);
    std::vector<MethodParamItem> params_bar;
    params_bar.emplace_back(container->CreateItem<PrimitiveTypeItem>(Type::TypeId::I32));
    params_bar.emplace_back(container->GetOrCreateClassItem("RefArg"));
    ProtoItem *proto_item_bar = container->GetOrCreateProtoItem(ret_type_bar, params_bar);
    MethodItem *method_item_bar =
        class_item->AddMethod(method_name_bar, proto_item_bar, ACC_PUBLIC | ACC_STATIC, params_bar);

    CodeItem *code_item_bar = container->CreateItem<CodeItem>(0, 2U, instructions);

    method_item_bar->SetCode(code_item_bar);

    StringItem *param_string_item_bar1 = container->GetOrCreateStringItem("arg0");
    StringItem *param_string_item_bar2 = container->GetOrCreateStringItem("arg1");

    LineNumberProgramItem *line_number_program_item_bar = container->CreateLineNumberProgramItem();
    DebugInfoItem *debug_info_item_bar = container->CreateItem<DebugInfoItem>(line_number_program_item_bar);
    method_item_bar->SetDebugInfo(debug_info_item_bar);

    // Add non static method with ref arg

    StringItem *method_name_baz = container->GetOrCreateStringItem("baz");

    PrimitiveTypeItem *ret_type_baz = container->CreateItem<PrimitiveTypeItem>(Type::TypeId::VOID);
    std::vector<MethodParamItem> params_baz;
    params_baz.emplace_back(container->GetOrCreateClassItem("RefArg"));
    params_baz.emplace_back(container->CreateItem<PrimitiveTypeItem>(Type::TypeId::U1));
    ProtoItem *proto_item_baz = container->GetOrCreateProtoItem(ret_type_baz, params_baz);
    MethodItem *method_item_baz = class_item->AddMethod(method_name_baz, proto_item_baz, ACC_PUBLIC, params_baz);

    CodeItem *code_item_baz = container->CreateItem<CodeItem>(0, 2U, instructions);

    method_item_baz->SetCode(code_item_baz);

    StringItem *param_string_item_baz1 = container->GetOrCreateStringItem("arg0");
    StringItem *param_string_item_baz2 = container->GetOrCreateStringItem("arg1");

    LineNumberProgramItem *line_number_program_item_baz = container->CreateLineNumberProgramItem();
    DebugInfoItem *debug_info_item_baz = container->CreateItem<DebugInfoItem>(line_number_program_item_baz);
    method_item_baz->SetDebugInfo(debug_info_item_baz);

    // Add debug info for the following source file:

    //  1 # file: asm.pa
    //  2 .function foo(i32 arg0) {
    //  3   ldai arg0
    //  4   stai v1     // START_LOCAL: reg=1, name="local_0", type="i32"
    //  5   ldai 2
    //  6   stai v2     // START_LOCAL_EXTENDED: reg=2, name="local_1",
    //  type="i32", type_signature="type_i32" 7               // END_LOCAL: reg=1
    //  8   stai v3     // START_LOCAL: reg=3, name="local_2", type="i32"
    //  9
    // 10   return.void
    // 11 }
    // 12 .function bar(i32 arg0, B arg1) { // static
    // 13   ldai arg0
    // 13   return.void
    // 14 }
    // 15 .function baz(B arg0, u1 arg1) { // non static
    // 17   ldai arg0
    // 16   return.void
    // 17 }

    container->ComputeLayout();

    // foo line number program
    auto *constant_pool = debug_info_item->GetConstantPool();
    // Line 3
    debug_info_item->SetLineNumber(3);
    line_number_program_item->EmitSetFile(constant_pool, source_file_item);
    line_number_program_item->EmitAdvancePc(constant_pool, 1);
    line_number_program_item->EmitAdvanceLine(constant_pool, 1);
    line_number_program_item->EmitSpecialOpcode(0, 0);
    // Line 4
    line_number_program_item->EmitStartLocal(constant_pool, 1, local_variable_name_0, local_variable_type_i32);
    line_number_program_item->EmitSpecialOpcode(1, 1);
    // Line 5
    line_number_program_item->EmitSpecialOpcode(1, 1);
    // Line 6
    line_number_program_item->EmitStartLocalExtended(constant_pool, 2U, local_variable_name_1, local_variable_type_i32,
                                                     local_variable_sig_type_i32);
    line_number_program_item->EmitEndLocal(1);
    line_number_program_item->EmitSpecialOpcode(1, 2U);
    // Line 8
    line_number_program_item->EmitStartLocal(constant_pool, 3U, local_variable_name_2, local_variable_type_i32);
    line_number_program_item->EmitAdvanceLine(constant_pool, 2U);
    line_number_program_item->EmitSpecialOpcode(0, 0);
    // Line 10
    line_number_program_item->EmitEnd();

    debug_info_item->AddParameter(param_string_item);

    method_item->SetDebugInfo(debug_info_item);

    // bar line number program
    auto *constant_pool_bar = debug_info_item_bar->GetConstantPool();
    debug_info_item_bar->SetLineNumber(13U);
    line_number_program_item_bar->EmitSetFile(constant_pool_bar, source_file_item);
    line_number_program_item_bar->EmitAdvancePc(constant_pool_bar, 1);
    line_number_program_item_bar->EmitAdvanceLine(constant_pool_bar, 1);
    line_number_program_item_bar->EmitSpecialOpcode(0, 0);
    line_number_program_item_bar->EmitEnd();

    debug_info_item_bar->AddParameter(param_string_item_bar1);
    debug_info_item_bar->AddParameter(param_string_item_bar2);

    method_item_bar->SetDebugInfo(debug_info_item_bar);

    // baz line number program
    auto *constant_pool_baz = debug_info_item_baz->GetConstantPool();
    debug_info_item_baz->SetLineNumber(15U);
    line_number_program_item_baz->EmitSetFile(constant_pool_baz, source_file_item);
    line_number_program_item_baz->EmitAdvancePc(constant_pool_baz, 1);
    line_number_program_item_baz->EmitAdvanceLine(constant_pool_baz, 1);
    line_number_program_item_baz->EmitSpecialOpcode(0, 0);
    line_number_program_item_baz->EmitEnd();

    debug_info_item_baz->AddParameter(param_string_item_baz1);
    debug_info_item_baz->AddParameter(param_string_item_baz2);

    method_item_baz->SetDebugInfo(debug_info_item_baz);
}

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
struct SourcePairLocation {
    std::string path;  // NOLINT(misc-non-private-member-variables-in-classes)
    size_t line;       // NOLINT(misc-non-private-member-variables-in-classes)

    bool operator==(const SourcePairLocation &other) const
    {
        return (path == other.path) && (line == other.line);
    }

    bool IsValid() const
    {
        return !path.empty();
    }
};

static std::optional<size_t> GetLineNumberByTableOffsetWrapper(const panda_file::LineNumberTable &table,
                                                               uint32_t offset)
{
    for (const auto &value : table) {
        if (value.offset == offset) {
            return value.line;
        }
    }
    return std::nullopt;
}

static std::optional<uint32_t> GetOffsetByTableLineNumberWrapper(const panda_file::LineNumberTable &table, size_t line)
{
    for (const auto &value : table) {
        if (value.line == line) {
            return value.offset;
        }
    }
    return std::nullopt;
}

static std::pair<File::EntityId, uint32_t> GetBreakpointAddressWrapper(DebugInfoExtractor extractor,
                                                                       const SourcePairLocation &source_location)
{
    auto pos = source_location.path.find_last_of("/\\");
    auto name = source_location.path;

    if (pos != std::string::npos) {
        name = name.substr(pos + 1);
    }

    std::vector<panda_file::File::EntityId> methods = extractor.GetMethodIdList();
    for (const auto &method : methods) {
        if (extractor.GetSourceFile(method) == source_location.path || extractor.GetSourceFile(method) == name) {
            panda_file::LineNumberTable line_table = extractor.GetLineNumberTable(method);
            if (line_table.empty()) {
                continue;
            }

            std::optional<size_t> offset = GetOffsetByTableLineNumberWrapper(line_table, source_location.line);
            if (offset == std::nullopt) {
                continue;
            }
            return {method, offset.value()};
        }
    }
    return {File::EntityId(), 0};
}

static std::vector<panda_file::LocalVariableInfo> GetLocalVariableInfoWrapper(DebugInfoExtractor extractor,
                                                                              File::EntityId method_id, size_t offset)
{
    std::vector<panda_file::LocalVariableInfo> variables = extractor.GetLocalVariableTable(method_id);
    std::vector<panda_file::LocalVariableInfo> result;

    for (const auto &variable : variables) {
        if (variable.start_offset <= offset && offset <= variable.end_offset) {
            result.push_back(variable);
        }
    }
    return result;
}

static SourcePairLocation GetSourcePairLocationWrapper(DebugInfoExtractor extractor, File::EntityId method_id,
                                                       uint32_t bytecode_offset)
{
    panda_file::LineNumberTable line_table = extractor.GetLineNumberTable(method_id);
    if (line_table.empty()) {
        return SourcePairLocation();
    }

    std::optional<size_t> line = GetLineNumberByTableOffsetWrapper(line_table, bytecode_offset);
    if (line == std::nullopt) {
        return SourcePairLocation();
    }

    return SourcePairLocation {extractor.GetSourceFile(method_id), line.value()};
}

static std::unique_ptr<const File> GetPandaFile(std::vector<uint8_t> &data)
{
    os::mem::ConstBytePtr ptr(reinterpret_cast<std::byte *>(data.data()), data.size(),
                              [](std::byte *, size_t) noexcept {});
    return File::OpenFromMemory(std::move(ptr));
}

class ExtractorTest : public testing::Test {
public:
    static void SetUpTestCase()
    {
        ItemContainer container;
        PreparePandaFile(&container);
        MemoryWriter writer;
        ASSERT_TRUE(container.Write(&writer));

        file_data = writer.GetData();
        panda_file = GetPandaFile(file_data);
    }

    static std::unique_ptr<const panda_file::File> panda_file;
    static std::vector<uint8_t> file_data;
};

std::unique_ptr<const panda_file::File> ExtractorTest::panda_file {nullptr};
std::vector<uint8_t> ExtractorTest::file_data;

TEST_F(ExtractorTest, DebugInfoTest)
{
    const panda_file::File *pf = panda_file.get();
    ASSERT_TRUE(pf != nullptr);
    DebugInfoExtractor extractor(pf);

    auto breakpoint1_address = GetBreakpointAddressWrapper(extractor, SourcePairLocation {G_SOURCE_FILE, 1});
    ASSERT_FALSE(breakpoint1_address.first.IsValid());
    auto [method_id, bytecode_offset] = GetBreakpointAddressWrapper(extractor, SourcePairLocation {G_SOURCE_FILE, 6});
    ASSERT_TRUE(method_id.IsValid());
    ASSERT_EQ(bytecode_offset, 3U);

    auto source_location = GetSourcePairLocationWrapper(extractor, method_id, 2);
    ASSERT_EQ(source_location.path, G_SOURCE_FILE);
    ASSERT_EQ(source_location.line, 5U);

    auto vars = GetLocalVariableInfoWrapper(extractor, method_id, 4);
    EXPECT_EQ(vars.size(), 2);
    ASSERT_EQ(vars[0].name, "local_1");
    ASSERT_EQ(vars[0].type, "I");
    ASSERT_EQ(vars[1].name, "local_2");
    ASSERT_EQ(vars[1].type, "I");
}

TEST_F(ExtractorTest, DebugInfoTestStaticWithRefArg)
{
    const panda_file::File *pf = panda_file.get();
    ASSERT_TRUE(pf != nullptr);
    DebugInfoExtractor extractor(pf);

    auto breakpoint1_address = GetBreakpointAddressWrapper(extractor, SourcePairLocation {G_SOURCE_FILE, 1});
    ASSERT_FALSE(breakpoint1_address.first.IsValid());
    auto method_id = GetBreakpointAddressWrapper(extractor, SourcePairLocation {G_SOURCE_FILE, 14}).first;
    ASSERT_TRUE(method_id.IsValid());

    auto vars = GetLocalVariableInfoWrapper(extractor, method_id, 14);
    EXPECT_EQ(vars.size(), 0);
}

TEST_F(ExtractorTest, DebugInfoTestNonStaticWithRefArg)
{
    const panda_file::File *pf = panda_file.get();
    ASSERT_TRUE(pf != nullptr);
    DebugInfoExtractor extractor(pf);

    auto breakpoint1_address = GetBreakpointAddressWrapper(extractor, SourcePairLocation {G_SOURCE_FILE, 1});
    ASSERT_FALSE(breakpoint1_address.first.IsValid());
    auto method_id = GetBreakpointAddressWrapper(extractor, SourcePairLocation {G_SOURCE_FILE, 16}).first;
    ASSERT_TRUE(method_id.IsValid());

    auto vars = GetLocalVariableInfoWrapper(extractor, method_id, 16);
    EXPECT_EQ(vars.size(), 0);
}

}  // namespace panda::panda_file::test
