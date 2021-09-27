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

#include "annotation_data_accessor.h"
#include "class_data_accessor-inl.h"
#include "code_data_accessor-inl.h"
#include "debug_data_accessor-inl.h"
#include "field_data_accessor-inl.h"
#include "file.h"
#include "file_format_version.h"
#include "file_item_container.h"
#include "file_writer.h"
#include "helpers.h"
#include "method_data_accessor-inl.h"
#include "method_handle_data_accessor.h"
#include "modifiers.h"
#include "os/file.h"
#include "proto_data_accessor-inl.h"
#include "value.h"

#include <cstddef>

#include <memory>
#include <vector>

#include <securec.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace panda::panda_file::test {

TEST(ItemContainer, DeduplicationTest)
{
    ItemContainer container;

    StringItem *string_item = container.GetOrCreateStringItem("1");
    EXPECT_EQ(string_item, container.GetOrCreateStringItem("1"));

    ClassItem *class_item = container.GetOrCreateClassItem("1");
    EXPECT_EQ(class_item, container.GetOrCreateClassItem("1"));

    ValueItem *int_item = container.GetOrCreateIntegerValueItem(1);
    EXPECT_EQ(int_item, container.GetOrCreateIntegerValueItem(1));

    ValueItem *long_item = container.GetOrCreateLongValueItem(1);
    EXPECT_EQ(long_item, container.GetOrCreateLongValueItem(1));
    EXPECT_NE(long_item, int_item);

    ValueItem *float_item = container.GetOrCreateFloatValueItem(1.0);
    EXPECT_EQ(float_item, container.GetOrCreateFloatValueItem(1.0));
    EXPECT_NE(float_item, int_item);
    EXPECT_NE(float_item, long_item);

    ValueItem *double_item = container.GetOrCreateDoubleValueItem(1.0);
    EXPECT_EQ(double_item, container.GetOrCreateDoubleValueItem(1.0));
    EXPECT_NE(double_item, int_item);
    EXPECT_NE(double_item, long_item);
    EXPECT_NE(double_item, float_item);
}

TEST(ItemContainer, TestFileOpen)
{
    using panda::os::file::Mode;
    using panda::os::file::Open;

    // Write panda file to disk
    ItemContainer container;

    const std::string file_name = "test_file_open.panda";
    auto writer = FileWriter(file_name);

    ASSERT_TRUE(container.Write(&writer));

    // Read panda file from disk
    EXPECT_NE(File::Open(file_name), nullptr);
}

TEST(ItemContainer, TestFileFormatVersionTooOld)
{
    const std::string file_name = "test_file_format_version_too_old.abc";
    {
        ItemContainer container;
        auto writer = FileWriter(file_name);

        File::Header header = {};
        header.magic = File::MAGIC;

        auto old = std::array<uint8_t, File::VERSION_SIZE>(minVersion);
        --old[3];

        header.version = old;
        header.file_size = sizeof(File::Header);

        for (uint8_t b : Span<uint8_t>(reinterpret_cast<uint8_t *>(&header), sizeof(header))) {
            writer.WriteByte(b);
        }
    }

    EXPECT_EQ(File::Open(file_name), nullptr);
}

TEST(ItemContainer, TestFileFormatVersionTooNew)
{
    const std::string file_name = "test_file_format_version_too_new.abc";
    {
        ItemContainer container;
        auto writer = FileWriter(file_name);

        File::Header header = {};
        header.magic = File::MAGIC;

        auto new_ = std::array<uint8_t, File::VERSION_SIZE>(minVersion);
        ++new_[3];

        header.version = new_;
        header.file_size = sizeof(File::Header);

        for (uint8_t b : Span<uint8_t>(reinterpret_cast<uint8_t *>(&header), sizeof(header))) {
            writer.WriteByte(b);
        }
    }

    EXPECT_EQ(File::Open(file_name), nullptr);
}

