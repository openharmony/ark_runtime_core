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

#include <iomanip>
#include <tuple>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "annotation_data_accessor.h"
#include "assembly-emitter.h"
#include "assembly-parser.h"
#include "class_data_accessor-inl.h"
#include "code_data_accessor-inl.h"
#include "debug_data_accessor-inl.h"
#include "field_data_accessor-inl.h"
#include "file_items.h"
#include "lexer.h"
#include "method_data_accessor-inl.h"
#include "param_annotations_data_accessor.h"
#include "proto_data_accessor-inl.h"
#include "utils/span.h"
#include "utils/leb128.h"
#include "utils/utf.h"

namespace panda::test {

using namespace panda::pandasm;

static const uint8_t *GetTypeDescriptor(const std::string &name, std::string *storage)
{
    *storage = "L" + name + ";";
    std::replace(storage->begin(), storage->end(), '.', '/');
    return utf::CStringAsMutf8(storage->c_str());
}

TEST(emittertests, test)
{
    Parser p;

    auto source = R"(            # 1
        .record R {              # 2
            i32 sf <static>      # 3
            i8  if               # 4
        }                        # 5
                                 # 6
        .function void main() {  # 7
            return.void          # 8
        }                        # 9
    )";

    std::string source_filename = "source.pa";
    auto res = p.Parse(source, source_filename);
    ASSERT_EQ(p.ShowError().err, Error::ErrorType::ERR_NONE);

    auto pf = AsmEmitter::Emit(res.Value());
    ASSERT_NE(pf, nullptr);

    // Check _GLOBAL class
    {
        std::string descriptor;
        auto class_id = pf->GetClassId(GetTypeDescriptor("_GLOBAL", &descriptor));
        ASSERT_TRUE(class_id.IsValid());
        ASSERT_FALSE(pf->IsExternal(class_id));

        panda_file::ClassDataAccessor cda(*pf, class_id);
        ASSERT_EQ(cda.GetSuperClassId().GetOffset(), 0U);
        ASSERT_EQ(cda.GetAccessFlags(), ACC_PUBLIC);
        ASSERT_EQ(cda.GetFieldsNumber(), 0U);
        ASSERT_EQ(cda.GetMethodsNumber(), 1U);
        ASSERT_EQ(cda.GetIfacesNumber(), 0U);

        ASSERT_FALSE(cda.GetSourceFileId().has_value());

        cda.EnumerateRuntimeAnnotations([](panda_file::File::EntityId) { ASSERT_TRUE(false); });

        cda.EnumerateAnnotations([](panda_file::File::EntityId) { ASSERT_TRUE(false); });

        cda.EnumerateFields([](panda_file::FieldDataAccessor &) { ASSERT_TRUE(false); });

        cda.EnumerateMethods([&](panda_file::MethodDataAccessor &mda) {
            ASSERT_FALSE(mda.IsExternal());
            ASSERT_EQ(mda.GetClassId(), class_id);
            ASSERT_EQ(utf::CompareMUtf8ToMUtf8(pf->GetStringData(mda.GetNameId()).data, utf::CStringAsMutf8("main")),
                      0);

            panda_file::ProtoDataAccessor pda(*pf, mda.GetProtoId());
            ASSERT_EQ(pda.GetNumArgs(), 0U);
            ASSERT_EQ(pda.GetReturnType().GetId(), panda_file::Type::TypeId::VOID);

            ASSERT_EQ(mda.GetAccessFlags(), ACC_STATIC);
            ASSERT_TRUE(mda.GetCodeId().has_value());

            panda_file::CodeDataAccessor cdacc(*pf, mda.GetCodeId().value());
            ASSERT_EQ(cdacc.GetNumVregs(), 0U);
            ASSERT_EQ(cdacc.GetNumArgs(), 0U);
            ASSERT_EQ(cdacc.GetCodeSize(), 1U);
            ASSERT_EQ(cdacc.GetTriesSize(), 0U);

            ASSERT_FALSE(mda.GetRuntimeParamAnnotationId().has_value());
            ASSERT_FALSE(mda.GetParamAnnotationId().has_value());
            ASSERT_TRUE(mda.GetDebugInfoId().has_value());

            panda_file::DebugInfoDataAccessor dda(*pf, mda.GetDebugInfoId().value());
            ASSERT_EQ(dda.GetLineStart(), 8U);
            ASSERT_EQ(dda.GetNumParams(), 0U);

            mda.EnumerateRuntimeAnnotations([](panda_file::File::EntityId) { ASSERT_TRUE(false); });
            mda.EnumerateAnnotations([](panda_file::File::EntityId) { ASSERT_TRUE(false); });
        });
    }

