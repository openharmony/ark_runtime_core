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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <sstream>
#include <type_traits>
#include <vector>

#include "assembly-parser.h"
#include "libpandabase/mem/pool_manager.h"
#include "libpandabase/utils/utf.h"
#include "libpandafile/bytecode_emitter.h"
#include "libpandafile/file.h"
#include "libpandafile/file_items.h"
#include "libpandafile/value.h"
#include "macros.h"
#include "runtime/bridge/bridge.h"
#include "runtime/include/class_linker.h"
#include "runtime/include/compiler_interface.h"
#include "runtime/include/mem/allocator.h"
#include "runtime/include/method.h"
#include "runtime/include/runtime.h"
#include "runtime/include/runtime_options.h"
#include "runtime/include/value-inl.h"
#include "runtime/interpreter/frame.h"
#include "runtime/mem/gc/gc.h"
#include "runtime/mem/internal_allocator.h"
#include "runtime/core/core_class_linker_extension.h"
#include "runtime/tests/class_linker_test_extension.h"
#include "runtime/tests/interpreter/test_interpreter.h"
#include "runtime/tests/interpreter/test_runtime_interface.h"
#include "runtime/include/coretypes/dyn_objects.h"
#include "runtime/include/hclass.h"
#include "runtime/handle_base-inl.h"
#include "runtime/handle_scope-inl.h"
#include "runtime/include/coretypes/native_pointer.h"

namespace panda::interpreter {

namespace test {

using DynClass = panda::coretypes::DynClass;
using DynObject = panda::coretypes::DynObject;

class InterpreterTest : public testing::Test {
public:
    InterpreterTest()
    {
        RuntimeOptions options;
        options.SetShouldLoadBootPandaFiles(false);
        options.SetShouldInitializeIntrinsics(false);
        options.SetRunGcInPlace(true);
        options.SetVerifyCallStack(false);
        options.SetGcType("epsilon");
        Runtime::Create(options);
        thread_ = panda::MTManagedThread::GetCurrent();
        thread_->ManagedCodeBegin();
    }

    ~InterpreterTest()
    {
        thread_->ManagedCodeEnd();
        Runtime::Destroy();
    }

protected:
    panda::MTManagedThread *thread_ {nullptr};
};

auto CreateFrame(size_t nregs, Method *method, Frame *prev)
{
    auto frame_deleter = [](Frame *frame) { RuntimeInterface::FreeFrame(frame); };
    std::unique_ptr<Frame, decltype(frame_deleter)> frame(RuntimeInterface::CreateFrame(nregs, method, prev),
                                                          frame_deleter);
    return frame;
}

static void InitializeFrame(Frame *f)
{
    ManagedThread::GetCurrent()->SetCurrentFrame(f);
    for (size_t i = 0; i < f->GetSize(); i++) {
        f->GetVReg(i).SetValue(static_cast<int64_t>(0));
        f->GetVReg(i).SetTag(static_cast<int64_t>(0));
    }
}

static std::unique_ptr<Class> CreateClass(panda_file::SourceLang lang)
{
    const std::string class_name("Foo");
    auto cls = std::make_unique<Class>(reinterpret_cast<const uint8_t *>(class_name.data()), lang, 0, 0,
                                       AlignUp(sizeof(Class), OBJECT_POINTER_SIZE));
    return cls;
}

static std::pair<PandaUniquePtr<Method>, std::unique_ptr<const panda_file::File>> CreateMethod(
    Class *klass, uint32_t access_flags, uint32_t nargs, uint32_t nregs, uint16_t *shorty,
    const std::vector<uint8_t> &bytecode)
{
    // Create panda_file

    panda_file::ItemContainer container;
    panda_file::ClassItem *class_item = container.GetOrCreateGlobalClassItem();
    class_item->SetAccessFlags(ACC_PUBLIC);

    panda_file::StringItem *method_name = container.GetOrCreateStringItem("test");
    panda_file::PrimitiveTypeItem *ret_type =
        container.CreateItem<panda_file::PrimitiveTypeItem>(panda_file::Type::TypeId::VOID);
    std::vector<panda_file::MethodParamItem> params;
    panda_file::ProtoItem *proto_item = container.GetOrCreateProtoItem(ret_type, params);
    panda_file::MethodItem *method_item =
        class_item->AddMethod(method_name, proto_item, ACC_PUBLIC | ACC_STATIC, params);

    panda_file::CodeItem *code_item = container.CreateItem<panda_file::CodeItem>(nregs, nargs, bytecode);
    method_item->SetCode(code_item);

    panda_file::MemoryWriter mem_writer;
    container.Write(&mem_writer);

    auto data = mem_writer.GetData();
    auto allocator = Runtime::GetCurrent()->GetInternalAllocator();
    auto buf = allocator->AllocArray<uint8_t>(data.size());
    (void)memcpy_s(buf, data.size(), data.data(), data.size());

    os::mem::ConstBytePtr ptr(reinterpret_cast<std::byte *>(buf), data.size(), [](std::byte *buffer, size_t) noexcept {
        auto a = Runtime::GetCurrent()->GetInternalAllocator();
        a->Free(buffer);
    });
    auto pf = panda_file::File::OpenFromMemory(std::move(ptr));

    // Create method
    auto method = MakePandaUnique<Method>(klass, pf.get(), method_item->GetFileId(), code_item->GetFileId(),
                                          access_flags | ACC_PUBLIC | ACC_STATIC, nargs, shorty);
    method->SetInterpreterEntryPoint();
    return {std::move(method), std::move(pf)};
}

static std::pair<PandaUniquePtr<Method>, std::unique_ptr<const panda_file::File>> CreateMethod(
    Class *klass, Frame *f, const std::vector<uint8_t> &bytecode)
{
    return CreateMethod(klass, 0, 0, f->GetSize(), nullptr, bytecode);
}

static std::unique_ptr<ClassLinker> CreateClassLinker([[maybe_unused]] ManagedThread *thread)
{
    std::vector<std::unique_ptr<ClassLinkerExtension>> extensions;
    extensions.push_back(std::make_unique<CoreClassLinkerExtension>());

    auto allocator = Runtime::GetCurrent()->GetInternalAllocator();
    auto class_linker = std::make_unique<ClassLinker>(allocator, std::move(extensions));
    if (!class_linker->Initialize()) {
        return nullptr;
    }

    return class_linker;
}

static ObjectHeader *CreateException(ManagedThread *thread)
{
    auto class_linker = CreateClassLinker(thread);
    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
    Class *cls = class_linker->GetExtension(ctx)->GetClassRoot(ClassRoot::OBJECT);
    ObjectHeader *exception = ObjectHeader::Create(cls);
    return exception;
}

TEST_F(InterpreterTest, TestMov)
{
    BytecodeEmitter emitter;

    constexpr int64_t IMM4_MAX = 7;
    constexpr int64_t IMM8_MAX = std::numeric_limits<int8_t>::max();
    constexpr int64_t IMM16_MAX = std::numeric_limits<int16_t>::max();
    constexpr int64_t IMM32_MAX = std::numeric_limits<int32_t>::max();
    constexpr int64_t IMM64_MAX = std::numeric_limits<int64_t>::max();

    constexpr uint16_t V4_MAX = 15;
    constexpr uint16_t V8_MAX = std::numeric_limits<uint8_t>::max();
    constexpr uint16_t V16_MAX = std::numeric_limits<uint16_t>::max();

    ObjectHeader *obj1 = ToPointer<ObjectHeader>(0xaabbccdd);
    ObjectHeader *obj2 = ToPointer<ObjectHeader>(0xaabbccdd + 0x100);
    ObjectHeader *obj3 = ToPointer<ObjectHeader>(0xaabbccdd + 0x200);

    emitter.Movi(0, IMM4_MAX);
    emitter.Movi(1, IMM8_MAX);
    emitter.Movi(2U, IMM16_MAX);
    emitter.Movi(3U, IMM32_MAX);
    emitter.MoviWide(4U, IMM64_MAX);

    emitter.Mov(V4_MAX, V4_MAX - 1);
    emitter.Mov(V8_MAX, V8_MAX - 1);
    emitter.Mov(V16_MAX, V16_MAX - 1);

    emitter.MovWide(V4_MAX - 2U, V4_MAX - 3U);
    emitter.MovWide(V16_MAX - 2U, V16_MAX - 3U);

    emitter.MovObj(V4_MAX - 4U, V4_MAX - 5U);
    emitter.MovObj(V8_MAX - 4U, V8_MAX - 5U);
    emitter.MovObj(V16_MAX - 4U, V16_MAX - 5U);

    emitter.ReturnVoid();

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

    auto f = CreateFrame(std::numeric_limits<uint16_t>::max() + 1, nullptr, nullptr);
    InitializeFrame(f.get());

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
    auto method = std::move(method_data.first);
    f->SetMethod(method.get());

    f->GetVReg(V4_MAX - 1).SetPrimitive(IMM64_MAX - 1);
    f->GetVReg(V8_MAX - 1).SetPrimitive(IMM64_MAX - 2U);
    f->GetVReg(V16_MAX - 1).SetPrimitive(IMM64_MAX - 3U);

    f->GetVReg(V4_MAX - 3U).SetPrimitive(IMM64_MAX - 4U);
    f->GetVReg(V16_MAX - 3U).SetPrimitive(IMM64_MAX - 5U);

    f->GetVReg(V4_MAX - 5U).SetReference(obj1);
    f->GetVReg(V8_MAX - 5U).SetReference(obj2);
    f->GetVReg(V16_MAX - 5U).SetReference(obj3);

    Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

    // Check movi

    EXPECT_EQ(f->GetVReg(0).GetLong(), IMM4_MAX);
    EXPECT_FALSE(f->GetVReg(0).HasObject());

    EXPECT_EQ(f->GetVReg(1).GetLong(), IMM8_MAX);
    EXPECT_FALSE(f->GetVReg(1).HasObject());

    EXPECT_EQ(f->GetVReg(2U).GetLong(), IMM16_MAX);
    EXPECT_FALSE(f->GetVReg(2U).HasObject());

    EXPECT_EQ(f->GetVReg(3U).GetLong(), IMM32_MAX);
    EXPECT_FALSE(f->GetVReg(3U).HasObject());

    EXPECT_EQ(f->GetVReg(4U).GetLong(), IMM64_MAX);
    EXPECT_FALSE(f->GetVReg(4U).HasObject());

    // Check mov

    EXPECT_EQ(f->GetVReg(V4_MAX).Get(), static_cast<int32_t>(IMM64_MAX - 1));
    EXPECT_FALSE(f->GetVReg(V4_MAX).HasObject());

    EXPECT_EQ(f->GetVReg(V8_MAX).Get(), static_cast<int32_t>(IMM64_MAX - 2U));
    EXPECT_FALSE(f->GetVReg(V8_MAX).HasObject());

    EXPECT_EQ(f->GetVReg(V16_MAX).Get(), static_cast<int32_t>(IMM64_MAX - 3U));
    EXPECT_FALSE(f->GetVReg(V16_MAX).HasObject());

    // Check mov.64

    EXPECT_EQ(f->GetVReg(V4_MAX - 2U).GetLong(), IMM64_MAX - 4U);
    EXPECT_FALSE(f->GetVReg(V4_MAX - 2U).HasObject());

    EXPECT_EQ(f->GetVReg(V16_MAX - 2U).GetLong(), IMM64_MAX - 5U);
    EXPECT_FALSE(f->GetVReg(V16_MAX - 2U).HasObject());

    // Check mov.obj

    EXPECT_EQ(f->GetVReg(V4_MAX - 4U).GetReference(), obj1);
    EXPECT_TRUE(f->GetVReg(V4_MAX - 4U).HasObject());

    EXPECT_EQ(f->GetVReg(V8_MAX - 4U).GetReference(), obj2);
    EXPECT_TRUE(f->GetVReg(V8_MAX - 4U).HasObject());

    EXPECT_EQ(f->GetVReg(V16_MAX - 4U).GetReference(), obj3);
    EXPECT_TRUE(f->GetVReg(V16_MAX - 4U).HasObject());
}

TEST_F(InterpreterTest, TestLoadStoreAccumulator)
{
    BytecodeEmitter emitter;

    constexpr int64_t IMM8_MAX = std::numeric_limits<int8_t>::max();
    constexpr int64_t IMM16_MAX = std::numeric_limits<int16_t>::max();
    constexpr int64_t IMM32_MAX = std::numeric_limits<int32_t>::max();
    constexpr int64_t IMM64_MAX = std::numeric_limits<int64_t>::max();

    ObjectHeader *obj = ToPointer<ObjectHeader>(0xaabbccdd);

    emitter.Ldai(IMM8_MAX);
    emitter.Sta(0);

    emitter.Ldai(IMM16_MAX);
    emitter.Sta(1);

    emitter.Ldai(IMM32_MAX);
    emitter.Sta(2U);

    emitter.LdaiWide(IMM64_MAX);
    emitter.StaWide(3);

    emitter.Lda(4);
    emitter.Sta(5);

    emitter.LdaWide(6);
    emitter.StaWide(7);

    emitter.LdaObj(8);
    emitter.StaObj(9);

    emitter.ReturnVoid();

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

    auto f = CreateFrame(16U, nullptr, nullptr);
    InitializeFrame(f.get());

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
    auto method = std::move(method_data.first);
    f->SetMethod(method.get());

    f->GetVReg(4U).SetPrimitive(IMM64_MAX - 1);
    f->GetVReg(6U).SetPrimitive(IMM64_MAX - 2U);
    f->GetVReg(8U).SetReference(obj);

    Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

    EXPECT_EQ(f->GetVReg(0).Get(), static_cast<int32_t>(IMM8_MAX));
    EXPECT_FALSE(f->GetVReg(0).HasObject());

    EXPECT_EQ(f->GetVReg(1).Get(), static_cast<int32_t>(IMM16_MAX));
    EXPECT_FALSE(f->GetVReg(1).HasObject());

    EXPECT_EQ(f->GetVReg(2U).Get(), static_cast<int32_t>(IMM32_MAX));
    EXPECT_FALSE(f->GetVReg(2U).HasObject());

    EXPECT_EQ(f->GetVReg(3U).GetLong(), IMM64_MAX);
    EXPECT_FALSE(f->GetVReg(3U).HasObject());

    EXPECT_EQ(f->GetVReg(5U).Get(), static_cast<int32_t>(IMM64_MAX - 1));
    EXPECT_FALSE(f->GetVReg(5U).HasObject());

    EXPECT_EQ(f->GetVReg(7U).GetLong(), IMM64_MAX - 2U);
    EXPECT_FALSE(f->GetVReg(7U).HasObject());

    EXPECT_EQ(f->GetVReg(9U).GetReference(), obj);
    EXPECT_TRUE(f->GetVReg(9U).HasObject());
}

TEST_F(InterpreterTest, TestLoadString)
{
    BytecodeEmitter emitter;

    emitter.LdaStr(RuntimeInterface::STRING_ID.AsFileId().GetOffset());
    emitter.ReturnObj();

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

    auto f = CreateFrame(16U, nullptr, nullptr);
    InitializeFrame(f.get());

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
    auto method = std::move(method_data.first);
    f->SetMethod(method.get());

    Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

    EXPECT_EQ(f->GetAcc().GetReference(),
              RuntimeInterface::ResolveString(thread_->GetVM(), *method, RuntimeInterface::STRING_ID));
    EXPECT_TRUE(f->GetAcc().HasObject());
}

void TestUnimpelemented(std::function<void(BytecodeEmitter *)> emit)
{
    BytecodeEmitter emitter;

    emit(&emitter);

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

    auto f = CreateFrame(16U, nullptr, nullptr);
    InitializeFrame(f.get());

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
    auto method = std::move(method_data.first);
    f->SetMethod(method.get());

    EXPECT_DEATH_IF_SUPPORTED(Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get()), "");
}

TEST_F(InterpreterTest, LoadType)
{
    BytecodeEmitter emitter;

    pandasm::Parser p;
    auto source = R"(
        .record R {}
    )";

