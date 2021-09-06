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

#include "libpandafile/bytecode_emitter.h"
#include "libpandafile/bytecode_instruction.h"
#include "libpandafile/file_items.h"
#include "libpandafile/value.h"
#include "runtime/bridge/bridge.h"
#include "runtime/entrypoints/entrypoints.h"
#include "runtime/include/method.h"
#include "runtime/include/runtime.h"
#include "runtime/include/thread.h"
#include "runtime/interpreter/frame.h"

using TypeId = panda::panda_file::Type::TypeId;
using Opcode = panda::BytecodeInstruction::Opcode;

namespace panda::test {

static std::string g_call_result;

class InterpreterToCompiledCodeBridgeTest : public testing::Test {
public:
    InterpreterToCompiledCodeBridgeTest()
    {
        RuntimeOptions options;
        options.SetShouldLoadBootPandaFiles(false);
        options.SetShouldInitializeIntrinsics(false);
        options.SetGcType("epsilon");
        Runtime::Create(options);

        thread_ = MTManagedThread::GetCurrent();
        thread_->ManagedCodeBegin();
        g_call_result = "";
    }

    ~InterpreterToCompiledCodeBridgeTest()
    {
        thread_->ManagedCodeEnd();
        Runtime::Destroy();
    }

    uint16_t *MakeShorty(const std::initializer_list<TypeId> &shorty)
    {
        constexpr size_t ELEM_SIZE = 4;
        constexpr size_t ELEM_COUNT = std::numeric_limits<uint16_t>::digits / ELEM_SIZE;

        uint16_t val = 0;
        uint32_t count = 0;
        for (auto it = shorty.begin(); it != shorty.end(); ++it) {
            if (count == ELEM_COUNT) {
                shorty_.push_back(val);
                val = 0;
                count = 0;
            }
            val |= static_cast<uint8_t>(*it) << ELEM_SIZE * count;
            ++count;
        }
        if (count == ELEM_COUNT) {
            shorty_.push_back(val);
            val = 0;
            count = 0;
        }
        shorty_.push_back(val);
        return shorty_.data();
    }

protected:
    MTManagedThread *thread_ {nullptr};
    std::vector<uint16_t> shorty_;
};

// Test interpreter -> compiled code bridge

template <typename Arg>
std::string ArgsToString(const Arg &arg)
{
    std::ostringstream out;
    out << arg;
    return out.str();
}

template <typename Arg, typename... Args>
std::string ArgsToString(const Arg &a0, Args... args)
{
    std::ostringstream out;
    out << a0 << ", " << ArgsToString(args...);
    return out.str();
}

template <typename... Args>
std::string PrintFunc(const char *ret, const char *name, Args... args)
{
    std::ostringstream out;
    out << ret << " " << name << "(" << ArgsToString(args...) << ")";
    return out.str();
}

static void VoidNoArg(Method *method)
{
    g_call_result = PrintFunc("void", __FUNCTION__, method);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, InvokeVoidNoArg)
{
    uint16_t *shorty = MakeShorty({TypeId::VOID});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(VoidNoArg));
    Frame *frame = CreateFrame(0, nullptr, nullptr);

    uint8_t insn[] = {static_cast<uint8_t>(Opcode::CALL_SHORT_V4_V4_ID16), 0x00, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "VoidNoArg", &callee));

    uint8_t insn2[] = {static_cast<uint8_t>(Opcode::CALL_ACC_SHORT_V4_IMM4_ID16), 0x00, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(insn2, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "VoidNoArg", &callee));

    g_call_result = "";
    InvokeCompiledCodeWithArgArray(nullptr, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "VoidNoArg", &callee));

    FreeFrame(frame);
}

static void InstanceVoidNoArg(Method *method, ObjectHeader *this_)
{
    g_call_result = PrintFunc("void", __FUNCTION__, method, this_);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, InvokeInstanceVoidNoArg)
{
    uint16_t *shorty = MakeShorty({TypeId::VOID});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), 0, 1, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(InstanceVoidNoArg));
    Frame *frame = CreateFrame(1, nullptr, nullptr);
    frame->GetAcc().SetReference(reinterpret_cast<ObjectHeader *>(5));
    frame->GetVReg(0).SetReference(reinterpret_cast<ObjectHeader *>(4));

    uint8_t insn[] = {static_cast<uint8_t>(Opcode::CALL_SHORT_V4_V4_ID16), 0x00, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "InstanceVoidNoArg", &callee, reinterpret_cast<const void *>(4)));

    uint8_t insn2[] = {static_cast<uint8_t>(Opcode::CALL_ACC_SHORT_V4_IMM4_ID16), 0x00, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(insn2, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "InstanceVoidNoArg", &callee, reinterpret_cast<const void *>(5)));

    g_call_result = "";
    int64_t args[] = {4};
    InvokeCompiledCodeWithArgArray(args, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "InstanceVoidNoArg", &callee, reinterpret_cast<const void *>(4)));

    FreeFrame(frame);
}

static uint8_t ByteNoArg(Method *method)
{
    g_call_result = PrintFunc("uint8_t", __FUNCTION__, method);
    return uint8_t(5U);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, InvokeByteNoArg)
{
    uint16_t *shorty = MakeShorty({TypeId::U8});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(ByteNoArg));
    Frame *frame = CreateFrame(0, nullptr, nullptr);
    uint8_t insn[] = {static_cast<uint8_t>(Opcode::CALL_SHORT_V4_V4_ID16), 0x00, 0, 0, 0, 0};

    g_call_result = "";
    InterpreterToCompiledCodeBridge(insn, frame, &callee, thread_);
    EXPECT_EQ(g_call_result, PrintFunc("uint8_t", "ByteNoArg", &callee));
    EXPECT_EQ(frame->GetAcc().Get(), uint8_t(5));
    EXPECT_EQ(frame->GetAcc().GetTag(), 0);

    uint8_t insn_acc[] = {static_cast<uint8_t>(Opcode::CALL_ACC_SHORT_V4_IMM4_ID16), 0x00, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(insn_acc, frame, &callee, thread_);
    EXPECT_EQ(g_call_result, PrintFunc("uint8_t", "ByteNoArg", &callee));
    EXPECT_EQ(frame->GetAcc().Get(), uint8_t(5));
    EXPECT_EQ(frame->GetAcc().GetTag(), 0);

    g_call_result = "";
    DecodedTaggedValue res = InvokeCompiledCodeWithArgArray(nullptr, frame, &callee, thread_);
    EXPECT_EQ(int32_t(res.value), uint8_t(5));
    EXPECT_EQ(res.tag, 0UL);
    EXPECT_EQ(g_call_result, PrintFunc("uint8_t", "ByteNoArg", &callee));

    FreeFrame(frame);
}

static int8_t SignedByteNoArg(Method *method)
{
    g_call_result = PrintFunc("int8_t", __FUNCTION__, method);
    return int8_t(-5U);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, InvokeSignedByteNoArg)
{
    uint16_t *shorty = MakeShorty({TypeId::I8});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(SignedByteNoArg));
    Frame *frame = CreateFrame(0, nullptr, nullptr);
    uint8_t insn[] = {static_cast<uint8_t>(Opcode::CALL_SHORT_V4_V4_ID16), 0x00, 0, 0, 0, 0};

    g_call_result = "";
    InterpreterToCompiledCodeBridge(insn, frame, &callee, thread_);
    EXPECT_EQ(g_call_result, PrintFunc("int8_t", "SignedByteNoArg", &callee));
    EXPECT_EQ(frame->GetAcc().Get(), int8_t(-5));
    EXPECT_EQ(frame->GetAcc().GetTag(), 0);

    uint8_t insn_acc[] = {static_cast<uint8_t>(Opcode::CALL_ACC_SHORT_V4_IMM4_ID16), 0x00, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(insn_acc, frame, &callee, thread_);
    EXPECT_EQ(g_call_result, PrintFunc("int8_t", "SignedByteNoArg", &callee));
    EXPECT_EQ(frame->GetAcc().Get(), int8_t(-5));
    EXPECT_EQ(frame->GetAcc().GetTag(), 0);

    g_call_result = "";
    DecodedTaggedValue res = InvokeCompiledCodeWithArgArray(nullptr, frame, &callee, thread_);
    EXPECT_EQ(int32_t(res.value), int8_t(-5));
    EXPECT_EQ(res.tag, 0UL);
    EXPECT_EQ(g_call_result, PrintFunc("int8_t", "SignedByteNoArg", &callee));

    FreeFrame(frame);
}

static bool BoolNoArg(Method *method)
{
    g_call_result = PrintFunc("bool", __FUNCTION__, method);
    return true;
}