    // Check R class
    {
        std::string descriptor;
        auto class_id = pf->GetClassId(GetTypeDescriptor("R", &descriptor));
        ASSERT_TRUE(class_id.IsValid());
        ASSERT_FALSE(pf->IsExternal(class_id));

        panda_file::ClassDataAccessor cda(*pf, class_id);
        ASSERT_EQ(cda.GetSuperClassId().GetOffset(), 0U);
        ASSERT_EQ(cda.GetAccessFlags(), 0);
        ASSERT_EQ(cda.GetFieldsNumber(), 2U);
        ASSERT_EQ(cda.GetMethodsNumber(), 0U);
        ASSERT_EQ(cda.GetIfacesNumber(), 0U);

        // We emit SET_FILE in debuginfo
        ASSERT_TRUE(cda.GetSourceFileId().has_value());

        EXPECT_EQ(std::string(reinterpret_cast<const char *>(pf->GetStringData(cda.GetSourceFileId().value()).data)),
                  source_filename);

        cda.EnumerateRuntimeAnnotations([](panda_file::File::EntityId) { ASSERT_TRUE(false); });

        cda.EnumerateAnnotations([](panda_file::File::EntityId) { ASSERT_TRUE(false); });

        struct FieldData {
            std::string name;
            panda_file::Type::TypeId type_id;
            uint32_t access_flags;
        };

        std::vector<FieldData> fields {{"sf", panda_file::Type::TypeId::I32, ACC_STATIC},
                                       {"if", panda_file::Type::TypeId::I8, 0}};

        size_t i = 0;
        cda.EnumerateFields([&](panda_file::FieldDataAccessor &fda) {
            ASSERT_FALSE(fda.IsExternal());
            ASSERT_EQ(fda.GetClassId(), class_id);
            ASSERT_EQ(utf::CompareMUtf8ToMUtf8(pf->GetStringData(fda.GetNameId()).data,
                                               utf::CStringAsMutf8(fields[i].name.c_str())),
                      0);

            ASSERT_EQ(fda.GetType(), panda_file::Type(fields[i].type_id).GetFieldEncoding());
            ASSERT_EQ(fda.GetAccessFlags(), fields[i].access_flags);

            fda.EnumerateRuntimeAnnotations([](panda_file::File::EntityId) { ASSERT_TRUE(false); });
            fda.EnumerateAnnotations([](panda_file::File::EntityId) { ASSERT_TRUE(false); });

            ++i;
        });

        cda.EnumerateMethods([&](panda_file::MethodDataAccessor &) { ASSERT_TRUE(false); });
    }
}

uint8_t GetSpecialOpcode(uint32_t pc_inc, int32_t line_inc)
{
    return (line_inc - panda_file::LineNumberProgramItem::LINE_BASE) +
           (pc_inc * panda_file::LineNumberProgramItem::LINE_RANGE) + panda_file::LineNumberProgramItem::OPCODE_BASE;
}