    auto res = p.Parse(source);
    auto class_pf = pandasm::AsmEmitter::Emit(res.Value());

    auto class_linker = CreateClassLinker(ManagedThread::GetCurrent());
    ASSERT_NE(class_linker, nullptr);

    class_linker->AddPandaFile(std::move(class_pf));

    PandaString descriptor;
    auto *thread = ManagedThread::GetCurrent();
    auto *ext = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
    Class *object_class = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("R"), &descriptor));
    ASSERT_TRUE(class_linker->InitializeClass(thread, object_class));

    emitter.LdaType(RuntimeInterface::TYPE_ID.AsIndex());
    emitter.ReturnObj();

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

    auto f = CreateFrame(16U, nullptr, nullptr);
    InitializeFrame(f.get());

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
    auto method = std::move(method_data.first);
    f->SetMethod(method.get());

    RuntimeInterface::SetupResolvedClass(object_class);

    Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

    RuntimeInterface::SetupResolvedClass(nullptr);

    EXPECT_EQ(coretypes::Class::FromRuntimeClass(object_class), f->GetAcc().GetReference());
}

void TestFcmp(double v1, double v2, int64_t value, bool is_cmpg = false)
{
    std::ostringstream ss;
    if (is_cmpg) {
        ss << "Test fcmpg.64";
    } else {
        ss << "Test fcmpl.64";
    }
    ss << ", v1 = " << v1 << ", v2 = " << v2;

    BytecodeEmitter emitter;

    if (is_cmpg) {
        emitter.FcmpgWide(0);
    } else {
        emitter.FcmplWide(0);
    }
    emitter.ReturnWide();

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

    auto f = CreateFrame(16U, nullptr, nullptr);
    InitializeFrame(f.get());

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
    auto method = std::move(method_data.first);
    f->SetMethod(method.get());

    f->GetAcc().SetPrimitive(v1);
    f->GetVReg(0).SetPrimitive(v2);

    Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

    EXPECT_EQ(f->GetAcc().GetLong(), value) << ss.str();
    EXPECT_FALSE(f->GetAcc().HasObject()) << ss.str();
}

TEST_F(InterpreterTest, TestFcmp)
{
    TestFcmp(nan(""), 1.0, 1, true);
    TestFcmp(1.0, nan(""), 1, true);
    TestFcmp(nan(""), nan(""), 1, true);
    TestFcmp(1.0, 2.0, -1, true);
    TestFcmp(1.0, 1.0, 0, true);
    TestFcmp(3.0, 2.0, 1, true);

    TestFcmp(nan(""), 1.0, -1);
    TestFcmp(1.0, nan(""), -1);
    TestFcmp(nan(""), nan(""), -1);
    TestFcmp(1.0, 2.0, -1);
    TestFcmp(1.0, 1.0, 0);
    TestFcmp(3.0, 2.0, 1);
}

void TestConditionalJmp(const std::string &mnemonic, int64_t v1, int64_t v2, int64_t r,
                        std::function<void(BytecodeEmitter *, uint8_t, const Label &)> emit)
{
    std::ostringstream ss;
    ss << "Test " << mnemonic << " with v1 = " << v1 << ", v2 = " << v2;

    {
        BytecodeEmitter emitter;
        Label label = emitter.CreateLabel();

        emit(&emitter, 0, label);
        emitter.MoviWide(1, -1);
        emitter.ReturnVoid();
        emitter.Bind(label);
        emitter.MoviWide(1, 1);
        emitter.ReturnVoid();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        f->GetAcc().SetPrimitive(v1);
        f->GetVReg(0).SetPrimitive(v2);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        EXPECT_EQ(f->GetVReg(1).GetLong(), r) << ss.str();
    }

    {
        BytecodeEmitter emitter;
        Label label1 = emitter.CreateLabel();
        Label label2 = emitter.CreateLabel();

        emitter.Jmp(label1);
        emitter.Bind(label2);
        emitter.MoviWide(1, 1);
        emitter.ReturnVoid();
        emitter.Bind(label1);
        emit(&emitter, 0, label2);
        emitter.MoviWide(1, -1);
        emitter.ReturnVoid();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        f->GetAcc().SetPrimitive(v1);
        f->GetVReg(0).SetPrimitive(v2);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        EXPECT_EQ(f->GetVReg(1).GetLong(), r) << ss.str();
        if (ManagedThread::GetCurrent()->GetLanguageContext().GetLanguage() != panda_file::SourceLang::ECMASCRIPT) {
            EXPECT_EQ(method->GetHotnessCounter(), r == 1 ? 1U : 0U) << ss.str();
        }
    }
}

void TestConditionalJmpz(const std::string &mnemonic, int64_t v, int64_t r,
                         std::function<void(BytecodeEmitter *, const Label &)> emit)
{
    std::ostringstream ss;
    ss << "Test " << mnemonic << " with v = " << v;

    {
        BytecodeEmitter emitter;
        Label label = emitter.CreateLabel();

        emit(&emitter, label);
        emitter.MoviWide(0, -1);
        emitter.ReturnVoid();
        emitter.Bind(label);
        emitter.MoviWide(0, 1);
        emitter.ReturnVoid();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        f->GetAcc().SetPrimitive(v);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        EXPECT_EQ(f->GetVReg(0).GetLong(), r) << ss.str();
    }

    {
        BytecodeEmitter emitter;
        Label label1 = emitter.CreateLabel();
        Label label2 = emitter.CreateLabel();

        emitter.Jmp(label1);
        emitter.Bind(label2);
        emitter.MoviWide(0, 1);
        emitter.ReturnVoid();
        emitter.Bind(label1);
        emit(&emitter, label2);
        emitter.MoviWide(0, -1);
        emitter.ReturnVoid();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        f->GetAcc().SetPrimitive(v);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        EXPECT_EQ(f->GetVReg(0).GetLong(), r) << ss.str();
        if (ManagedThread::GetCurrent()->GetLanguageContext().GetLanguage() != panda_file::SourceLang::ECMASCRIPT) {
            EXPECT_EQ(method->GetHotnessCounter(), r == 1 ? 1U : 0U) << ss.str();
        }
    }
}

TEST_F(InterpreterTest, TestConditionalJumps)
{
    // Test jmpz

    TestConditionalJmpz("jeqz", 0, 1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jeqz(label); });
    TestConditionalJmpz("jeqz", 1, -1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jeqz(label); });
    TestConditionalJmpz("jeqz", -1, -1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jeqz(label); });

    TestConditionalJmpz("jnez", 0, -1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jnez(label); });
    TestConditionalJmpz("jnez", 1, 1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jnez(label); });
    TestConditionalJmpz("jnez", -1, 1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jnez(label); });

    TestConditionalJmpz("jltz", -1, 1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jltz(label); });
    TestConditionalJmpz("jltz", 0, -1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jltz(label); });
    TestConditionalJmpz("jltz", 1, -1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jltz(label); });

    TestConditionalJmpz("jgtz", 1, 1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jgtz(label); });
    TestConditionalJmpz("jgtz", 0, -1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jgtz(label); });
    TestConditionalJmpz("jgtz", -1, -1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jgtz(label); });

    TestConditionalJmpz("jlez", -1, 1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jlez(label); });
    TestConditionalJmpz("jlez", 0, 1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jlez(label); });
    TestConditionalJmpz("jlez", 1, -1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jlez(label); });

    TestConditionalJmpz("jgez", 1, 1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jgez(label); });
    TestConditionalJmpz("jgez", 0, 1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jgez(label); });
    TestConditionalJmpz("jgez", -1, -1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jgez(label); });

    // Test jmp

    TestConditionalJmp("jeq", 2, 2, 1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jeq(reg, label); });
    TestConditionalJmp("jeq", 1, 2, -1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jeq(reg, label); });
    TestConditionalJmp("jeq", 2, 1, -1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jeq(reg, label); });

    TestConditionalJmp("jne", 2, 2, -1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jne(reg, label); });
    TestConditionalJmp("jne", 1, 2, 1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jne(reg, label); });
    TestConditionalJmp("jne", 2, 1, 1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jne(reg, label); });

    TestConditionalJmp("jlt", 2, 2, -1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jlt(reg, label); });
    TestConditionalJmp("jlt", 1, 2, 1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jlt(reg, label); });
    TestConditionalJmp("jlt", 2, 1, -1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jlt(reg, label); });

    TestConditionalJmp("jgt", 2, 2, -1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jgt(reg, label); });
    TestConditionalJmp("jgt", 1, 2, -1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jgt(reg, label); });
    TestConditionalJmp("jgt", 2, 1, 1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jgt(reg, label); });

    TestConditionalJmp("jle", 2, 2, 1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jle(reg, label); });
    TestConditionalJmp("jle", 1, 2, 1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jle(reg, label); });
    TestConditionalJmp("jle", 2, 1, -1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jle(reg, label); });

    TestConditionalJmp("jge", 2, 2, 1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jge(reg, label); });
    TestConditionalJmp("jge", 1, 2, -1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jge(reg, label); });
    TestConditionalJmp("jge", 2, 1, 1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jge(reg, label); });
}

template <class T>
void TestBinOp2(const std::string &mnemonic, T v1, T v2, T r, std::function<void(BytecodeEmitter *, uint8_t)> emit,
                bool is_div = false)
{
    std::ostringstream ss;
    ss << "Test " << mnemonic << " with sizeof(T) = " << sizeof(T);
    ss << ", v1 = " << v1 << ", v2 = " << v2;

    BytecodeEmitter emitter;

    emit(&emitter, 0);
    emitter.ReturnWide();

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

    bool is_arithmetic_exception_expected = is_div && v2 == 0;

    if (is_arithmetic_exception_expected) {
        RuntimeInterface::SetCatchBlockPcOffset(bytecode.size());
        emitter.ReturnObj();
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();
    }

    auto f = CreateFrame(16U, nullptr, nullptr);
    InitializeFrame(f.get());

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
    auto method = std::move(method_data.first);
    f->SetMethod(method.get());

    f->GetAcc().SetPrimitive(v1);
    f->GetVReg(0).SetPrimitive(v2);

    auto *thread = ManagedThread::GetCurrent();
    ObjectHeader *exception = CreateException(thread);
    if (is_arithmetic_exception_expected) {
        RuntimeInterface::SetArithmeticExceptionData({true});
        thread->SetException(exception);
    }

    Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

    if (is_arithmetic_exception_expected) {
        RuntimeInterface::SetArithmeticExceptionData({false});

        auto *curr_thread = ManagedThread::GetCurrent();
        ASSERT_FALSE(curr_thread->HasPendingException()) << ss.str();
        ASSERT_EQ(f->GetAcc().GetReference(), exception) << ss.str();
    } else {
        EXPECT_EQ(f->GetAcc().GetAs<T>(), r) << ss.str();
    }
}

TEST_F(InterpreterTest, TestBinOp2)
{
    constexpr size_t BITWIDTH = std::numeric_limits<uint64_t>::digits;
    constexpr int64_t I32_MAX = std::numeric_limits<int32_t>::max();
    constexpr int64_t I16_MAX = std::numeric_limits<int16_t>::max();

    TestBinOp2<int64_t>("add2", I32_MAX, 2, I32_MAX + 2,
                        [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Add2Wide(reg); });

    TestBinOp2<int32_t>("add2", I16_MAX, 2, I16_MAX + 2,
                        [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Add2(reg); });

    TestBinOp2<double>("fadd2", 1.0, 2.0, 1.0 + 2.0,
                       [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Fadd2Wide(reg); });

    TestBinOp2<int64_t>("sub2", 1, 2, 1 - 2, [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Sub2Wide(reg); });

    TestBinOp2<int32_t>("sub2", 1, 2, 1 - 2, [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Sub2(reg); });

    TestBinOp2<double>("fsub2", 1.0, 2.0, 1.0 - 2.0,
                       [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Fsub2Wide(reg); });

    TestBinOp2<int64_t>("mul2", I32_MAX, 3, I32_MAX * 3,
                        [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Mul2Wide(reg); });

    TestBinOp2<int32_t>("mul2", I16_MAX, 3, I16_MAX * 3,
                        [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Mul2(reg); });

    TestBinOp2<double>("fmul2", 2.0, 3.0, 2.0 * 3.0,
                       [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Fmul2Wide(reg); });

    TestBinOp2<double>("fdiv2", 5.0, 2.0, 5.0 / 2.0,
                       [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Fdiv2Wide(reg); });

    TestBinOp2<double>("fmod2", 10.0, 3.3, fmod(10.0, 3.3),
                       [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Fmod2Wide(reg); });

    TestBinOp2<int64_t>("and2", 0xaabbccdd11223344, 0xffffffff00000000, 0xaabbccdd00000000,
                        [](BytecodeEmitter *emitter, uint8_t reg) { emitter->And2Wide(reg); });

    TestBinOp2<int64_t>("or2", 0xaabbccdd, 0xffff00000000, 0xffffaabbccdd,
                        [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Or2Wide(reg); });

    TestBinOp2<int64_t>("xor2", 0xaabbccdd11223344, 0xffffffffffffffff, 0xaabbccdd11223344 ^ 0xffffffffffffffff,
                        [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Xor2Wide(reg); });

    TestBinOp2<int64_t>("shl2", 0xaabbccdd, 16, 0xaabbccdd0000,
                        [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Shl2Wide(reg); });

    TestBinOp2<int64_t>("shl2", 0xaabbccdd, BITWIDTH + 16, 0xaabbccdd0000,
                        [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Shl2Wide(reg); });

    TestBinOp2<int64_t>("shr2", 0xaabbccdd11223344, 32, 0xaabbccdd,
                        [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Shr2Wide(reg); });

    TestBinOp2<int64_t>("shr2", 0xaabbccdd11223344, BITWIDTH + 32, 0xaabbccdd,
                        [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Shr2Wide(reg); });

    TestBinOp2<int64_t>("ashr2", 0xaabbccdd11223344, 32, 0xffffffffaabbccdd,
                        [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Ashr2Wide(reg); });

    TestBinOp2<int64_t>("ashr2", 0xaabbccdd11223344, BITWIDTH + 32, 0xffffffffaabbccdd,
                        [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Ashr2Wide(reg); });

    TestBinOp2<int64_t>(
        "div2", 0xabbccdd11223344, 32, 0x55de66e889119a,
        [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Div2Wide(reg); }, true);

    TestBinOp2<int64_t>(
        "div2", 0xabbccdd11223344, 0, 0, [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Div2Wide(reg); }, true);

    TestBinOp2<int64_t>(
        "mod2", 0xabbccdd11223344, 32, 4, [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Mod2Wide(reg); }, true);

    TestBinOp2<int64_t>(
        "mod2", 0xabbccdd11223344, 0, 0, [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Mod2Wide(reg); }, true);

    TestBinOp2<int32_t>("and", 0xaabbccdd, 0xffff, 0xccdd,
                        [](BytecodeEmitter *emitter, uint8_t reg) { emitter->And2(reg); });

    TestBinOp2<int32_t>("or", 0xaabbccdd, 0xffff, 0xaabbffff,
                        [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Or2(reg); });

    TestBinOp2<int32_t>("xor2", 0xaabbccdd, 0xffffffff, 0xaabbccdd ^ 0xffffffff,
                        [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Xor2(reg); });

    TestBinOp2<int32_t>("shl2", 0xaabbccdd, 16, 0xccdd0000,
                        [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Shl2(reg); });

    TestBinOp2<int32_t>("shl2", 0xaabbccdd, BITWIDTH + 16, 0xccdd0000,
                        [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Shl2(reg); });

    TestBinOp2<int32_t>("shr2", 0xaabbccdd, 16, 0xaabb,
                        [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Shr2(reg); });

    TestBinOp2<int32_t>("shr2", 0xaabbccdd, BITWIDTH + 16, 0xaabb,
                        [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Shr2(reg); });

    TestBinOp2<int32_t>("ashr2", 0xaabbccdd, 16, 0xffffaabb,
                        [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Ashr2(reg); });

    TestBinOp2<int32_t>("ashr2", 0xaabbccdd, BITWIDTH + 16, 0xffffaabb,
                        [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Ashr2(reg); });

    TestBinOp2<int32_t>(
        "div2", 0xabbccdd, 16, 0xabbccd, [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Div2(reg); }, true);

    TestBinOp2<int32_t>(
        "div2", 0xabbccdd, 0, 0, [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Div2(reg); }, true);

    TestBinOp2<int32_t>(
        "mod2", 0xabbccdd, 16, 0xd, [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Mod2(reg); }, true);

    TestBinOp2<int32_t>(
        "mod2", 0xabbccdd, 0, 0, [](BytecodeEmitter *emitter, uint8_t reg) { emitter->Mod2(reg); }, true);
}

