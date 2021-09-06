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

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "assembly-parser.h"
#include "libpandafile/bytecode_emitter.h"
#include "libpandafile/bytecode_instruction.h"
#include "libpandafile/file_items.h"
#include "libpandafile/shorty_iterator.h"
#include "libpandafile/value.h"
#include "runtime/arch/helpers.h"
#include "runtime/bridge/bridge.h"
#include "runtime/entrypoints/entrypoints.h"
#include "runtime/include/class_linker.h"
#include "runtime/include/method.h"
#include "runtime/include/runtime.h"
#include "runtime/include/thread.h"
#include "runtime/tests/interpreter/test_runtime_interface.h"
#include "invokation_helper.h"

using TypeId = panda::panda_file::Type::TypeId;
using BytecodeEmitter = panda::BytecodeEmitter;

namespace panda::test {

#ifndef PANDA_TARGET_ARM32

int32_t CmpDynImpl(Method *, DecodedTaggedValue v1, DecodedTaggedValue v2)
{
    return v1.value == v2.value && v1.tag == v2.tag ? 0 : 1;
}

DecodedTaggedValue LdUndefinedImpl(Method *method)
{
    return Runtime::GetCurrent()->GetLanguageContext(*method).GetInitialDecodedValue();
}

class CompiledCodeToInterpreterBridgeTest : public testing::Test {
public:
    void SetUp()
    {
        // See comment in InvokeEntryPoint function
        if constexpr (RUNTIME_ARCH == Arch::AARCH32) {
            return;
        }

        RuntimeOptions options;
        options.SetShouldLoadBootPandaFiles(false);
        options.SetShouldInitializeIntrinsics(false);
        options.SetGcType("epsilon");

        Runtime::Create(options);
        thread_ = MTManagedThread::GetCurrent();
        thread_->ManagedCodeBegin();
        SetUpHelperFunctions("PandaAssembly");
    }

    ~CompiledCodeToInterpreterBridgeTest()
    {
        if constexpr (RUNTIME_ARCH == Arch::AARCH32) {
            return;
        }
        thread_->ManagedCodeEnd();
        Runtime::Destroy();
    }

    void SetUpHelperFunctions(const std::string &language)
    {
        Runtime *runtime = Runtime::GetCurrent();
        ClassLinker *class_linker = runtime->GetClassLinker();

        std::string source = ".language " + language + "\n" + R"(
            .record TestUtils {}
            .function i32 TestUtils.cmpDyn(any a0, any a1) <native>
            .function any TestUtils.ldundefined() <native>
        )";
        pandasm::Parser p;
        auto res = p.Parse(source);
        ASSERT_TRUE(res.HasValue());
        std::unique_ptr<const panda_file::File> pf = pandasm::AsmEmitter::Emit(res.Value());
        class_linker->AddPandaFile(std::move(pf));