TEST(emittertests, debuginfo)
{
    Parser p;

    auto source = R"(
        .function void main() {
            ldai.64 0   # line 3, pc 0
                        # line 4
                        # line 5
                        # line 6
                        # line 7
                        # line 8
                        # line 9
                        # line 10
                        # line 11
                        # line 12
                        # line 13
                        # line 14
            ldai.64 1   # line 15, pc 9
            return.void # line 16, pc 18
        }
    )";

    std::string source_filename = "source.pa";
    auto res = p.Parse(source, source_filename);
    ASSERT_EQ(p.ShowError().err, Error::ErrorType::ERR_NONE);

    auto pf = AsmEmitter::Emit(res.Value());
    ASSERT_NE(pf, nullptr);

    std::string descriptor;
    auto class_id = pf->GetClassId(GetTypeDescriptor("_GLOBAL", &descriptor));
    ASSERT_TRUE(class_id.IsValid());

    panda_file::ClassDataAccessor cda(*pf, class_id);

    cda.EnumerateMethods([&](panda_file::MethodDataAccessor &mda) {
        panda_file::CodeDataAccessor cdacc(*pf, mda.GetCodeId().value());
        ASSERT_TRUE(mda.GetDebugInfoId().has_value());

        panda_file::DebugInfoDataAccessor dda(*pf, mda.GetDebugInfoId().value());
        ASSERT_EQ(dda.GetLineStart(), 3U);
        ASSERT_EQ(dda.GetNumParams(), 0U);

        const uint8_t *program = dda.GetLineNumberProgram();
        Span<const uint8_t> constant_pool = dda.GetConstantPool();

        std::vector<uint8_t> opcodes {static_cast<uint8_t>(panda_file::LineNumberProgramItem::Opcode::SET_FILE),
                                      static_cast<uint8_t>(panda_file::LineNumberProgramItem::Opcode::ADVANCE_PC),
                                      static_cast<uint8_t>(panda_file::LineNumberProgramItem::Opcode::ADVANCE_LINE),
                                      GetSpecialOpcode(0, 0),
                                      GetSpecialOpcode(9, 1),
                                      static_cast<uint8_t>(panda_file::LineNumberProgramItem::Opcode::END_SEQUENCE)};

        EXPECT_THAT(opcodes, ::testing::ElementsAreArray(program, opcodes.size()));

        size_t size {};
        bool is_full {};
        size_t constant_pool_offset = 0;

        uint32_t offset {};

        std::tie(offset, size, is_full) = leb128::DecodeUnsigned<uint32_t>(&constant_pool[constant_pool_offset]);
        constant_pool_offset += size;
        ASSERT_TRUE(is_full);
        EXPECT_EQ(
            std::string(reinterpret_cast<const char *>(pf->GetStringData(panda_file::File::EntityId(offset)).data)),
            source_filename);

        uint32_t pc_inc;
        std::tie(pc_inc, size, is_full) = leb128::DecodeUnsigned<uint32_t>(&constant_pool[constant_pool_offset]);
        constant_pool_offset += size;
        ASSERT_TRUE(is_full);
        EXPECT_EQ(pc_inc, 9U);

        int32_t line_inc;
        std::tie(line_inc, size, is_full) = leb128::DecodeSigned<int32_t>(&constant_pool[constant_pool_offset]);
        constant_pool_offset += size;
        ASSERT_TRUE(is_full);
        EXPECT_EQ(line_inc, 12);

        EXPECT_EQ(constant_pool_offset, constant_pool.size());
    });
}