template <class T>
void TestBinOp(const std::string &mnemonic, T v1, T v2, T r,
               std::function<void(BytecodeEmitter *, uint8_t, uint8_t)> emit, bool is_div = false)
{
    std::ostringstream ss;
    ss << "Test " << mnemonic << " with sizeof(T) = " << sizeof(T);
    ss << ", v1 = " << v1 << ", v2 = " << v2;

    BytecodeEmitter emitter;

    emit(&emitter, 0, 1);
    emitter.ReturnWide();

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

    bool is_arithmetic_exception_expected = is_div && v2 == 0;

    if (is_arithmetic_exception_expected) {
        RuntimeInterface::SetCatchBlockPcOffset(bytecode.size());
        emitter.ReturnObj();
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();
    }

    auto f = CreateFrame(16U, nullptr, nullptr);
    InitializeFrame(f.get());

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
    auto method = std::move(method_data.first);
    f->SetMethod(method.get());

    f->GetVReg(0).SetPrimitive(v1);
    f->GetVReg(1).SetPrimitive(v2);

    auto *thread = ManagedThread::GetCurrent();
    ObjectHeader *exception = CreateException(thread);
    if (is_arithmetic_exception_expected) {
        RuntimeInterface::SetArithmeticExceptionData({true});
        thread->SetException(exception);
    }

    Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

    if (is_arithmetic_exception_expected) {
        RuntimeInterface::SetArithmeticExceptionData({false});

        auto *curr_thread = ManagedThread::GetCurrent();
        ASSERT_FALSE(curr_thread->HasPendingException());
        ASSERT_EQ(f->GetAcc().GetReference(), exception);
    } else {
        EXPECT_EQ(f->GetAcc().GetAs<T>(), r) << ss.str();
    }
}

TEST_F(InterpreterTest, TestBinOp)
{
    constexpr size_t BITWIDTH = std::numeric_limits<uint32_t>::digits;
    constexpr int64_t I16_MAX = std::numeric_limits<int16_t>::max();

    TestBinOp<int32_t>("add", I16_MAX, 2, I16_MAX + 2,
                       [](BytecodeEmitter *emitter, uint8_t reg1, uint8_t reg2) { emitter->Add(reg1, reg2); });

    TestBinOp<int32_t>("sub", 1, 2, 1 - 2U,
                       [](BytecodeEmitter *emitter, uint8_t reg1, uint8_t reg2) { emitter->Sub(reg1, reg2); });

    TestBinOp<int32_t>("mul", I16_MAX, 3, I16_MAX * 3,
                       [](BytecodeEmitter *emitter, uint8_t reg1, uint8_t reg2) { emitter->Mul(reg1, reg2); });

    TestBinOp<int32_t>("and", 0xaabbccdd, 0xffff, 0xccdd,
                       [](BytecodeEmitter *emitter, uint8_t reg1, uint8_t reg2) { emitter->And(reg1, reg2); });

    TestBinOp<int32_t>("or", 0xaabbccdd, 0xffff, 0xaabbffff,
                       [](BytecodeEmitter *emitter, uint8_t reg1, uint8_t reg2) { emitter->Or(reg1, reg2); });

    TestBinOp<int32_t>("xor", 0xaabbccdd, 0xffffffff, 0xaabbccdd ^ 0xffffffff,
                       [](BytecodeEmitter *emitter, uint8_t reg1, uint8_t reg2) { emitter->Xor(reg1, reg2); });

    TestBinOp<int32_t>("shl", 0xaabbccdd, 16, 0xccdd0000,
                       [](BytecodeEmitter *emitter, uint8_t reg1, uint8_t reg2) { emitter->Shl(reg1, reg2); });

    TestBinOp<int32_t>("shl", 0xaabbccdd, BITWIDTH + 16, 0xccdd0000,
                       [](BytecodeEmitter *emitter, uint8_t reg1, uint8_t reg2) { emitter->Shl(reg1, reg2); });

    TestBinOp<int32_t>("shr", 0xaabbccdd, 16, 0xaabb,
                       [](BytecodeEmitter *emitter, uint8_t reg1, uint8_t reg2) { emitter->Shr(reg1, reg2); });

    TestBinOp<int32_t>("shr", 0xaabbccdd, BITWIDTH + 16, 0xaabb,
                       [](BytecodeEmitter *emitter, uint8_t reg1, uint8_t reg2) { emitter->Shr(reg1, reg2); });

    TestBinOp<int32_t>("ashr", 0xaabbccdd, 16, 0xffffaabb,
                       [](BytecodeEmitter *emitter, uint8_t reg1, uint8_t reg2) { emitter->Ashr(reg1, reg2); });

    TestBinOp<int32_t>("ashr", 0xaabbccdd, BITWIDTH + 16, 0xffffaabb,
                       [](BytecodeEmitter *emitter, uint8_t reg1, uint8_t reg2) { emitter->Ashr(reg1, reg2); });

    TestBinOp<int32_t>(
        "div", 0xabbccdd, 16, 0xabbccd,
        [](BytecodeEmitter *emitter, uint8_t reg1, uint8_t reg2) { emitter->Div(reg1, reg2); }, true);

    TestBinOp<int32_t>(
        "div", 0xabbccdd, 0, 0, [](BytecodeEmitter *emitter, uint8_t reg1, uint8_t reg2) { emitter->Div(reg1, reg2); },
        true);

    TestBinOp<int32_t>(
        "mod", 0xabbccdd, 16, 0xd,
        [](BytecodeEmitter *emitter, uint8_t reg1, uint8_t reg2) { emitter->Mod(reg1, reg2); }, true);

    TestBinOp<int32_t>(
        "mod", 0xabbccdd, 0, 0, [](BytecodeEmitter *emitter, uint8_t reg1, uint8_t reg2) { emitter->Mod(reg1, reg2); },
        true);
}

void TestBinOpImm(const std::string &mnemonic, int32_t v1, int8_t v2, int32_t r,
                  std::function<void(BytecodeEmitter *, int8_t)> emit, bool is_div = false)
{
    std::ostringstream ss;
    ss << "Test " << mnemonic << " with v1 = " << v1 << ", v2 = " << static_cast<int32_t>(v2);

    BytecodeEmitter emitter;

    emit(&emitter, v2);
    emitter.ReturnWide();

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

    bool is_arithmetic_exception_expected = is_div && v2 == 0;

    if (is_arithmetic_exception_expected) {
        RuntimeInterface::SetCatchBlockPcOffset(bytecode.size());
        emitter.ReturnObj();
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();
    }

    auto f = CreateFrame(16U, nullptr, nullptr);
    InitializeFrame(f.get());

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
    auto method = std::move(method_data.first);
    f->SetMethod(method.get());

    f->GetAcc().SetPrimitive(v1);

    auto *thread = ManagedThread::GetCurrent();
    ObjectHeader *exception = CreateException(thread);
    if (is_arithmetic_exception_expected) {
        RuntimeInterface::SetArithmeticExceptionData({true});
        thread->SetException(exception);
    }

    Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

    if (is_arithmetic_exception_expected) {
        RuntimeInterface::SetArithmeticExceptionData({false});

        auto *curr_thread = ManagedThread::GetCurrent();
        ASSERT_FALSE(curr_thread->HasPendingException()) << ss.str();
        ASSERT_EQ(f->GetAcc().GetReference(), exception) << ss.str();
    } else {
        EXPECT_EQ(f->GetAcc().Get(), r) << ss.str();
    }
}

TEST_F(InterpreterTest, TestBinOpImm)
{
    constexpr size_t BITWIDTH = std::numeric_limits<uint32_t>::digits;
    constexpr int64_t I16_MAX = std::numeric_limits<int16_t>::max();

    TestBinOpImm("addi", I16_MAX, 2, I16_MAX + 2, [](BytecodeEmitter *emitter, int8_t imm) { emitter->Addi(imm); });

    TestBinOpImm("subi", 1, 2, 1 - 2U, [](BytecodeEmitter *emitter, int8_t imm) { emitter->Subi(imm); });

    TestBinOpImm("muli", I16_MAX, 3, I16_MAX * 3, [](BytecodeEmitter *emitter, int8_t imm) { emitter->Muli(imm); });

    TestBinOpImm("andi", 0xaabbccdd, 0xf, 0xd, [](BytecodeEmitter *emitter, int8_t imm) { emitter->Andi(imm); });

    TestBinOpImm("ori", 0xaabbccdd, 0xf, 0xaabbccdf, [](BytecodeEmitter *emitter, int8_t imm) { emitter->Ori(imm); });

    TestBinOpImm("xori", 0xaabbccdd, 0xf, 0xaabbccdd ^ 0xf,
                 [](BytecodeEmitter *emitter, int8_t imm) { emitter->Xori(imm); });

    TestBinOpImm("shli", 0xaabbccdd, 16, 0xccdd0000, [](BytecodeEmitter *emitter, int8_t imm) { emitter->Shli(imm); });

    TestBinOpImm("shli", 0xaabbccdd, BITWIDTH + 16, 0xccdd0000,
                 [](BytecodeEmitter *emitter, int8_t imm) { emitter->Shli(imm); });

    TestBinOpImm("shri", 0xaabbccdd, 16, 0xaabb, [](BytecodeEmitter *emitter, int8_t imm) { emitter->Shri(imm); });

    TestBinOpImm("shri", 0xaabbccdd, BITWIDTH + 16, 0xaabb,
                 [](BytecodeEmitter *emitter, int8_t imm) { emitter->Shri(imm); });

    TestBinOpImm("ashri", 0xaabbccdd, 16, 0xffffaabb,
                 [](BytecodeEmitter *emitter, int8_t imm) { emitter->Ashri(imm); });

    TestBinOpImm("ashri", 0xaabbccdd, BITWIDTH + 16, 0xffffaabb,
                 [](BytecodeEmitter *emitter, int8_t imm) { emitter->Ashri(imm); });

    TestBinOpImm(
        "divi", 0xabbccdd, 16, 0xabbccd, [](BytecodeEmitter *emitter, int8_t imm) { emitter->Divi(imm); }, true);

    TestBinOpImm(
        "divi", 0xabbccdd, 0, 0, [](BytecodeEmitter *emitter, int8_t imm) { emitter->Divi(imm); }, true);

    TestBinOpImm(
        "modi", 0xabbccdd, 16, 0xd, [](BytecodeEmitter *emitter, int8_t imm) { emitter->Modi(imm); }, true);

    TestBinOpImm(
        "modi", 0xabbccdd, 0, 0, [](BytecodeEmitter *emitter, int8_t imm) { emitter->Modi(imm); }, true);
}

template <class T, class R>
void TestUnaryOp(const std::string &mnemonic, T v, R r, std::function<void(BytecodeEmitter *)> emit)
{
    std::ostringstream ss;
    ss << "Test " << mnemonic << " with v = " << v;

    BytecodeEmitter emitter;

    emit(&emitter);
    emitter.ReturnWide();

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

    auto f = CreateFrame(16U, nullptr, nullptr);
    InitializeFrame(f.get());

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
    auto method = std::move(method_data.first);
    f->SetMethod(method.get());

    f->GetAcc().SetPrimitive(v);

    Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

    EXPECT_EQ(f->GetAcc().GetAs<R>(), r) << ss.str();
}

TEST_F(InterpreterTest, TestUnaryOp)
{
    constexpr int64_t I32_MIN = std::numeric_limits<int32_t>::min();
    constexpr int64_t I64_MIN = std::numeric_limits<int64_t>::min();

    TestUnaryOp<int64_t, int64_t>("neg", I64_MIN + 1, -(I64_MIN + 1),
                                  [](BytecodeEmitter *emitter) { emitter->NegWide(); });

    TestUnaryOp<int32_t, int64_t>("neg", I32_MIN + 1, -(I32_MIN + 1), [](BytecodeEmitter *emitter) { emitter->Neg(); });

    TestUnaryOp<double, double>("fneg", 1.0, -1.0, [](BytecodeEmitter *emitter) { emitter->FnegWide(); });

    TestUnaryOp<int64_t, int64_t>("not", 0, 0xffffffffffffffff, [](BytecodeEmitter *emitter) { emitter->NotWide(); });

    TestUnaryOp<int32_t, int32_t>("not", 0, 0xffffffff, [](BytecodeEmitter *emitter) { emitter->Not(); });
}

TEST_F(InterpreterTest, TestInci)
{
    BytecodeEmitter emitter;
    constexpr int32_t R0_VALUE = 2;
    constexpr int32_t R1_VALUE = -3;
    emitter.Inci(0, R0_VALUE);
    emitter.Inci(1, R1_VALUE);
    emitter.ReturnWide();

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

    auto f = CreateFrame(16U, nullptr, nullptr);
    InitializeFrame(f.get());

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
    auto method = std::move(method_data.first);
    f->SetMethod(method.get());

    f->GetVReg(0).SetPrimitive(-R0_VALUE);
    f->GetVReg(1).SetPrimitive(-R1_VALUE);

    Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

    EXPECT_EQ(f->GetVReg(0).GetAs<int32_t>(), 0);
    EXPECT_EQ(f->GetVReg(1).GetAs<int32_t>(), 0);
}