TEST(ItemContainer, TestFileFormatVersionValid)
{
    const std::string file_name = "test_file_format_version_valid.abc";
    {
        ItemContainer container;
        auto writer = FileWriter(file_name);

        File::Header header;
        (void)memset_s(&header, sizeof(header), 0, sizeof(header));
        header.magic = File::MAGIC;
        header.version = {0, 0, 0, 2};
        header.file_size = sizeof(File::Header);

        for (uint8_t b : Span<uint8_t>(reinterpret_cast<uint8_t *>(&header), sizeof(header))) {
            writer.WriteByte(b);
        }
    }

    EXPECT_NE(File::Open(file_name), nullptr);
}

static std::unique_ptr<const File> GetPandaFile(std::vector<uint8_t> &data)
{
    os::mem::ConstBytePtr ptr(reinterpret_cast<std::byte *>(data.data()), data.size(),
                              [](std::byte *, size_t) noexcept {});
    return File::OpenFromMemory(std::move(ptr));
}

TEST(ItemContainer, TestClasses)
{
    // Write panda file to memory

    ItemContainer container;

    ClassItem *empty_class_item = container.GetOrCreateClassItem("Foo");

    ClassItem *class_item = container.GetOrCreateClassItem("Bar");
    class_item->SetAccessFlags(ACC_PUBLIC);
    class_item->SetSuperClass(empty_class_item);

    // Add interface
    ClassItem *iface_item = container.GetOrCreateClassItem("Iface");
    iface_item->SetAccessFlags(ACC_PUBLIC);

    class_item->AddInterface(iface_item);

    // Add method
    StringItem *method_name = container.GetOrCreateStringItem("foo");

    PrimitiveTypeItem *ret_type = container.CreateItem<PrimitiveTypeItem>(Type::TypeId::VOID);
    std::vector<MethodParamItem> params;
    ProtoItem *proto_item = container.GetOrCreateProtoItem(ret_type, params);

    MethodItem *method_item = class_item->AddMethod(method_name, proto_item, ACC_PUBLIC | ACC_STATIC, params);

    // Add field
    StringItem *field_name = container.GetOrCreateStringItem("field");
    PrimitiveTypeItem *field_type = container.CreateItem<PrimitiveTypeItem>(Type::TypeId::I32);

    FieldItem *field_item = class_item->AddField(field_name, field_type, ACC_PUBLIC);

    // Add runtime annotation
    std::vector<AnnotationItem::Elem> runtime_elems;
    std::vector<AnnotationItem::Tag> runtime_tags;
    AnnotationItem *runtime_annotation_item =
        container.CreateItem<AnnotationItem>(class_item, runtime_elems, runtime_tags);

    class_item->AddRuntimeAnnotation(runtime_annotation_item);

    // Add annotation
    std::vector<AnnotationItem::Elem> elems;
    std::vector<AnnotationItem::Tag> tags;
    AnnotationItem *annotation_item = container.CreateItem<AnnotationItem>(class_item, elems, tags);

    class_item->AddAnnotation(annotation_item);

    // Add source file
    StringItem *source_file = container.GetOrCreateStringItem("source_file");

    class_item->SetSourceFile(source_file);

    MemoryWriter mem_writer;

    ASSERT_TRUE(container.Write(&mem_writer));

    // Read panda file from memory
    auto data = mem_writer.GetData();
    auto panda_file = GetPandaFile(data);

    ASSERT_NE(panda_file, nullptr);

    EXPECT_THAT(panda_file->GetHeader()->version, ::testing::ElementsAre(0, 0, 0, 2));
    EXPECT_EQ(panda_file->GetHeader()->file_size, mem_writer.GetData().size());
    EXPECT_EQ(panda_file->GetHeader()->foreign_off, 0U);
    EXPECT_EQ(panda_file->GetHeader()->foreign_size, 0U);
    EXPECT_EQ(panda_file->GetHeader()->num_classes, 3U);
    EXPECT_EQ(panda_file->GetHeader()->class_idx_off, sizeof(File::Header));

    const uint32_t *class_index =
        reinterpret_cast<const uint32_t *>(panda_file->GetBase() + panda_file->GetHeader()->class_idx_off);
    EXPECT_EQ(class_index[0], class_item->GetOffset());
    EXPECT_EQ(class_index[1], empty_class_item->GetOffset());

    std::vector<uint8_t> class_name {'B', 'a', 'r', '\0'};
    auto class_id = panda_file->GetClassId(class_name.data());
    EXPECT_EQ(class_id.GetOffset(), class_item->GetOffset());

    ClassDataAccessor class_data_accessor(*panda_file, class_id);
    EXPECT_EQ(class_data_accessor.GetSuperClassId().GetOffset(), empty_class_item->GetOffset());
    EXPECT_EQ(class_data_accessor.GetAccessFlags(), ACC_PUBLIC);
    EXPECT_EQ(class_data_accessor.GetFieldsNumber(), 1U);
    EXPECT_EQ(class_data_accessor.GetMethodsNumber(), 1U);
    EXPECT_EQ(class_data_accessor.GetIfacesNumber(), 1U);
    EXPECT_TRUE(class_data_accessor.GetSourceFileId().has_value());
    EXPECT_EQ(class_data_accessor.GetSourceFileId().value().GetOffset(), source_file->GetOffset());
    EXPECT_EQ(class_data_accessor.GetSize(), class_item->GetSize());

    class_data_accessor.EnumerateInterfaces([&](File::EntityId id) {
        EXPECT_EQ(id.GetOffset(), iface_item->GetOffset());

        ClassDataAccessor iface_class_data_accessor(*panda_file, id);
        EXPECT_EQ(iface_class_data_accessor.GetSuperClassId().GetOffset(), 0U);
        EXPECT_EQ(iface_class_data_accessor.GetAccessFlags(), ACC_PUBLIC);
        EXPECT_EQ(iface_class_data_accessor.GetFieldsNumber(), 0U);
        EXPECT_EQ(iface_class_data_accessor.GetMethodsNumber(), 0U);
        EXPECT_EQ(iface_class_data_accessor.GetIfacesNumber(), 0U);
        EXPECT_FALSE(iface_class_data_accessor.GetSourceFileId().has_value());
        EXPECT_EQ(iface_class_data_accessor.GetSize(), iface_item->GetSize());
    });

    class_data_accessor.EnumerateRuntimeAnnotations([&](File::EntityId id) {
        EXPECT_EQ(id.GetOffset(), runtime_annotation_item->GetOffset());

        AnnotationDataAccessor data_accessor(*panda_file, id);
        EXPECT_EQ(data_accessor.GetAnnotationId().GetOffset(), runtime_annotation_item->GetOffset());
        EXPECT_EQ(data_accessor.GetClassId().GetOffset(), class_item->GetOffset());
        EXPECT_EQ(data_accessor.GetCount(), 0U);
    });

    // Annotation is the same as the runtime one, so we deduplicate it
    EXPECT_FALSE(annotation_item->NeedsEmit());
    annotation_item = runtime_annotation_item;

    class_data_accessor.EnumerateAnnotations([&](File::EntityId id) {
        EXPECT_EQ(id.GetOffset(), annotation_item->GetOffset());

        AnnotationDataAccessor data_accessor(*panda_file, id);
        EXPECT_EQ(data_accessor.GetAnnotationId().GetOffset(), annotation_item->GetOffset());
        EXPECT_EQ(data_accessor.GetClassId().GetOffset(), class_item->GetOffset());
        EXPECT_EQ(data_accessor.GetCount(), 0U);
    });

    class_data_accessor.EnumerateFields([&](FieldDataAccessor &data_accessor) {
        EXPECT_EQ(data_accessor.GetFieldId().GetOffset(), field_item->GetOffset());
        EXPECT_EQ(data_accessor.GetClassId().GetOffset(), class_item->GetOffset());
        EXPECT_EQ(data_accessor.GetNameId().GetOffset(), field_name->GetOffset());
        EXPECT_EQ(data_accessor.GetType(), Type(Type::TypeId::I32).GetFieldEncoding());
        EXPECT_EQ(data_accessor.GetAccessFlags(), ACC_PUBLIC);
        EXPECT_FALSE(data_accessor.GetValue<int32_t>().has_value());
        EXPECT_EQ(data_accessor.GetSize(), field_item->GetSize());

        data_accessor.EnumerateRuntimeAnnotations([](File::EntityId) { EXPECT_TRUE(false); });
        data_accessor.EnumerateAnnotations([](File::EntityId) { EXPECT_TRUE(false); });
    });

    class_data_accessor.EnumerateMethods([&](MethodDataAccessor &data_accessor) {
        EXPECT_FALSE(data_accessor.IsExternal());
        EXPECT_EQ(data_accessor.GetMethodId().GetOffset(), method_item->GetOffset());
        EXPECT_EQ(data_accessor.GetClassId().GetOffset(), class_item->GetOffset());
        EXPECT_EQ(data_accessor.GetNameId().GetOffset(), method_name->GetOffset());
        EXPECT_EQ(data_accessor.GetProtoId().GetOffset(), proto_item->GetOffset());
        EXPECT_EQ(data_accessor.GetAccessFlags(), ACC_PUBLIC | ACC_STATIC);
        EXPECT_FALSE(data_accessor.GetCodeId().has_value());
        EXPECT_EQ(data_accessor.GetSize(), method_item->GetSize());
        EXPECT_FALSE(data_accessor.GetRuntimeParamAnnotationId().has_value());
        EXPECT_FALSE(data_accessor.GetParamAnnotationId().has_value());
        EXPECT_FALSE(data_accessor.GetDebugInfoId().has_value());

        data_accessor.EnumerateRuntimeAnnotations([](File::EntityId) { EXPECT_TRUE(false); });
        data_accessor.EnumerateAnnotations([](File::EntityId) { EXPECT_TRUE(false); });
    });

    ClassDataAccessor empty_class_data_accessor(*panda_file, File::EntityId(empty_class_item->GetOffset()));
    EXPECT_EQ(empty_class_data_accessor.GetSuperClassId().GetOffset(), 0U);
    EXPECT_EQ(empty_class_data_accessor.GetAccessFlags(), 0U);
    EXPECT_EQ(empty_class_data_accessor.GetFieldsNumber(), 0U);
    EXPECT_EQ(empty_class_data_accessor.GetMethodsNumber(), 0U);
    EXPECT_EQ(empty_class_data_accessor.GetIfacesNumber(), 0U);
    EXPECT_FALSE(empty_class_data_accessor.GetSourceFileId().has_value());
    EXPECT_EQ(empty_class_data_accessor.GetSize(), empty_class_item->GetSize());
}