TEST(emittertests, exceptions)
{
    Parser p;

    auto source = R"(
        .record Exception1 {}
        .record Exception2 {}

        .function void main() {
            ldai.64 0
        try_begin:
            ldai.64 1
            ldai.64 2
        try_end:
            ldai.64 3
        catch_begin1:
            ldai.64 4
        catch_begin2:
            ldai.64 5
        catchall_begin:
            ldai.64 6

        .catch Exception1, try_begin, try_end, catch_begin1
        .catch Exception2, try_begin, try_end, catch_begin2
        .catchall try_begin, try_end, catchall_begin
        }
    )";

    auto res = p.Parse(source);
    ASSERT_EQ(p.ShowError().err, Error::ErrorType::ERR_NONE);

    auto pf = AsmEmitter::Emit(res.Value());
    ASSERT_NE(pf, nullptr);

    std::string descriptor;

    auto class_id = pf->GetClassId(GetTypeDescriptor("_GLOBAL", &descriptor));
    ASSERT_TRUE(class_id.IsValid());

    panda_file::ClassDataAccessor cda(*pf, class_id);

    cda.EnumerateMethods([&](panda_file::MethodDataAccessor &mda) {
        panda_file::CodeDataAccessor cdacc(*pf, mda.GetCodeId().value());
        ASSERT_EQ(cdacc.GetNumVregs(), 0U);
        ASSERT_EQ(cdacc.GetNumArgs(), 0U);
        ASSERT_EQ(cdacc.GetTriesSize(), 1);

        cdacc.EnumerateTryBlocks([&](panda_file::CodeDataAccessor::TryBlock &try_block) {
            EXPECT_EQ(try_block.GetStartPc(), 9);
            EXPECT_EQ(try_block.GetLength(), 18);
            EXPECT_EQ(try_block.GetNumCatches(), 3);

            struct CatchInfo {
                panda_file::File::EntityId type_id;
                uint32_t handler_pc;
            };

            std::vector<CatchInfo> catch_infos {{pf->GetClassId(GetTypeDescriptor("Exception1", &descriptor)), 4 * 9},
                                                {pf->GetClassId(GetTypeDescriptor("Exception2", &descriptor)), 5 * 9},
                                                {panda_file::File::EntityId(), 6 * 9}};

            size_t i = 0;
            try_block.EnumerateCatchBlocks([&](panda_file::CodeDataAccessor::CatchBlock &catch_block) {
                auto idx = catch_block.GetTypeIdx();
                auto id = idx != panda_file::INVALID_INDEX ? pf->ResolveClassIndex(mda.GetMethodId(), idx)
                                                           : panda_file::File::EntityId();
                EXPECT_EQ(id, catch_infos[i].type_id);
                EXPECT_EQ(catch_block.GetHandlerPc(), catch_infos[i].handler_pc);
                ++i;

                return true;
            });

            return true;
        });
    });
}

TEST(emittertests, errors)
{
    {
        Parser p;
        auto source = R"(
            .record A {
                B b
            }
        )";

        auto res = p.Parse(source);
        ASSERT_EQ(p.ShowError().err, Error::ErrorType::ERR_NONE);

        auto pf = AsmEmitter::Emit(res.Value());
        ASSERT_EQ(pf, nullptr);
        ASSERT_EQ(AsmEmitter::GetLastError(), "Field A.b has undefined type");
    }

    {
        Parser p;
        auto source = R"(
            .function void A.b() {}
        )";

        auto res = p.Parse(source);
        ASSERT_EQ(p.ShowError().err, Error::ErrorType::ERR_NONE);

        auto pf = AsmEmitter::Emit(res.Value());
        ASSERT_EQ(pf, nullptr);
        ASSERT_EQ(AsmEmitter::GetLastError(), "Function A.b is bound to undefined record A");
    }

    {
        Parser p;
        auto source = R"(
            .function A b() {}
        )";

        auto res = p.Parse(source);
        ASSERT_EQ(p.ShowError().err, Error::ErrorType::ERR_NONE);

        auto pf = AsmEmitter::Emit(res.Value());
        ASSERT_EQ(pf, nullptr);
        ASSERT_EQ(AsmEmitter::GetLastError(), "Function b has undefined return type");
    }

    {
        Parser p;
        auto source = R"(
            .function void a(b a0) {}
        )";

        auto res = p.Parse(source);
        ASSERT_EQ(p.ShowError().err, Error::ErrorType::ERR_NONE);

        auto pf = AsmEmitter::Emit(res.Value());
        ASSERT_EQ(pf, nullptr);
        ASSERT_EQ(AsmEmitter::GetLastError(), "Argument 0 of function a has undefined type");
    }

    {
        Parser p;
        auto source = R"(
            .record A <external>
            .function void A.x() {}
        )";

        auto res = p.Parse(source);
        ASSERT_EQ(p.ShowError().err, Error::ErrorType::ERR_NONE);

        auto pf = AsmEmitter::Emit(res.Value());
        ASSERT_EQ(pf, nullptr);
        ASSERT_EQ(AsmEmitter::GetLastError(), "Non-external function A.x is bound to external record");
    }

    {
        Ins i;
        Function f("test_fuzz_imms", pandasm::extensions::Language::ECMASCRIPT);
        Program prog;
        i.opcode = Opcode::LDAI_64;
        i.imms.clear();
        f.ins.push_back(i);
        prog.function_table.emplace("test_fuzz_imms", std::move(f));

        auto pf = AsmEmitter::Emit(prog);
        ASSERT_EQ(pf, nullptr);
        ASSERT_EQ(AsmEmitter::GetLastError(), "Internal error during emitting function: test_fuzz_imms");
    }

    {
        Ins i;
        Function f("test_fuzz_regs", pandasm::extensions::Language::ECMASCRIPT);
        Program prog;
        i.opcode = Opcode::LDA;
        i.regs.clear();
        f.ins.push_back(i);
        prog.function_table.emplace("test_fuzz_regs", std::move(f));

        auto pf = AsmEmitter::Emit(prog);
        ASSERT_EQ(pf, nullptr);
        ASSERT_EQ(AsmEmitter::GetLastError(), "Internal error during emitting function: test_fuzz_regs");
    }

    {
        Ins i;
        Function f("test_fuzz_ids", pandasm::extensions::Language::ECMASCRIPT);
        Program prog;
        i.opcode = Opcode::LDA_STR;
        i.ids.push_back("testFuzz");
        f.ins.push_back(i);
        prog.function_table.emplace("test_fuzz_ids", std::move(f));
        prog.strings.insert("testFuz_");

        auto pf = AsmEmitter::Emit(prog);
        ASSERT_EQ(pf, nullptr);
        ASSERT_EQ(AsmEmitter::GetLastError(), "Internal error during emitting function: test_fuzz_ids");
    }
}