TEST_F(InterpreterToCompiledCodeBridgeTest, InvokeBoolNoArg)
{
    uint16_t *shorty = MakeShorty({TypeId::U1});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(BoolNoArg));
    Frame *frame = CreateFrame(0, nullptr, nullptr);
    uint8_t insn[] = {static_cast<uint8_t>(Opcode::CALL_SHORT_V4_V4_ID16), 0x00, 0, 0, 0, 0};

    g_call_result = "";
    InterpreterToCompiledCodeBridge(insn, frame, &callee, thread_);
    EXPECT_EQ(g_call_result, PrintFunc("bool", "BoolNoArg", &callee));
    EXPECT_EQ(frame->GetAcc().Get(), true);
    EXPECT_EQ(frame->GetAcc().GetTag(), 0);

    g_call_result = "";
    DecodedTaggedValue res = InvokeCompiledCodeWithArgArray(nullptr, frame, &callee, thread_);
    EXPECT_EQ(int32_t(res.value), true);
    EXPECT_EQ(res.tag, 0UL);
    EXPECT_EQ(g_call_result, PrintFunc("bool", "BoolNoArg", &callee));

    FreeFrame(frame);
}

static uint16_t ShortNoArg(Method *method)
{
    g_call_result = PrintFunc("uint16_t", __FUNCTION__, method);
    return uint16_t(5U);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, InvokeShortNoArg)
{
    uint16_t *shorty = MakeShorty({TypeId::U16});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(ShortNoArg));
    Frame *frame = CreateFrame(0, nullptr, nullptr);
    uint8_t insn[] = {static_cast<uint8_t>(Opcode::CALL_SHORT_V4_V4_ID16), 0x00, 0, 0, 0, 0};

    g_call_result = "";
    InterpreterToCompiledCodeBridge(insn, frame, &callee, thread_);
    EXPECT_EQ(g_call_result, PrintFunc("uint16_t", "ShortNoArg", &callee));
    EXPECT_EQ(frame->GetAcc().Get(), uint16_t(5));
    EXPECT_EQ(frame->GetAcc().GetTag(), 0);

    g_call_result = "";
    DecodedTaggedValue res = InvokeCompiledCodeWithArgArray(nullptr, frame, &callee, thread_);
    EXPECT_EQ(int32_t(res.value), uint16_t(5));
    EXPECT_EQ(res.tag, 0UL);
    EXPECT_EQ(g_call_result, PrintFunc("uint16_t", "ShortNoArg", &callee));

    FreeFrame(frame);
}

static int16_t SignedShortNoArg(Method *method)
{
    g_call_result = PrintFunc("int16_t", __FUNCTION__, method);
    return int16_t(-5U);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, InvokeSignedShortNoArg)
{
    uint16_t *shorty = MakeShorty({TypeId::I16});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(SignedShortNoArg));
    Frame *frame = CreateFrame(0, nullptr, nullptr);
    uint8_t insn[] = {static_cast<uint8_t>(Opcode::CALL_SHORT_V4_V4_ID16), 0x00, 0, 0, 0, 0};

    g_call_result = "";
    InterpreterToCompiledCodeBridge(insn, frame, &callee, thread_);
    EXPECT_EQ(g_call_result, PrintFunc("int16_t", "SignedShortNoArg", &callee));
    EXPECT_EQ(frame->GetAcc().Get(), int16_t(-5));
    EXPECT_EQ(frame->GetAcc().GetTag(), 0);

    g_call_result = "";
    DecodedTaggedValue res = InvokeCompiledCodeWithArgArray(nullptr, frame, &callee, thread_);
    EXPECT_EQ(int32_t(res.value), int16_t(-5));
    EXPECT_EQ(res.tag, 0UL);
    EXPECT_EQ(g_call_result, PrintFunc("int16_t", "SignedShortNoArg", &callee));

    FreeFrame(frame);
}

static int32_t IntNoArg(Method *method)
{
    g_call_result = PrintFunc("int32_t", __FUNCTION__, method);
    return 5U;
}

TEST_F(InterpreterToCompiledCodeBridgeTest, InvokeIntNoArg)
{
    uint16_t *shorty = MakeShorty({TypeId::I32});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(IntNoArg));
    Frame *frame = CreateFrame(0, nullptr, nullptr);
    uint8_t insn[] = {static_cast<uint8_t>(Opcode::CALL_SHORT_V4_V4_ID16), 0x00, 0, 0, 0, 0};

    g_call_result = "";
    InterpreterToCompiledCodeBridge(insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("int32_t", "IntNoArg", &callee));
    ASSERT_EQ(frame->GetAcc().Get(), 5);
    EXPECT_EQ(frame->GetAcc().GetTag(), 0);

    g_call_result = "";
    DecodedTaggedValue res = InvokeCompiledCodeWithArgArray(nullptr, frame, &callee, thread_);
    ASSERT_EQ(res.value, 5);
    EXPECT_EQ(res.tag, 0UL);
    ASSERT_EQ(g_call_result, PrintFunc("int32_t", "IntNoArg", &callee));

    FreeFrame(frame);
}

static int64_t LongNoArg(Method *method)
{
    g_call_result = PrintFunc("int64_t", __FUNCTION__, method);
    return 8U;
}

TEST_F(InterpreterToCompiledCodeBridgeTest, InvokeLongNoArg)
{
    uint16_t *shorty = MakeShorty({TypeId::I64});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(LongNoArg));
    Frame *frame = CreateFrame(0, nullptr, nullptr);
    uint8_t insn[] = {static_cast<uint8_t>(Opcode::CALL_SHORT_V4_V4_ID16), 0x00, 0, 0, 0, 0};

    g_call_result = "";
    InterpreterToCompiledCodeBridge(insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("int64_t", "LongNoArg", &callee));
    ASSERT_EQ(frame->GetAcc().Get(), 8);
    EXPECT_EQ(frame->GetAcc().GetTag(), 0);

    g_call_result = "";
    DecodedTaggedValue res = InvokeCompiledCodeWithArgArray(nullptr, frame, &callee, thread_);
    ASSERT_EQ(res.value, 8);
    EXPECT_EQ(res.tag, 0UL);
    ASSERT_EQ(g_call_result, PrintFunc("int64_t", "LongNoArg", &callee));

    FreeFrame(frame);
}

static double DoubleNoArg(Method *method)
{
    g_call_result = PrintFunc("double", __FUNCTION__, method);
    return 3.0;  // 3.0 - test constant
}

TEST_F(InterpreterToCompiledCodeBridgeTest, InvokeDoubleNoArg)
{
    uint16_t *shorty = MakeShorty({TypeId::F64});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(DoubleNoArg));
    Frame *frame = CreateFrame(0, nullptr, nullptr);
    uint8_t insn[] = {static_cast<uint8_t>(Opcode::CALL_SHORT_V4_V4_ID16), 0x00, 0, 0, 0, 0};

    g_call_result = "";
    InterpreterToCompiledCodeBridge(insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("double", "DoubleNoArg", &callee));
    ASSERT_EQ(frame->GetAcc().GetDouble(), 3.0);
    EXPECT_EQ(frame->GetAcc().GetTag(), 0);

    g_call_result = "";
    DecodedTaggedValue res = InvokeCompiledCodeWithArgArray(nullptr, frame, &callee, thread_);
    ASSERT_EQ(bit_cast<double>(res.value), 3.0);
    EXPECT_EQ(res.tag, 0UL);
    ASSERT_EQ(g_call_result, PrintFunc("double", "DoubleNoArg", &callee));

    FreeFrame(frame);
}

static ObjectHeader *ObjNoArg(Method *method)
{
    g_call_result = PrintFunc("Object", __FUNCTION__, method);
    return nullptr;
}

TEST_F(InterpreterToCompiledCodeBridgeTest, InvokeObjNoArg)
{
    uint16_t *shorty = MakeShorty({TypeId::REFERENCE});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(ObjNoArg));
    Frame *frame = CreateFrame(0, nullptr, nullptr);
    uint8_t insn[] = {static_cast<uint8_t>(Opcode::CALL_SHORT_V4_V4_ID16), 0x00, 0, 0, 0, 0};

    g_call_result = "";
    InterpreterToCompiledCodeBridge(insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("Object", "ObjNoArg", &callee));
    ASSERT_EQ(frame->GetAcc().GetReference(), nullptr);
    EXPECT_EQ(frame->GetAcc().GetTag(), 1);

    g_call_result = "";
    DecodedTaggedValue res = InvokeCompiledCodeWithArgArray(nullptr, frame, &callee, thread_);
    ASSERT_EQ(reinterpret_cast<ObjectHeader *>(res.value), nullptr);
    EXPECT_EQ(res.tag, 1UL);
    ASSERT_EQ(g_call_result, PrintFunc("Object", "ObjNoArg", &callee));

    FreeFrame(frame);
}

