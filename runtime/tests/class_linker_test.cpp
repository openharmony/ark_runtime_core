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

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <ostream>
#include <unordered_set>
#include <vector>

#include "assembly-emitter.h"
#include "assembly-parser.h"
#include "libpandabase/utils/utf.h"
#include "libpandafile/modifiers.h"
#include "runtime/include/class-inl.h"
#include "runtime/include/class_linker-inl.h"
#include "runtime/include/class_linker.h"
#include "runtime/include/coretypes/tagged_value.h"
#include "runtime/include/object_header.h"
#include "runtime/include/runtime.h"
#include "runtime/core/core_class_linker_extension.h"
#include "runtime/tests/class_linker_test_extension.h"

namespace panda::test {

class ClassLinkerTest : public testing::Test {
public:
    ClassLinkerTest()
    {
        // Just for internal allocator
        RuntimeOptions options;
        options.SetShouldLoadBootPandaFiles(false);
        options.SetShouldInitializeIntrinsics(false);
        options.SetGcType("epsilon");
        options.SetHeapSizeLimit(64_MB);
        Runtime::Create(options);
        thread_ = panda::MTManagedThread::GetCurrent();
        thread_->ManagedCodeBegin();
    }

    ~ClassLinkerTest()
    {
        thread_->ManagedCodeEnd();
        Runtime::Destroy();
    }

protected:
    panda::MTManagedThread *thread_ {nullptr};
};

static std::unique_ptr<ClassLinker> CreateClassLinker(ManagedThread *thread)
{
    std::vector<std::unique_ptr<ClassLinkerExtension>> extensions;
    extensions.push_back(std::make_unique<CoreClassLinkerExtension>());

    auto allocator = thread->GetVM()->GetHeapManager()->GetInternalAllocator();
    auto class_linker = std::make_unique<ClassLinker>(allocator, std::move(extensions));
    if (!class_linker->Initialize()) {
        return nullptr;
    }

    return class_linker;
}

TEST_F(ClassLinkerTest, TestGetClass)
{
    pandasm::Parser p;

    auto source = R"(
        .function void main() {
            return.void
        }
    )";

    auto res = p.Parse(source);
    auto pf = pandasm::AsmEmitter::Emit(res.Value());

    auto class_linker = CreateClassLinker(thread_);
    ASSERT_NE(class_linker, nullptr);

    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto *ext = class_linker->GetExtension(ctx);

    auto *pf_ptr = pf.get();
    class_linker->AddPandaFile(std::move(pf));

    Class *klass = nullptr;

    {
        // Use temporary string to load class. Class loader shouldn't store it.
        auto descriptor = std::make_unique<PandaString>();
        klass = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("_GLOBAL"), descriptor.get()));
    }

    PandaString descriptor;

    EXPECT_EQ(klass, ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("_GLOBAL"), &descriptor)));
    EXPECT_EQ(klass->GetBase(), ext->GetClassRoot(ClassRoot::OBJECT));
    EXPECT_EQ(klass->GetPandaFile(), pf_ptr);
    EXPECT_EQ(klass->GetMethods().size(), 1U);
    EXPECT_EQ(klass->GetComponentSize(), 0U);
}

TEST_F(ClassLinkerTest, TestEnumerateClasses)
{
    pandasm::Parser p;

    auto source = R"(
        .function void main() {
            return.void
        }
    )";

    auto res = p.Parse(source);
    auto pf = pandasm::AsmEmitter::Emit(res.Value());

    auto class_linker = CreateClassLinker(thread_);
    ASSERT_NE(class_linker, nullptr);

    class_linker->AddPandaFile(std::move(pf));

    PandaString descriptor;

    // Load _GLOBAL class
    auto *ext = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
    ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("_GLOBAL"), &descriptor));

    std::set<std::string> classes {"panda.Object",
                                   "panda.String",
                                   "panda.Class",
                                   "[Lpanda/String;",
                                   "u1",
                                   "i8",
                                   "u8",
                                   "i16",
                                   "u16",
                                   "i32",
                                   "u32",
                                   "i64",
                                   "u64",
                                   "f32",
                                   "f64",
                                   "any",
                                   "[Z",
                                   "[B",
                                   "[H",
                                   "[S",
                                   "[C",
                                   "[I",
                                   "[U",
                                   "[J",
                                   "[Q",
                                   "[F",
                                   "[D",
                                   "[A",
                                   "[Lpanda/Class;",
                                   "_GLOBAL"};

    std::set<std::string> loaded_classes;

    class_linker->EnumerateClasses([&](Class *k) {
        loaded_classes.emplace(k->GetName());
        return true;
    });

    EXPECT_EQ(loaded_classes, classes);
}