enum class ItemType { RECORD, FIELD, FUNCTION, PARAMETER };

std::string ItemTypeToString(ItemType item_type)
{
    switch (item_type) {
        case ItemType::RECORD:
            return "record";
        case ItemType::FIELD:
            return "field";
        case ItemType::FUNCTION:
            return "function";
        case ItemType::PARAMETER:
            return "parameter";
        default:
            break;
    }

    UNREACHABLE();
    return "";
}

template <Value::Type type>
auto GetAnnotationElementValue(size_t idx)
{
    using T = ValueTypeHelperT<type>;

    if constexpr (std::is_arithmetic_v<T>) {
        if constexpr (type == Value::Type::U1) {
            return static_cast<uint32_t>(idx == 0 ? 0 : 1);
        }

        auto res = idx == 0 ? std::numeric_limits<T>::min() : std::numeric_limits<T>::max();

        if constexpr (type == Value::Type::I8) {
            return static_cast<int32_t>(res);
        }
        if constexpr (type == Value::Type::U8) {
            return static_cast<uint32_t>(res);
        }
        if constexpr (std::is_floating_point_v<T>) {
            return res * (idx == 0 ? 10 : 0.1);
        }

        return res;
    } else {
        switch (type) {
            case Value::Type::STRING:
                return idx == 0 ? "123" : "456";
            case Value::Type::RECORD:
                return idx == 0 ? "A" : "B";
            case Value::Type::ENUM:
                return idx == 0 ? "E.E1" : "E.E2";
            case Value::Type::ANNOTATION:
                return idx == 0 ? "id_A" : "id_B";
            default:
                break;
        }

        UNREACHABLE();
        return "";
    }
}