TEST(ItemContainer, TestMethods)
{
    // Write panda file to memory
    ItemContainer container;

    ClassItem *class_item = container.GetOrCreateClassItem("A");
    class_item->SetAccessFlags(ACC_PUBLIC);

    StringItem *method_name = container.GetOrCreateStringItem("foo");

    PrimitiveTypeItem *ret_type = container.CreateItem<PrimitiveTypeItem>(Type::TypeId::VOID);
    std::vector<MethodParamItem> params;
    ProtoItem *proto_item = container.GetOrCreateProtoItem(ret_type, params);

    MethodItem *method_item = class_item->AddMethod(method_name, proto_item, ACC_PUBLIC | ACC_STATIC, params);

    std::vector<uint8_t> instructions {1, 2, 3, 4};
    CodeItem *code_item = container.CreateItem<CodeItem>(0, 2, instructions);

    method_item->SetCode(code_item);

    MemoryWriter mem_writer;

    ASSERT_TRUE(container.Write(&mem_writer));

    // Read panda file from memory
    auto data = mem_writer.GetData();
    auto panda_file = GetPandaFile(data);

    ASSERT_NE(panda_file, nullptr);

    ClassDataAccessor class_data_accessor(*panda_file, File::EntityId(class_item->GetOffset()));

    class_data_accessor.EnumerateMethods([&](MethodDataAccessor &data_accessor) {
        EXPECT_FALSE(data_accessor.IsExternal());
        EXPECT_EQ(data_accessor.GetMethodId().GetOffset(), method_item->GetOffset());
        EXPECT_EQ(data_accessor.GetClassId().GetOffset(), class_item->GetOffset());
        EXPECT_EQ(data_accessor.GetNameId().GetOffset(), method_name->GetOffset());
        EXPECT_EQ(data_accessor.GetProtoId().GetOffset(), proto_item->GetOffset());
        EXPECT_EQ(data_accessor.GetAccessFlags(), ACC_PUBLIC | ACC_STATIC);
        EXPECT_EQ(data_accessor.GetSize(), method_item->GetSize());

        auto code_id = data_accessor.GetCodeId();
        EXPECT_TRUE(code_id.has_value());
        EXPECT_EQ(code_id.value().GetOffset(), code_item->GetOffset());

        CodeDataAccessor code_data_accessor(*panda_file, code_id.value());
        EXPECT_EQ(code_data_accessor.GetNumVregs(), 0U);
        EXPECT_EQ(code_data_accessor.GetNumArgs(), 2U);
        EXPECT_EQ(code_data_accessor.GetCodeSize(), instructions.size());
        EXPECT_THAT(instructions, ::testing::ElementsAreArray(code_data_accessor.GetInstructions(),
                                                              code_data_accessor.GetCodeSize()));

        EXPECT_EQ(code_data_accessor.GetTriesSize(), 0U);
        EXPECT_EQ(code_data_accessor.GetSize(), code_item->GetSize());

        code_data_accessor.EnumerateTryBlocks([](const CodeDataAccessor::TryBlock &) {
            EXPECT_TRUE(false);
            return false;
        });

        EXPECT_FALSE(data_accessor.GetDebugInfoId().has_value());

        EXPECT_FALSE(data_accessor.GetRuntimeParamAnnotationId().has_value());

        EXPECT_FALSE(data_accessor.GetParamAnnotationId().has_value());

        data_accessor.EnumerateRuntimeAnnotations([](File::EntityId) { EXPECT_TRUE(false); });

        data_accessor.EnumerateAnnotations([](File::EntityId) { EXPECT_TRUE(false); });
    });
}

