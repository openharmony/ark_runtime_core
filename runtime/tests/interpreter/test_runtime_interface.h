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

#ifndef PANDA_RUNTIME_TESTS_INTERPRETER_TEST_RUNTIME_INTERFACE_H_
#define PANDA_RUNTIME_TESTS_INTERPRETER_TEST_RUNTIME_INTERFACE_H_

#include <gtest/gtest.h>

#include <cstdint>

#include "libpandafile/file.h"
#include "libpandafile/file_items.h"
#include "runtime/include/coretypes/array-inl.h"
#include "runtime/include/coretypes/string.h"
#include "runtime/include/method.h"
#include "runtime/include/runtime.h"
#include "runtime/include/panda_vm.h"
#include "runtime/include/runtime_notification.h"
#include "runtime/interpreter/frame.h"
#include "runtime/mem/gc/gc.h"

namespace panda::interpreter::test {

class DummyGC : public panda::mem::GC {
public:
    explicit DummyGC(panda::mem::ObjectAllocatorBase *object_allocator, const panda::mem::GCSettings &settings);
    ~DummyGC() {}
    // NOLINTNEXTLINE(misc-unused-parameters)
    void WaitForGC([[maybe_unused]] const GCTask &task) override {}
    void InitGCBits([[maybe_unused]] panda::ObjectHeader *obj_header) override {}
    void InitGCBitsForAllocationInTLAB([[maybe_unused]] panda::ObjectHeader *obj_header) override {}
    void Trigger() override {}

private:
    size_t VerifyHeap() override
    {
        return 0;
    }
    void InitializeImpl() override {}

    void PreRunPhasesImpl() override {}
    void RunPhasesImpl([[maybe_unused]] const GCTask &task) override {}
    void MarkReferences([[maybe_unused]] PandaStackTL<ObjectHeader *> *references,
                        [[maybe_unused]] panda::mem::GCPhase gc_phase) override
    {
    }
    void VisitRoots([[maybe_unused]] const GCRootVisitor &gc_root_visitor,
                    [[maybe_unused]] mem::VisitGCRootFlags flags) override
    {
    }
    void VisitClassRoots([[maybe_unused]] const GCRootVisitor &gc_root_visitor) override {}
    void VisitCardTableRoots([[maybe_unused]] mem::CardTable *card_table,
                             [[maybe_unused]] const GCRootVisitor &gc_root_visitor,
                             [[maybe_unused]] const MemRangeChecker &range_checker,
                             [[maybe_unused]] const ObjectChecker &range_object_checker,
                             [[maybe_unused]] const ObjectChecker &from_object_checker,
                             [[maybe_unused]] const uint32_t processed_flag) override
    {
    }
    void CommonUpdateRefsToMovedObjects([[maybe_unused]] const mem::UpdateRefInAllocator &update_allocator) override {}
    void UpdateVmRefs() override {}
    void UpdateGlobalObjectStorage() override {}
    void UpdateClassLinkerContextRoots() override {}
    void UpdateThreadLocals() override {}
};

template <class T>
static T *ToPointer(size_t value)
{
    return reinterpret_cast<T *>(AlignUp(value, alignof(T)));
}

class RuntimeInterface {
public:
    static constexpr bool NEED_READ_BARRIER = false;
    static constexpr bool NEED_WRITE_BARRIER = false;

    using InvokeMethodHandler = std::function<Value(ManagedThread *, Method *, Value *)>;

    struct NullPointerExceptionData {
        bool expected {false};
    };

    struct ArithmeticException {
        bool expected {false};
    };

    struct ArrayIndexOutOfBoundsExceptionData {
        bool expected {false};
        coretypes::array_ssize_t idx;
        coretypes::array_size_t length;
    };

    struct NegativeArraySizeExceptionData {
        bool expected {false};
        coretypes::array_ssize_t size;
    };

    struct ClassCastExceptionData {
        bool expected {false};
        Class *dst_type;
        Class *src_type;
    };

    struct AbstractMethodError {
        bool expected {false};
        Method *method;
    };

    struct ArrayStoreExceptionData {
        bool expected {false};
        Class *array_class;
        Class *elem_class;
    };

    static constexpr BytecodeId METHOD_ID {0xaabb};
    static constexpr BytecodeId FIELD_ID {0xeeff};
    static constexpr BytecodeId STRING_ID {0x11223344};
    static constexpr BytecodeId TYPE_ID {0x5566};
    static constexpr BytecodeId LITERALARRAY_ID {0x7788};

    static coretypes::String *ResolveString([[maybe_unused]] PandaVM *vm, [[maybe_unused]] const Method &caller,
                                            BytecodeId id)
    {
        EXPECT_EQ(id, STRING_ID);
        return ToPointer<coretypes::String>(0x55667788);
    }

    static coretypes::Array *ResolveLiteralArray([[maybe_unused]] PandaVM *vm, [[maybe_unused]] const Method &caller,
                                                 BytecodeId id)
    {
        EXPECT_EQ(id, STRING_ID);
        return ToPointer<coretypes::Array>(0x7788);
    }