static DecodedTaggedValue VRegNoArg(Method *method)
{
    g_call_result = PrintFunc("vreg", __FUNCTION__, method);
    return DecodedTaggedValue(5U, 7U);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, InvokeVRegNoArg)
{
    uint16_t *shorty = MakeShorty({TypeId::TAGGED});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(VRegNoArg));
    Frame *frame = CreateFrame(0, nullptr, nullptr);
    uint8_t insn[] = {static_cast<uint8_t>(Opcode::CALL_SHORT_V4_V4_ID16), 0x00, 0, 0, 0, 0};

    g_call_result = "";
    InterpreterToCompiledCodeBridge(insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("vreg", "VRegNoArg", &callee));
    ASSERT_EQ(frame->GetAcc().GetValue(), 5);
    ASSERT_EQ(frame->GetAcc().GetTag(), 7);

    g_call_result = "";
    DecodedTaggedValue res = InvokeCompiledCodeWithArgArray(nullptr, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("vreg", "VRegNoArg", &callee));
    ASSERT_EQ(res.value, 5);
    ASSERT_EQ(res.tag, 7);

    FreeFrame(frame);
}

static void VoidInt(Method *method, int32_t a0)
{
    g_call_result = PrintFunc("void", __FUNCTION__, method, a0);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, InvokeInt)
{
    uint16_t *shorty = MakeShorty({TypeId::VOID, TypeId::I32});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(VoidInt));
    Frame *frame = CreateFrame(2, nullptr, nullptr);
    frame->GetVReg(1).Set(5);

    uint8_t call_short_insn[] = {static_cast<uint8_t>(Opcode::CALL_SHORT_V4_V4_ID16), 0x01, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_short_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "VoidInt", &callee, 5));

    uint8_t call_range_insn[] = {static_cast<uint8_t>(Opcode::CALL_RANGE_V8_ID16), 0x01, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_range_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "VoidInt", &callee, 5));

    g_call_result = "";
    int64_t arg = 5;
    InvokeCompiledCodeWithArgArray(&arg, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "VoidInt", &callee, 5));

    frame->GetVReg(1).Set(0);
    frame->GetAcc().Set(5);
    uint8_t call_acc_insn[] = {static_cast<uint8_t>(Opcode::CALL_ACC_SHORT_V4_IMM4_ID16), 0x00, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_acc_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "VoidInt", &callee, 5));

    FreeFrame(frame);
}

static void InstanceVoidInt(Method *method, ObjectHeader *this_, int32_t a0)
{
    g_call_result = PrintFunc("void", __FUNCTION__, method, this_, a0);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, InvokeInstanceInt)
{
    uint16_t *shorty = MakeShorty({TypeId::VOID, TypeId::I32});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), 0, 0, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(InstanceVoidInt));
    Frame *frame = CreateFrame(2, nullptr, nullptr);
    frame->GetVReg(0).SetReference(reinterpret_cast<ObjectHeader *>(4));
    frame->GetVReg(1).Set(5);

    uint8_t call_short_insn[] = {static_cast<uint8_t>(Opcode::CALL_SHORT_V4_V4_ID16), 0x10, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_short_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "InstanceVoidInt", &callee, reinterpret_cast<const void *>(4), 5));

    uint8_t call_range_insn[] = {static_cast<uint8_t>(Opcode::CALL_RANGE_V8_ID16), 0x00, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_range_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "InstanceVoidInt", &callee, reinterpret_cast<const void *>(4), 5));

    g_call_result = "";
    int64_t args[] = {4, 5};
    InvokeCompiledCodeWithArgArray(args, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "InstanceVoidInt", &callee, reinterpret_cast<const void *>(4), 5));

    frame->GetVReg(1).Set(0);
    frame->GetAcc().Set(5);
    uint8_t call_acc_insn[] = {static_cast<uint8_t>(Opcode::CALL_ACC_SHORT_V4_IMM4_ID16), 0x10, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_acc_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "InstanceVoidInt", &callee, reinterpret_cast<const void *>(4), 5));

    FreeFrame(frame);
}

static void VoidVReg(Method *method, int64_t value, int64_t tag)
{
    g_call_result = PrintFunc("void", __FUNCTION__, method, value, tag);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, InvokeVReg)
{
    uint16_t *shorty = MakeShorty({TypeId::VOID, TypeId::TAGGED});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(VoidVReg));
    Frame *frame = CreateFrame(2, nullptr, nullptr);
    frame->GetVReg(1).SetValue(5);
    frame->GetVReg(1).SetTag(8);

    uint8_t call_short_insn[] = {static_cast<uint8_t>(Opcode::CALL_SHORT_V4_V4_ID16), 0x01, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_short_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "VoidVReg", &callee, 5, 8));

    uint8_t call_range_insn[] = {static_cast<uint8_t>(Opcode::CALL_RANGE_V8_ID16), 0x01, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_range_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "VoidVReg", &callee, 5, 8));

    g_call_result = "";
    int64_t arg[] = {5U, 8};
    InvokeCompiledCodeWithArgArray(arg, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "VoidVReg", &callee, 5, 8));

    frame->GetVReg(1).SetValue(0);
    frame->GetVReg(1).SetTag(0);
    frame->GetAcc().SetValue(5);
    frame->GetAcc().SetTag(8);
    uint8_t call_acc_short[] = {static_cast<uint8_t>(Opcode::CALL_ACC_SHORT_V4_IMM4_ID16), 0x01, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_acc_short, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "VoidVReg", &callee, 5U, 8U));
    FreeFrame(frame);
}

static void VoidIntVReg(Method *method, int32_t a0, int64_t value, int64_t tag)
{
    g_call_result = PrintFunc("void", __FUNCTION__, method, a0, value, tag);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, InvokeIntVReg)
{
    uint16_t *shorty = MakeShorty({TypeId::VOID, TypeId::I32, TypeId::TAGGED});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(VoidIntVReg));
    Frame *frame = CreateFrame(2, nullptr, nullptr);
    frame->GetVReg(0).SetValue(2U);
    frame->GetVReg(0).SetTag(0);
    frame->GetVReg(1).SetValue(5);
    frame->GetVReg(1).SetTag(8);

    uint8_t call_short_insn[] = {static_cast<uint8_t>(Opcode::CALL_SHORT_V4_V4_ID16), 0x10, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_short_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "VoidIntVReg", &callee, 2, 5, 8));

    uint8_t call_range_insn[] = {static_cast<uint8_t>(Opcode::CALL_RANGE_V8_ID16), 0x00, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_range_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "VoidIntVReg", &callee, 2, 5, 8));

    g_call_result = "";
    int64_t arg[] = {2, 5U, 8};
    InvokeCompiledCodeWithArgArray(arg, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "VoidIntVReg", &callee, 2, 5, 8));

    frame->GetAcc().SetValue(5);
    frame->GetAcc().SetTag(8);
    frame->GetVReg(1).SetValue(0);
    frame->GetVReg(1).SetTag(0);
    uint8_t call_acc_short_insn[] = {static_cast<uint8_t>(Opcode::CALL_ACC_SHORT_V4_IMM4_ID16), 0x10, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_acc_short_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "VoidIntVReg", &callee, 2, 5, 8));

    FreeFrame(frame);
}

