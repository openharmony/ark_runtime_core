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

#include <vector>

#include "assembly-parser.h"
#include "libpandafile/value.h"
#include "runtime/entrypoints/entrypoints.h"
#include "runtime/include/class_linker.h"
#include "runtime/include/method.h"
#include "runtime/include/runtime.h"

namespace panda::test {

class MethodTest : public testing::Test {
public:
    MethodTest()
    {
        RuntimeOptions options;
        options.SetHeapSizeLimit(128_MB);
        options.SetShouldLoadBootPandaFiles(false);
        options.SetShouldInitializeIntrinsics(false);
        options.SetGcType("epsilon");
        Runtime::Create(options);
        thread_ = panda::MTManagedThread::GetCurrent();
        thread_->ManagedCodeBegin();
    }

    ~MethodTest()
    {
        thread_->ManagedCodeEnd();
        Runtime::Destroy();
    }

    void VerifyLineNumber(const std::vector<int> &lines)
    {
        pandasm::Parser p;
        auto source = R"(
            .function i32 foo() {
                movi v0, 0x64               # offset 0x0, size 3
                mov v1, v0                  # offset 0x3, size 2
                mod v0, v1                  # offset 0x5, size 2
                sta v0                      # offset 0x7, size 2
                mov v2, v0                  # offset 0x9, size 2
                mov v0, v1                  # offset 0xb, size 2
                sta v0                      # offset 0xd, size 2
                mov v2, v0                  # offset 0xf, size 2
                mov v0, v1                  # offset 0x11, size 2
                lda v0                      # offset 0x13, size 2
                return                      # offset 0x15, size 1
                movi v0, 0x1                # offset 0x16, size 2
                lda v0                      # offset 0x18, size 2
                return                      # offset 0x20, size 1
            }
        )";
        // when verifying line numbers, we do not care about the exact instructions.
        // we only care about the sequence of line numbers.
        const std::vector<int> offsets {0x0, 0x3, 0x5, 0x7, 0x9, 0xb, 0xd, 0xf, 0x11, 0x13, 0x15, 0x16, 0x18, 0x20};
        auto res = p.Parse(source);
        auto &prog = res.Value();
        const std::string name = "foo";
        ASSERT_NE(prog.function_table.find(name), prog.function_table.end());
        auto &insVec = prog.function_table.find(name)->second.ins;
        const int insNum = insVec.size();
        ASSERT_EQ(lines.size(), insNum);

        for (int i = 0; i < insNum; i++) {
            insVec[i].ins_debug.SetLineNumber(lines[i]);
        }

        auto pf = pandasm::AsmEmitter::Emit(res.Value());
        ASSERT_NE(pf, nullptr);

        ClassLinker *classLinker = Runtime::GetCurrent()->GetClassLinker();
        classLinker->AddPandaFile(std::move(pf));
        auto *extension = classLinker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);

        PandaString descriptor;

        Class *klass = extension->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("_GLOBAL"), &descriptor));
        ASSERT_NE(klass, nullptr);

        Method *method = klass->GetDirectMethod(utf::CStringAsMutf8("foo"));
        ASSERT_NE(method, nullptr);

        for (int i = 0; i < insNum; i++) {
            ASSERT_EQ(method->GetLineNumFromBytecodeOffset(offsets[i]), lines[i]) << "do not match on i = " << i;
        }
    }

protected:
    panda::MTManagedThread *thread_ {nullptr};
};

TEST_F(MethodTest, SetIntrinsic)
{
    Method method(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), 0, 0, nullptr);
    ASSERT_FALSE(method.IsIntrinsic());

    auto intrinsic = intrinsics::Intrinsic::MATH_COS_F64;
    method.SetIntrinsic(intrinsic);
    ASSERT_TRUE(method.IsIntrinsic());

    ASSERT_EQ(method.GetIntrinsic(), intrinsic);
}

static int32_t EntryPoint([[maybe_unused]] Method *method)
{
    return 0;
}