TEST(emittertests, language)
{
    {
        Parser p;
        auto source = R"(
            .function void foo() {}
        )";

        auto res = p.Parse(source);
        ASSERT_EQ(p.ShowError().err, Error::ErrorType::ERR_NONE);

        auto pf = AsmEmitter::Emit(res.Value());
        ASSERT_NE(pf, nullptr);

        std::string descriptor;

        auto class_id = pf->GetClassId(GetTypeDescriptor("_GLOBAL", &descriptor));
        ASSERT_TRUE(class_id.IsValid());

        panda_file::ClassDataAccessor cda(*pf, class_id);

        ASSERT_FALSE(cda.GetSourceLang());

        cda.EnumerateMethods([&](panda_file::MethodDataAccessor &mda) { ASSERT_FALSE(mda.GetSourceLang()); });
    }
}

TEST(emittertests, constructors)
{
    {
        Parser p;
        auto source = R"(
            .record R {}
            .function void R.foo(R a0) <ctor> {}
        )";

        auto res = p.Parse(source);
        ASSERT_EQ(p.ShowError().err, Error::ErrorType::ERR_NONE);

        auto pf = AsmEmitter::Emit(res.Value());
        ASSERT_NE(pf, nullptr);

        std::string descriptor;

        auto class_id = pf->GetClassId(GetTypeDescriptor("R", &descriptor));
        ASSERT_TRUE(class_id.IsValid());

        panda_file::ClassDataAccessor cda(*pf, class_id);

        cda.EnumerateMethods([&](panda_file::MethodDataAccessor &mda) {
            auto *name = utf::Mutf8AsCString(pf->GetStringData(mda.GetNameId()).data);
            ASSERT_STREQ(name, ".ctor");
        });
    }

    {
        Parser p;
        auto source = R"(
            .record R {}
            .function void R.foo(R a0) <cctor> {}
        )";

        auto res = p.Parse(source);
        ASSERT_EQ(p.ShowError().err, Error::ErrorType::ERR_NONE);

        auto pf = AsmEmitter::Emit(res.Value());
        ASSERT_NE(pf, nullptr);

        std::string descriptor;

        auto class_id = pf->GetClassId(GetTypeDescriptor("R", &descriptor));
        ASSERT_TRUE(class_id.IsValid());

        panda_file::ClassDataAccessor cda(*pf, class_id);

        cda.EnumerateMethods([&](panda_file::MethodDataAccessor &mda) {
            auto *name = utf::Mutf8AsCString(pf->GetStringData(mda.GetNameId()).data);
            ASSERT_STREQ(name, ".cctor");
        });
    }
}