// arm max number of register parameters
static void Void3Int(Method *method, int32_t a0, int32_t a1, int32_t a2)
{
    g_call_result = PrintFunc("void", __FUNCTION__, method, a0, a1, a2);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, Invoke3Int)
{
    uint16_t *shorty = MakeShorty({TypeId::VOID, TypeId::I32, TypeId::I32, TypeId::I32});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(Void3Int));
    Frame *frame = CreateFrame(3, nullptr, nullptr);
    frame->GetAcc().Set(0);
    frame->GetVReg(0).Set(1);
    frame->GetVReg(1).Set(2);
    frame->GetVReg(2U).Set(3);

    uint8_t call_insn[] = {static_cast<uint8_t>(Opcode::CALL_V4_V4_V4_V4_ID16), 0x10, 0x02, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void3Int", &callee, 1, 2, 3));

    // callee(acc, v1, v2)
    uint8_t call_acc_insn[] = {static_cast<uint8_t>(Opcode::CALL_ACC_V4_V4_V4_IMM4_ID16), 0x21, 0x00, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_acc_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void3Int", &callee, 0, 2, 3));

    uint8_t call_range_insn[] = {static_cast<uint8_t>(Opcode::CALL_RANGE_V8_ID16), 0x00, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_range_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void3Int", &callee, 1, 2, 3));

    int64_t args[] = {1, 2, 3};
    g_call_result = "";
    InvokeCompiledCodeWithArgArray(args, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void3Int", &callee, 1, 2, 3));

    FreeFrame(frame);
}

static void Void2IntLongInt(Method *method, int32_t a0, int32_t a1, int64_t a2, int32_t a3)
{
    g_call_result = PrintFunc("void", __FUNCTION__, method, a0, a1, a2, a3);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, Invoke2IntLongInt)
{
    uint16_t *shorty = MakeShorty({TypeId::VOID, TypeId::I32, TypeId::I32, TypeId::I64, TypeId::I32});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(Void2IntLongInt));
    Frame *frame = CreateFrame(4, nullptr, nullptr);
    frame->GetVReg(0).Set(1);
    frame->GetVReg(1).Set(2);
    frame->GetVReg(2U).Set(3);
    frame->GetVReg(3U).Set(4);

    uint8_t call_insn[] = {static_cast<uint8_t>(Opcode::CALL_V4_V4_V4_V4_ID16), 0x10, 0x32, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void2IntLongInt", &callee, 1, 2, 3, 4));

    uint8_t call_range_insn[] = {static_cast<uint8_t>(Opcode::CALL_RANGE_V8_ID16), 0x00, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_range_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void2IntLongInt", &callee, 1, 2, 3, 4));

    int64_t args[] = {1, 2, 3, 4};
    g_call_result = "";
    InvokeCompiledCodeWithArgArray(args, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void2IntLongInt", &callee, 1, 2, 3, 4));

    frame->GetVReg(2U).Set(0);
    frame->GetAcc().Set(3);
    uint8_t call_acc_insn[] = {static_cast<uint8_t>(Opcode::CALL_ACC_V4_V4_V4_IMM4_ID16), 0x10, 0x23, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_acc_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void2IntLongInt", &callee, 1, 2, 3, 4));

    FreeFrame(frame);
}

static void VoidLong(Method *method, int64_t a0)
{
    g_call_result = PrintFunc("void", __FUNCTION__, method, a0);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, InvokeLong)
{
    uint16_t *shorty = MakeShorty({TypeId::VOID, TypeId::I64});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(VoidLong));
    Frame *frame = CreateFrame(1, nullptr, nullptr);
    frame->GetVReg(0).Set(9);

    uint8_t call_insn[] = {static_cast<uint8_t>(Opcode::CALL_SHORT_V4_V4_ID16), 0x00, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "VoidLong", &callee, 9));

    uint8_t call_range_insn[] = {static_cast<uint8_t>(Opcode::CALL_RANGE_V8_ID16), 0x00, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_range_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "VoidLong", &callee, 9));

    int64_t args[] = {9};
    g_call_result = "";
    InvokeCompiledCodeWithArgArray(args, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "VoidLong", &callee, 9));

    FreeFrame(frame);
}

static void VoidDouble(Method *method, double a0)
{
    g_call_result = PrintFunc("void", __FUNCTION__, method, a0);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, InvokeDouble)
{
    uint16_t *shorty = MakeShorty({TypeId::VOID, TypeId::F64});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(VoidDouble));
    Frame *frame = CreateFrame(1, nullptr, nullptr);
    frame->GetVReg(0).Set(4.0);

    uint8_t call_insn[] = {static_cast<uint8_t>(Opcode::CALL_SHORT_V4_V4_ID16), 0x00, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "VoidDouble", &callee, 4.0));

    uint8_t call_range_insn[] = {static_cast<uint8_t>(Opcode::CALL_RANGE_V8_ID16), 0x00, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_range_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "VoidDouble", &callee, 4.0));

    int64_t args[] = {bit_cast<int64_t>(4.0)};
    g_call_result = "";
    InvokeCompiledCodeWithArgArray(args, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "VoidDouble", &callee, 4.0));

    FreeFrame(frame);
}