    static Method *ResolveMethod([[maybe_unused]] ManagedThread *thread, [[maybe_unused]] const Method &caller,
                                 BytecodeId id)
    {
        EXPECT_EQ(id, METHOD_ID);
        return resolved_method;
    }

    static Field *ResolveField([[maybe_unused]] ManagedThread *thread, [[maybe_unused]] const Method &caller,
                               BytecodeId id)
    {
        EXPECT_EQ(id, FIELD_ID);
        return resolved_field;
    }

    template <bool need_init>
    static Class *ResolveClass([[maybe_unused]] ManagedThread *thread, [[maybe_unused]] const Method &caller,
                               BytecodeId id)
    {
        EXPECT_EQ(id, TYPE_ID);
        return resolved_class;
    }

    static uint32_t FindCatchBlock([[maybe_unused]] const Method &method, [[maybe_unused]] ObjectHeader *exception,
                                   [[maybe_unused]] uint32_t pc)
    {
        return catch_block_pc_offset;
    }

    static void SetCatchBlockPcOffset(uint32_t pc_offset)
    {
        catch_block_pc_offset = pc_offset;
    }

    static uint32_t GetCompilerHotnessThreshold()
    {
        return jit_threshold;
    }

    static bool IsCompilerEnableJit()
    {
        return true;
    }

    static void SetCompilerHotnessThreshold(uint32_t threshold)
    {
        jit_threshold = threshold;
    }

    static void SetCurrentFrame([[maybe_unused]] ManagedThread *thread, Frame *frame)
    {
        ASSERT_NE(frame, nullptr);
    }

    static RuntimeNotificationManager *GetNotificationManager()
    {
        return nullptr;
    }

    static void SetupResolvedMethod(Method *method)
    {
        ManagedThread::GetCurrent()->GetInterpreterCache()->Clear();
        resolved_method = method;
    }

    static void SetupResolvedField(Field *field)
    {
        ManagedThread::GetCurrent()->GetInterpreterCache()->Clear();
        resolved_field = field;
    }

    static void SetupResolvedClass(Class *klass)
    {
        ManagedThread::GetCurrent()->GetInterpreterCache()->Clear();
        resolved_class = klass;
    }

    static void SetupCatchBlockPcOffset(uint32_t pc_offset)
    {
        catch_block_pc_offset = pc_offset;
    }

    static coretypes::Array *CreateArray(Class *klass, coretypes::array_size_t length)
    {
        EXPECT_EQ(klass, array_class);
        EXPECT_EQ(length, array_length);
        return array_object;
    }

    static void SetupArrayClass(Class *klass)
    {
        array_class = klass;
    }

    static void SetupArrayLength(coretypes::array_size_t length)
    {
        array_length = length;
    }

    static void SetupArrayObject(coretypes::Array *obj)
    {
        array_object = obj;
    }

    static ObjectHeader *CreateObject(Class *klass)
    {
        EXPECT_EQ(klass, object_class);
        return object;
    }

    static void SetupObjectClass(Class *klass)
    {
        object_class = klass;
    }

    static void SetupObject(ObjectHeader *obj)
    {
        object = obj;
    }

    static Value InvokeMethod(ManagedThread *thread, Method *method, Value *args)
    {
        return invoke_handler(thread, method, args);
    }

    static void SetupInvokeMethodHandler(InvokeMethodHandler handler)
    {
        invoke_handler = handler;
    }

    // Throw exceptions

    static void ThrowNullPointerException()
    {
        ASSERT_TRUE(npe_data.expected);
    }

    static void ThrowArrayIndexOutOfBoundsException(coretypes::array_ssize_t idx, coretypes::array_size_t length)
    {
        ASSERT_TRUE(array_oob_exception_data.expected);
        ASSERT_EQ(array_oob_exception_data.idx, idx);
        ASSERT_EQ(array_oob_exception_data.length, length);
    }

    static void ThrowNegativeArraySizeException(coretypes::array_ssize_t size)
    {
        ASSERT_TRUE(array_neg_size_exception_data.expected);
        ASSERT_EQ(array_neg_size_exception_data.size, size);
    }

    static void ThrowArithmeticException()
    {
        ASSERT_TRUE(arithmetic_exception_data.expected);
    }

    static void ThrowClassCastException(Class *dst_type, Class *src_type)
    {
        ASSERT_TRUE(class_cast_exception_data.expected);
        ASSERT_EQ(class_cast_exception_data.dst_type, dst_type);
        ASSERT_EQ(class_cast_exception_data.src_type, src_type);
    }

    static void ThrowAbstractMethodError(Method *method)
    {
        ASSERT_TRUE(abstract_method_error_data.expected);
        ASSERT_EQ(abstract_method_error_data.method, method);
    }

    static void ThrowOutOfMemoryError([[maybe_unused]] const PandaString &msg) {}