void TestProtos(size_t n)
{
    constexpr size_t ELEM_WIDTH = 4;
    constexpr size_t ELEM_PER16 = 16 / ELEM_WIDTH;

    // Write panda file to memory
    ItemContainer container;

    ClassItem *class_item = container.GetOrCreateClassItem("A");
    class_item->SetAccessFlags(ACC_PUBLIC);

    StringItem *method_name = container.GetOrCreateStringItem("foo");

    std::vector<Type::TypeId> types {Type::TypeId::VOID, Type::TypeId::I32};
    std::vector<ClassItem *> ref_types;

    PrimitiveTypeItem *ret_type = container.CreateItem<PrimitiveTypeItem>(Type::TypeId::VOID);
    std::vector<MethodParamItem> params;

    params.emplace_back(container.CreateItem<PrimitiveTypeItem>(Type::TypeId::I32));

    for (size_t i = 0; i < ELEM_PER16 * 2U - 2U; i++) {  // ret, arg1
        params.emplace_back(container.GetOrCreateClassItem("B"));
        types.push_back(Type::TypeId::REFERENCE);
        ref_types.push_back(container.GetOrCreateClassItem("B"));
        params.emplace_back(container.CreateItem<PrimitiveTypeItem>(Type::TypeId::F64));
        types.push_back(Type::TypeId::F64);
    }

    for (size_t i = 0; i < n; i++) {
        params.emplace_back(container.CreateItem<PrimitiveTypeItem>(Type::TypeId::F32));
        types.push_back(Type::TypeId::F32);
    }

    ProtoItem *proto_item = container.GetOrCreateProtoItem(ret_type, params);

    MethodItem *method_item = class_item->AddMethod(method_name, proto_item, ACC_PUBLIC | ACC_STATIC, params);

    MemoryWriter mem_writer;

    ASSERT_TRUE(container.Write(&mem_writer));

    // Read panda file from memory
    auto data = mem_writer.GetData();
    auto panda_file = GetPandaFile(data);

    ASSERT_NE(panda_file, nullptr);

    ClassDataAccessor class_data_accessor(*panda_file, File::EntityId(class_item->GetOffset()));

    class_data_accessor.EnumerateMethods([&](MethodDataAccessor &data_accessor) {
        EXPECT_EQ(data_accessor.GetMethodId().GetOffset(), method_item->GetOffset());
        EXPECT_EQ(data_accessor.GetProtoId().GetOffset(), proto_item->GetOffset());

        ProtoDataAccessor proto_data_accessor(*panda_file, data_accessor.GetProtoId());
        EXPECT_EQ(proto_data_accessor.GetProtoId().GetOffset(), proto_item->GetOffset());

        size_t num = 0;
        size_t nref = 0;
        proto_data_accessor.EnumerateTypes([&](Type t) {
            EXPECT_EQ(t.GetEncoding(), Type(types[num]).GetEncoding());
            ++num;

            if (!t.IsPrimitive()) {
                ++nref;
            }
        });

        EXPECT_EQ(num, types.size());

        for (size_t i = 0; i < num - 1; i++) {
            EXPECT_EQ(proto_data_accessor.GetArgType(i).GetEncoding(), Type(types[i + 1]).GetEncoding());
        }

        EXPECT_EQ(proto_data_accessor.GetReturnType().GetEncoding(), Type(types[0]).GetEncoding());

        EXPECT_EQ(nref, ref_types.size());

        for (size_t i = 0; i < nref; i++) {
            EXPECT_EQ(proto_data_accessor.GetReferenceType(0).GetOffset(), ref_types[i]->GetOffset());
        }

        size_t size = ((num + ELEM_PER16) / ELEM_PER16 + nref) * sizeof(uint16_t);

        EXPECT_EQ(proto_data_accessor.GetSize(), size);
        EXPECT_EQ(proto_data_accessor.GetSize(), proto_item->GetSize());
    });
}