TEST_F(InterpreterTest, TestCast)
{
    constexpr int64_t I64_MAX = std::numeric_limits<int64_t>::max();
    constexpr int32_t I32_MAX = std::numeric_limits<int32_t>::max();
    constexpr int64_t I64_MIN = std::numeric_limits<int64_t>::min();
    constexpr int32_t I32_MIN = std::numeric_limits<int32_t>::min();

    constexpr double F64_MAX = std::numeric_limits<double>::max();
    constexpr double F64_PINF = std::numeric_limits<double>::infinity();
    constexpr double F64_NINF = -F64_PINF;

    double f64 = 64.0;

    TestUnaryOp("i32toi64", I32_MAX, static_cast<int64_t>(I32_MAX),
                [](BytecodeEmitter *emitter) { emitter->I32toi64(); });

    TestUnaryOp("i32tof64", I32_MAX, static_cast<double>(I32_MAX),
                [](BytecodeEmitter *emitter) { emitter->I32tof64(); });

    TestUnaryOp("i64toi32", I64_MAX, static_cast<int32_t>(I64_MAX),
                [](BytecodeEmitter *emitter) { emitter->I64toi32(); });

    TestUnaryOp("i64tof64", I64_MAX, static_cast<double>(I64_MAX),
                [](BytecodeEmitter *emitter) { emitter->I64tof64(); });

    TestUnaryOp("F64toi32", F64_MAX, I32_MAX, [](BytecodeEmitter *emitter) { emitter->F64toi32(); });
    TestUnaryOp("F64toi32", F64_PINF, I32_MAX, [](BytecodeEmitter *emitter) { emitter->F64toi32(); });
    TestUnaryOp("F64toi32", -F64_MAX, I32_MIN, [](BytecodeEmitter *emitter) { emitter->F64toi32(); });
    TestUnaryOp("F64toi32", F64_NINF, I32_MIN, [](BytecodeEmitter *emitter) { emitter->F64toi32(); });
    TestUnaryOp("F64toi32", nan(""), 0, [](BytecodeEmitter *emitter) { emitter->F64toi32(); });
    TestUnaryOp("F64toi32", f64, static_cast<int32_t>(f64), [](BytecodeEmitter *emitter) { emitter->F64toi32(); });

    TestUnaryOp("F64toi64", F64_MAX, I64_MAX, [](BytecodeEmitter *emitter) { emitter->F64toi64(); });
    TestUnaryOp("F64toi64", F64_PINF, I64_MAX, [](BytecodeEmitter *emitter) { emitter->F64toi64(); });
    TestUnaryOp("F64toi64", -F64_MAX, I64_MIN, [](BytecodeEmitter *emitter) { emitter->F64toi64(); });
    TestUnaryOp("F64toi64", F64_NINF, I64_MIN, [](BytecodeEmitter *emitter) { emitter->F64toi64(); });
    TestUnaryOp("F64toi64", nan(""), 0, [](BytecodeEmitter *emitter) { emitter->F64toi64(); });
    TestUnaryOp("F64toi64", f64, static_cast<int64_t>(f64), [](BytecodeEmitter *emitter) { emitter->F64toi64(); });
}

// clang-format off

template <panda_file::Type::TypeId type_id>
struct ArrayComponentTypeHelper {
    using type = std::conditional_t<type_id == panda_file::Type::TypeId::U1, uint8_t,
                 std::conditional_t<type_id == panda_file::Type::TypeId::I8, int8_t,
                 std::conditional_t<type_id == panda_file::Type::TypeId::U8, uint8_t,
                 std::conditional_t<type_id == panda_file::Type::TypeId::I16, int16_t,
                 std::conditional_t<type_id == panda_file::Type::TypeId::U16, uint16_t,
                 std::conditional_t<type_id == panda_file::Type::TypeId::I32, int32_t,
                 std::conditional_t<type_id == panda_file::Type::TypeId::U32, uint32_t,
                 std::conditional_t<type_id == panda_file::Type::TypeId::I64, int64_t,
                 std::conditional_t<type_id == panda_file::Type::TypeId::U64, uint64_t,
                 std::conditional_t<type_id == panda_file::Type::TypeId::F32, float,
                 std::conditional_t<type_id == panda_file::Type::TypeId::F64, double,
                 std::conditional_t<type_id == panda_file::Type::TypeId::REFERENCE, ObjectHeader*, void>>>>>>>>>>>>;
};

// clang-format on

template <panda_file::Type::TypeId type_id>
using ArrayComponentTypeHelperT = typename ArrayComponentTypeHelper<type_id>::type;

template <panda_file::Type::TypeId type_id>
struct ArrayStoredTypeHelperT {
    using type = typename ArrayComponentTypeHelper<type_id>::type;
};

template <>
struct ArrayStoredTypeHelperT<panda_file::Type::TypeId::REFERENCE> {
    using type = object_pointer_type;
};

template <panda_file::Type::TypeId type_id>
typename ArrayStoredTypeHelperT<type_id>::type CastIfRef(ArrayComponentTypeHelperT<type_id> value)
{
    if constexpr (type_id == panda_file::Type::TypeId::REFERENCE) {
        return static_cast<object_pointer_type>(reinterpret_cast<uintptr_t>(value));
    } else {
        return value;
    }
}

coretypes::Array *AllocArray(Class *cls, [[maybe_unused]] size_t elem_size, size_t length)
{
    return coretypes::Array::Create(cls, length);
}

ObjectHeader *AllocObject(Class *cls)
{
    return ObjectHeader::Create(cls);
}

template <class T>
static T GetStoreValue([[maybe_unused]] Class *cls)
{
    if constexpr (std::is_same_v<T, ObjectHeader *>) {
        return AllocObject(cls);
    }

    return std::numeric_limits<T>::max();
}

template <class T>
static T GetLoadValue([[maybe_unused]] Class *cls)
{
    if constexpr (std::is_same_v<T, ObjectHeader *>) {
        return AllocObject(cls);
    }

    return std::numeric_limits<T>::min() + 1;
}

PandaString GetArrayClassName(panda_file::Type::TypeId component_type_id)
{
    PandaString descriptor;

    if (component_type_id == panda_file::Type::TypeId::REFERENCE) {
        ClassHelper::GetArrayDescriptor(utf::CStringAsMutf8("panda.Object"), 1, &descriptor);
        return descriptor;
    }

    ClassHelper::GetPrimitiveArrayDescriptor(panda_file::Type(component_type_id), 1, &descriptor);
    return descriptor;
}

template <panda_file::Type::TypeId component_type_id>
static void TestArray()
{
    std::ostringstream ss;
    ss << "Test with component type id " << static_cast<uint32_t>(component_type_id);

    using component_type = ArrayComponentTypeHelperT<component_type_id>;
    using stored_type = typename ArrayStoredTypeHelperT<component_type_id>::type;

    BytecodeEmitter emitter;

    constexpr int64_t ARRAY_LENGTH = 10;
    constexpr size_t STORE_IDX = ARRAY_LENGTH - 1;
    constexpr size_t LOAD_IDX = 0;

    auto class_linker = CreateClassLinker(ManagedThread::GetCurrent());
    ASSERT_NE(class_linker, nullptr) << ss.str();

    auto ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
    PandaString array_class_name = GetArrayClassName(component_type_id);
    Class *array_class = class_linker->GetExtension(ctx)->GetClass(utf::CStringAsMutf8(array_class_name.c_str()));
    Class *elem_class = array_class->GetComponentType();

    const component_type STORE_VALUE = GetStoreValue<component_type>(elem_class);
    const component_type LOAD_VALUE = GetLoadValue<component_type>(elem_class);

    emitter.Movi(0, ARRAY_LENGTH);
    emitter.Newarr(1, 0, RuntimeInterface::TYPE_ID.AsIndex());

    if constexpr (component_type_id == panda_file::Type::TypeId::REFERENCE) {
        emitter.LdaObj(4);
    } else if constexpr (component_type_id == panda_file::Type::TypeId::F32) {
        emitter.FldaiWide(bit_cast<int64_t>(static_cast<double>(STORE_VALUE)));
    } else if constexpr (component_type_id == panda_file::Type::TypeId::F64) {
        emitter.FldaiWide(bit_cast<int64_t>(STORE_VALUE));
    } else {
        emitter.LdaiWide(static_cast<int64_t>(STORE_VALUE));
    }

    emitter.Movi(2U, STORE_IDX);

    if constexpr (component_type_id != panda_file::Type::TypeId::REFERENCE) {
        switch (component_type_id) {
            case panda_file::Type::TypeId::U1:
            case panda_file::Type::TypeId::U8: {
                emitter.Starr8(1, 2U);
                emitter.Ldai(LOAD_IDX);
                emitter.Ldarru8(1);
                break;
            }
            case panda_file::Type::TypeId::I8: {
                emitter.Starr8(1, 2U);
                emitter.Ldai(LOAD_IDX);
                emitter.Ldarr8(1);
                break;
            }
            case panda_file::Type::TypeId::U16: {
                emitter.Starr16(1, 2U);
                emitter.Ldai(LOAD_IDX);
                emitter.Ldarru16(1);
                break;
            }
            case panda_file::Type::TypeId::I16: {
                emitter.Starr16(1, 2U);
                emitter.Ldai(LOAD_IDX);
                emitter.Ldarr16(1);
                break;
            }
            case panda_file::Type::TypeId::U32:
            case panda_file::Type::TypeId::I32: {
                emitter.Starr(1, 2U);
                emitter.Ldai(LOAD_IDX);
                emitter.Ldarr(1);
                break;
            }
            case panda_file::Type::TypeId::U64:
            case panda_file::Type::TypeId::I64: {
                emitter.StarrWide(1, 2U);
                emitter.Ldai(LOAD_IDX);
                emitter.LdarrWide(1);
                break;
            }
            case panda_file::Type::TypeId::F32: {
                emitter.Fstarr32(1, 2U);
                emitter.Ldai(LOAD_IDX);
                emitter.Fldarr32(1);
                break;
            }
            case panda_file::Type::TypeId::F64: {
                emitter.FstarrWide(1, 2U);
                emitter.Ldai(LOAD_IDX);
                emitter.FldarrWide(1);
                break;
            }
            default: {
                UNREACHABLE();
                break;
            }
        }
    } else {
        emitter.StarrObj(1, 2U);
        emitter.Ldai(LOAD_IDX);
        emitter.LdarrObj(1);
    }

    if constexpr (component_type_id != panda_file::Type::TypeId::REFERENCE) {
        emitter.StaWide(3U);
    } else {
        emitter.StaObj(3U);
    }

    emitter.Lenarr(1);
    emitter.Return();

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

    auto f = CreateFrame(16U, nullptr, nullptr);
    InitializeFrame(f.get());

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
    auto method = std::move(method_data.first);
    f->SetMethod(method.get());

    if constexpr (component_type_id == panda_file::Type::TypeId::REFERENCE) {
        f->GetVReg(4U).SetReference(STORE_VALUE);
    }

    coretypes::Array *array = AllocArray(array_class, sizeof(stored_type), ARRAY_LENGTH);
    array->Set<component_type>(LOAD_IDX, LOAD_VALUE);

    RuntimeInterface::SetupResolvedClass(array_class);
    RuntimeInterface::SetupArrayClass(array_class);
    RuntimeInterface::SetupArrayLength(ARRAY_LENGTH);
    RuntimeInterface::SetupArrayObject(array);

    Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

    RuntimeInterface::SetupResolvedClass(nullptr);
    RuntimeInterface::SetupArrayClass(nullptr);
    RuntimeInterface::SetupArrayObject(nullptr);

    ASSERT_EQ(f->GetAcc().Get(), ARRAY_LENGTH) << ss.str();

    auto *result = static_cast<coretypes::Array *>(f->GetVReg(1).GetReference());
    EXPECT_EQ(result, array) << ss.str();

    EXPECT_EQ(f->GetVReg(3U).GetAs<component_type>(), LOAD_VALUE) << ss.str();

    std::vector<stored_type> data(ARRAY_LENGTH);
    data[LOAD_IDX] = CastIfRef<component_type_id>(LOAD_VALUE);
    data[STORE_IDX] = CastIfRef<component_type_id>(STORE_VALUE);

    EXPECT_THAT(data, ::testing::ElementsAreArray(reinterpret_cast<stored_type *>(array->GetData()), ARRAY_LENGTH))
        << ss.str();
}

TEST_F(InterpreterTest, TestArray)
{
    TestArray<panda_file::Type::TypeId::U1>();
    TestArray<panda_file::Type::TypeId::I8>();
    TestArray<panda_file::Type::TypeId::U8>();
    TestArray<panda_file::Type::TypeId::I16>();
    TestArray<panda_file::Type::TypeId::U16>();
    TestArray<panda_file::Type::TypeId::I32>();
    TestArray<panda_file::Type::TypeId::U32>();
    TestArray<panda_file::Type::TypeId::I64>();
    TestArray<panda_file::Type::TypeId::U64>();
    TestArray<panda_file::Type::TypeId::F32>();
    TestArray<panda_file::Type::TypeId::F64>();
    TestArray<panda_file::Type::TypeId::REFERENCE>();
}

void TestNewArrayExceptions()
{
    // Test with negative size
    {
        BytecodeEmitter emitter;

        emitter.Movi(0, -1);
        emitter.Newarr(0, 0, RuntimeInterface::TYPE_ID.AsIndex());
        emitter.Movi(0, 0);
        emitter.Return();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        RuntimeInterface::SetCatchBlockPcOffset(bytecode.size());

        emitter.ReturnObj();

        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        RuntimeInterface::SetNegativeArraySizeExceptionData({true, -1});

        auto *thread = ManagedThread::GetCurrent();
        ObjectHeader *exception = CreateException(thread);
        thread->SetException(exception);

        Execute(thread, bytecode.data(), f.get());

        RuntimeInterface::SetNegativeArraySizeExceptionData({false, 0});

        ASSERT_FALSE(thread->HasPendingException());
        ASSERT_EQ(f->GetAcc().GetReference(), exception);
    }

    // Test with zero size
    {
        BytecodeEmitter emitter;

        emitter.Movi(0, 0);
        emitter.Newarr(0, 0, RuntimeInterface::TYPE_ID.AsIndex());
        emitter.LdaObj(0);
        emitter.ReturnObj();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        auto class_linker = CreateClassLinker(ManagedThread::GetCurrent());
        ASSERT_NE(class_linker, nullptr);

        LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
        Class *array_class = class_linker->GetExtension(ctx)->GetClassRoot(ClassRoot::ARRAY_U1);
        coretypes::Array *array = AllocArray(array_class, 1, 0);

        RuntimeInterface::SetupResolvedClass(array_class);
        RuntimeInterface::SetupArrayClass(array_class);
        RuntimeInterface::SetupArrayLength(0);
        RuntimeInterface::SetupArrayObject(array);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        RuntimeInterface::SetupResolvedClass(nullptr);
        RuntimeInterface::SetupArrayClass(nullptr);
        RuntimeInterface::SetupArrayObject(nullptr);

        EXPECT_EQ(array, f->GetAcc().GetReference());
    }
}