static void TestPrimitiveClassRoot(const ClassLinkerExtension &class_linker_ext, ClassRoot class_root,
                                   panda_file::Type::TypeId type_id)
{
    std::string msg = "Test with class root ";
    msg += static_cast<int>(class_root);

    Class *klass = class_linker_ext.GetClassRoot(class_root);
    ASSERT_NE(klass, nullptr) << msg;
    EXPECT_EQ(klass->GetBase(), nullptr) << msg;
    EXPECT_EQ(klass->GetComponentSize(), 0U) << msg;
    EXPECT_EQ(klass->GetFlags(), 0U) << msg;
    EXPECT_EQ(klass->GetAccessFlags(), ACC_PUBLIC | ACC_FINAL | ACC_ABSTRACT) << msg;
    EXPECT_EQ(klass->GetType().GetId(), type_id) << msg;
    EXPECT_FALSE(klass->IsArrayClass()) << msg;
    EXPECT_FALSE(klass->IsStringClass()) << msg;
    EXPECT_TRUE(klass->IsPrimitive()) << msg;
    EXPECT_TRUE(klass->IsAbstract()) << msg;
    EXPECT_FALSE(klass->IsInstantiable()) << msg;
}

static size_t GetComponentSize(ClassRoot component_root)
{
    switch (component_root) {
        case ClassRoot::U1:
        case ClassRoot::I8:
        case ClassRoot::U8:
            return sizeof(uint8_t);
        case ClassRoot::I16:
        case ClassRoot::U16:
            return sizeof(uint16_t);
        case ClassRoot::I32:
        case ClassRoot::U32:
        case ClassRoot::F32:
            return sizeof(uint32_t);
        case ClassRoot::I64:
        case ClassRoot::U64:
        case ClassRoot::F64:
            return sizeof(uint64_t);
        default:
            UNREACHABLE();
    }
}

static void TestArrayClassRoot(const ClassLinkerExtension &class_linker_ext, ClassRoot class_root,
                               ClassRoot component_root)
{
    std::string msg = "Test with class root ";
    msg += static_cast<int>(class_root);

    Class *klass = class_linker_ext.GetClassRoot(class_root);
    Class *component_class = class_linker_ext.GetClassRoot(component_root);
    ASSERT_NE(klass, nullptr) << msg;
    EXPECT_EQ(klass->GetBase(), class_linker_ext.GetClassRoot(ClassRoot::OBJECT)) << msg;
    EXPECT_EQ(klass->GetComponentType(), component_class) << msg;
    EXPECT_EQ(klass->GetComponentSize(), GetComponentSize(component_root)) << msg;
    EXPECT_EQ(klass->GetFlags(), 0U) << msg;
    EXPECT_EQ(klass->GetAccessFlags(), ACC_PUBLIC | ACC_FINAL | ACC_ABSTRACT) << msg;
    EXPECT_EQ(klass->GetType().GetId(), panda_file::Type::TypeId::REFERENCE) << msg;
    EXPECT_EQ(klass->IsObjectArrayClass(), !component_class->IsPrimitive()) << msg;
    EXPECT_TRUE(klass->IsArrayClass()) << msg;
    EXPECT_FALSE(klass->IsStringClass()) << msg;
    EXPECT_FALSE(klass->IsPrimitive()) << msg;
    EXPECT_TRUE(klass->IsAbstract()) << msg;
    EXPECT_TRUE(klass->IsInstantiable()) << msg;
}