TEST(ItemContainer, TestProtos)
{
    TestProtos(0);
    TestProtos(1);
    TestProtos(2);
    TestProtos(7);
}

TEST(ItemContainer, TestDebugInfo)
{
    // Write panda file to memory
    ItemContainer container;

    ClassItem *class_item = container.GetOrCreateClassItem("A");
    class_item->SetAccessFlags(ACC_PUBLIC);

    StringItem *method_name = container.GetOrCreateStringItem("foo");

    PrimitiveTypeItem *ret_type = container.CreateItem<PrimitiveTypeItem>(Type::TypeId::VOID);
    std::vector<MethodParamItem> params;
    params.emplace_back(container.CreateItem<PrimitiveTypeItem>(Type::TypeId::I32));
    ProtoItem *proto_item = container.GetOrCreateProtoItem(ret_type, params);
    MethodItem *method_item = class_item->AddMethod(method_name, proto_item, ACC_PUBLIC | ACC_STATIC, params);

    StringItem *source_file_item = container.GetOrCreateStringItem("<source>");
    StringItem *source_code_item = container.GetOrCreateStringItem("let a = 1;");
    StringItem *param_string_item = container.GetOrCreateStringItem("a0");

    LineNumberProgramItem *line_number_program_item = container.CreateLineNumberProgramItem();
    DebugInfoItem *debug_info_item = container.CreateItem<DebugInfoItem>(line_number_program_item);
    method_item->SetDebugInfo(debug_info_item);

    // Add debug info
    container.ComputeLayout();

    std::vector<uint8_t> opcodes {
        static_cast<uint8_t>(LineNumberProgramItem::Opcode::SET_SOURCE_CODE),
        static_cast<uint8_t>(LineNumberProgramItem::Opcode::SET_FILE),
        static_cast<uint8_t>(LineNumberProgramItem::Opcode::SET_PROLOGUE_END),
        static_cast<uint8_t>(LineNumberProgramItem::Opcode::ADVANCE_PC),
        static_cast<uint8_t>(LineNumberProgramItem::Opcode::ADVANCE_LINE),
        static_cast<uint8_t>(LineNumberProgramItem::Opcode::SET_EPILOGUE_BEGIN),
        static_cast<uint8_t>(LineNumberProgramItem::Opcode::END_SEQUENCE),
    };

    auto *constant_pool = debug_info_item->GetConstantPool();
    debug_info_item->SetLineNumber(5);
    line_number_program_item->EmitSetSourceCode(constant_pool, source_code_item);
    line_number_program_item->EmitSetFile(constant_pool, source_file_item);
    line_number_program_item->EmitPrologEnd();
    line_number_program_item->EmitAdvancePc(constant_pool, 10);
    line_number_program_item->EmitAdvanceLine(constant_pool, -5);
    line_number_program_item->EmitEpilogBegin();
    line_number_program_item->EmitEnd();

    debug_info_item->AddParameter(param_string_item);

    method_item->SetDebugInfo(debug_info_item);

    MemoryWriter mem_writer;

    ASSERT_TRUE(container.Write(&mem_writer));

    // Read panda file from memory
    auto data = mem_writer.GetData();
    auto panda_file = GetPandaFile(data);

    ASSERT_NE(panda_file, nullptr);

    ClassDataAccessor class_data_accessor(*panda_file, File::EntityId(class_item->GetOffset()));

    class_data_accessor.EnumerateMethods([&](MethodDataAccessor &data_accessor) {
        EXPECT_EQ(data_accessor.GetMethodId().GetOffset(), method_item->GetOffset());
        EXPECT_EQ(data_accessor.GetSize(), method_item->GetSize());

        auto debug_info_id = data_accessor.GetDebugInfoId();
        EXPECT_TRUE(debug_info_id.has_value());

        EXPECT_EQ(debug_info_id.value().GetOffset(), debug_info_item->GetOffset());

        DebugInfoDataAccessor dda(*panda_file, debug_info_id.value());
        EXPECT_EQ(dda.GetDebugInfoId().GetOffset(), debug_info_item->GetOffset());
        EXPECT_EQ(dda.GetLineStart(), 5U);
        EXPECT_EQ(dda.GetNumParams(), params.size());

        dda.EnumerateParameters([&](File::EntityId id) { EXPECT_EQ(id.GetOffset(), param_string_item->GetOffset()); });

        auto cp = dda.GetConstantPool();
        EXPECT_EQ(cp.size(), constant_pool->size());
        EXPECT_THAT(*constant_pool, ::testing::ElementsAreArray(cp.data(), cp.Size()));

        EXPECT_EQ(helpers::ReadULeb128(&cp), source_code_item->GetOffset());
        EXPECT_EQ(helpers::ReadULeb128(&cp), source_file_item->GetOffset());
        EXPECT_EQ(helpers::ReadULeb128(&cp), 10U);
        EXPECT_EQ(helpers::ReadLeb128(&cp), -5);

        const uint8_t *line_number_program = dda.GetLineNumberProgram();
        EXPECT_EQ(panda_file->GetIdFromPointer(line_number_program).GetOffset(), line_number_program_item->GetOffset());
        EXPECT_EQ(line_number_program_item->GetSize(), opcodes.size());

        EXPECT_THAT(opcodes, ::testing::ElementsAreArray(line_number_program, opcodes.size()));

        EXPECT_EQ(dda.GetSize(), debug_info_item->GetSize());
    });
}