static void Void4Int(Method *method, int32_t a0, int32_t a1, int32_t a2, int32_t a3)
{
    g_call_result = PrintFunc("void", __FUNCTION__, method, a0, a1, a2, a3);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, Invoke4Int)
{
    uint16_t *shorty = MakeShorty({TypeId::VOID, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(Void4Int));
    Frame *frame = CreateFrame(4, nullptr, nullptr);
    frame->GetVReg(0).Set(1);
    frame->GetVReg(1).Set(2);
    frame->GetVReg(2U).Set(3);
    frame->GetVReg(3U).Set(4);

    uint8_t call_insn[] = {static_cast<uint8_t>(Opcode::CALL_V4_V4_V4_V4_ID16), 0x10, 0x32, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void4Int", &callee, 1, 2, 3, 4));

    uint8_t call_range_insn[] = {static_cast<uint8_t>(Opcode::CALL_RANGE_V8_ID16), 0x00, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_range_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void4Int", &callee, 1, 2, 3, 4));

    int64_t args[] = {1, 2, 3, 4};
    g_call_result = "";
    InvokeCompiledCodeWithArgArray(args, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void4Int", &callee, 1, 2, 3, 4));

    frame->GetVReg(3U).Set(0);
    frame->GetAcc().Set(4);
    uint8_t call_acc_insn[] = {static_cast<uint8_t>(Opcode::CALL_ACC_V4_V4_V4_IMM4_ID16), 0x10, 0x32, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_acc_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void4Int", &callee, 1, 2, 3, 4));

    FreeFrame(frame);
}

static void Void2Long(Method *method, int64_t a0, int64_t a1)
{
    g_call_result = PrintFunc("void", __FUNCTION__, method, a0, a1);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, Invoke2Long)
{
    uint16_t *shorty = MakeShorty({TypeId::VOID, TypeId::I64, TypeId::I64});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(Void2Long));
    Frame *frame = CreateFrame(2, nullptr, nullptr);
    frame->GetVReg(0).Set(3);
    frame->GetVReg(1).Set(9);

    uint8_t call_insn[] = {static_cast<uint8_t>(Opcode::CALL_SHORT_V4_V4_ID16), 0x10, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void2Long", &callee, 3, 9));

    uint8_t call_range_insn[] = {static_cast<uint8_t>(Opcode::CALL_RANGE_V8_ID16), 0x00, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_range_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void2Long", &callee, 3, 9));

    int64_t args[] = {3, 9};
    g_call_result = "";
    InvokeCompiledCodeWithArgArray(args, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void2Long", &callee, 3, 9));

    FreeFrame(frame);
}

static void Void4IntDouble(Method *method, int32_t a0, int32_t a1, int32_t a2, int32_t a3, double a4)
{
    g_call_result = PrintFunc("void", __FUNCTION__, method, a0, a1, a2, a3, a4);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, Invoke4IntDouble)
{
    uint16_t *shorty = MakeShorty({TypeId::VOID, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::F64});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(Void4IntDouble));
    Frame *frame = CreateFrame(5U, nullptr, nullptr);
    frame->GetVReg(0).Set(1);
    frame->GetVReg(1).Set(2);
    frame->GetVReg(2U).Set(3);
    frame->GetVReg(3U).Set(4);
    frame->GetVReg(4U).Set(5.0);

    uint8_t call_range_insn[] = {static_cast<uint8_t>(Opcode::CALL_RANGE_V8_ID16), 0x00, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_range_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void4IntDouble", &callee, 1, 2, 3, 4, 5.0));

    int64_t args[] = {1, 2, 3, 4, bit_cast<int64_t>(5.0)};
    g_call_result = "";
    InvokeCompiledCodeWithArgArray(args, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void4IntDouble", &callee, 1, 2, 3, 4, 5.0));

    FreeFrame(frame);
}

// aarch64 max number of register parameters
static void Void7Int(Method *method, int32_t a0, int32_t a1, int32_t a2, int32_t a3, int32_t a4, int32_t a5U,
                     int32_t a6)
{
    g_call_result = PrintFunc("void", __FUNCTION__, method, a0, a1, a2, a3, a4, a5U, a6);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, Invoke7Int)
{
    uint16_t *shorty = MakeShorty(
        {TypeId::VOID, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(Void7Int));
    Frame *frame = CreateFrame(7, nullptr, nullptr);
    frame->GetVReg(0).Set(1);
    frame->GetVReg(1).Set(2);
    frame->GetVReg(2U).Set(3);
    frame->GetVReg(3U).Set(4);
    frame->GetVReg(4U).Set(5);
    frame->GetVReg(5U).Set(6);
    frame->GetVReg(6U).Set(7);

    uint8_t call_range_insn[] = {static_cast<uint8_t>(Opcode::CALL_RANGE_V8_ID16), 0x00, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_range_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void7Int", &callee, 1, 2, 3, 4, 5, 6, 7));

    int64_t args[] = {1, 2, 3, 4, 5, 6, 7};
    g_call_result = "";
    InvokeCompiledCodeWithArgArray(args, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void7Int", &callee, 1, 2, 3, 4, 5, 6, 7));

    FreeFrame(frame);
}

static void Void7Int8Double(Method *method, int32_t a0, int32_t a1, int32_t a2, int32_t a3, int32_t a4, int32_t a5U,
                            int32_t a6, double d0, double d1, double d2, double d3, double d4, double d5U, double d6,
                            double d7)
{
    g_call_result =
        PrintFunc("void", __FUNCTION__, method, a0, a1, a2, a3, a4, a5U, a6, d0, d1, d2, d3, d4, d5U, d6, d7);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, Invoke7Int8Double)
{
    uint16_t *shorty = MakeShorty({TypeId::VOID, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32,
                                   TypeId::I32, TypeId::I32, TypeId::F64, TypeId::F64, TypeId::F64, TypeId::F64,
                                   TypeId::F64, TypeId::F64, TypeId::F64, TypeId::F64});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(Void7Int8Double));
    Frame *frame = CreateFrame(15U, nullptr, nullptr);
    frame->GetVReg(0).Set(1);
    frame->GetVReg(1).Set(2);
    frame->GetVReg(2U).Set(3);
    frame->GetVReg(3U).Set(4);
    frame->GetVReg(4U).Set(5);
    frame->GetVReg(5U).Set(6);
    frame->GetVReg(6U).Set(7);
    frame->GetVReg(7U).Set(8.0);
    frame->GetVReg(8U).Set(9.0);
    frame->GetVReg(9U).Set(10.0);
    frame->GetVReg(10U).Set(11.0);
    frame->GetVReg(11U).Set(12.0);
    frame->GetVReg(12U).Set(13.0);
    frame->GetVReg(13U).Set(14.0);
    frame->GetVReg(14U).Set(15.0);

    uint8_t call_range_insn[] = {static_cast<uint8_t>(Opcode::CALL_RANGE_V8_ID16), 0x00, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_range_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void7Int8Double", &callee, 1, 2, 3, 4, 5, 6, 7, 8.0, 9.0, 10.0, 11.0,
                                       12.0, 13.0, 14.0, 15.0));

    int64_t args[] = {1,
                      2U,
                      3U,
                      4U,
                      5U,
                      6U,
                      7U,
                      bit_cast<int64_t>(8.0),
                      bit_cast<int64_t>(9.0),
                      bit_cast<int64_t>(10.0),
                      bit_cast<int64_t>(11.0),
                      bit_cast<int64_t>(12.0),
                      bit_cast<int64_t>(13.0),
                      bit_cast<int64_t>(14.0),
                      bit_cast<int64_t>(15.0)};
    g_call_result = "";
    InvokeCompiledCodeWithArgArray(args, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void7Int8Double", &callee, 1, 2, 3, 4, 5, 6, 7, 8.0, 9.0, 10.0, 11.0,
                                       12.0, 13.0, 14.0, 15.0));

    FreeFrame(frame);
}

static void Void8Int(Method *method, int32_t a0, int32_t a1, int32_t a2, int32_t a3, int32_t a4, int32_t a5U,
                     int32_t a6, int32_t a7)
{
    g_call_result = PrintFunc("void", __FUNCTION__, method, a0, a1, a2, a3, a4, a5U, a6, a7);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, Invoke8Int)
{
    uint16_t *shorty = MakeShorty({TypeId::VOID, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32,
                                   TypeId::I32, TypeId::I32, TypeId::I32});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(Void8Int));
    Frame *frame = CreateFrame(8, nullptr, nullptr);
    frame->GetVReg(0).Set(1);
    frame->GetVReg(1).Set(2);
    frame->GetVReg(2U).Set(3);
    frame->GetVReg(3U).Set(4);
    frame->GetVReg(4U).Set(5);
    frame->GetVReg(5U).Set(6);
    frame->GetVReg(6U).Set(7);
    frame->GetVReg(7U).Set(8);

    uint8_t call_range_insn[] = {static_cast<uint8_t>(Opcode::CALL_RANGE_V8_ID16), 0x00, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_range_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void8Int", &callee, 1, 2, 3, 4, 5, 6, 7, 8));

    int64_t args[] = {1, 2, 3, 4, 5, 6, 7, 8};
    g_call_result = "";
    InvokeCompiledCodeWithArgArray(args, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void8Int", &callee, 1, 2, 3, 4, 5, 6, 7, 8));

    FreeFrame(frame);
}

static void Void6IntVReg(Method *method, int32_t a0, int32_t a1, int32_t a2, int32_t a3, int32_t a4, int32_t a5U,
                         int64_t value, int64_t tag)
{
    g_call_result = PrintFunc("void", __FUNCTION__, method, a0, a1, a2, a3, a4, a5U, value, tag);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, Invoke6IntVReg)
{
    uint16_t *shorty = MakeShorty(
        {TypeId::VOID, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::TAGGED});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(Void6IntVReg));
    Frame *frame = CreateFrame(8, nullptr, nullptr);
    frame->GetVReg(0).Set(1);
    frame->GetVReg(1).Set(2);
    frame->GetVReg(2U).Set(3);
    frame->GetVReg(3U).Set(4);
    frame->GetVReg(4U).Set(5);
    frame->GetVReg(5U).Set(6);
    frame->GetVReg(6U).SetValue(7);
    frame->GetVReg(6U).SetTag(8);

    uint8_t call_range_insn[] = {static_cast<uint8_t>(Opcode::CALL_RANGE_V8_ID16), 0x00, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_range_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void6IntVReg", &callee, 1, 2, 3, 4, 5, 6, 7, 8));

    int64_t args[] = {1, 2, 3, 4, 5, 6, 7, 8};
    g_call_result = "";
    InvokeCompiledCodeWithArgArray(args, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void6IntVReg", &callee, 1, 2, 3, 4, 5, 6, 7, 8));

    FreeFrame(frame);
}

static void Void7IntVReg(Method *method, int32_t a0, int32_t a1, int32_t a2, int32_t a3, int32_t a4, int32_t a5U,
                         int32_t a6, int64_t value, int64_t tag)
{
    g_call_result = PrintFunc("void", __FUNCTION__, method, a0, a1, a2, a3, a4, a5U, a6, value, tag);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, Invoke7IntVReg)
{
    uint16_t *shorty = MakeShorty({TypeId::VOID, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32,
                                   TypeId::I32, TypeId::I32, TypeId::TAGGED});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(Void7IntVReg));
    Frame *frame = CreateFrame(8, nullptr, nullptr);
    frame->GetVReg(0).Set(1);
    frame->GetVReg(1).Set(2);
    frame->GetVReg(2U).Set(3);
    frame->GetVReg(3U).Set(4);
    frame->GetVReg(4U).Set(5);
    frame->GetVReg(5U).Set(6);
    frame->GetVReg(6U).Set(7);
    frame->GetVReg(7U).SetValue(8);
    frame->GetVReg(7U).SetTag(9);

    uint8_t call_range_insn[] = {static_cast<uint8_t>(Opcode::CALL_RANGE_V8_ID16), 0x00, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_range_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void7IntVReg", &callee, 1, 2, 3, 4, 5, 6, 7, 8, 9));

    int64_t args[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    g_call_result = "";
    InvokeCompiledCodeWithArgArray(args, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void7IntVReg", &callee, 1, 2, 3, 4, 5, 6, 7, 8, 9));

    FreeFrame(frame);
}

static void Void8Int9Double(Method *method, int32_t a0, int32_t a1, int32_t a2, int32_t a3, int32_t a4, int32_t a5U,
                            int32_t a6, int32_t a7, double d0, double d1, double d2, double d3, double d4, double d5U,
                            double d6, double d7, double d8)
{
    g_call_result =
        PrintFunc("void", __FUNCTION__, method, a0, a1, a2, a3, a4, a5U, a6, a7, d0, d1, d2, d3, d4, d5U, d6, d7, d8);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, Invoke8Int9Double)
{
    uint16_t *shorty = MakeShorty({TypeId::VOID, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32, TypeId::I32,
                                   TypeId::I32, TypeId::I32, TypeId::I32, TypeId::F64, TypeId::F64, TypeId::F64,
                                   TypeId::F64, TypeId::F64, TypeId::F64, TypeId::F64, TypeId::F64, TypeId::F64});
    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, shorty);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(Void8Int9Double));
    Frame *frame = CreateFrame(17, nullptr, nullptr);
    frame->GetVReg(0).Set(1);
    frame->GetVReg(1).Set(2);
    frame->GetVReg(2U).Set(3);
    frame->GetVReg(3U).Set(4);
    frame->GetVReg(4U).Set(5);
    frame->GetVReg(5U).Set(6);
    frame->GetVReg(6U).Set(7);
    frame->GetVReg(7U).Set(8);
    frame->GetVReg(8U).Set(9.0);
    frame->GetVReg(9U).Set(10.0);
    frame->GetVReg(10U).Set(11.0);
    frame->GetVReg(11U).Set(12.0);
    frame->GetVReg(12U).Set(13.0);
    frame->GetVReg(13U).Set(14.0);
    frame->GetVReg(14U).Set(15.0);
    frame->GetVReg(15U).Set(16.0);
    frame->GetVReg(16U).Set(17.0);

    uint8_t call_range_insn[] = {static_cast<uint8_t>(Opcode::CALL_RANGE_V8_ID16), 0x00, 0, 0, 0, 0};
    g_call_result = "";
    InterpreterToCompiledCodeBridge(call_range_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void8Int9Double", &callee, 1, 2, 3, 4, 5, 6, 7, 8, 9.0, 10.0, 11.0,
                                       12.0, 13.0, 14.0, 15.0, 16.0, 17.0));

    int64_t args[] = {1,
                      2,
                      3,
                      4,
                      5,
                      6,
                      7,
                      8,
                      bit_cast<int64_t>(9.0),
                      bit_cast<int64_t>(10.0),
                      bit_cast<int64_t>(11.0),
                      bit_cast<int64_t>(12.0),
                      bit_cast<int64_t>(13.0),
                      bit_cast<int64_t>(14.0),
                      bit_cast<int64_t>(15.0),
                      bit_cast<int64_t>(16.0),
                      bit_cast<int64_t>(17.0)};
    g_call_result = "";
    InvokeCompiledCodeWithArgArray(args, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("void", "Void8Int9Double", &callee, 1, 2, 3, 4, 5, 6, 7, 8, 9.0, 10.0, 11.0,
                                       12.0, 13.0, 14.0, 15.0, 16.0, 17.0));

    FreeFrame(frame);
}

#if !defined(PANDA_TARGET_ARM32) && !defined(PANDA_TARGET_X86)
static DecodedTaggedValue NoArgDyn(Method *method, uint32_t num_args, int64_t func, int64_t func_tag)
{
    g_call_result = PrintFunc("any", __FUNCTION__, method, num_args, func, func_tag);
    return DecodedTaggedValue(1, 2U);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, InvokeNoArgDyn)
{
    Frame *frame = CreateFrame(1, nullptr, nullptr);
    frame->GetVReg(0).Set(0xABC);
    frame->GetVReg(0).SetTag(0);

    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, nullptr);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(NoArgDyn));

    uint8_t insn[] = {static_cast<uint8_t>(Opcode::CALLI_DYN_SHORT_IMM4_V4_V4_V4), 0x00, 0x00};
    g_call_result = "";
    InterpreterToCompiledCodeBridgeDyn(insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("any", "NoArgDyn", &callee, 0, 0xABC, 0));
    ASSERT_EQ(frame->GetAcc().GetValue(), 1);
    ASSERT_EQ(frame->GetAcc().GetTag(), 2);

    uint8_t range_insn[] = {static_cast<uint8_t>(Opcode::CALLI_DYN_RANGE_IMM16_V16), 0x00, 0x00, 0x00, 0x00};
    g_call_result = "";
    frame->GetAcc().SetValue(0);
    frame->GetAcc().SetTag(0);
    InterpreterToCompiledCodeBridgeDyn(range_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("any", "NoArgDyn", &callee, 0, 0xABC, 0));
    ASSERT_EQ(frame->GetAcc().GetValue(), 1);
    ASSERT_EQ(frame->GetAcc().GetTag(), 2);

    g_call_result = "";
    int64_t args[] = {0xABC, 0x0};
    DecodedTaggedValue res = InvokeCompiledCodeWithArgArrayDyn(args, 0, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("any", "NoArgDyn", &callee, 0, 0xABC, 0));
    ASSERT_EQ(res.value, 1);
    ASSERT_EQ(res.tag, 2);

    FreeFrame(frame);
}

static DecodedTaggedValue OneArgDyn(Method *method, uint32_t num_args, int64_t func, int64_t func_tag, int64_t val,
                                    int64_t tag)
{
    g_call_result = PrintFunc("any", __FUNCTION__, method, num_args, func, func_tag, val, tag);
    return DecodedTaggedValue(3U, 4U);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, InvokeOneArgDyn)
{
    Frame *frame = CreateFrame(2, nullptr, nullptr);
    frame->GetVReg(0).Set(0xABC);
    frame->GetVReg(0).SetTag(0);
    frame->GetVReg(1).SetValue(2);
    frame->GetVReg(1).SetTag(3);

    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, nullptr);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(OneArgDyn));

    uint8_t insn[] = {static_cast<uint8_t>(Opcode::CALLI_DYN_SHORT_IMM4_V4_V4_V4), 0x01, 0x01};
    g_call_result = "";
    InterpreterToCompiledCodeBridgeDyn(insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("any", "OneArgDyn", &callee, 1, 0xABC, 0, 2, 3));
    ASSERT_EQ(frame->GetAcc().GetValue(), 3);
    ASSERT_EQ(frame->GetAcc().GetTag(), 4);

    uint8_t range_insn[] = {static_cast<uint8_t>(Opcode::CALLI_DYN_RANGE_IMM16_V16), 0x01, 0x00, 0x00, 0x00};
    frame->GetAcc().SetValue(0);
    frame->GetAcc().SetTag(0);
    g_call_result = "";
    InterpreterToCompiledCodeBridgeDyn(range_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("any", "OneArgDyn", &callee, 1, 0xABC, 0, 2, 3));
    ASSERT_EQ(frame->GetAcc().GetValue(), 3);
    ASSERT_EQ(frame->GetAcc().GetTag(), 4);

    g_call_result = "";
    int64_t args[] = {0xABC, 0x0, 0x2, 0x3};
    DecodedTaggedValue res = InvokeCompiledCodeWithArgArrayDyn(args, 1, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("any", "OneArgDyn", &callee, 1, 0xABC, 0, 2, 3));
    ASSERT_EQ(res.value, 3);
    ASSERT_EQ(res.tag, 4);

    FreeFrame(frame);
}

static DecodedTaggedValue OneVarArgDyn(Method *method, uint32_t num_args, int64_t func, int64_t func_tag, ...)
{
    DecodedTaggedValue res;
    if (num_args != 1) {
        g_call_result = PrintFunc("any", __FUNCTION__, method, num_args, func, func_tag);
        res = DecodedTaggedValue(0, 0);
    } else {
        va_list args;
        va_start(args, func_tag);
        int64_t val = va_arg(args, int64_t);
        int64_t tag = va_arg(args, int64_t);
        g_call_result = PrintFunc("any", __FUNCTION__, method, num_args, func, func_tag, val, tag);
        va_end(args);
        res = DecodedTaggedValue(5U, 6U);
    }
    return res;
}

TEST_F(InterpreterToCompiledCodeBridgeTest, InvokeOneVarArgDyn)
{
    Frame *frame = CreateFrame(2, nullptr, nullptr);
    frame->GetVReg(0).Set(0xABC);
    frame->GetVReg(0).SetTag(0);
    frame->GetVReg(1).SetValue(2);
    frame->GetVReg(1).SetTag(3);

    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, nullptr);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(OneVarArgDyn));

    uint8_t insn[] = {static_cast<uint8_t>(Opcode::CALLI_DYN_SHORT_IMM4_V4_V4_V4), 0x01, 0x01};
    g_call_result = "";
    InterpreterToCompiledCodeBridgeDyn(insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("any", "OneVarArgDyn", &callee, 1, 0xABC, 0, 2, 3));
    ASSERT_EQ(frame->GetAcc().GetValue(), 5);
    ASSERT_EQ(frame->GetAcc().GetTag(), 6);

    g_call_result = "";
    int64_t args[] = {0xABC, 0x0, 0x2, 0x3};
    DecodedTaggedValue res = InvokeCompiledCodeWithArgArrayDyn(args, 1, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("any", "OneVarArgDyn", &callee, 1, 0xABC, 0, 2, 3));
    ASSERT_EQ(res.value, 5);
    ASSERT_EQ(res.tag, 6);

    FreeFrame(frame);
}

static DecodedTaggedValue TwoArgDyn(Method *method, uint32_t num_args, int64_t func, int64_t func_tag, int64_t val1,
                                    int64_t tag1, int64_t val2, int64_t tag2)
{
    g_call_result = PrintFunc("any", __FUNCTION__, method, num_args, func, func_tag, val1, tag1, val2, tag2);
    return DecodedTaggedValue(1, 3U);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, InvokeTwoArgDyn)
{
    Frame *frame = CreateFrame(3, nullptr, nullptr);
    frame->GetVReg(0).Set(0xABC);
    frame->GetVReg(0).SetTag(0);
    frame->GetVReg(1).SetValue(2);
    frame->GetVReg(1).SetTag(3);
    frame->GetVReg(2U).SetValue(4);
    frame->GetVReg(2U).SetTag(5);

    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, nullptr);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(TwoArgDyn));

    uint8_t insn[] = {static_cast<uint8_t>(Opcode::CALLI_DYN_SHORT_IMM4_V4_V4_V4), 0x02, 0x12};
    g_call_result = "";
    InterpreterToCompiledCodeBridgeDyn(insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("any", "TwoArgDyn", &callee, 2, 0xABC, 0, 4, 5, 2, 3));
    ASSERT_EQ(frame->GetAcc().GetValue(), 1);
    ASSERT_EQ(frame->GetAcc().GetTag(), 3);

    uint8_t range_insn[] = {static_cast<uint8_t>(Opcode::CALLI_DYN_RANGE_IMM16_V16), 0x02, 0x00, 0x00, 0x00};
    g_call_result = "";
    frame->GetAcc().SetValue(0);
    frame->GetAcc().SetTag(0);
    InterpreterToCompiledCodeBridgeDyn(range_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("any", "TwoArgDyn", &callee, 2, 0xABC, 0, 2, 3, 4, 5));
    ASSERT_EQ(frame->GetAcc().GetValue(), 1);
    ASSERT_EQ(frame->GetAcc().GetTag(), 3);

    g_call_result = "";
    int64_t args[] = {0xABC, 0x0, 0x2, 0x3, 0x4, 0x5};
    DecodedTaggedValue res = InvokeCompiledCodeWithArgArrayDyn(args, 2, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("any", "TwoArgDyn", &callee, 2, 0xABC, 0, 2, 3, 4, 5));
    ASSERT_EQ(res.value, 1);
    ASSERT_EQ(res.tag, 3);

    FreeFrame(frame);
}

static DecodedTaggedValue TwoVarArgDyn(Method *method, uint32_t num_args, int64_t func, int64_t func_tag, ...)
{
    DecodedTaggedValue res;
    if (num_args != 2U) {
        g_call_result = PrintFunc("any", __FUNCTION__, method, num_args, func, func_tag);
        res = DecodedTaggedValue(0, 0);
    } else {
        va_list args;
        va_start(args, func_tag);
        int64_t val1 = va_arg(args, int64_t);
        int64_t tag1 = va_arg(args, int64_t);
        int64_t val2 = va_arg(args, int64_t);
        int64_t tag2 = va_arg(args, int64_t);
        g_call_result = PrintFunc("any", __FUNCTION__, method, num_args, func, func_tag, val1, tag1, val2, tag2);
        va_end(args);
        res = DecodedTaggedValue(2U, 5U);
    }
    return res;
}

TEST_F(InterpreterToCompiledCodeBridgeTest, InvokeTwoVarArgDyn)
{
    Frame *frame = CreateFrame(3, nullptr, nullptr);
    frame->GetVReg(0).Set(0xABC);
    frame->GetVReg(0).SetTag(0);
    frame->GetVReg(1).SetValue(2U);
    frame->GetVReg(1).SetTag(3U);
    frame->GetVReg(2U).SetValue(4U);
    frame->GetVReg(2U).SetTag(5U);

    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, nullptr);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(TwoVarArgDyn));

    uint8_t insn[] = {static_cast<uint8_t>(Opcode::CALLI_DYN_SHORT_IMM4_V4_V4_V4), 0x02, 0x21};
    g_call_result = "";
    InterpreterToCompiledCodeBridgeDyn(insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("any", "TwoVarArgDyn", &callee, 2, 0xABC, 0, 2, 3, 4, 5));
    ASSERT_EQ(frame->GetAcc().GetValue(), 2);
    ASSERT_EQ(frame->GetAcc().GetTag(), 5);

    g_call_result = "";
    int64_t args[] = {0xABC, 0x0, 0x2, 0x3, 0x4, 0x5};
    DecodedTaggedValue res = InvokeCompiledCodeWithArgArrayDyn(args, 2, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("any", "TwoVarArgDyn", &callee, 2, 0xABC, 0, 2, 3, 4, 5));
    ASSERT_EQ(res.value, 2);
    ASSERT_EQ(res.tag, 5);

    FreeFrame(frame);
}