TEST_F(ClassLinkerTest, TestClassRoots)
{
    auto class_linker = CreateClassLinker(thread_);
    ASSERT_NE(class_linker, nullptr);

    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto *ext = class_linker->GetExtension(ctx);

    Class *object_class = ext->GetClassRoot(ClassRoot::OBJECT);
    ASSERT_NE(object_class, nullptr);
    EXPECT_EQ(object_class->GetBase(), nullptr);
    EXPECT_EQ(object_class->GetComponentSize(), 0U);
    EXPECT_EQ(object_class->GetFlags(), 0U);
    EXPECT_EQ(object_class->GetType().GetId(), panda_file::Type::TypeId::REFERENCE);
    EXPECT_FALSE(object_class->IsArrayClass());
    EXPECT_FALSE(object_class->IsObjectArrayClass());
    EXPECT_FALSE(object_class->IsStringClass());
    EXPECT_FALSE(object_class->IsPrimitive());

    Class *string_class = ext->GetClassRoot(ClassRoot::STRING);
    ASSERT_NE(string_class, nullptr);
    EXPECT_EQ(string_class->GetBase(), object_class);
    EXPECT_EQ(string_class->GetComponentSize(), 0U);
    EXPECT_EQ(string_class->GetFlags(), Class::STRING_CLASS);
    EXPECT_EQ(string_class->GetType().GetId(), panda_file::Type::TypeId::REFERENCE);
    EXPECT_FALSE(string_class->IsArrayClass());
    EXPECT_FALSE(string_class->IsObjectArrayClass());
    EXPECT_TRUE(string_class->IsStringClass());
    EXPECT_FALSE(string_class->IsPrimitive());

    TestPrimitiveClassRoot(*ext, ClassRoot::U1, panda_file::Type::TypeId::U1);
    TestPrimitiveClassRoot(*ext, ClassRoot::I8, panda_file::Type::TypeId::I8);
    TestPrimitiveClassRoot(*ext, ClassRoot::U8, panda_file::Type::TypeId::U8);
    TestPrimitiveClassRoot(*ext, ClassRoot::I16, panda_file::Type::TypeId::I16);
    TestPrimitiveClassRoot(*ext, ClassRoot::U16, panda_file::Type::TypeId::U16);
    TestPrimitiveClassRoot(*ext, ClassRoot::I32, panda_file::Type::TypeId::I32);
    TestPrimitiveClassRoot(*ext, ClassRoot::U32, panda_file::Type::TypeId::U32);
    TestPrimitiveClassRoot(*ext, ClassRoot::I64, panda_file::Type::TypeId::I64);
    TestPrimitiveClassRoot(*ext, ClassRoot::U64, panda_file::Type::TypeId::U64);
    TestPrimitiveClassRoot(*ext, ClassRoot::F32, panda_file::Type::TypeId::F32);
    TestPrimitiveClassRoot(*ext, ClassRoot::F64, panda_file::Type::TypeId::F64);

    TestArrayClassRoot(*ext, ClassRoot::ARRAY_U1, ClassRoot::U1);
    TestArrayClassRoot(*ext, ClassRoot::ARRAY_I8, ClassRoot::I8);
    TestArrayClassRoot(*ext, ClassRoot::ARRAY_U8, ClassRoot::U8);
    TestArrayClassRoot(*ext, ClassRoot::ARRAY_I16, ClassRoot::I16);
    TestArrayClassRoot(*ext, ClassRoot::ARRAY_U16, ClassRoot::U16);
    TestArrayClassRoot(*ext, ClassRoot::ARRAY_I32, ClassRoot::I32);
    TestArrayClassRoot(*ext, ClassRoot::ARRAY_U32, ClassRoot::U32);
    TestArrayClassRoot(*ext, ClassRoot::ARRAY_I64, ClassRoot::I64);
    TestArrayClassRoot(*ext, ClassRoot::ARRAY_U64, ClassRoot::U64);
    TestArrayClassRoot(*ext, ClassRoot::ARRAY_F32, ClassRoot::F32);
    TestArrayClassRoot(*ext, ClassRoot::ARRAY_F64, ClassRoot::F64);
}

struct FieldData {
    std::string name;
    size_t size;
    size_t offset;

    bool operator==(const FieldData &other) const
    {
        return name == other.name && size == other.size && offset == other.offset;
    }

    friend std::ostream &operator<<(std::ostream &os, const FieldData &field_data)
    {
        return os << "{ name: \"" << field_data.name << "\", size: " << field_data.size
                  << ", offset: " << field_data.offset << " }";
    }
};

struct FieldDataHash {
    size_t operator()(const FieldData &field_data) const
    {
        return std::hash<std::string>()(field_data.name);
    }
};