template <panda_file::Type::TypeId component_type_id>
void TestLoadArrayExceptions()
{
    std::ostringstream ss;
    ss << "Test with component type id " << static_cast<uint32_t>(component_type_id);

    using component_type = ArrayComponentTypeHelperT<component_type_id>;

    constexpr int32_t ARRAY_LENGTH = 10;

    // Test NullPointerException

    {
        BytecodeEmitter emitter;

        if constexpr (component_type_id != panda_file::Type::TypeId::REFERENCE) {
            switch (sizeof(component_type)) {
                case sizeof(uint8_t): {
                    emitter.Ldarr8(0);
                    break;
                }
                case sizeof(uint16_t): {
                    emitter.Ldarr16(0);
                    break;
                }
                case sizeof(uint32_t): {
                    emitter.Ldarr(0);
                    break;
                }
                case sizeof(uint64_t): {
                    emitter.LdarrWide(0);
                    break;
                }
                default: {
                    UNREACHABLE();
                    break;
                }
            }
        } else {
            emitter.LdarrObj(0);
        }

        emitter.ReturnVoid();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

        RuntimeInterface::SetCatchBlockPcOffset(bytecode.size());

        emitter.ReturnObj();

        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        f->GetVReg(0).SetReference(nullptr);
        f->GetAcc().SetPrimitive(-1);

        RuntimeInterface::SetNullPointerExceptionData({true});

        auto *thread = ManagedThread::GetCurrent();
        ObjectHeader *exception = CreateException(thread);
        thread->SetException(exception);

        Execute(thread, bytecode.data(), f.get());

        RuntimeInterface::SetNullPointerExceptionData({false});

        ASSERT_FALSE(thread->HasPendingException()) << ss.str();
        ASSERT_EQ(f->GetAcc().GetReference(), exception) << ss.str();
    }

    // Test OOB exception

    {
        BytecodeEmitter emitter;

        if constexpr (component_type_id != panda_file::Type::TypeId::REFERENCE) {
            switch (sizeof(component_type)) {
                case sizeof(uint8_t): {
                    emitter.Ldarr8(0);
                    break;
                }
                case sizeof(uint16_t): {
                    emitter.Ldarr16(0);
                    break;
                }
                case sizeof(uint32_t): {
                    emitter.Ldarr(0);
                    break;
                }
                case sizeof(uint64_t): {
                    emitter.LdarrWide(0);
                    break;
                }
                default: {
                    UNREACHABLE();
                    break;
                }
            }
        } else {
            emitter.LdarrObj(0);
        }

        emitter.ReturnVoid();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

        RuntimeInterface::SetCatchBlockPcOffset(bytecode.size());

        emitter.ReturnObj();

        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        auto class_linker = CreateClassLinker(ManagedThread::GetCurrent());
        ASSERT_NE(class_linker, nullptr) << ss.str();

        PandaString array_class_name = GetArrayClassName(component_type_id);
        auto ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
        Class *array_class = class_linker->GetExtension(ctx)->GetClass(utf::CStringAsMutf8(array_class_name.c_str()));
        coretypes::Array *array = AllocArray(array_class, sizeof(component_type), ARRAY_LENGTH);

        f->GetVReg(0).SetReference(array);
        f->GetAcc().SetPrimitive(-1);

        RuntimeInterface::SetArrayIndexOutOfBoundsExceptionData({true, -1, ARRAY_LENGTH});

        RuntimeInterface::SetupResolvedClass(array_class);
        RuntimeInterface::SetupArrayClass(array_class);
        RuntimeInterface::SetupArrayLength(ARRAY_LENGTH);
        RuntimeInterface::SetupArrayObject(array);

        auto *thread = ManagedThread::GetCurrent();
        ObjectHeader *exception = CreateException(thread);
        thread->SetException(exception);

        Execute(thread, bytecode.data(), f.get());

        RuntimeInterface::SetupResolvedClass(nullptr);
        RuntimeInterface::SetupArrayClass(nullptr);
        RuntimeInterface::SetupArrayObject(nullptr);

        RuntimeInterface::SetArrayIndexOutOfBoundsExceptionData({false, 0, 0});

        ASSERT_FALSE(thread->HasPendingException()) << ss.str();
        ASSERT_EQ(f->GetAcc().GetReference(), exception) << ss.str();
    }

    {
        BytecodeEmitter emitter;

        if constexpr (component_type_id != panda_file::Type::TypeId::REFERENCE) {
            switch (sizeof(component_type)) {
                case sizeof(uint8_t): {
                    emitter.Ldarr8(0);
                    break;
                }
                case sizeof(uint16_t): {
                    emitter.Ldarr16(0);
                    break;
                }
                case sizeof(uint32_t): {
                    emitter.Ldarr(0);
                    break;
                }
                case sizeof(uint64_t): {
                    emitter.LdarrWide(0);
                    break;
                }
                default: {
                    UNREACHABLE();
                    break;
                }
            }
        } else {
            emitter.LdarrObj(0);
        }

        emitter.ReturnVoid();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

        RuntimeInterface::SetCatchBlockPcOffset(bytecode.size());

        emitter.ReturnObj();

        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        auto class_linker = CreateClassLinker(ManagedThread::GetCurrent());
        ASSERT_NE(class_linker, nullptr) << ss.str();

        PandaString array_class_name = GetArrayClassName(component_type_id);
        auto ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
        Class *array_class = class_linker->GetExtension(ctx)->GetClass(utf::CStringAsMutf8(array_class_name.c_str()));
        coretypes::Array *array = AllocArray(array_class, sizeof(component_type), ARRAY_LENGTH);

        f->GetVReg(0).SetReference(array);
        f->GetAcc().SetPrimitive(ARRAY_LENGTH);

        RuntimeInterface::SetArrayIndexOutOfBoundsExceptionData({true, ARRAY_LENGTH, ARRAY_LENGTH});

        RuntimeInterface::SetupResolvedClass(array_class);
        RuntimeInterface::SetupArrayClass(array_class);
        RuntimeInterface::SetupArrayLength(ARRAY_LENGTH);
        RuntimeInterface::SetupArrayObject(array);

        auto *thread = ManagedThread::GetCurrent();
        ObjectHeader *exception = CreateException(thread);
        thread->SetException(exception);

        Execute(thread, bytecode.data(), f.get());

        RuntimeInterface::SetupResolvedClass(nullptr);
        RuntimeInterface::SetupArrayClass(nullptr);
        RuntimeInterface::SetupArrayObject(nullptr);

        RuntimeInterface::SetArrayIndexOutOfBoundsExceptionData({false, 0, 0});

        ASSERT_FALSE(thread->HasPendingException()) << ss.str();
        ASSERT_EQ(f->GetAcc().GetReference(), exception) << ss.str();
    }
}

template <panda_file::Type::TypeId component_type_id>
void TestStoreArrayExceptions()
{
    std::ostringstream ss;
    ss << "Test with component type id " << static_cast<uint32_t>(component_type_id);

    using component_type = ArrayComponentTypeHelperT<component_type_id>;

    constexpr int32_t ARRAY_LENGTH = 10;

    auto class_linker = CreateClassLinker(ManagedThread::GetCurrent());
    ASSERT_NE(class_linker, nullptr) << ss.str();

    PandaString array_class_name = GetArrayClassName(component_type_id);
    auto ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
    Class *array_class = class_linker->GetExtension(ctx)->GetClass(utf::CStringAsMutf8(array_class_name.c_str()));
    Class *elem_class = array_class->GetComponentType();

    const component_type STORE_VALUE = GetStoreValue<component_type>(elem_class);

    // Test NullPointerException

    {
        BytecodeEmitter emitter;

        if constexpr (component_type_id != panda_file::Type::TypeId::REFERENCE) {
            switch (sizeof(component_type)) {
                case sizeof(uint8_t): {
                    emitter.Starr8(0, 1);
                    break;
                }
                case sizeof(uint16_t): {
                    emitter.Starr16(0, 1);
                    break;
                }
                case sizeof(uint32_t): {
                    emitter.Starr(0, 1);
                    break;
                }
                case sizeof(uint64_t): {
                    emitter.StarrWide(0, 1);
                    break;
                }
                default: {
                    UNREACHABLE();
                    break;
                }
            }
        } else {
            emitter.StarrObj(0, 1);
        }

        emitter.ReturnVoid();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

        RuntimeInterface::SetCatchBlockPcOffset(bytecode.size());

        emitter.ReturnObj();

        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        if constexpr (component_type_id != panda_file::Type::TypeId::REFERENCE) {
            f->GetAcc().SetPrimitive(STORE_VALUE);
        } else {
            f->GetAcc().SetReference(STORE_VALUE);
        }
        f->GetVReg(0).SetReference(nullptr);
        f->GetVReg(1).SetPrimitive(-1);

        RuntimeInterface::SetNullPointerExceptionData({true});

        auto *thread = ManagedThread::GetCurrent();
        ObjectHeader *exception = CreateException(thread);
        thread->SetException(exception);

        Execute(thread, bytecode.data(), f.get());

        RuntimeInterface::SetNullPointerExceptionData({false});

        ASSERT_FALSE(thread->HasPendingException()) << ss.str();
        ASSERT_EQ(f->GetAcc().GetReference(), exception) << ss.str();
    }

    // Test OOB exception

    {
        BytecodeEmitter emitter;

        if constexpr (component_type_id != panda_file::Type::TypeId::REFERENCE) {
            switch (sizeof(component_type)) {
                case sizeof(uint8_t): {
                    emitter.Starr8(0, 1);
                    break;
                }
                case sizeof(uint16_t): {
                    emitter.Starr16(0, 1);
                    break;
                }
                case sizeof(uint32_t): {
                    emitter.Starr(0, 1);
                    break;
                }
                case sizeof(uint64_t): {
                    emitter.StarrWide(0, 1);
                    break;
                }
                default: {
                    UNREACHABLE();
                    break;
                }
            }
        } else {
            emitter.StarrObj(0, 1);
        }

        emitter.ReturnVoid();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

        RuntimeInterface::SetCatchBlockPcOffset(bytecode.size());

        emitter.ReturnObj();

        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        coretypes::Array *array = AllocArray(array_class, sizeof(component_type), ARRAY_LENGTH);

        if constexpr (component_type_id != panda_file::Type::TypeId::REFERENCE) {
            f->GetAcc().SetPrimitive(STORE_VALUE);
        } else {
            f->GetAcc().SetReference(STORE_VALUE);
        }
        f->GetVReg(0).SetReference(array);
        f->GetVReg(1).SetPrimitive(-1);

        RuntimeInterface::SetArrayIndexOutOfBoundsExceptionData({true, -1, ARRAY_LENGTH});

        RuntimeInterface::SetupResolvedClass(array_class);
        RuntimeInterface::SetupArrayClass(array_class);
        RuntimeInterface::SetupArrayLength(ARRAY_LENGTH);
        RuntimeInterface::SetupArrayObject(array);

        auto *thread = ManagedThread::GetCurrent();
        ObjectHeader *exception = CreateException(thread);
        thread->SetException(exception);

        Execute(thread, bytecode.data(), f.get());

        RuntimeInterface::SetupResolvedClass(nullptr);
        RuntimeInterface::SetupArrayClass(nullptr);
        RuntimeInterface::SetupArrayObject(nullptr);

        RuntimeInterface::SetArrayIndexOutOfBoundsExceptionData({false, 0, 0});

        ASSERT_FALSE(thread->HasPendingException()) << ss.str();
        ASSERT_EQ(f->GetAcc().GetReference(), exception) << ss.str();
    }

    {
        BytecodeEmitter emitter;

        if constexpr (component_type_id != panda_file::Type::TypeId::REFERENCE) {
            switch (sizeof(component_type)) {
                case sizeof(uint8_t): {
                    emitter.Starr8(0, 1);
                    break;
                }
                case sizeof(uint16_t): {
                    emitter.Starr16(0, 1);
                    break;
                }
                case sizeof(uint32_t): {
                    emitter.Starr(0, 1);
                    break;
                }
                case sizeof(uint64_t): {
                    emitter.StarrWide(0, 1);
                    break;
                }
                default: {
                    UNREACHABLE();
                    break;
                }
            }
        } else {
            emitter.StarrObj(0, 1);
        }

        emitter.ReturnVoid();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

        RuntimeInterface::SetCatchBlockPcOffset(bytecode.size());

        emitter.ReturnObj();

        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        coretypes::Array *array = AllocArray(array_class, sizeof(component_type), ARRAY_LENGTH);

        if constexpr (component_type_id != panda_file::Type::TypeId::REFERENCE) {
            f->GetAcc().SetPrimitive(STORE_VALUE);
        } else {
            f->GetAcc().SetReference(STORE_VALUE);
        }
        f->GetVReg(0).SetReference(array);
        f->GetVReg(1).SetPrimitive(ARRAY_LENGTH);

        RuntimeInterface::SetArrayIndexOutOfBoundsExceptionData({true, ARRAY_LENGTH, ARRAY_LENGTH});

        RuntimeInterface::SetupResolvedClass(array_class);
        RuntimeInterface::SetupArrayClass(array_class);
        RuntimeInterface::SetupArrayLength(ARRAY_LENGTH);
        RuntimeInterface::SetupArrayObject(array);

        auto *thread = ManagedThread::GetCurrent();
        ObjectHeader *exception = CreateException(thread);
        thread->SetException(exception);

        Execute(thread, bytecode.data(), f.get());

        RuntimeInterface::SetupResolvedClass(nullptr);
        RuntimeInterface::SetupArrayClass(nullptr);
        RuntimeInterface::SetupArrayObject(nullptr);

        RuntimeInterface::SetArrayIndexOutOfBoundsExceptionData({false, 0, 0});

        ASSERT_FALSE(thread->HasPendingException()) << ss.str();
        ASSERT_EQ(f->GetAcc().GetReference(), exception) << ss.str();
    }
}

void TestArrayLenException()
{
    // Test NullPointerException

    BytecodeEmitter emitter;

    emitter.Lenarr(0);
    emitter.Return();

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

    RuntimeInterface::SetCatchBlockPcOffset(bytecode.size());

    emitter.ReturnObj();

    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

    auto f = CreateFrame(16U, nullptr, nullptr);
    InitializeFrame(f.get());

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
    auto method = std::move(method_data.first);
    f->SetMethod(method.get());

    f->GetVReg(0).SetReference(nullptr);

    RuntimeInterface::SetNullPointerExceptionData({true});

    auto *thread = ManagedThread::GetCurrent();
    ObjectHeader *exception = CreateException(thread);
    thread->SetException(exception);

    Execute(thread, bytecode.data(), f.get());

    RuntimeInterface::SetNullPointerExceptionData({false});

    ASSERT_FALSE(thread->HasPendingException());
    ASSERT_EQ(f->GetAcc().GetReference(), exception);
}

ObjectHeader *AllocObject(BaseClass *cls)
{
    return ObjectHeader::Create(cls);
}

TEST_F(InterpreterTest, TestNewobj)
{
    BytecodeEmitter emitter;

    emitter.Newobj(0, RuntimeInterface::TYPE_ID.AsIndex());
    emitter.LdaObj(0);
    emitter.ReturnObj();

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

    auto f = CreateFrame(16U, nullptr, nullptr);
    InitializeFrame(f.get());

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
    auto method = std::move(method_data.first);
    f->SetMethod(method.get());

    pandasm::Parser p;
    auto source = R"(
        .record R {}
    )";

    auto res = p.Parse(source);
    auto class_pf = pandasm::AsmEmitter::Emit(res.Value());

    auto class_linker = CreateClassLinker(ManagedThread::GetCurrent());
    ASSERT_NE(class_linker, nullptr);

    class_linker->AddPandaFile(std::move(class_pf));

    PandaString descriptor;
    auto *thread = ManagedThread::GetCurrent();
    auto *ext = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
    Class *object_class = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("R"), &descriptor));
    ASSERT_TRUE(class_linker->InitializeClass(thread, object_class));

    ObjectHeader *obj = AllocObject(object_class);

    RuntimeInterface::SetupResolvedClass(object_class);
    RuntimeInterface::SetupObjectClass(object_class);
    RuntimeInterface::SetupObject(obj);

    Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

    RuntimeInterface::SetupResolvedClass(nullptr);
    RuntimeInterface::SetupObjectClass(nullptr);
    RuntimeInterface::SetupObject(nullptr);

    EXPECT_EQ(obj, f->GetAcc().GetReference());
}