TEST(ItemContainer, ForeignItems)
{
    ItemContainer container;

    // Create foreign class
    ForeignClassItem *class_item = container.GetOrCreateForeignClassItem("ForeignClass");

    // Create foreign field
    StringItem *field_name = container.GetOrCreateStringItem("foreign_field");
    PrimitiveTypeItem *field_type = container.CreateItem<PrimitiveTypeItem>(Type::TypeId::I32);
    ForeignFieldItem *field_item = container.CreateItem<ForeignFieldItem>(class_item, field_name, field_type);

    // Create foreign method
    StringItem *method_name = container.GetOrCreateStringItem("ForeignMethod");
    PrimitiveTypeItem *ret_type = container.CreateItem<PrimitiveTypeItem>(Type::TypeId::VOID);
    std::vector<MethodParamItem> params;
    params.emplace_back(container.CreateItem<PrimitiveTypeItem>(Type::TypeId::I32));
    ProtoItem *proto_item = container.GetOrCreateProtoItem(ret_type, params);
    ForeignMethodItem *method_item = container.CreateItem<ForeignMethodItem>(class_item, method_name, proto_item, 0);

    MemoryWriter mem_writer;

    ASSERT_TRUE(container.Write(&mem_writer));

    // Read panda file from memory
    auto data = mem_writer.GetData();
    auto panda_file = GetPandaFile(data);

    ASSERT_NE(panda_file, nullptr);

    EXPECT_EQ(panda_file->GetHeader()->foreign_off, class_item->GetOffset());

    size_t foreign_size = class_item->GetSize() + field_item->GetSize() + method_item->GetSize();
    EXPECT_EQ(panda_file->GetHeader()->foreign_size, foreign_size);

    ASSERT_TRUE(panda_file->IsExternal(class_item->GetFileId()));

    MethodDataAccessor method_data_accessor(*panda_file, method_item->GetFileId());
    EXPECT_EQ(method_data_accessor.GetMethodId().GetOffset(), method_item->GetOffset());
    EXPECT_EQ(method_data_accessor.GetSize(), method_item->GetSize());
    EXPECT_EQ(method_data_accessor.GetClassId().GetOffset(), class_item->GetOffset());
    EXPECT_EQ(method_data_accessor.GetNameId().GetOffset(), method_name->GetOffset());
    EXPECT_EQ(method_data_accessor.GetProtoId().GetOffset(), proto_item->GetOffset());
    EXPECT_TRUE(method_data_accessor.IsExternal());

    FieldDataAccessor field_data_accessor(*panda_file, field_item->GetFileId());
    EXPECT_EQ(field_data_accessor.GetFieldId().GetOffset(), field_item->GetOffset());
    EXPECT_EQ(field_data_accessor.GetSize(), field_item->GetSize());
    EXPECT_EQ(field_data_accessor.GetClassId().GetOffset(), class_item->GetOffset());
    EXPECT_EQ(field_data_accessor.GetNameId().GetOffset(), field_name->GetOffset());
    EXPECT_EQ(field_data_accessor.GetType(), field_type->GetType().GetFieldEncoding());
    EXPECT_TRUE(field_data_accessor.IsExternal());
}