TEST_F(MethodTest, Invoke)
{
    pandasm::Parser p;

    auto source = R"(
        .function i32 g() {
            ldai 0
            return
        }

        .function i32 f() {
            ldai 0
            return
        }

        .function void main() {
            call f
            return.void
        }
    )";

    auto res = p.Parse(source);
    auto pf = pandasm::AsmEmitter::Emit(res.Value());
    ASSERT_NE(pf, nullptr);

    ClassLinker *class_linker = Runtime::GetCurrent()->GetClassLinker();
    class_linker->AddPandaFile(std::move(pf));
    auto *extension = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);

    PandaString descriptor;

    Class *klass = extension->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("_GLOBAL"), &descriptor));
    ASSERT_NE(klass, nullptr);

    Method *main_method = klass->GetDirectMethod(utf::CStringAsMutf8("main"));
    ASSERT_NE(main_method, nullptr);

    Method *f_method = klass->GetDirectMethod(utf::CStringAsMutf8("f"));
    ASSERT_NE(f_method, nullptr);

    Method *g_method = klass->GetDirectMethod(utf::CStringAsMutf8("g"));
    ASSERT_NE(g_method, nullptr);

    g_method->SetCompiledEntryPoint(reinterpret_cast<const void *>(EntryPoint));

    EXPECT_EQ(f_method->GetHotnessCounter(), 0U);

    auto frame_deleter = [](Frame *frame) { FreeFrame(frame); };
    std::unique_ptr<Frame, decltype(frame_deleter)> frame(CreateFrame(0, main_method, nullptr), frame_deleter);

    ManagedThread *thread = ManagedThread::GetCurrent();
    thread->SetCurrentFrame(frame.get());

    // Invoke f calls interpreter

    std::vector<Value> args;
    Value v = f_method->Invoke(ManagedThread::GetCurrent(), args.data());
    EXPECT_EQ(v.GetAs<int64_t>(), 0);
    EXPECT_EQ(f_method->GetHotnessCounter(), 1U);
    EXPECT_EQ(ManagedThread::GetCurrent(), thread);

    // Invoke f called compiled code

    f_method->SetCompiledEntryPoint(reinterpret_cast<const void *>(EntryPoint));

    v = f_method->Invoke(ManagedThread::GetCurrent(), args.data());
    EXPECT_EQ(v.GetAs<int64_t>(), 0);
    EXPECT_EQ(f_method->GetHotnessCounter(), 2U);
    EXPECT_EQ(ManagedThread::GetCurrent(), thread);
}

TEST_F(MethodTest, CheckTaggedReturnType)
{
    pandasm::Parser p;

    auto source = R"(

        .function any Foo(any a0) {
            lda.dyn a0
            return.dyn
        }
    )";

    auto res = p.Parse(source);
    auto pf = pandasm::AsmEmitter::Emit(res.Value());
    ASSERT_NE(pf, nullptr);

    ClassLinker *class_linker = Runtime::GetCurrent()->GetClassLinker();
    class_linker->AddPandaFile(std::move(pf));
    auto *extension = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
    PandaString descriptor;

    Class *klass = extension->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("_GLOBAL"), &descriptor));
    ASSERT_NE(klass, nullptr);

    Method *method = klass->GetDirectMethod(utf::CStringAsMutf8("Foo"));
    ASSERT_NE(method, nullptr);

    std::vector<Value> args;
    args.emplace_back(Value(1, interpreter::TypeTag::INT));
    Value v = method->Invoke(ManagedThread::GetCurrent(), args.data());
    DecodedTaggedValue decoded = v.GetDecodedTaggedValue();
    EXPECT_EQ(decoded.value, 1);
    EXPECT_EQ(decoded.tag, interpreter::TypeTag::INT);
}

TEST_F(MethodTest, VirtualMethod)
{
    pandasm::Parser p;

    auto source = R"(
        .record R {}

        .function void R.foo(R a0, i32 a1) {
            return
        }
    )";

    auto res = p.Parse(source);
    auto pf = pandasm::AsmEmitter::Emit(res.Value());
    ASSERT_NE(pf, nullptr);

    ClassLinker *class_linker = Runtime::GetCurrent()->GetClassLinker();
    class_linker->AddPandaFile(std::move(pf));
    auto *extension = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);

    PandaString descriptor;

    Class *klass = extension->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("R"), &descriptor));
    ASSERT_NE(klass, nullptr);

    Method *method = klass->GetDirectMethod(utf::CStringAsMutf8("foo"));
    ASSERT_NE(method, nullptr);

    ASSERT_FALSE(method->IsStatic());
    ASSERT_EQ(method->GetNumArgs(), 2);
    ASSERT_EQ(method->GetArgType(0).GetId(), panda_file::Type::TypeId::REFERENCE);
    ASSERT_EQ(method->GetArgType(1).GetId(), panda_file::Type::TypeId::I32);
}