TEST_F(InterpreterTest, TestInitobj)
{
    {
        BytecodeEmitter emitter;

        emitter.InitobjShort(0, 2, RuntimeInterface::METHOD_ID.AsIndex());
        emitter.ReturnObj();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        pandasm::Parser p;
        auto source = R"(
            .record R {}

            .function void R.ctor(R a0, i32 a1, i32 a2) <static> {
                return.void
            }
        )";

        auto res = p.Parse(source);
        auto class_pf = pandasm::AsmEmitter::Emit(res.Value());

        auto class_linker = CreateClassLinker(ManagedThread::GetCurrent());
        ASSERT_NE(class_linker, nullptr);

        class_linker->AddPandaFile(std::move(class_pf));

        PandaString descriptor;

        auto *thread = ManagedThread::GetCurrent();
        auto *ext = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
        Class *object_class = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("R"), &descriptor));
        ASSERT_TRUE(class_linker->InitializeClass(thread, object_class));

        Method *ctor = object_class->GetMethods().data();
        ObjectHeader *obj = AllocObject(object_class);

        f->GetVReg(0).Set(10);
        f->GetVReg(2U).Set(20);

        bool has_errors = false;

        RuntimeInterface::SetupInvokeMethodHandler(
            [&]([[maybe_unused]] ManagedThread *t, Method *m, Value *args) -> Value {
                if (m != ctor) {
                    has_errors = true;
                    return Value(nullptr);
                }

                Span<Value> sp(args, m->GetNumArgs());
                if (sp[0].GetAs<ObjectHeader *>() != obj) {
                    has_errors = true;
                    return Value(nullptr);
                }

                if (sp[1].GetAs<int32_t>() != f->GetVReg(0).Get()) {
                    has_errors = true;
                    return Value(nullptr);
                }

                if (sp[2].GetAs<int32_t>() != f->GetVReg(2U).Get()) {
                    has_errors = true;
                    return Value(nullptr);
                }

                return Value(nullptr);
            });

        RuntimeInterface::SetupResolvedMethod(ctor);
        RuntimeInterface::SetupResolvedClass(object_class);
        RuntimeInterface::SetupObjectClass(object_class);
        RuntimeInterface::SetupObject(obj);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        ASSERT_FALSE(has_errors);

        RuntimeInterface::SetupInvokeMethodHandler({});
        RuntimeInterface::SetupResolvedMethod(nullptr);
        RuntimeInterface::SetupResolvedClass(nullptr);
        RuntimeInterface::SetupObjectClass(nullptr);
        RuntimeInterface::SetupObject(nullptr);

        EXPECT_EQ(obj, f->GetAcc().GetReference());
    }

    {
        BytecodeEmitter emitter;

        emitter.Initobj(0, 2, 3, 5, RuntimeInterface::METHOD_ID.AsIndex());
        emitter.ReturnObj();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        pandasm::Parser p;
        auto source = R"(
            .record R {}

            .function void R.ctor(R a0, i32 a1, i32 a2, i32 a3, i32 a4) <static> {
                return.void
            }
        )";

        auto res = p.Parse(source);
        auto class_pf = pandasm::AsmEmitter::Emit(res.Value());

        auto class_linker = CreateClassLinker(ManagedThread::GetCurrent());
        ASSERT_NE(class_linker, nullptr);

        class_linker->AddPandaFile(std::move(class_pf));

        PandaString descriptor;

        auto *thread = ManagedThread::GetCurrent();
        auto *ext = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
        Class *object_class = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("R"), &descriptor));
        ASSERT_TRUE(class_linker->InitializeClass(thread, object_class));

        Method *ctor = object_class->GetMethods().data();
        ObjectHeader *obj = AllocObject(object_class);

        f->GetVReg(0).Set(10);
        f->GetVReg(2U).Set(20);
        f->GetVReg(3U).Set(30);
        f->GetVReg(5U).Set(40);

        bool has_errors = false;

        RuntimeInterface::SetupInvokeMethodHandler(
            [&]([[maybe_unused]] ManagedThread *t, Method *m, Value *args) -> Value {
                if (m != ctor) {
                    has_errors = true;
                    return Value(nullptr);
                }

                Span<Value> sp(args, m->GetNumArgs());
                if (sp[0].GetAs<ObjectHeader *>() != obj) {
                    has_errors = true;
                    return Value(nullptr);
                }

                if (sp[1].GetAs<int32_t>() != f->GetVReg(0).Get()) {
                    has_errors = true;
                    return Value(nullptr);
                }

                if (sp[2].GetAs<int32_t>() != f->GetVReg(2U).Get()) {
                    has_errors = true;
                    return Value(nullptr);
                }

                if (sp[3].GetAs<int32_t>() != f->GetVReg(3U).Get()) {
                    has_errors = true;
                    return Value(nullptr);
                }

                if (sp[4].GetAs<int32_t>() != f->GetVReg(5U).Get()) {
                    has_errors = true;
                    return Value(nullptr);
                }

                return Value(nullptr);
            });

        RuntimeInterface::SetupResolvedMethod(ctor);
        RuntimeInterface::SetupResolvedClass(object_class);
        RuntimeInterface::SetupObjectClass(object_class);
        RuntimeInterface::SetupObject(obj);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        ASSERT_FALSE(has_errors);

        RuntimeInterface::SetupInvokeMethodHandler({});
        RuntimeInterface::SetupResolvedMethod(nullptr);
        RuntimeInterface::SetupResolvedClass(nullptr);
        RuntimeInterface::SetupObjectClass(nullptr);
        RuntimeInterface::SetupObject(nullptr);

        EXPECT_EQ(obj, f->GetAcc().GetReference());
    }

    {
        BytecodeEmitter emitter;

        emitter.InitobjRange(2, RuntimeInterface::METHOD_ID.AsIndex());
        emitter.ReturnObj();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        pandasm::Parser p;
        auto source = R"(
            .record R {}

            .function void R.ctor(R a0, i32 a1, i32 a2, i32 a3, i32 a4, i32 a5) <static> {
                return.void
            }
        )";

        auto res = p.Parse(source);
        auto class_pf = pandasm::AsmEmitter::Emit(res.Value());

        auto class_linker = CreateClassLinker(ManagedThread::GetCurrent());
        ASSERT_NE(class_linker, nullptr);

        class_linker->AddPandaFile(std::move(class_pf));

        PandaString descriptor;

        auto *thread = ManagedThread::GetCurrent();
        auto *ext = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
        Class *object_class = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("R"), &descriptor));
        ASSERT_TRUE(class_linker->InitializeClass(thread, object_class));

        Method *ctor = object_class->GetMethods().data();
        ObjectHeader *obj = AllocObject(object_class);

        f->GetVReg(2U).Set(10U);
        f->GetVReg(3U).Set(20U);
        f->GetVReg(4U).Set(30U);
        f->GetVReg(5U).Set(40U);
        f->GetVReg(6U).Set(50U);

        bool has_errors = false;

        RuntimeInterface::SetupInvokeMethodHandler(
            [&]([[maybe_unused]] ManagedThread *t, Method *m, Value *args) -> Value {
                if (m != ctor) {
                    has_errors = true;
                    return Value(nullptr);
                }

                Span<Value> sp(args, m->GetNumArgs());
                if (sp[0].GetAs<ObjectHeader *>() != obj) {
                    has_errors = true;
                    return Value(nullptr);
                }

                if (sp[1].GetAs<int32_t>() != f->GetVReg(2U).Get()) {
                    has_errors = true;
                    return Value(nullptr);
                }

                if (sp[2].GetAs<int32_t>() != f->GetVReg(3U).Get()) {
                    has_errors = true;
                    return Value(nullptr);
                }

                if (sp[3].GetAs<int32_t>() != f->GetVReg(4U).Get()) {
                    has_errors = true;
                    return Value(nullptr);
                }

                if (sp[4].GetAs<int32_t>() != f->GetVReg(5U).Get()) {
                    has_errors = true;
                    return Value(nullptr);
                }

                if (sp[5].GetAs<int32_t>() != f->GetVReg(6U).Get()) {
                    has_errors = true;
                    return Value(nullptr);
                }

                return Value(nullptr);
            });

        RuntimeInterface::SetupResolvedMethod(ctor);
        RuntimeInterface::SetupResolvedClass(object_class);
        RuntimeInterface::SetupObjectClass(object_class);
        RuntimeInterface::SetupObject(obj);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        ASSERT_FALSE(has_errors);

        RuntimeInterface::SetupInvokeMethodHandler({});
        RuntimeInterface::SetupResolvedMethod(nullptr);
        RuntimeInterface::SetupResolvedClass(nullptr);
        RuntimeInterface::SetupObjectClass(nullptr);
        RuntimeInterface::SetupObject(nullptr);

        EXPECT_EQ(obj, f->GetAcc().GetReference());
    }
}

void TestLoadStoreField(bool is_static)
{
    BytecodeEmitter emitter;

    if (is_static) {
        emitter.Ldstatic(RuntimeInterface::FIELD_ID.AsIndex());
        emitter.StaWide(1);
        emitter.LdaWide(2U);
        emitter.Ststatic(RuntimeInterface::FIELD_ID.AsIndex());
        emitter.Ldstatic(RuntimeInterface::FIELD_ID.AsIndex());
    } else {
        emitter.Ldobj(0, RuntimeInterface::FIELD_ID.AsIndex());
        emitter.StaWide(1);
        emitter.LdaWide(2U);
        emitter.Stobj(0, RuntimeInterface::FIELD_ID.AsIndex());
        emitter.Ldobj(0, RuntimeInterface::FIELD_ID.AsIndex());
    }
    emitter.ReturnWide();

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

    auto f = CreateFrame(16U, nullptr, nullptr);
    InitializeFrame(f.get());

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
    auto method = std::move(method_data.first);
    f->SetMethod(method.get());

    pandasm::Parser p;
    std::string source;

    if (is_static) {
        source = R"(
            .record R {
                u1  sf_u1  <static>
                i8  sf_i8  <static>
                u8  sf_u8  <static>
                i16 sf_i16 <static>
                u16 sf_u16 <static>
                i32 sf_i32 <static>
                u32 sf_u32 <static>
                i64 sf_i64 <static>
                u64 sf_u64 <static>
                f32 sf_f32 <static>
                f64 sf_f64 <static>
            }
        )";
    } else {
        source = R"(
            .record R {
                u1  if_u1
                i8  if_i8
                u8  if_u8
                i16 if_i16
                u16 if_u16
                i32 if_i32
                u32 if_u32
                i64 if_i64
                u64 if_u64
                f32 if_f32
                f64 if_f64
            }
        )";
    }

    auto res = p.Parse(source);
    auto class_pf = pandasm::AsmEmitter::Emit(res.Value());

    auto class_linker = CreateClassLinker(ManagedThread::GetCurrent());
    ASSERT_NE(class_linker, nullptr);

    class_linker->AddPandaFile(std::move(class_pf));

    PandaString descriptor;

    auto *ext = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
    Class *object_class = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("R"), &descriptor));
    ASSERT_TRUE(class_linker->InitializeClass(ManagedThread::GetCurrent(), object_class));
    ObjectHeader *obj = nullptr;

    if (!is_static) {
        obj = AllocObject(object_class);
        f->GetVReg(0).SetReference(obj);
    }

    std::vector<panda_file::Type::TypeId> types {
        panda_file::Type::TypeId::U1,  panda_file::Type::TypeId::I8,  panda_file::Type::TypeId::U8,
        panda_file::Type::TypeId::I16, panda_file::Type::TypeId::U16, panda_file::Type::TypeId::I32,
        panda_file::Type::TypeId::U32, panda_file::Type::TypeId::I64, panda_file::Type::TypeId::U64,
        panda_file::Type::TypeId::F32, panda_file::Type::TypeId::F64};

    Span<Field> fields = is_static ? object_class->GetStaticFields() : object_class->GetInstanceFields();
    for (size_t i = 0; i < fields.size(); i++) {
        Field *field = &fields[i];

        std::ostringstream ss;
        ss << "Test field " << reinterpret_cast<const char *>(field->GetName().data);

        constexpr float FLOAT_VALUE = 1.0;
        constexpr double DOUBLE_VALUE = 2.0;
        int64_t value = 0;

        switch (field->GetType().GetId()) {
            case panda_file::Type::TypeId::U1: {
                value = std::numeric_limits<uint8_t>::max();
                f->GetVReg(2U).SetPrimitive(value);
                break;
            }
            case panda_file::Type::TypeId::I8: {
                value = std::numeric_limits<int8_t>::min();
                f->GetVReg(2U).SetPrimitive(value);
                break;
            }
            case panda_file::Type::TypeId::U8: {
                value = std::numeric_limits<uint8_t>::max();
                f->GetVReg(2U).SetPrimitive(value);
                break;
            }
            case panda_file::Type::TypeId::I16: {
                value = std::numeric_limits<int16_t>::min();
                f->GetVReg(2U).SetPrimitive(value);
                break;
            }
            case panda_file::Type::TypeId::U16: {
                value = std::numeric_limits<uint16_t>::max();
                f->GetVReg(2U).SetPrimitive(value);
                break;
            }
            case panda_file::Type::TypeId::I32: {
                value = std::numeric_limits<int32_t>::min();
                f->GetVReg(2U).SetPrimitive(value);
                break;
            }
            case panda_file::Type::TypeId::U32: {
                value = std::numeric_limits<uint32_t>::max();
                f->GetVReg(2U).SetPrimitive(value);
                break;
            }
            case panda_file::Type::TypeId::I64: {
                value = std::numeric_limits<int64_t>::min();
                f->GetVReg(2U).SetPrimitive(value);
                break;
            }
            case panda_file::Type::TypeId::U64: {
                value = std::numeric_limits<uint64_t>::max();
                f->GetVReg(2U).SetPrimitive(value);
                break;
            }
            case panda_file::Type::TypeId::F32: {
                f->GetVReg(2U).SetPrimitive(FLOAT_VALUE);
                break;
            }
            case panda_file::Type::TypeId::F64: {
                f->GetVReg(2U).SetPrimitive(DOUBLE_VALUE);
                break;
            }
            default: {
                UNREACHABLE();
                break;
            }
        }

        RuntimeInterface::SetupResolvedField(field);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        RuntimeInterface::SetupResolvedField(nullptr);

        switch (field->GetType().GetId()) {
            case panda_file::Type::TypeId::F32: {
                EXPECT_EQ(f->GetAcc().GetFloat(), FLOAT_VALUE) << ss.str();
                break;
            }
            case panda_file::Type::TypeId::F64: {
                EXPECT_EQ(f->GetAcc().GetDouble(), DOUBLE_VALUE) << ss.str();
                break;
            }
            default: {
                EXPECT_EQ(f->GetAcc().GetLong(), value) << ss.str();
                break;
            }
        }

        EXPECT_EQ(f->GetVReg(1).GetLong(), 0) << ss.str();
    }
}