    static void ThrowVerificationException([[maybe_unused]] const PandaString &msg)
    {
        // ASSERT_TRUE verification_of_method_exception_data.expected
        // ASSERT_EQ verification_of_method_exception_data.msg, msg
    }

    static void ThrowArrayStoreException(Class *array_klass, Class *elem_class)
    {
        ASSERT_TRUE(array_store_exception_data.expected);
        ASSERT_EQ(array_store_exception_data.array_class, array_klass);
        ASSERT_EQ(array_store_exception_data.elem_class, elem_class);
    }

    static void SetArrayStoreException(ArrayStoreExceptionData data)
    {
        array_store_exception_data = data;
    }

    static void SetNullPointerExceptionData(NullPointerExceptionData data)
    {
        npe_data = data;
    }

    static void SetArrayIndexOutOfBoundsExceptionData(ArrayIndexOutOfBoundsExceptionData data)
    {
        array_oob_exception_data = data;
    }

    static void SetNegativeArraySizeExceptionData(NegativeArraySizeExceptionData data)
    {
        array_neg_size_exception_data = data;
    }

    static void SetArithmeticExceptionData(ArithmeticException data)
    {
        arithmetic_exception_data = data;
    }

    static void SetClassCastExceptionData(ClassCastExceptionData data)
    {
        class_cast_exception_data = data;
    }

    static void SetAbstractMethodErrorData(AbstractMethodError data)
    {
        abstract_method_error_data = data;
    }

    static Frame *CreateFrame(size_t nregs, Method *method, Frame *prev)
    {
        auto allocator = Thread::GetCurrent()->GetVM()->GetHeapManager()->GetInternalAllocator();
        Frame *mem = static_cast<Frame *>(
            allocator->Allocate(panda::Frame::GetSize(nregs), GetLogAlignment(8U), ManagedThread::GetCurrent()));
        return (new (mem) panda::Frame(method, prev, nregs));
    }

    static Frame *CreateFrameWithActualArgs(size_t nregs, size_t num_actual_args, Method *method, Frame *prev)
    {
        return CreateFrameWithActualArgs(nregs, nregs, num_actual_args, method, prev);
    }

    static Frame *CreateFrameWithActualArgs(size_t size, size_t nregs, size_t num_actual_args, Method *method,
                                            Frame *prev)
    {
        auto allocator = Thread::GetCurrent()->GetVM()->GetHeapManager()->GetInternalAllocator();
        Frame *mem = static_cast<Frame *>(
            allocator->Allocate(panda::Frame::GetSize(size), GetLogAlignment(8U), ManagedThread::GetCurrent()));
        if (UNLIKELY(mem == nullptr)) {
            return nullptr;
        }
        return (new (mem) panda::Frame(method, prev, nregs, num_actual_args));
    }

    static void FreeFrame(Frame *frame)
    {
        auto allocator = Thread::GetCurrent()->GetVM()->GetHeapManager()->GetInternalAllocator();
        allocator->Free(frame);
    }

    static mem::GC *GetGC()
    {
        return &panda::interpreter::test::RuntimeInterface::dummy_gc;
    }

    static const uint8_t *GetMethodName([[maybe_unused]] Method *caller, [[maybe_unused]] BytecodeId method_id)
    {
        return nullptr;
    }

    static Class *GetMethodClass([[maybe_unused]] Method *caller, [[maybe_unused]] BytecodeId method_id)
    {
        return resolved_class;
    }

    static uint32_t GetMethodArgumentsCount([[maybe_unused]] Method *caller, [[maybe_unused]] BytecodeId method_id)
    {
        return 0;
    }

    static void CollectRoots([[maybe_unused]] Frame *frame) {}

    static void Safepoint() {}

    static LanguageContext GetLanguageContext(const Method &method)
    {
        return Runtime::GetCurrent()->GetLanguageContext(*method.GetClass());
    }

private:
    static ArrayIndexOutOfBoundsExceptionData array_oob_exception_data;

    static NegativeArraySizeExceptionData array_neg_size_exception_data;

    static NullPointerExceptionData npe_data;

    static ArithmeticException arithmetic_exception_data;

    static ClassCastExceptionData class_cast_exception_data;

    static AbstractMethodError abstract_method_error_data;

    static ArrayStoreExceptionData array_store_exception_data;

    static coretypes::Array *array_object;

    static Class *array_class;

    static coretypes::array_size_t array_length;

    static ObjectHeader *object;

    static Class *object_class;

    static Class *resolved_class;

    static uint32_t catch_block_pc_offset;

    static Method *resolved_method;

    static Field *resolved_field;

    static InvokeMethodHandler invoke_handler;

    static uint32_t jit_threshold;

    static panda::interpreter::test::DummyGC dummy_gc;
};

}  // namespace panda::interpreter::test

#endif  // PANDA_RUNTIME_TESTS_INTERPRETER_TEST_RUNTIME_INTERFACE_H_