static DecodedTaggedValue ThreeArgDyn(Method *method, uint32_t num_args, int64_t func, int64_t func_tag, int64_t val1,
                                      int64_t tag1, int64_t val2, int64_t tag2, int64_t val3, int64_t tag3)
{
    g_call_result =
        PrintFunc("any", __FUNCTION__, method, num_args, func, func_tag, val1, tag1, val2, tag2, val3, tag3);
    return DecodedTaggedValue(1, 2U);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, InvokeThreeArgDyn)
{
    Frame *frame = CreateFrame(4U, nullptr, nullptr);
    frame->GetVReg(0).Set(0xABC);
    frame->GetVReg(0).SetTag(0);
    frame->GetVReg(1).SetValue(2);
    frame->GetVReg(1).SetTag(3);
    frame->GetVReg(2U).SetValue(4);
    frame->GetVReg(2U).SetTag(5);
    frame->GetVReg(3U).SetValue(6);
    frame->GetVReg(3U).SetTag(7);

    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, nullptr);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(ThreeArgDyn));

    uint8_t insn[] = {static_cast<uint8_t>(Opcode::CALLI_DYN_IMM4_V4_V4_V4_V4_V4), 0x03, 0x12, 0x03};
    g_call_result = "";
    InterpreterToCompiledCodeBridgeDyn(insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("any", "ThreeArgDyn", &callee, 3, 0xABC, 0, 4, 5, 2, 3, 6, 7U));
    ASSERT_EQ(frame->GetAcc().GetValue(), 1);
    ASSERT_EQ(frame->GetAcc().GetTag(), 2);

    uint8_t range_insn[] = {static_cast<uint8_t>(Opcode::CALLI_DYN_RANGE_IMM16_V16), 0x03, 0x00, 0x00, 0x00};
    g_call_result = "";
    frame->GetAcc().SetValue(0);
    frame->GetAcc().SetTag(0);
    InterpreterToCompiledCodeBridgeDyn(range_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("any", "ThreeArgDyn", &callee, 3, 0xABC, 0, 2, 3, 4, 5, 6, 7U));
    ASSERT_EQ(frame->GetAcc().GetValue(), 1);
    ASSERT_EQ(frame->GetAcc().GetTag(), 2);

    g_call_result = "";
    int64_t args[] = {0xABC, 0x0, 0x2, 0x3, 0x4, 0x5U, 0x6, 0x7};
    DecodedTaggedValue res = InvokeCompiledCodeWithArgArrayDyn(args, 3, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("any", "ThreeArgDyn", &callee, 3, 0xABC, 0, 2, 3, 4, 5, 6, 7U));
    ASSERT_EQ(res.value, 1);
    ASSERT_EQ(res.tag, 2);

    FreeFrame(frame);
}