TEST(emittertests, field_value)
{
    Parser p;

    auto source = R"(
        .record panda.String <external>

        .record R {
            u1 f_u1 <value=1>
            i8 f_i8 <value=2>
            u8 f_u8 <value=128>
            i16 f_i16 <value=256>
            u16 f_u16 <value=32768>
            i32 f_i32 <value=65536>
            u32 f_u32 <value=2147483648>
            i64 f_i64 <value=4294967296>
            u64 f_u64 <value=9223372036854775808>
            f32 f_f32 <value=1.0>
            f64 f_f64 <value=2.0>
            panda.String f_str <value="str">
        }
    )";

    struct FieldData {
        std::string name;
        panda_file::Type::TypeId type_id;
        std::variant<int32_t, uint32_t, int64_t, uint64_t, float, double, std::string> value;
    };

    std::vector<FieldData> data {
        {"f_u1", panda_file::Type::TypeId::U1, static_cast<uint32_t>(1)},
        {"f_i8", panda_file::Type::TypeId::I8, static_cast<int32_t>(2)},
        {"f_u8", panda_file::Type::TypeId::U8, static_cast<uint32_t>(128)},
        {"f_i16", panda_file::Type::TypeId::I16, static_cast<int32_t>(256)},
        {"f_u16", panda_file::Type::TypeId::U16, static_cast<uint32_t>(32768)},
        {"f_i32", panda_file::Type::TypeId::I32, static_cast<int32_t>(65536)},
        {"f_u32", panda_file::Type::TypeId::U32, static_cast<uint32_t>(2147483648)},
        {"f_i64", panda_file::Type::TypeId::I64, static_cast<int64_t>(4294967296)},
        {"f_u64", panda_file::Type::TypeId::U64, static_cast<uint64_t>(9223372036854775808ULL)},
        {"f_f32", panda_file::Type::TypeId::F32, static_cast<float>(1.0)},
        {"f_f64", panda_file::Type::TypeId::F64, static_cast<double>(2.0)},
        {"f_str", panda_file::Type::TypeId::REFERENCE, "str"}};

    auto res = p.Parse(source);
    ASSERT_EQ(p.ShowError().err, Error::ErrorType::ERR_NONE);

    auto pf = AsmEmitter::Emit(res.Value());
    ASSERT_NE(pf, nullptr);

    std::string descriptor;
    auto class_id = pf->GetClassId(GetTypeDescriptor("R", &descriptor));
    ASSERT_TRUE(class_id.IsValid());
    ASSERT_FALSE(pf->IsExternal(class_id));

    panda_file::ClassDataAccessor cda(*pf, class_id);
    ASSERT_EQ(cda.GetFieldsNumber(), data.size());

    auto panda_string_id = pf->GetClassId(GetTypeDescriptor("panda.String", &descriptor));

    size_t idx = 0;
    cda.EnumerateFields([&](panda_file::FieldDataAccessor &fda) {
        const FieldData &field_data = data[idx];

        ASSERT_EQ(utf::CompareMUtf8ToMUtf8(pf->GetStringData(fda.GetNameId()).data,
                                           utf::CStringAsMutf8(field_data.name.c_str())),
                  0);

        panda_file::Type type(field_data.type_id);
        uint32_t type_value;
        if (type.IsReference()) {
            type_value = panda_string_id.GetOffset();
        } else {
            type_value = type.GetFieldEncoding();
        }

        ASSERT_EQ(fda.GetType(), type_value);

        switch (field_data.type_id) {
            case panda_file::Type::TypeId::U1: {
                auto result = fda.GetValue<uint8_t>();
                ASSERT_TRUE(result);
                ASSERT_EQ(result.value(), std::get<uint32_t>(field_data.value));
                break;
            }
            case panda_file::Type::TypeId::I8: {
                auto result = fda.GetValue<int8_t>();
                ASSERT_TRUE(result);
                ASSERT_EQ(result.value(), std::get<int32_t>(field_data.value));
                break;
            }
            case panda_file::Type::TypeId::U8: {
                auto result = fda.GetValue<uint8_t>();
                ASSERT_TRUE(result);
                ASSERT_EQ(result.value(), std::get<uint32_t>(field_data.value));
                break;
            }
            case panda_file::Type::TypeId::I16: {
                auto result = fda.GetValue<int16_t>();
                ASSERT_TRUE(result);
                ASSERT_EQ(result.value(), std::get<int32_t>(field_data.value));
                break;
            }
            case panda_file::Type::TypeId::U16: {
                auto result = fda.GetValue<uint16_t>();
                ASSERT_TRUE(result);
                ASSERT_EQ(result.value(), std::get<uint32_t>(field_data.value));
                break;
            }
            case panda_file::Type::TypeId::I32: {
                auto result = fda.GetValue<int32_t>();
                ASSERT_TRUE(result);
                ASSERT_EQ(result.value(), std::get<int32_t>(field_data.value));
                break;
            }
            case panda_file::Type::TypeId::U32: {
                auto result = fda.GetValue<uint32_t>();
                ASSERT_TRUE(result);
                ASSERT_EQ(result.value(), std::get<uint32_t>(field_data.value));
                break;
            }
            case panda_file::Type::TypeId::I64: {
                auto result = fda.GetValue<int64_t>();
                ASSERT_TRUE(result);
                ASSERT_EQ(result.value(), std::get<int64_t>(field_data.value));
                break;
            }
            case panda_file::Type::TypeId::U64: {
                auto result = fda.GetValue<uint64_t>();
                ASSERT_TRUE(result);
                ASSERT_EQ(result.value(), std::get<uint64_t>(field_data.value));
                break;
            }
            case panda_file::Type::TypeId::F32: {
                auto result = fda.GetValue<float>();
                ASSERT_TRUE(result);
                ASSERT_EQ(result.value(), std::get<float>(field_data.value));
                break;
            }
            case panda_file::Type::TypeId::F64: {
                auto result = fda.GetValue<double>();
                ASSERT_TRUE(result);
                ASSERT_EQ(result.value(), std::get<double>(field_data.value));
                break;
            }
            case panda_file::Type::TypeId::REFERENCE: {
                auto result = fda.GetValue<uint32_t>();
                ASSERT_TRUE(result);

                panda_file::File::EntityId string_id(result.value());
                auto val = std::get<std::string>(field_data.value);

                ASSERT_EQ(utf::CompareMUtf8ToMUtf8(pf->GetStringData(string_id).data, utf::CStringAsMutf8(val.c_str())),
                          0);
                break;
            }
            default: {
                UNREACHABLE();
                break;
            }
        }

        ++idx;
    });
}