size_t GetSize(const Field &field)
{
    size_t size = 0;

    switch (field.GetType().GetId()) {
        case panda_file::Type::TypeId::U1:
        case panda_file::Type::TypeId::I8:
        case panda_file::Type::TypeId::U8: {
            size = 1;
            break;
        }
        case panda_file::Type::TypeId::I16:
        case panda_file::Type::TypeId::U16: {
            size = 2U;
            break;
        }
        case panda_file::Type::TypeId::I32:
        case panda_file::Type::TypeId::U32:
        case panda_file::Type::TypeId::F32: {
            size = 4U;
            break;
        }
        case panda_file::Type::TypeId::I64:
        case panda_file::Type::TypeId::U64:
        case panda_file::Type::TypeId::F64: {
            size = 8U;
            break;
        }
        case panda_file::Type::TypeId::REFERENCE: {
            size = ClassHelper::OBJECT_POINTER_SIZE;
            break;
        }
        case panda_file::Type::TypeId::TAGGED: {
            size = coretypes::TaggedValue::TaggedTypeSize();
            break;
        }
        default: {
            UNREACHABLE();
            break;
        }
    }

    return size;
}

void UpdateOffsets(std::vector<FieldData> *fields, size_t offset)
{
    for (auto &field : *fields) {
        offset = AlignUp(offset, field.size);
        field.offset = offset;
        offset += field.size;
    }
}

TEST_F(ClassLinkerTest, FieldLayout)
{
    pandasm::Parser p;

    auto source = R"(
        .record R1 {}

        .record R2 {
            # static fields

            u1  sf_u1  <static>
            i16 sf_i16 <static>
            i8  sf_i8  <static>
            i32 sf_i32 <static>
            u8  sf_u8  <static>
            f64 sf_f64 <static>
            u32 sf_u32 <static>
            u16 sf_u16 <static>
            i64 sf_i64 <static>
            f32 sf_f32 <static>
            u64 sf_u64 <static>
            R1  sf_ref <static>
            any sf_any <static>

            # instance fields

            i16 if_i16
            u1  if_u1
            i8  if_i8
            f64 if_f64
            i32 if_i32
            u8  if_u8
            u32 if_u32
            u16 if_u16
            f32 if_f32
            i64 if_i64
            u64 if_u64
            R2  if_ref
            any if_any
        }
    )";

    auto res = p.Parse(source);
    auto pf = pandasm::AsmEmitter::Emit(res.Value());

    auto class_linker = CreateClassLinker(thread_);
    ASSERT_NE(class_linker, nullptr);

    class_linker->AddPandaFile(std::move(pf));

    PandaString descriptor;
    auto *ext = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
    Class *klass = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("R2"), &descriptor));
    ASSERT_NE(klass, nullptr);

    std::vector<FieldData> sorted_sfields {{"sf_ref", ClassHelper::OBJECT_POINTER_SIZE, 0},
                                           {"sf_any", coretypes::TaggedValue::TaggedTypeSize(), 0},
                                           {"sf_f64", sizeof(double), 0},
                                           {"sf_i64", sizeof(int64_t), 0},
                                           {"sf_u64", sizeof(uint64_t), 0},
                                           {"sf_i32", sizeof(int32_t), 0},
                                           {"sf_u32", sizeof(uint32_t), 0},
                                           {"sf_f32", sizeof(float), 0},
                                           {"sf_i16", sizeof(int16_t), 0},
                                           {"sf_u16", sizeof(uint16_t), 0},
                                           {"sf_u1", sizeof(uint8_t), 0},
                                           {"sf_i8", sizeof(int8_t), 0},
                                           {"sf_u8", sizeof(uint8_t), 0}};

    std::vector<FieldData> sorted_ifields {{"if_ref", ClassHelper::OBJECT_POINTER_SIZE, 0},
                                           {"if_any", coretypes::TaggedValue::TaggedTypeSize(), 0},
                                           {"if_f64", sizeof(double), 0},
                                           {"if_i64", sizeof(int64_t), 0},
                                           {"if_u64", sizeof(uint64_t), 0},
                                           {"if_i32", sizeof(int32_t), 0},
                                           {"if_u32", sizeof(uint32_t), 0},
                                           {"if_f32", sizeof(float), 0},
                                           {"if_i16", sizeof(int16_t), 0},
                                           {"if_u16", sizeof(uint16_t), 0},
                                           {"if_u1", sizeof(uint8_t), 0},
                                           {"if_i8", sizeof(int8_t), 0},
                                           {"if_u8", sizeof(uint8_t), 0}};

    size_t offset = klass->GetStaticFieldsOffset();

    if (!IsAligned<sizeof(double)>(offset + ClassHelper::OBJECT_POINTER_SIZE)) {
        FieldData data {"sf_i32", sizeof(int32_t), 0};
        sorted_sfields.erase(std::remove(sorted_sfields.begin(), sorted_sfields.end(), data));
        sorted_sfields.insert(sorted_sfields.cbegin() + 1, data);
    }

    UpdateOffsets(&sorted_sfields, offset);

    offset = ObjectHeader::ObjectHeaderSize();

    if (!IsAligned<sizeof(double)>(offset + ClassHelper::OBJECT_POINTER_SIZE)) {
        FieldData data {"if_i32", sizeof(int32_t), 0};
        sorted_ifields.erase(std::remove(sorted_ifields.begin(), sorted_ifields.end(), data));
        sorted_ifields.insert(sorted_ifields.cbegin() + 1, data);
    }

    UpdateOffsets(&sorted_ifields, offset);

    auto field_cmp = [](const FieldData &f1, const FieldData &f2) { return f1.offset < f2.offset; };

    std::vector<FieldData> sfields;
    for (const auto &field : klass->GetStaticFields()) {
        sfields.push_back({utf::Mutf8AsCString(field.GetName().data), GetSize(field), field.GetOffset()});
    }
    std::sort(sfields.begin(), sfields.end(), field_cmp);
    EXPECT_EQ(sfields, sorted_sfields);

    std::unordered_set<FieldData, FieldDataHash> ifields;

    for (const auto &field : klass->GetInstanceFields()) {
        ifields.insert({utf::Mutf8AsCString(field.GetName().data), GetSize(field), field.GetOffset()});
    }

    std::unordered_set<FieldData, FieldDataHash> sorted_ifields_set(sorted_ifields.cbegin(), sorted_ifields.cend());
    EXPECT_EQ(ifields, sorted_ifields_set);
}