void TestLoadStoreObjectField(bool is_static)
{
    BytecodeEmitter emitter;

    std::ostringstream ss;
    ss << "Test load/store ";
    if (is_static) {
        ss << "static ";
    }
    ss << "object field";

    if (is_static) {
        emitter.LdstaticObj(RuntimeInterface::FIELD_ID.AsIndex());
        emitter.StaObj(1);
        emitter.LdaObj(2U);
        emitter.StstaticObj(RuntimeInterface::FIELD_ID.AsIndex());
        emitter.LdstaticObj(RuntimeInterface::FIELD_ID.AsIndex());
    } else {
        emitter.LdobjObj(0, RuntimeInterface::FIELD_ID.AsIndex());
        emitter.StaObj(1);
        emitter.LdaObj(2U);
        emitter.StobjObj(0, RuntimeInterface::FIELD_ID.AsIndex());
        emitter.LdobjObj(0, RuntimeInterface::FIELD_ID.AsIndex());
    }
    emitter.ReturnObj();

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

    auto f = CreateFrame(16U, nullptr, nullptr);
    InitializeFrame(f.get());

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
    auto method = std::move(method_data.first);
    f->SetMethod(method.get());

    pandasm::Parser p;
    std::string source;

    if (is_static) {
        source = R"(
            .record R {
                R sf_ref <static>
            }
        )";
    } else {
        source = R"(
            .record R {
                R sf_ref
            }
        )";
    }

    auto res = p.Parse(source);
    auto class_pf = pandasm::AsmEmitter::Emit(res.Value());

    auto class_linker = CreateClassLinker(ManagedThread::GetCurrent());
    ASSERT_NE(class_linker, nullptr) << ss.str();

    class_linker->AddPandaFile(std::move(class_pf));

    PandaString descriptor;

    auto *ext = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
    Class *object_class = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("R"), &descriptor));
    ASSERT_TRUE(class_linker->InitializeClass(ManagedThread::GetCurrent(), object_class)) << ss.str();

    ObjectHeader *obj = nullptr;

    if (!is_static) {
        obj = AllocObject(object_class);
        f->GetVReg(0).SetReference(obj);
    }

    Span<Field> fields = is_static ? object_class->GetStaticFields() : object_class->GetInstanceFields();
    Field *field = &fields[0];

    ObjectHeader *ref_value = ToPointer<ObjectHeader>(0xaabbccdd);

    f->GetVReg(2U).SetReference(ref_value);

    RuntimeInterface::SetupResolvedField(field);

    Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

    RuntimeInterface::SetupResolvedField(nullptr);

    EXPECT_EQ(f->GetAcc().GetReference(), ref_value) << ss.str();
    EXPECT_EQ(f->GetVReg(1).GetReference(), nullptr) << ss.str();
}

TEST_F(InterpreterTest, TestLoadStoreField)
{
    TestLoadStoreField(false);
    TestLoadStoreObjectField(false);
}

TEST_F(InterpreterTest, TestLoadStoreStaticField)
{
    TestLoadStoreField(true);
    TestLoadStoreObjectField(true);
}

TEST_F(InterpreterTest, TestObjectExceptions)
{
    // Test NullPointerException

    {
        BytecodeEmitter emitter;
        emitter.Stobj(0, RuntimeInterface::FIELD_ID.AsIndex());
        emitter.ReturnVoid();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        RuntimeInterface::SetCatchBlockPcOffset(bytecode.size());

        emitter.ReturnObj();

        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());
        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        pandasm::Parser p;
        auto source = R"(
            .record R {
                i32 if
            }
        )";

        auto res = p.Parse(source);
        auto class_pf = pandasm::AsmEmitter::Emit(res.Value());

        auto class_linker = CreateClassLinker(ManagedThread::GetCurrent());
        ASSERT_NE(class_linker, nullptr);

        class_linker->AddPandaFile(std::move(class_pf));

        PandaString descriptor;

        auto *ext = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
        Class *object_class = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("R"), &descriptor));
        Field *field = object_class->GetInstanceFields().data();

        f->GetVReg(0).SetReference(nullptr);
        f->GetAcc().Set(0);

        RuntimeInterface::SetupResolvedField(field);
        RuntimeInterface::SetNullPointerExceptionData({true});

        auto *thread = ManagedThread::GetCurrent();
        ObjectHeader *exception = CreateException(thread);
        thread->SetException(exception);

        Execute(thread, bytecode.data(), f.get());

        RuntimeInterface::SetupResolvedField(nullptr);
        RuntimeInterface::SetNullPointerExceptionData({false});

        ASSERT_FALSE(thread->HasPendingException());
        ASSERT_EQ(f->GetAcc().GetReference(), exception);
    }

    {
        BytecodeEmitter emitter;
        emitter.StobjObj(0, RuntimeInterface::FIELD_ID.AsIndex());
        emitter.ReturnVoid();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        RuntimeInterface::SetCatchBlockPcOffset(bytecode.size());

        emitter.ReturnObj();

        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());
        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        pandasm::Parser p;
        auto source = R"(
            .record R {
                R if
            }
        )";

        auto res = p.Parse(source);
        auto class_pf = pandasm::AsmEmitter::Emit(res.Value());

        auto class_linker = CreateClassLinker(ManagedThread::GetCurrent());
        ASSERT_NE(class_linker, nullptr);

        class_linker->AddPandaFile(std::move(class_pf));

        PandaString descriptor;

        auto *ext = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
        Class *object_class = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("R"), &descriptor));
        Field *field = object_class->GetInstanceFields().data();

        f->GetVReg(0).SetReference(nullptr);
        f->GetAcc().SetReference(nullptr);

        RuntimeInterface::SetupResolvedField(field);
        RuntimeInterface::SetNullPointerExceptionData({true});

        auto *thread = ManagedThread::GetCurrent();
        ObjectHeader *exception = CreateException(thread);
        thread->SetException(exception);

        Execute(thread, bytecode.data(), f.get());

        RuntimeInterface::SetupResolvedField(nullptr);
        RuntimeInterface::SetNullPointerExceptionData({false});

        ASSERT_FALSE(thread->HasPendingException());
        ASSERT_EQ(f->GetAcc().GetReference(), exception);
    }

    {
        BytecodeEmitter emitter;
        emitter.Ldobj(0, RuntimeInterface::FIELD_ID.AsIndex());
        emitter.ReturnVoid();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        RuntimeInterface::SetCatchBlockPcOffset(bytecode.size());

        emitter.ReturnObj();

        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());
        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        pandasm::Parser p;
        auto source = R"(
            .record R {
                i32 if
            }
        )";

        auto res = p.Parse(source);
        auto class_pf = pandasm::AsmEmitter::Emit(res.Value());

        auto class_linker = CreateClassLinker(ManagedThread::GetCurrent());
        ASSERT_NE(class_linker, nullptr);

        class_linker->AddPandaFile(std::move(class_pf));

        PandaString descriptor;

        auto *ext = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
        Class *object_class = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("R"), &descriptor));
        Field *field = object_class->GetInstanceFields().data();

        f->GetVReg(0).SetReference(nullptr);

        RuntimeInterface::SetupResolvedField(field);
        RuntimeInterface::SetNullPointerExceptionData({true});

        auto *thread = ManagedThread::GetCurrent();
        ObjectHeader *exception = CreateException(thread);
        thread->SetException(exception);

        Execute(thread, bytecode.data(), f.get());

        RuntimeInterface::SetupResolvedField(nullptr);
        RuntimeInterface::SetNullPointerExceptionData({false});

        ASSERT_FALSE(thread->HasPendingException());
        ASSERT_EQ(f->GetAcc().GetReference(), exception);
    }

    {
        BytecodeEmitter emitter;
        emitter.LdobjObj(0, RuntimeInterface::FIELD_ID.AsIndex());
        emitter.ReturnVoid();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        RuntimeInterface::SetCatchBlockPcOffset(bytecode.size());

        emitter.ReturnObj();

        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());
        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        pandasm::Parser p;
        auto source = R"(
            .record R {
                R if
            }
        )";

        auto res = p.Parse(source);
        auto class_pf = pandasm::AsmEmitter::Emit(res.Value());

        auto class_linker = CreateClassLinker(ManagedThread::GetCurrent());
        ASSERT_NE(class_linker, nullptr);

        class_linker->AddPandaFile(std::move(class_pf));

        PandaString descriptor;

        auto *ext = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
        Class *object_class = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("R"), &descriptor));
        Field *field = object_class->GetInstanceFields().data();

        f->GetVReg(0).SetReference(nullptr);

        RuntimeInterface::SetupResolvedField(field);
        RuntimeInterface::SetNullPointerExceptionData({true});

        auto *thread = ManagedThread::GetCurrent();
        ObjectHeader *exception = CreateException(thread);
        thread->SetException(exception);

        Execute(thread, bytecode.data(), f.get());

        RuntimeInterface::SetupResolvedField(nullptr);
        RuntimeInterface::SetNullPointerExceptionData({false});

        ASSERT_FALSE(thread->HasPendingException());
        ASSERT_EQ(f->GetAcc().GetReference(), exception);
    }
}

TEST_F(InterpreterTest, TestArrayExceptions)
{
    TestNewArrayExceptions();

    TestLoadArrayExceptions<panda_file::Type::TypeId::I8>();
    TestLoadArrayExceptions<panda_file::Type::TypeId::I16>();
    TestLoadArrayExceptions<panda_file::Type::TypeId::I32>();
    TestLoadArrayExceptions<panda_file::Type::TypeId::I64>();
    TestLoadArrayExceptions<panda_file::Type::TypeId::REFERENCE>();

    TestStoreArrayExceptions<panda_file::Type::TypeId::I8>();
    TestStoreArrayExceptions<panda_file::Type::TypeId::I16>();
    TestStoreArrayExceptions<panda_file::Type::TypeId::I32>();
    TestStoreArrayExceptions<panda_file::Type::TypeId::I64>();
    TestStoreArrayExceptions<panda_file::Type::TypeId::REFERENCE>();

    TestArrayLenException();
}

TEST_F(InterpreterTest, TestReturns)
{
    int64_t value = 0xaabbccdd11223344;
    ObjectHeader *obj = ToPointer<ObjectHeader>(0xaabbccdd);

    {
        BytecodeEmitter emitter;

        emitter.Return();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        f->GetAcc().SetPrimitive(value);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        EXPECT_EQ(f->GetAcc().Get(), static_cast<int32_t>(value));
        EXPECT_FALSE(f->GetAcc().HasObject());
    }

    {
        BytecodeEmitter emitter;

        emitter.ReturnWide();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        f->GetAcc().SetPrimitive(value);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        EXPECT_EQ(f->GetAcc().GetLong(), value);
        EXPECT_FALSE(f->GetAcc().HasObject());
    }

    {
        BytecodeEmitter emitter;

        emitter.ReturnObj();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        f->GetAcc().SetReference(obj);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        EXPECT_EQ(f->GetAcc().GetReference(), obj);
        EXPECT_TRUE(f->GetAcc().HasObject());
    }
}

TEST_F(InterpreterTest, TestCheckCast)
{
    {
        BytecodeEmitter emitter;
        emitter.Checkcast(RuntimeInterface::TYPE_ID.AsIndex());
        emitter.ReturnVoid();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());
        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        pandasm::Parser p;
        auto source = R"(
            .record R {}
        )";

        auto res = p.Parse(source);
        auto class_pf = pandasm::AsmEmitter::Emit(res.Value());

        auto class_linker = CreateClassLinker(ManagedThread::GetCurrent());
        ASSERT_NE(class_linker, nullptr);

        class_linker->AddPandaFile(std::move(class_pf));

        PandaString descriptor;
        auto *thread = ManagedThread::GetCurrent();
        auto *ext = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
        Class *object_class = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("R"), &descriptor));
        ASSERT_TRUE(class_linker->InitializeClass(thread, object_class));

        f->GetAcc().SetReference(nullptr);

        RuntimeInterface::SetupResolvedClass(object_class);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        RuntimeInterface::SetupResolvedClass(nullptr);
    }

    {
        BytecodeEmitter emitter;
        emitter.Checkcast(RuntimeInterface::TYPE_ID.AsIndex());
        emitter.ReturnVoid();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());
        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        pandasm::Parser p;
        auto source = R"(
            .record R {}
        )";

        auto res = p.Parse(source);
        auto class_pf = pandasm::AsmEmitter::Emit(res.Value());

        auto class_linker = CreateClassLinker(ManagedThread::GetCurrent());
        ASSERT_NE(class_linker, nullptr);

        LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);

        class_linker->AddPandaFile(std::move(class_pf));

        PandaString descriptor;
        auto *thread = ManagedThread::GetCurrent();
        auto *ext = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto *object_class = ext->GetClass(ClassHelper::GetArrayDescriptor(utf::CStringAsMutf8("R"), 2, &descriptor));
        ASSERT_TRUE(class_linker->InitializeClass(thread, object_class));

        auto *obj = AllocArray(object_class, sizeof(uint8_t), 0);

        f->GetAcc().SetReference(obj);

        auto *dst_class = class_linker->GetExtension(ctx)->GetClassRoot(ClassRoot::OBJECT);
        ASSERT_TRUE(class_linker->InitializeClass(thread, dst_class));
        RuntimeInterface::SetupResolvedClass(dst_class);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        RuntimeInterface::SetupResolvedClass(nullptr);
    }
}

TEST_F(InterpreterTest, TestIsInstance)
{
    {
        BytecodeEmitter emitter;
        emitter.Isinstance(RuntimeInterface::TYPE_ID.AsIndex());
        emitter.Return();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());
        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        pandasm::Parser p;
        auto source = R"(
            .record R {}
        )";

        auto res = p.Parse(source);
        auto class_pf = pandasm::AsmEmitter::Emit(res.Value());

        auto class_linker = CreateClassLinker(ManagedThread::GetCurrent());
        ASSERT_NE(class_linker, nullptr);

        class_linker->AddPandaFile(std::move(class_pf));

        PandaString descriptor;
        auto *thread = ManagedThread::GetCurrent();
        auto *ext = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
        Class *object_class = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("R"), &descriptor));
        ASSERT_TRUE(class_linker->InitializeClass(thread, object_class));

        f->GetAcc().SetReference(nullptr);

        RuntimeInterface::SetupResolvedClass(object_class);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        RuntimeInterface::SetupResolvedClass(nullptr);

        ASSERT_EQ(f->GetAcc().Get(), 0);
    }

    {
        BytecodeEmitter emitter;
        emitter.Isinstance(RuntimeInterface::TYPE_ID.AsIndex());
        emitter.Return();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());
        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        pandasm::Parser p;
        auto source = R"(
            .record R {}
        )";

        auto res = p.Parse(source);
        auto class_pf = pandasm::AsmEmitter::Emit(res.Value());

        auto class_linker = CreateClassLinker(ManagedThread::GetCurrent());
        ASSERT_NE(class_linker, nullptr);

        LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);

        class_linker->AddPandaFile(std::move(class_pf));

        PandaString descriptor;
        auto *thread = ManagedThread::GetCurrent();
        auto *ext = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto *object_class = ext->GetClass(ClassHelper::GetArrayDescriptor(utf::CStringAsMutf8("R"), 2, &descriptor));
        ASSERT_TRUE(class_linker->InitializeClass(thread, object_class));

        auto *obj = AllocArray(object_class, sizeof(uint8_t), 0);

        f->GetAcc().SetReference(obj);

        auto *dst_class = class_linker->GetExtension(ctx)->GetClassRoot(ClassRoot::OBJECT);
        ASSERT_TRUE(class_linker->InitializeClass(thread, dst_class));
        RuntimeInterface::SetupResolvedClass(dst_class);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        RuntimeInterface::SetupResolvedClass(nullptr);

        ASSERT_EQ(f->GetAcc().Get(), 1);
    }
}