TEST_F(MethodTest, GetLineNumFromBytecodeOffset1)
{
    pandasm::Parser p;

    auto source = R"(          # line 1
        .function void foo() { # line 2
            mov v0, v1         # line 3, offset 0, size 2
            mov v100, v200     # line 4, offset 2, size 3
            movi v0, 4         # line 5, offset 5, size 2
            movi v0, 100       # line 6, offset 7, size 3
            movi v0, 300       # line 7, offset 10, size 4
            return.void        # line 8, offset 14, size 1
        }
    )";

    auto res = p.Parse(source);
    auto pf = pandasm::AsmEmitter::Emit(res.Value());
    ASSERT_NE(pf, nullptr);

    ClassLinker *class_linker = Runtime::GetCurrent()->GetClassLinker();
    class_linker->AddPandaFile(std::move(pf));
    auto *extension = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);

    PandaString descriptor;

    Class *klass = extension->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("_GLOBAL"), &descriptor));
    ASSERT_NE(klass, nullptr);

    Method *method = klass->GetDirectMethod(utf::CStringAsMutf8("foo"));
    ASSERT_NE(method, nullptr);

    ASSERT_EQ(method->GetLineNumFromBytecodeOffset(0), 3);
    ASSERT_EQ(method->GetLineNumFromBytecodeOffset(2U), 4);
    ASSERT_EQ(method->GetLineNumFromBytecodeOffset(5), 5);
    ASSERT_EQ(method->GetLineNumFromBytecodeOffset(7), 6);
    ASSERT_EQ(method->GetLineNumFromBytecodeOffset(10), 7);
    ASSERT_EQ(method->GetLineNumFromBytecodeOffset(14), 8);

    ASSERT_EQ(method->GetLineNumFromBytecodeOffset(20), 8);
}

TEST_F(MethodTest, GetLineNumFromBytecodeOffset2)
{
    VerifyLineNumber({4, 4, 4, 4, 4, 6, 6, 6, 6, 6, 6, 8, 8, 8});
}

TEST_F(MethodTest, GetLineNumFromBytecodeOffset3)
{
    VerifyLineNumber({4, 4, 4, 4, 4, 7, 5, 5, 6, 6, 6, 8, 8, 8});
}

TEST_F(MethodTest, GetLineNumFromBytecodeOffset4)
{
    VerifyLineNumber({3, 3, 4, 4, 6, 6, 10, 5, 8, 9, 9, 4, 4, 12});
}

TEST_F(MethodTest, GetLineNumFromBytecodeOffset5)
{
    VerifyLineNumber({4, 4, 4, 4, 6, 6, 7, 8, 8, 8, 9, 4, 4, 12});
}

TEST_F(MethodTest, GetLineNumFromBytecodeOffset6)
{
    VerifyLineNumber({4, 17, 5, 7, 7, 13, 19, 19, 11, 10, 2, 7, 8, 18});
}

TEST_F(MethodTest, GetLineNumFromBytecodeOffset7)
{
    VerifyLineNumber({4, 5, 7, 9, 10, 11, 13, 14, 15, 16, 6, 1, 3, 2});
}

TEST_F(MethodTest, GetLineNumFromBytecodeOffset8)
{
    VerifyLineNumber({3, 4, 4, 5, 6, 6, 7, 9, 10, 11, 12, 13, 14, 14});
}

TEST_F(MethodTest, GetLineNumFromBytecodeOffset9)
{
    VerifyLineNumber({3, 4, 5, 6, 6, 7, 9, 10, 16, 12, 13, 14, 15, 11});
}

TEST_F(MethodTest, GetClassSourceFile)
{
    pandasm::Parser p;

    auto source = R"(
        .record R {}

        .function void R.foo() {
            return.void
        }

        .function void foo() {
            return.void
        }
    )";

    std::string source_filename = "source.pa";
    auto res = p.Parse(source, source_filename);
    auto pf = pandasm::AsmEmitter::Emit(res.Value());
    ASSERT_NE(pf, nullptr);

    ClassLinker *class_linker = Runtime::GetCurrent()->GetClassLinker();
    auto *extension = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
    class_linker->AddPandaFile(std::move(pf));

    PandaString descriptor;

    {
        Class *klass = extension->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("_GLOBAL"), &descriptor));
        ASSERT_NE(klass, nullptr);

        Method *method = klass->GetDirectMethod(utf::CStringAsMutf8("foo"));
        ASSERT_NE(method, nullptr);

        auto result = method->GetClassSourceFile();
        ASSERT_EQ(result.data, nullptr);
    }

    {
        Class *klass = extension->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("R"), &descriptor));
        ASSERT_NE(klass, nullptr);

        Method *method = klass->GetDirectMethod(utf::CStringAsMutf8("foo"));
        ASSERT_NE(method, nullptr);

        auto result = method->GetClassSourceFile();
        ASSERT_TRUE(utf::IsEqual(result.data, utf::CStringAsMutf8(source_filename.data())));
    }
}