TEST_F(ClassLinkerTest, ResolveExternalClass)
{
    uint32_t offset;

    auto class_linker = CreateClassLinker(thread_);
    ASSERT_NE(class_linker, nullptr);

    {
        pandasm::Parser p;

        auto source = R"(
            .record Ext.R <external>

            .function void main() {
                newarr v0, v0, Ext.R[]
                return.void
            }
        )";

        auto res = p.Parse(source);
        ASSERT_TRUE(res);
        auto pf = pandasm::AsmEmitter::Emit(res.Value());

        // 0 - "LExt/R;"
        // 1 - "L_GLOBAL;"
        // 2U - "[LExt/R;"
        offset = pf->GetClasses()[2];

        class_linker->AddPandaFile(std::move(pf));
    }

    PandaString descriptor;

    auto *ext = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto *klass = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("_GLOBAL"), &descriptor));
    ASSERT_NE(klass, nullptr);

    auto *method = klass->GetDirectMethod(utf::CStringAsMutf8("main"));
    ASSERT_NE(method, nullptr);

    auto *external_class = class_linker->GetClass(*method, panda_file::File::EntityId(offset));
    ASSERT_EQ(external_class, nullptr);

    {
        pandasm::Parser p;

        auto ext_source = R"(
            .record Ext {}
            .record Ext.R {}
        )";

        auto res = p.Parse(ext_source);
        auto ext_pf = pandasm::AsmEmitter::Emit(res.Value());

        class_linker->AddPandaFile(std::move(ext_pf));
    }

    external_class = class_linker->GetClass(*method, panda_file::File::EntityId(offset));
    ASSERT_NE(external_class, nullptr);

    EXPECT_STREQ(utf::Mutf8AsCString(external_class->GetDescriptor()),
                 utf::Mutf8AsCString(ClassHelper::GetArrayDescriptor(utf::CStringAsMutf8("Ext.R"), 1, &descriptor)));
}