        auto descriptor = std::make_unique<PandaString>();
        panda_file::SourceLang lang;
        if (language == "ECMAScript") {
            lang = panda_file::SourceLang::ECMASCRIPT;
        } else if (language == "PandaAssembly") {
            lang = panda_file::SourceLang::PANDA_ASSEMBLY;
        } else {
            UNREACHABLE();
        }
        auto *extension = class_linker->GetExtension(lang);
        Class *klass =
            extension->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("TestUtils"), descriptor.get()));

        Method *cmpDyn = klass->GetDirectMethod(utf::CStringAsMutf8("cmpDyn"));
        ASSERT_NE(cmpDyn, nullptr);
        cmpDyn->SetCompiledEntryPoint(reinterpret_cast<const void *>(CmpDynImpl));

        Method *ldundefined = klass->GetDirectMethod(utf::CStringAsMutf8("ldundefined"));
        ASSERT_NE(ldundefined, nullptr);
        ldundefined->SetCompiledEntryPoint(reinterpret_cast<const void *>(LdUndefinedImpl));
    }

    Method *MakeNoArgsMethod(TypeId ret_type, int64_t ret)
    {
        Runtime *runtime = Runtime::GetCurrent();
        ClassLinker *class_linker = runtime->GetClassLinker();
        LanguageContext ctx = runtime->GetLanguageContext(lang_);

        std::ostringstream out;
        out << ".language " << ctx << '\n';
        if (ret_type == TypeId::REFERENCE) {
            // 'operator <<' for TypeId::REFERENCE returns 'reference'. So create a class to handle this situation.
            out << ".record reference {}\n";
        }
        out << ".function " << panda_file::Type(ret_type) << " main() {\n";
        if (TypeId::F32 <= ret_type && ret_type <= TypeId::F64) {
            out << "fldai.64 " << bit_cast<double>(ret) << '\n';
            out << "return.64\n";
        } else if (TypeId::I64 <= ret_type && ret_type <= TypeId::U64) {
            out << "ldai.64 " << ret << '\n';
            out << "return.64\n";
        } else if (ret_type == TypeId::REFERENCE) {
            out << "lda.null\n";
            out << "return.obj\n";
        } else if (ret_type == TypeId::TAGGED) {
            out << "ldai.dyn " << ret << '\n';
            out << "return.dyn\n";
        } else {
            out << "ldai " << ret << '\n';
            out << "return\n";
        }
        out << "}";

        pandasm::Parser p;
        auto res = p.Parse(out.str());
        std::unique_ptr<const panda_file::File> pf = pandasm::AsmEmitter::Emit(res.Value());
        class_linker->AddPandaFile(std::move(pf));

        auto descriptor = std::make_unique<PandaString>();
        auto *extension = class_linker->GetExtension(ctx);
        Class *klass =
            extension->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("_GLOBAL"), descriptor.get()));

        Method *main = klass->GetDirectMethod(utf::CStringAsMutf8("main"));
        main->SetInterpreterEntryPoint();
        return main;
    }

    Method *MakeCheckArgsMethod(const std::initializer_list<TypeId> &shorty, const std::initializer_list<int64_t> args,
                                bool is_instance = false)
    {
        Runtime *runtime = Runtime::GetCurrent();
        ClassLinker *class_linker = runtime->GetClassLinker();
        LanguageContext ctx = runtime->GetLanguageContext(lang_);

        std::ostringstream out;
        std::ostringstream signature;
        std::ostringstream body;
        uint32_t arg_num = 0;

        if (is_instance) {
            signature << "Test a0";
            body << "lda.null\n";
            body << "jne.obj a0, fail\n";
            ++arg_num;
        }
        auto shorty_it = shorty.begin();
        TypeId ret_type = *shorty_it++;
        auto args_it = args.begin();
        while (shorty_it != shorty.end()) {
            if (args_it == args.end()) {
                // only dynamic methods could be called with less arguments than declared
                ASSERT(*shorty_it == TypeId::TAGGED);
            }
            if (arg_num > 0) {
                signature << ", ";
            }
            if ((TypeId::F32 <= *shorty_it && *shorty_it <= TypeId::U64) || *shorty_it == TypeId::REFERENCE ||
                *shorty_it == TypeId::TAGGED) {
                signature << panda_file::Type(*shorty_it) << " a" << arg_num;
                if (TypeId::F32 <= *shorty_it && *shorty_it <= TypeId::F64) {
                    body << "fldai.64 " << bit_cast<double>(*args_it) << '\n';
                    body << "fcmpg.64 a" << arg_num << '\n';
                    body << "jnez fail\n";
                } else if (TypeId::I64 <= *shorty_it && *shorty_it <= TypeId::U64) {
                    body << "ldai.64 " << *args_it << '\n';
                    body << "cmp.64 a" << arg_num << '\n';
                    body << "jnez fail\n";
                } else if (*shorty_it == TypeId::TAGGED) {
                    if (args_it == args.end()) {
                        body << "call.short TestUtils.ldundefined\n";
                    } else {
                        body << "ldai.dyn " << *args_it << '\n';
                    }
                    body << "sta.dyn v0\n";
                    body << "call.short TestUtils.cmpDyn, v0, a" << arg_num << '\n';
                    body << "jnez fail\n";
                } else {
                    body << "lda.null\n";
                    body << "jne.obj a" << arg_num << ", fail\n";
                }
            } else {
                signature << "i32 a" << arg_num;
                body << "ldai " << *args_it << '\n';
                body << "jne a" << arg_num << ", fail\n";
            }
            ++shorty_it;
            if (args_it != args.end()) {
                ++args_it;
            }
            ++arg_num;
        }
        if (ret_type == TypeId::TAGGED) {
            body << "ldai.dyn 1\n";
            body << "return.dyn\n";
            body << "fail:\n";
            body << "ldai.dyn 0\n";
            body << "return.dyn\n";
        } else {
            body << "ldai 1\n";
            body << "return\n";
            body << "fail:\n";
            body << "ldai 0\n";
            body << "return\n";
        }

        out << ".language " << ctx << '\n';
        out << ".record TestUtils <external>\n";
        out << ".function i32 TestUtils.cmpDyn(any a0, any a1) <external>\n";
        out << ".function any TestUtils.ldundefined() <external>\n";
        out << ".record reference {}\n";
        out << ".record Test {}\n";
        out << ".function " << panda_file::Type(ret_type) << " Test.main(" << signature.str() << ") {\n";
        out << body.str();
        out << "}";

        pandasm::Parser p;
        auto res = p.Parse(out.str());
        std::unique_ptr<const panda_file::File> pf = pandasm::AsmEmitter::Emit(res.Value());
        class_linker->AddPandaFile(std::move(pf));

        auto descriptor = std::make_unique<PandaString>();
        auto *extension = class_linker->GetExtension(ctx);
        Class *klass = extension->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("Test"), descriptor.get()));

        Method *main = klass->GetDirectMethod(utf::CStringAsMutf8("main"));
        main->SetInterpreterEntryPoint();
        return main;
    }

    MTManagedThread *thread_ {nullptr};
    panda_file::SourceLang lang_ {panda_file::SourceLang::PANDA_ASSEMBLY};
};