static DecodedTaggedValue ThreeVarArgDyn(Method *method, uint32_t num_args, int64_t func, int64_t func_tag, ...)
{
    DecodedTaggedValue res;
    if (num_args != 3U) {
        g_call_result = PrintFunc("any", __FUNCTION__, method, num_args, func, func_tag);
        res = DecodedTaggedValue(0, 0);
    } else {
        va_list args;
        va_start(args, func_tag);
        int64_t val1 = va_arg(args, int64_t);
        int64_t tag1 = va_arg(args, int64_t);
        int64_t val2 = va_arg(args, int64_t);
        int64_t tag2 = va_arg(args, int64_t);
        int64_t val3 = va_arg(args, int64_t);
        int64_t tag3 = va_arg(args, int64_t);
        g_call_result =
            PrintFunc("any", __FUNCTION__, method, num_args, func, func_tag, val1, tag1, val2, tag2, val3, tag3);
        va_end(args);
        res = DecodedTaggedValue(2U, 3U);
    }
    return res;
}

TEST_F(InterpreterToCompiledCodeBridgeTest, InvokeThreeVarArgDyn)
{
    Frame *frame = CreateFrame(4, nullptr, nullptr);
    frame->GetVReg(0).Set(0xABC);
    frame->GetVReg(0).SetTag(0);
    frame->GetVReg(1).SetValue(2);
    frame->GetVReg(1).SetTag(3);
    frame->GetVReg(2U).SetValue(4);
    frame->GetVReg(2U).SetTag(5);
    frame->GetVReg(3U).SetValue(6);
    frame->GetVReg(3U).SetTag(7);

    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, nullptr);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(ThreeVarArgDyn));

    uint8_t insn[] = {static_cast<uint8_t>(Opcode::CALLI_DYN_IMM4_V4_V4_V4_V4_V4), 0x03, 0x21, 0x03};
    g_call_result = "";
    InterpreterToCompiledCodeBridgeDyn(insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("any", "ThreeVarArgDyn", &callee, 3, 0xABC, 0, 2, 3, 4, 5, 6, 7U));
    ASSERT_EQ(frame->GetAcc().GetValue(), 2);
    ASSERT_EQ(frame->GetAcc().GetTag(), 3);

    g_call_result = "";
    int64_t args[] = {0xABC, 0x0, 0x2, 0x3, 0x4, 0x5U, 0x6, 0x7};
    DecodedTaggedValue res = InvokeCompiledCodeWithArgArrayDyn(args, 3, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("any", "ThreeVarArgDyn", &callee, 3, 0xABC, 0, 2, 3, 4, 5, 6, 7U));
    ASSERT_EQ(res.value, 2);
    ASSERT_EQ(res.tag, 3);

    FreeFrame(frame);
}