TEST_F(ClassLinkerTest, ArrayClass)
{
    pandasm::Parser p;

    auto source = R"(
        .record R {}
    )";

    auto res = p.Parse(source);
    auto pf = pandasm::AsmEmitter::Emit(res.Value());

    auto class_linker = CreateClassLinker(thread_);
    ASSERT_NE(class_linker, nullptr);

    class_linker->AddPandaFile(std::move(pf));

    PandaString descriptor;

    auto *ext = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto *klass = ext->GetClass(ClassHelper::GetArrayDescriptor(utf::CStringAsMutf8("UnknownClass"), 1, &descriptor));
    ASSERT_EQ(klass, nullptr);

    for (size_t i = 0; i < 256; i++) {
        auto *cls = ext->GetClass(ClassHelper::GetArrayDescriptor(utf::CStringAsMutf8("R"), i, &descriptor));
        ASSERT_NE(cls, nullptr);
        EXPECT_EQ(utf::Mutf8AsCString(cls->GetDescriptor()), descriptor);
    }
}

static Method *GetMethod(ClassLinker *class_linker, const char *class_name, const char *method_name)
{
    PandaString descriptor;
    auto *ext = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto *klass = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8(class_name), &descriptor));
    return klass->GetDirectMethod(utf::CStringAsMutf8(method_name));
}

static std::unordered_set<Method *> GetMethodsSet(Span<Method> methods)
{
    std::unordered_set<Method *> set;
    for (auto &method : methods) {
        set.insert(&method);
    }

    return set;
}

TEST_F(ClassLinkerTest, VTable)
{
    {
        pandasm::Parser p;

        auto source = R"(
            .record A {}

            .function void A.f1() {}
            .function void A.f2(i32 a0) {}

            .function void A.f3(A a0) {}
            .function void A.f4(A a0, i32 a1) {}
        )";

        auto res = p.Parse(source);
        auto pf = pandasm::AsmEmitter::Emit(res.Value());

        auto class_linker = CreateClassLinker(thread_);
        ASSERT_NE(class_linker, nullptr);

        class_linker->AddPandaFile(std::move(pf));

        PandaString descriptor;

        auto *ext = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto *class_a = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("A"), &descriptor));
        ASSERT_NE(class_a, nullptr);

        auto smethods = class_a->GetStaticMethods();
        ASSERT_EQ(smethods.size(), 2);

        auto vmethods = class_a->GetVirtualMethods();
        ASSERT_EQ(vmethods.size(), 2);

        {
            auto set = GetMethodsSet(smethods);
            ASSERT_NE(set.find(GetMethod(class_linker.get(), "A", "f1")), set.cend());
            ASSERT_NE(set.find(GetMethod(class_linker.get(), "A", "f2")), set.cend());
        }

        {
            auto set = GetMethodsSet(vmethods);
            ASSERT_NE(set.find(GetMethod(class_linker.get(), "A", "f3")), set.cend());
            ASSERT_NE(set.find(GetMethod(class_linker.get(), "A", "f4")), set.cend());
        }

        {
            auto vtable = class_a->GetVTable();
            ASSERT_EQ(vtable.size(), vmethods.size());

            for (size_t i = 0; i < vmethods.size(); i++) {
                ASSERT_EQ(vtable[vmethods[i].GetVTableIndex()], &vmethods[i]);
            }
        }
    }
}

TEST_F(ClassLinkerTest, PrimitiveClasses)
{
    auto class_linker = CreateClassLinker(thread_);
    ASSERT_NE(class_linker, nullptr);

    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto *ext = class_linker->GetExtension(ctx);

    PandaString descriptor;

    auto type = panda_file::Type(panda_file::Type::TypeId::I32);

    auto *primitive_class = ext->GetClass(ClassHelper::GetPrimitiveDescriptor(type, &descriptor));
    ASSERT_NE(primitive_class, nullptr);
    EXPECT_STREQ(utf::Mutf8AsCString(primitive_class->GetDescriptor()),
                 utf::Mutf8AsCString(ClassHelper::GetPrimitiveDescriptor(type, &descriptor)));

    auto *primitive_array_class1 = ext->GetClass(ClassHelper::GetPrimitiveArrayDescriptor(type, 1, &descriptor));
    ASSERT_NE(primitive_array_class1, nullptr);
    EXPECT_STREQ(utf::Mutf8AsCString(primitive_array_class1->GetDescriptor()),
                 utf::Mutf8AsCString(ClassHelper::GetPrimitiveArrayDescriptor(type, 1, &descriptor)));

    auto *primitive_array_class2 = ext->GetClass(ClassHelper::GetPrimitiveArrayDescriptor(type, 2, &descriptor));
    ASSERT_NE(primitive_array_class2, nullptr);
    EXPECT_STREQ(utf::Mutf8AsCString(primitive_array_class2->GetDescriptor()),
                 utf::Mutf8AsCString(ClassHelper::GetPrimitiveArrayDescriptor(type, 2, &descriptor)));
}