TEST_F(CompiledCodeToInterpreterBridgeTest, InvokeVoidNoArg)
{
    auto method = MakeNoArgsMethod(TypeId::VOID, 0);
    InvokeEntryPoint<void>(method);
}

TEST_F(CompiledCodeToInterpreterBridgeTest, InvokeIntNoArg)
{
    auto method = MakeNoArgsMethod(TypeId::I32, 5);
    int32_t res = InvokeEntryPoint<int32_t>(method);
    ASSERT_EQ(res, 5);
}

TEST_F(CompiledCodeToInterpreterBridgeTest, InvokeLongNoArg)
{
    auto method = MakeNoArgsMethod(TypeId::I64, 7);

    int64_t res = InvokeEntryPoint<int64_t>(method);
    ASSERT_EQ(res, 7);
}

TEST_F(CompiledCodeToInterpreterBridgeTest, InvokeDoubleNoArg)
{
    auto method = MakeNoArgsMethod(TypeId::F64, bit_cast<int64_t>(3.0));

    auto res = InvokeEntryPoint<double>(method);
    ASSERT_EQ(res, 3.0);
}

TEST_F(CompiledCodeToInterpreterBridgeTest, InvokeObjNoArg)
{
    auto method = MakeNoArgsMethod(TypeId::REFERENCE, 0);

    ObjectHeader *res = InvokeEntryPoint<ObjectHeader *>(method);
    ASSERT_EQ(res, nullptr);
}

TEST_F(CompiledCodeToInterpreterBridgeTest, InvokeTaggedNoArg)
{
    auto method = MakeNoArgsMethod(TypeId::TAGGED, 1);

    DecodedTaggedValue res = InvokeEntryPoint<DecodedTaggedValue>(method);
    ASSERT_EQ(res.value, coretypes::TaggedValue(1).GetRawData());
    ASSERT_EQ(res.tag, interpreter::INT);
}

/// Args tests:
TEST_F(CompiledCodeToInterpreterBridgeTest, InvokeInt)
{
    auto method = MakeCheckArgsMethod({TypeId::I32, TypeId::I32}, {5});

    int32_t res = InvokeEntryPoint<int32_t>(method, 5);
    ASSERT_EQ(res, 1);
}

TEST_F(CompiledCodeToInterpreterBridgeTest, InvokeInstanceInt)
{
    auto method = MakeCheckArgsMethod({TypeId::I32}, {0, 5}, true);

    int32_t res = InvokeEntryPoint<int32_t>(method, nullptr, 5);
    ASSERT_EQ(res, 1);
}

TEST_F(CompiledCodeToInterpreterBridgeTest, Invoke3Int)
{
    auto method = MakeCheckArgsMethod({TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32}, {3, 2, 1});

    int32_t res = InvokeEntryPoint<int32_t>(method, 3, 2, 1);
    ASSERT_EQ(res, 1);
}

TEST_F(CompiledCodeToInterpreterBridgeTest, InvokeLong)
{
    auto method = MakeCheckArgsMethod({TypeId::I32, TypeId::I64}, {7});

    int32_t res = InvokeEntryPoint<int32_t>(method, 7);
    ASSERT_EQ(res, 1);
}