static int32_t StackTraceEntryPoint([[maybe_unused]] Method *method)
{
    auto thread = ManagedThread::GetCurrent();

    struct StackTraceData {
        std::string func_name;
        int32_t line_num;

        bool operator==(const StackTraceData &st) const
        {
            return func_name == st.func_name && line_num == st.line_num;
        }
    };

    std::vector<StackTraceData> expected {{"f3", 31},   {"f2", 26}, {".cctor", 14},
                                          {".ctor", 9}, {"f1", 20}, {"main", 41}};
    std::vector<StackTraceData> trace;
    for (StackWalker stack(thread); stack.HasFrame(); stack.NextFrame()) {
        auto pc = stack.GetBytecodePc();
        auto *method_from_frame = stack.GetMethod();
        auto line_num = method_from_frame->GetLineNumFromBytecodeOffset(pc);
        std::string func_name(utf::Mutf8AsCString(method_from_frame->GetName().data));
        trace.push_back({func_name, line_num});
    }

    if (trace == expected) {
        return 0;
    }

    return 1;
}

TEST_F(MethodTest, StackTrace)
{
    pandasm::Parser p;

    auto source = R"(                           # 1
        .record R1 {}                           # 2
                                                # 3
        .record R2 {                            # 4
            i32 f1 <static>                     # 5
        }                                       # 6
        .function void R1.ctor(R1 a0) <ctor> {  # 7
            ldai 0                              # 8
            ldstatic R2.f1                      # 9
            return.void                         # 10
        }                                       # 11
                                                # 12
        .function void R2.cctor() <cctor> {     # 13
            call f2                             # 14
            ststatic R2.f1                      # 15
            return.void                         # 16
        }                                       # 17
                                                # 18
        .function i32 f1() {                    # 19
            initobj R1.ctor                     # 20
            ldstatic R2.f1                      # 21
            return                              # 22
        }                                       # 23
                                                # 24
        .function i32 f2() {                    # 25
            call f3                             # 26
            return                              # 27
        }                                       # 28
                                                # 29
        .function i32 f3() {                    # 30
            call f4                             # 31
            return                              # 32
        }                                       # 33
                                                # 34
        .function i32 f4() {                    # 35
            ldai 0                              # 36
            return                              # 37
        }                                       # 38
                                                # 39
        .function i32 main() {                  # 40
            call f1                             # 41
            return                              # 42
        }                                       # 43
    )";

    auto res = p.Parse(source);
    ASSERT_TRUE(res) << res.Error().message;

    auto pf = pandasm::AsmEmitter::Emit(res.Value());
    ASSERT_NE(pf, nullptr) << pandasm::AsmEmitter::GetLastError();

    ClassLinker *class_linker = Runtime::GetCurrent()->GetClassLinker();
    class_linker->AddPandaFile(std::move(pf));
    auto *extension = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);

    PandaString descriptor;

    Class *klass = extension->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("_GLOBAL"), &descriptor));
    ASSERT_NE(klass, nullptr);

    Method *main_method = klass->GetDirectMethod(utf::CStringAsMutf8("main"));
    ASSERT_NE(main_method, nullptr);

    Method *f4_method = klass->GetDirectMethod(utf::CStringAsMutf8("f4"));
    ASSERT_NE(f4_method, nullptr);

    f4_method->SetCompiledEntryPoint(reinterpret_cast<const void *>(StackTraceEntryPoint));

    ManagedThread *thread = ManagedThread::GetCurrent();
    thread->SetCurrentFrame(nullptr);

    std::vector<Value> args;
    Value v = main_method->Invoke(thread, args.data());
    EXPECT_EQ(v.GetAs<int32_t>(), 0);
}

}  // namespace panda::test