class TestClassLinkerContext : public ClassLinkerContext {
public:
    TestClassLinkerContext(const uint8_t *descriptor, bool need_copy_descriptor, Class *klass,
                           [[maybe_unused]] panda_file::SourceLang lang)
        : descriptor_(descriptor), need_copy_descriptor_(need_copy_descriptor), klass_(klass)
    {
#ifndef NDEBUG
        lang_ = lang;
#endif  // NDEBUG
    }

    Class *LoadClass(const uint8_t *descriptor, bool need_copy_descriptor,
                     [[maybe_unused]] ClassLinkerErrorHandler *error_handler = nullptr) override
    {
        is_success_ = utf::IsEqual(descriptor, descriptor_) && need_copy_descriptor == need_copy_descriptor_;
        InsertClass(klass_);
        return klass_;
    }

    bool IsSuccess() const
    {
        return is_success_;
    }

private:
    const uint8_t *descriptor_;
    bool need_copy_descriptor_ {};
    Class *klass_;
    bool is_success_ {false};
};

TEST_F(ClassLinkerTest, LoadContext)
{
    pandasm::Parser p;

    auto source = R"(
        .record A {}
        .record B {}
    )";

    auto res = p.Parse(source);
    auto pf = pandasm::AsmEmitter::Emit(res.Value());

    auto class_linker = CreateClassLinker(thread_);
    ASSERT_NE(class_linker, nullptr);

    class_linker->AddPandaFile(std::move(pf));

    PandaString descriptor;
    auto *ext = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto *class_a = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("A"), &descriptor));

    ASSERT_NE(class_a, nullptr);
    ASSERT_EQ(class_a->GetLoadContext()->IsBootContext(), true);

    auto *class_b = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("B"), &descriptor));

    ASSERT_NE(class_b, nullptr);
    ASSERT_EQ(class_b->GetLoadContext()->IsBootContext(), true);

    auto *desc = ClassHelper::GetDescriptor(utf::CStringAsMutf8("B"), &descriptor);
    TestClassLinkerContext ctx(desc, true, class_b, panda_file::SourceLang::PANDA_ASSEMBLY);
    auto *class_b_ctx = ext->GetClass(desc, true, &ctx);

    ASSERT_TRUE(ctx.IsSuccess());
    ASSERT_EQ(class_b_ctx, class_b);

    bool is_matched = false;
    ctx.EnumerateClasses([&is_matched](Class *klass) {
        is_matched = klass->GetName() == "B";
        return true;
    });

    ASSERT_TRUE(is_matched);

    auto *class_array_b =
        class_linker->GetClass(ClassHelper::GetArrayDescriptor(utf::CStringAsMutf8("B"), 1, &descriptor), true, &ctx);

    ASSERT_NE(class_array_b, nullptr);
    ASSERT_EQ(class_array_b->GetLoadContext(), ext->GetBootContext());

    {
        PandaUnorderedSet<Class *> expected {class_b};
        PandaUnorderedSet<Class *> classes;
        ctx.EnumerateClasses([&](Class *klass) {
            classes.insert(klass);
            return true;
        });

        ASSERT_EQ(classes, expected);
    }

    {
        PandaUnorderedSet<Class *> classes;
        class_linker->EnumerateClasses([&](Class *klass) {
            classes.insert(klass);
            return true;
        });

        ASSERT_NE(classes.find(class_a), classes.cend());
        ASSERT_EQ(*classes.find(class_a), class_a);

        ASSERT_NE(classes.find(class_b), classes.cend());
        ASSERT_EQ(*classes.find(class_b), class_b);

        ASSERT_NE(classes.find(class_array_b), classes.cend());
        ASSERT_EQ(*classes.find(class_array_b), class_array_b);
    }
}

}  // namespace panda::test