static DecodedTaggedValue FourArgDyn(Method *method, uint32_t num_args, int64_t func, int64_t func_tag, int64_t val1,
                                     int64_t tag1, int64_t val2, int64_t tag2, int64_t val3, int64_t tag3, int64_t val4,
                                     int64_t tag4)
{
    g_call_result = PrintFunc("any", __FUNCTION__, method, num_args, func, func_tag, val1, tag1, val2, tag2, val3, tag3,
                              val4, tag4);
    return DecodedTaggedValue(2U, 3U);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, InvokeFourArgDyn)
{
    Frame *frame = CreateFrame(5U, nullptr, nullptr);
    frame->GetVReg(0).Set(0xABC);
    frame->GetVReg(0).SetTag(0);
    frame->GetVReg(1).SetValue(2);
    frame->GetVReg(1).SetTag(3);
    frame->GetVReg(2U).SetValue(4);
    frame->GetVReg(2U).SetTag(5);
    frame->GetVReg(3U).SetValue(6);
    frame->GetVReg(3U).SetTag(7);
    frame->GetVReg(4U).SetValue(8);
    frame->GetVReg(4U).SetTag(9);

    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, nullptr);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(FourArgDyn));

    uint8_t insn[] = {static_cast<uint8_t>(Opcode::CALLI_DYN_IMM4_V4_V4_V4_V4_V4), 0x04, 0x12, 0x43};
    g_call_result = "";
    InterpreterToCompiledCodeBridgeDyn(insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("any", "FourArgDyn", &callee, 4, 0xABC, 0, 4, 5, 2, 3, 6, 7, 8, 9));
    ASSERT_EQ(frame->GetAcc().GetValue(), 2);
    ASSERT_EQ(frame->GetAcc().GetTag(), 3);

    uint8_t range_insn[] = {static_cast<uint8_t>(Opcode::CALLI_DYN_RANGE_IMM16_V16), 0x04, 0x00, 0x00, 0x00};
    g_call_result = "";
    frame->GetAcc().SetValue(0);
    frame->GetAcc().SetTag(0);
    InterpreterToCompiledCodeBridgeDyn(range_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("any", "FourArgDyn", &callee, 4, 0xABC, 0, 2, 3, 4, 5, 6, 7, 8, 9));
    ASSERT_EQ(frame->GetAcc().GetValue(), 2);
    ASSERT_EQ(frame->GetAcc().GetTag(), 3);

    g_call_result = "";
    int64_t args[] = {0xABC, 0x0, 0x2, 0x3, 0x4, 0x5U, 0x6, 0x7, 0x8, 0x9};
    DecodedTaggedValue res = InvokeCompiledCodeWithArgArrayDyn(args, 4, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("any", "FourArgDyn", &callee, 4, 0xABC, 0, 2, 3, 4, 5, 6, 7U, 8, 9));
    ASSERT_EQ(res.value, 2);
    ASSERT_EQ(res.tag, 3);

    FreeFrame(frame);
}

static DecodedTaggedValue FourVarArgDyn(Method *method, uint32_t num_args, int64_t func, int64_t func_tag, ...)
{
    DecodedTaggedValue res;
    if (num_args != 4U) {
        g_call_result = PrintFunc("any", __FUNCTION__, method, num_args, func, func_tag);
        res = DecodedTaggedValue(0, 0);
    } else {
        va_list args;
        va_start(args, func_tag);
        int64_t val1 = va_arg(args, int64_t);
        int64_t tag1 = va_arg(args, int64_t);
        int64_t val2 = va_arg(args, int64_t);
        int64_t tag2 = va_arg(args, int64_t);
        int64_t val3 = va_arg(args, int64_t);
        int64_t tag3 = va_arg(args, int64_t);
        int64_t val4 = va_arg(args, int64_t);
        int64_t tag4 = va_arg(args, int64_t);
        g_call_result = PrintFunc("any", __FUNCTION__, method, num_args, func, func_tag, val1, tag1, val2, tag2, val3,
                                  tag3, val4, tag4);
        va_end(args);
        res = DecodedTaggedValue(2U, 4U);
    }
    return res;
}

TEST_F(InterpreterToCompiledCodeBridgeTest, InvokeFourVarArgDyn)
{
    Frame *frame = CreateFrame(5U, nullptr, nullptr);
    frame->GetVReg(0).Set(0xABC);
    frame->GetVReg(1).SetValue(2);
    frame->GetVReg(1).SetTag(3);
    frame->GetVReg(2U).SetValue(4);
    frame->GetVReg(2U).SetTag(5);
    frame->GetVReg(3U).SetValue(6);
    frame->GetVReg(3U).SetTag(7);
    frame->GetVReg(4U).SetValue(8);
    frame->GetVReg(4U).SetTag(9);

    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, nullptr);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(FourVarArgDyn));

    uint8_t insn[] = {static_cast<uint8_t>(Opcode::CALLI_DYN_IMM4_V4_V4_V4_V4_V4), 0x04, 0x21, 0x43};
    g_call_result = "";
    InterpreterToCompiledCodeBridgeDyn(insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("any", "FourVarArgDyn", &callee, 4, 0xABC, 0, 2, 3, 4, 5, 6, 7, 8, 9));
    ASSERT_EQ(frame->GetAcc().GetValue(), 2);
    ASSERT_EQ(frame->GetAcc().GetTag(), 4);

    g_call_result = "";
    int64_t args[] = {0xABC, 0x0, 0x2, 0x3, 0x4, 0x5U, 0x6, 0x7, 0x8, 0x9};
    DecodedTaggedValue res = InvokeCompiledCodeWithArgArrayDyn(args, 4, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("any", "FourVarArgDyn", &callee, 4, 0xABC, 0, 2, 3, 4, 5, 6, 7, 8, 9));
    ASSERT_EQ(res.value, 2);
    ASSERT_EQ(res.tag, 4);

    FreeFrame(frame);
}

static DecodedTaggedValue FiveArgDyn(Method *method, uint32_t num_args, int64_t func, int64_t func_tag, int64_t val1,
                                     int64_t tag1, int64_t val2, int64_t tag2, int64_t val3, int64_t tag3, int64_t val4,
                                     int64_t tag4, int64_t val5U, int64_t tag5)
{
    g_call_result = PrintFunc("any", __FUNCTION__, method, num_args, func, func_tag, val1, tag1, val2, tag2, val3, tag3,
                              val4, tag4, val5U, tag5);
    return DecodedTaggedValue(1, 5U);
}

TEST_F(InterpreterToCompiledCodeBridgeTest, InvokeFiveArgDyn)
{
    Frame *frame = CreateFrame(6U, nullptr, nullptr);
    frame->GetVReg(0).Set(0xABC);
    frame->GetVReg(0).SetTag(0);
    frame->GetVReg(1).SetValue(2);
    frame->GetVReg(1).SetTag(3);
    frame->GetVReg(2U).SetValue(4);
    frame->GetVReg(2U).SetTag(5);
    frame->GetVReg(3U).SetValue(6);
    frame->GetVReg(3U).SetTag(7);
    frame->GetVReg(4U).SetValue(8);
    frame->GetVReg(4U).SetTag(9);
    frame->GetVReg(5U).SetValue(10);
    frame->GetVReg(5U).SetTag(11);

    Method callee(nullptr, nullptr, panda_file::File::EntityId(), panda_file::File::EntityId(), ACC_STATIC, 0, nullptr);
    callee.SetCompiledEntryPoint(reinterpret_cast<const void *>(FiveArgDyn));

    uint8_t range_insn[] = {static_cast<uint8_t>(Opcode::CALLI_DYN_RANGE_IMM16_V16), 0x05U, 0x00, 0x00, 0x00};
    g_call_result = "";
    InterpreterToCompiledCodeBridgeDyn(range_insn, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("any", "FiveArgDyn", &callee, 5U, 0xABC, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11));
    ASSERT_EQ(frame->GetAcc().GetValue(), 1);
    ASSERT_EQ(frame->GetAcc().GetTag(), 5);

    g_call_result = "";
    int64_t args[] = {0xABC, 0x0, 0x2, 0x3, 0x4, 0x5U, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB};
    DecodedTaggedValue res = InvokeCompiledCodeWithArgArrayDyn(args, 5U, frame, &callee, thread_);
    ASSERT_EQ(g_call_result, PrintFunc("any", "FiveArgDyn", &callee, 5U, 0xABC, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11));
    ASSERT_EQ(res.value, 1);
    ASSERT_EQ(res.tag, 5);

    FreeFrame(frame);
}
#endif
}  // namespace panda::test