TEST(emittertests, tagged_in_func_decl)
{
    Parser p;
    auto source = R"(
        .function any foo(any a0) <noimpl>
    )";

    auto res = p.Parse(source);
    ASSERT_EQ(p.ShowError().err, Error::ErrorType::ERR_NONE);

    auto pf = AsmEmitter::Emit(res.Value());
    ASSERT_NE(pf, nullptr);

    std::string descriptor;

    auto class_id = pf->GetClassId(GetTypeDescriptor("_GLOBAL", &descriptor));
    ASSERT_TRUE(class_id.IsValid());

    panda_file::ClassDataAccessor cda(*pf, class_id);

    size_t num_methods = 0;
    const auto tagged = panda_file::Type(panda_file::Type::TypeId::TAGGED);
    cda.EnumerateMethods([&](panda_file::MethodDataAccessor &mda) {
        panda_file::ProtoDataAccessor pda(*pf, mda.GetProtoId());
        ASSERT_EQ(tagged, pda.GetReturnType());
        ASSERT_EQ(1, pda.GetNumArgs());
        ASSERT_EQ(tagged, pda.GetArgType(0));

        ++num_methods;
    });
    ASSERT_EQ(1, num_methods);
}

TEST(emittertests, tagged_in_field_decl)
{
    Parser p;
    auto source = R"(
        .record Test {
            any foo
        }
    )";

    auto res = p.Parse(source);
    ASSERT_EQ(p.ShowError().err, Error::ErrorType::ERR_NONE);

    auto pf = AsmEmitter::Emit(res.Value());
    ASSERT_NE(pf, nullptr);

    std::string descriptor;

    auto class_id = pf->GetClassId(GetTypeDescriptor("Test", &descriptor));
    ASSERT_TRUE(class_id.IsValid());

    panda_file::ClassDataAccessor cda(*pf, class_id);

    size_t num_fields = 0;
    const auto tagged = panda_file::Type(panda_file::Type::TypeId::TAGGED);
    cda.EnumerateFields([&](panda_file::FieldDataAccessor &fda) {
        uint32_t type = fda.GetType();
        ASSERT_EQ(tagged.GetFieldEncoding(), type);

        ++num_fields;
    });
    ASSERT_EQ(1, num_fields);
}

TEST(emittertests, get_GLOBAL_lang_for_JS_func)
{
    Parser p;
    auto source = R"(
        .language ECMAScript

        .function any main() {
            return.dyn
        }
    )";

    auto res = p.Parse(source);
    ASSERT_EQ(p.ShowError().err, Error::ErrorType::ERR_NONE);

    auto pf = AsmEmitter::Emit(res.Value());
    ASSERT_NE(pf, nullptr);

    std::string descriptor;

    auto class_id = pf->GetClassId(GetTypeDescriptor("_GLOBAL", &descriptor));
    ASSERT_TRUE(class_id.IsValid());

    panda_file::ClassDataAccessor cda(*pf, class_id);

    ASSERT_TRUE(cda.GetSourceLang().has_value());
    ASSERT_EQ(cda.GetSourceLang(), panda_file::SourceLang::ECMASCRIPT);
}

}  // namespace panda::test