TEST_F(InterpreterTest, TestThrow)
{
    {
        BytecodeEmitter emitter;

        emitter.Throw(1);
        emitter.Movi(0, 16);
        emitter.Return();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());
        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        auto *thread = ManagedThread::GetCurrent();
        ObjectHeader *exception = CreateException(thread);

        f->GetVReg(1).SetReference(exception);
        f->GetVReg(0).SetPrimitive(0);

        RuntimeInterface::SetCatchBlockPcOffset(panda_file::INVALID_OFFSET);

        Execute(thread, bytecode.data(), f.get());

        ASSERT_TRUE(thread->HasPendingException());
        ASSERT_EQ(thread->GetException(), exception);
        ASSERT_EQ(f->GetVReg(0).Get(), 0);

        thread->ClearException();
    }

    {
        BytecodeEmitter emitter;

        emitter.Throw(1);
        emitter.Movi(0, 16);
        emitter.Return();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        RuntimeInterface::SetCatchBlockPcOffset(bytecode.size());

        emitter.Movi(0, 32);
        emitter.ReturnObj();

        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        auto *thread = ManagedThread::GetCurrent();
        ObjectHeader *exception = CreateException(thread);
        thread->SetException(exception);

        f->GetVReg(1).SetReference(exception);
        f->GetVReg(0).SetPrimitive(0);

        Execute(thread, bytecode.data(), f.get());

        ASSERT_FALSE(thread->HasPendingException());
        ASSERT_EQ(f->GetAcc().GetReference(), exception);
        ASSERT_EQ(f->GetVReg(0).Get(), 32);
    }

    // Test NullPointerException
    {
        BytecodeEmitter emitter;

        emitter.Throw(1);
        emitter.Movi(0, 16);
        emitter.Return();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        RuntimeInterface::SetCatchBlockPcOffset(bytecode.size());

        emitter.Movi(0, 32);
        emitter.ReturnObj();

        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        f->GetVReg(1).SetReference(nullptr);
        f->GetVReg(0).SetPrimitive(0);

        RuntimeInterface::SetNullPointerExceptionData({true});

        auto *thread = ManagedThread::GetCurrent();
        ObjectHeader *exception = CreateException(thread);
        thread->SetException(exception);

        Execute(thread, bytecode.data(), f.get());

        RuntimeInterface::SetNullPointerExceptionData({false});

        ASSERT_FALSE(thread->HasPendingException());
        ASSERT_EQ(f->GetAcc().GetReference(), exception);
        ASSERT_EQ(f->GetVReg(0).Get(), 32);
    }
}

static void MakeShorty(size_t num_args, std::vector<uint16_t> *buf)
{
    static constexpr uint8_t I64 = static_cast<uint8_t>(panda_file::Type::TypeId::I64);
    static constexpr size_t ELEM_SIZE = 4;
    static constexpr size_t ELEM_COUNT = std::numeric_limits<uint16_t>::digits / ELEM_SIZE;

    uint16_t val = 0;
    uint32_t count = 1;
    ++num_args;  // consider the return value
    while (num_args > 0) {
        if (count == ELEM_COUNT) {
            buf->push_back(val);
            val = 0;
            count = 0;
        }
        val |= I64 << ELEM_SIZE * count;
        ++count;
        --num_args;
    }
    if (count == ELEM_COUNT) {
        buf->push_back(val);
        val = 0;
    }
    buf->push_back(val);
}

template <bool is_dynamic = false>
static std::pair<PandaUniquePtr<Method>, std::unique_ptr<const panda_file::File>> CreateResolvedMethod(
    Class *klass, size_t vreg_num, const std::vector<int64_t> args, std::vector<uint8_t> *bytecode,
    std::vector<uint16_t> *shorty_buf)
{
    BytecodeEmitter emitter;
    Label label = emitter.CreateLabel();

    size_t start_idx = 0;
    if constexpr (is_dynamic) {
        ++start_idx;  // skip function object
    }
    for (size_t i = start_idx; i < args.size(); i++) {
        emitter.LdaiWide(args[i]);
        emitter.Jne(vreg_num + i, label);
    }

    emitter.LdaiWide(1);
    emitter.ReturnWide();
    emitter.Bind(label);
    emitter.LdaiWide(0);
    emitter.ReturnWide();

    [[maybe_unused]] auto res = emitter.Build(&*bytecode);

    ASSERT(res == BytecodeEmitter::ErrorCode::SUCCESS);

    MakeShorty(args.size(), shorty_buf);

    return CreateMethod(klass, 0, args.size(), vreg_num, shorty_buf->data(), *bytecode);
}

TEST_F(InterpreterTest, TestCalls)
{
    size_t vreg_num = 10;

    {
        BytecodeEmitter emitter;

        emitter.CallShort(1, 3, RuntimeInterface::METHOD_ID.AsIndex());
        emitter.ReturnWide();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());

        std::vector<int64_t> args = {1, 2};
        f->GetVReg(1).SetPrimitive(args[0]);
        f->GetVReg(3U).SetPrimitive(args[1]);

        auto klass = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(klass.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        std::vector<uint16_t> shorty_buf;
        std::vector<uint8_t> method_bytecode;
        auto resolved_method_data = CreateResolvedMethod(klass.get(), vreg_num, args, &method_bytecode, &shorty_buf);
        auto resolved_method = std::move(resolved_method_data.first);

        RuntimeInterface::SetupResolvedMethod(resolved_method.get());

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        if (ManagedThread::GetCurrent()->GetLanguageContext().GetLanguage() != panda_file::SourceLang::ECMASCRIPT) {
            EXPECT_EQ(resolved_method->GetHotnessCounter(), 1U);
        }

        RuntimeInterface::SetupResolvedMethod(nullptr);

        EXPECT_EQ(f->GetAcc().GetLong(), 1);
    }

    {
        BytecodeEmitter emitter;

        emitter.Call(1, 3, 5, 7, RuntimeInterface::METHOD_ID.AsIndex());
        emitter.ReturnWide();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());

        std::vector<int64_t> args = {1, 2, 3};
        f->GetVReg(1).SetPrimitive(args[0]);
        f->GetVReg(3U).SetPrimitive(args[1]);
        f->GetVReg(5U).SetPrimitive(args[2]);

        auto klass = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(klass.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        std::vector<uint16_t> shorty_buf;
        std::vector<uint8_t> method_bytecode;
        auto resolved_method_data = CreateResolvedMethod(klass.get(), vreg_num, args, &method_bytecode, &shorty_buf);
        auto resolved_method = std::move(resolved_method_data.first);

        RuntimeInterface::SetupResolvedMethod(resolved_method.get());

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        if (ManagedThread::GetCurrent()->GetLanguageContext().GetLanguage() != panda_file::SourceLang::ECMASCRIPT) {
            EXPECT_EQ(resolved_method->GetHotnessCounter(), 1U);
        }

        RuntimeInterface::SetupResolvedMethod(nullptr);

        EXPECT_EQ(f->GetAcc().GetLong(), 1);
    }

    {
        BytecodeEmitter emitter;

        emitter.CallRange(3, RuntimeInterface::METHOD_ID.AsIndex());
        emitter.ReturnWide();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());

        std::vector<int64_t> args = {1, 2, 3, 4, 5, 6, 7};
        for (size_t i = 0; i < args.size(); i++) {
            f->GetVReg(3 + i).SetPrimitive(args[i]);
        }

        auto klass = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(klass.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        std::vector<uint16_t> shorty_buf;
        std::vector<uint8_t> method_bytecode;
        auto resolved_method_data = CreateResolvedMethod(klass.get(), vreg_num, args, &method_bytecode, &shorty_buf);
        auto resolved_method = std::move(resolved_method_data.first);

        RuntimeInterface::SetupResolvedMethod(resolved_method.get());

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        if (ManagedThread::GetCurrent()->GetLanguageContext().GetLanguage() != panda_file::SourceLang::ECMASCRIPT) {
            EXPECT_EQ(resolved_method->GetHotnessCounter(), 1U);
        }

        RuntimeInterface::SetupResolvedMethod(nullptr);

        EXPECT_EQ(f->GetAcc().GetLong(), 1);
    }
}

void TestVirtualCallExceptions()
{
    // Test AbstractMethodError

    {
        BytecodeEmitter emitter;

        emitter.CallVirtRange(0, RuntimeInterface::METHOD_ID.AsIndex());
        emitter.Return();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        RuntimeInterface::SetCatchBlockPcOffset(bytecode.size());

        emitter.ReturnObj();

        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16U, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
        auto method = std::move(method_data.first);
        f->SetMethod(method.get());

        pandasm::Parser p;
        auto source = R"(
            .record A {}

            .function i32 A.foo(A a0) <noimpl>
        )";

        auto res = p.Parse(source);
        auto class_pf = pandasm::AsmEmitter::Emit(res.Value());

        auto class_linker = CreateClassLinker(ManagedThread::GetCurrent());
        ASSERT_NE(class_linker, nullptr);

        class_linker->AddPandaFile(std::move(class_pf));

        PandaString descriptor;

        auto *ext = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
        Class *object_class = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("A"), &descriptor));
        Method *callee = object_class->GetMethods().data();
        ObjectHeader *obj = AllocObject(object_class);

        f->GetVReg(0).SetReference(obj);

        RuntimeInterface::SetupResolvedMethod(callee);
        RuntimeInterface::SetAbstractMethodErrorData({true, callee});

        auto *thread = ManagedThread::GetCurrent();
        ObjectHeader *exception = CreateException(thread);
        thread->SetException(exception);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        RuntimeInterface::SetAbstractMethodErrorData({false, nullptr});
        RuntimeInterface::SetupResolvedMethod(nullptr);

        ASSERT_FALSE(thread->HasPendingException());
        ASSERT_EQ(f->GetAcc().GetReference(), exception);
    }
}

TEST_F(InterpreterTest, TestVirtualCallExceptions)
{
    TestVirtualCallExceptions();
}

int64_t EntryPoint([[maybe_unused]] Method *method, int64_t a0, int64_t a1)
{
    return 100U + a0 + a1;
}

TEST_F(InterpreterTest, TestCallNative)
{
    size_t vreg_num = 10;

    BytecodeEmitter emitter;

    emitter.CallShort(1, 3, RuntimeInterface::METHOD_ID.AsIndex());
    emitter.ReturnWide();

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

    auto f = CreateFrame(16U, nullptr, nullptr);
    InitializeFrame(f.get());

    std::vector<int64_t> args1 = {1, 2};
    f->GetVReg(1).SetPrimitive(args1[0]);
    f->GetVReg(3U).SetPrimitive(args1[1]);

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto method_data = CreateMethod(cls.get(), f.get(), bytecode);
    auto method = std::move(method_data.first);
    f->SetMethod(method.get());

    std::vector<uint16_t> shorty_buf;
    std::vector<uint8_t> method_bytecode;
    auto resolved_method_data = CreateResolvedMethod(cls.get(), vreg_num, args1, &method_bytecode, &shorty_buf);
    auto resolved_method = std::move(resolved_method_data.first);

    RuntimeInterface::SetCompilerHotnessThreshold(1);

    resolved_method->SetCompiledEntryPoint(reinterpret_cast<const void *>(EntryPoint));

    RuntimeInterface::SetupResolvedMethod(resolved_method.get());

    Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

    RuntimeInterface::SetupResolvedMethod(nullptr);

    EXPECT_EQ(f->GetAcc().GetLong(), 103);
}

TEST_F(InterpreterTest, ResolveCtorClass)
{
    pandasm::Parser p;

    PandaStringStream ss;
    ss << R"(
        .record R1 {}

        .function void R1.ctor(R1 a0) <ctor> {
            return.void
        }
    )";

    constexpr size_t METHOD_COUNT = panda_file::MAX_INDEX_16;
    for (size_t i = 0; i < METHOD_COUNT; i++) {
        ss << ".function void R1.f" << i << "() {" << std::endl;
        ss << "    call R1.f" << i << std::endl;
        ss << "    return.void" << std::endl;
        ss << "}" << std::endl;
    }

    ss << R"(
        .record R2 {}

        .function R1 R2.foo() {
            initobj R1.ctor
            return.obj
        }
    )";

    auto source = ss.str();
    auto res = p.Parse(source.c_str());
    ASSERT_TRUE(res) << res.Error().message;

    auto pf = pandasm::AsmEmitter::Emit(res.Value());
    ASSERT_NE(pf, nullptr) << pandasm::AsmEmitter::GetLastError();

    ClassLinker *class_linker = Runtime::GetCurrent()->GetClassLinker();
    class_linker->AddPandaFile(std::move(pf));

    PandaString descriptor;

    auto *ext = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
    Class *klass = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("R2"), &descriptor));
    ASSERT_NE(klass, nullptr);

    Method *method = klass->GetDirectMethod(utf::CStringAsMutf8("foo"));
    ASSERT_NE(method, nullptr);

    std::vector<Value> args;
    Value v = method->Invoke(ManagedThread::GetCurrent(), args.data());
    ASSERT_FALSE(ManagedThread::GetCurrent()->HasPendingException());

    auto *ret = v.GetAs<ObjectHeader *>();
    ASSERT_NE(ret, nullptr);

    ASSERT_EQ(ret->ClassAddr<panda::Class>()->GetName(), "R1");
}

TEST_F(InterpreterTest, ResolveField)
{
    pandasm::Parser p;

    PandaStringStream ss;
    ss << R"(
        .record R1 {
            i32 f <static>
        }

        .function void R1.cctor() <cctor> {
            ldai 10
            ststatic R1.f
            return.void
        }

        .function i32 R1.get() {
            ldstatic R1.f
            return
        }
    )";

    constexpr size_t METHOD_COUNT = panda_file::MAX_INDEX_16;
    for (size_t i = 0; i < METHOD_COUNT; i++) {
        ss << ".function void R1.f" << i << "() {" << std::endl;
        ss << "    call R1.f" << i << std::endl;
        ss << "    return.void" << std::endl;
        ss << "}" << std::endl;
    }

    ss << R"(
        .record R2 {
            i32 f <static>
        }

        .function void R2.cctor() <cctor> {
            ldai 20
            ststatic R2.f
            return.void
        }

        .function i32 R2.get() {
            ldstatic R2.f
            return
        }
    )";

    for (size_t i = 0; i < METHOD_COUNT; i++) {
        ss << ".function void R2.f" << i << "() {" << std::endl;
        ss << "    call R2.f" << i << std::endl;
        ss << "    return.void" << std::endl;
        ss << "}" << std::endl;
    }

    auto source = ss.str();
    auto res = p.Parse(source.c_str());
    ASSERT_TRUE(res) << res.Error().message;

    auto pf = pandasm::AsmEmitter::Emit(res.Value());
    ASSERT_NE(pf, nullptr) << pandasm::AsmEmitter::GetLastError();

    ClassLinker *class_linker = Runtime::GetCurrent()->GetClassLinker();
    class_linker->AddPandaFile(std::move(pf));
    auto *extension = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);

    PandaString descriptor;

    {
        Class *klass = extension->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("R1"), &descriptor));
        ASSERT_NE(klass, nullptr);

        Method *method = klass->GetDirectMethod(utf::CStringAsMutf8("get"));
        ASSERT_NE(method, nullptr);

        std::vector<Value> args;
        Value v = method->Invoke(ManagedThread::GetCurrent(), args.data());
        ASSERT_FALSE(ManagedThread::GetCurrent()->HasPendingException());

        auto ret = v.GetAs<int32_t>();
        ASSERT_EQ(ret, 10);
    }

    {
        Class *klass = extension->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("R2"), &descriptor));
        ASSERT_NE(klass, nullptr);

        Method *method = klass->GetDirectMethod(utf::CStringAsMutf8("get"));
        ASSERT_NE(method, nullptr);

        std::vector<Value> args;
        Value v = method->Invoke(ManagedThread::GetCurrent(), args.data());
        ASSERT_FALSE(ManagedThread::GetCurrent()->HasPendingException());

        auto ret = v.GetAs<int32_t>();
        ASSERT_EQ(ret, 20);
    }
}

}  // namespace test
}  // namespace panda::interpreter