TEST(ItemContainer, EmptyContainerChecksum)
{
    using panda::os::file::Mode;
    using panda::os::file::Open;

    // Write panda file to disk
    ItemContainer container;

    const std::string file_name = "test_empty_checksum.ark";
    auto writer = FileWriter(file_name);

    // Initial value of adler32
    EXPECT_EQ(writer.GetChecksum(), 1);
    ASSERT_TRUE(container.Write(&writer));

    // At least header was written so the checksum should be changed
    auto container_checksum = writer.GetChecksum();
    EXPECT_NE(container_checksum, 1);

    // Read panda file from disk
    auto file = File::Open(file_name);
    EXPECT_NE(file, nullptr);
    EXPECT_EQ(file->GetHeader()->checksum, container_checksum);
}

TEST(ItemContainer, ContainerChecksum)
{
    using panda::os::file::Mode;
    using panda::os::file::Open;

    uint32_t empty_checksum = 0;
    {
        ItemContainer container;
        const std::string file_name = "test_checksum_empty.ark";
        auto writer = FileWriter(file_name);
        ASSERT_TRUE(container.Write(&writer));
        empty_checksum = writer.GetChecksum();
    }
    ASSERT(empty_checksum != 0);

    // Create not empty container
    ItemContainer container;
    container.GetOrCreateClassItem("C");

    const std::string file_name = "test_checksum.ark";
    auto writer = FileWriter(file_name);

    ASSERT_TRUE(container.Write(&writer));

    // This checksum must be different from the empty one (collision may happen though)
    auto container_checksum = writer.GetChecksum();
    EXPECT_NE(empty_checksum, container_checksum);

    // Read panda file from disk
    auto file = File::Open(file_name);
    EXPECT_NE(file, nullptr);
    EXPECT_EQ(file->GetHeader()->checksum, container_checksum);
}

}  // namespace panda::panda_file::test