TEST_F(CompiledCodeToInterpreterBridgeTest, InvokeDouble)
{
    auto method = MakeCheckArgsMethod({TypeId::I32, TypeId::F64}, {bit_cast<int64_t>(2.0)});

    int32_t res = InvokeEntryPoint<int32_t>(method, 2.0);
    ASSERT_EQ(res, 1);
}

TEST_F(CompiledCodeToInterpreterBridgeTest, Invoke4Int)
{
    auto method = MakeCheckArgsMethod({TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32}, {4, 3, 2, 1});

    int32_t res = InvokeEntryPoint<int32_t>(method, 4, 3, 2, 1);
    ASSERT_EQ(res, 1);
}

TEST_F(CompiledCodeToInterpreterBridgeTest, Invoke2Long)
{
    auto method = MakeCheckArgsMethod({TypeId::I32, TypeId::I64, TypeId::I64}, {7, 8});

    int32_t res = InvokeEntryPoint<int32_t>(method, 7, 8);
    ASSERT_EQ(res, 1);
}

TEST_F(CompiledCodeToInterpreterBridgeTest, Invoke4IntDouble)
{
    auto method = MakeCheckArgsMethod({TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::F64},
                                      {4, 3, 2, 1, bit_cast<int64_t>(8.0)});

    int32_t res = InvokeEntryPoint<int32_t>(method, 4, 3, 2, 1, 8.0);
    ASSERT_EQ(res, 1);
}

TEST_F(CompiledCodeToInterpreterBridgeTest, Invoke7Int)
{
    auto method = MakeCheckArgsMethod(
        {TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32},
        {7, 6, 5, 4, 3, 2, 1});

    int32_t res = InvokeEntryPoint<int32_t>(method, 7, 6, 5, 4, 3, 2, 1);
    ASSERT_EQ(res, 1);
}

TEST_F(CompiledCodeToInterpreterBridgeTest, Invoke7Int8Double)
{
    auto method = MakeCheckArgsMethod({TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32,
                                       TypeId::I32, TypeId::I32, TypeId::F64, TypeId::F64, TypeId::F64, TypeId::F64,
                                       TypeId::F64, TypeId::F64, TypeId::F64, TypeId::F64},
                                      {7, 6, 5, 4, 3, 2, 1, bit_cast<int64_t>(10.0), bit_cast<int64_t>(11.0),
                                       bit_cast<int64_t>(12.0), bit_cast<int64_t>(13.0), bit_cast<int64_t>(14.0),
                                       bit_cast<int64_t>(15.0), bit_cast<int64_t>(16.0), bit_cast<int64_t>(17.0)});

    int32_t res =
        InvokeEntryPoint<int32_t>(method, 7, 6, 5, 4, 3, 2, 1, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0, 17.0);
    ASSERT_EQ(res, 1);
}

TEST_F(CompiledCodeToInterpreterBridgeTest, Invoke8Int)
{
    auto method = MakeCheckArgsMethod({TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32,
                                       TypeId::I32, TypeId::I32, TypeId::I32},
                                      {8, 7, 6, 5, 4, 3, 2, 1});

    int32_t res = InvokeEntryPoint<int32_t>(method, 8, 7, 6, 5, 4, 3, 2, 1);
    ASSERT_EQ(res, 1);
}

TEST_F(CompiledCodeToInterpreterBridgeTest, Invoke8Int9Double)
{
    auto method = MakeCheckArgsMethod({TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32,
                                       TypeId::I32, TypeId::I32, TypeId::I32, TypeId::F64, TypeId::F64, TypeId::F64,
                                       TypeId::F64, TypeId::F64, TypeId::F64, TypeId::F64, TypeId::F64, TypeId::F64},
                                      {8, 7, 6, 5, 4, 3, 2, 1, bit_cast<int64_t>(10.0), bit_cast<int64_t>(11.0),
                                       bit_cast<int64_t>(12.0), bit_cast<int64_t>(13.0), bit_cast<int64_t>(14.0),
                                       bit_cast<int64_t>(15.0), bit_cast<int64_t>(16.0), bit_cast<int64_t>(17.0),
                                       bit_cast<int64_t>(18.0)});

    int32_t res =
        InvokeEntryPoint<int32_t>(method, 8, 7, 6, 5, 4, 3, 2, 1, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0, 17.0, 18.0);
    ASSERT_EQ(res, 1);
}

// Dynamic functions

#endif  // !PANDA_TARGET_ARM32

}  // namespace panda::test
