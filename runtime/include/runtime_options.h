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

#ifndef PANDA_RUNTIME_INCLUDE_RUNTIME_OPTIONS_H_
#define PANDA_RUNTIME_INCLUDE_RUNTIME_OPTIONS_H_

#include "generated/runtime_options_gen.h"
#include "utils/logger.h"

namespace panda {
class JSNApi;

namespace test {
class GetJniNameTest;
class MethodTest;
class CompilerThreadPoolTest;
class MockThreadPoolTest;
class ObjectHeaderTest;
class ThreadTest;
class ClassLinkerTest;
class InterpreterToCompiledCodeBridgeTest;
class CompiledCodeToInterpreterBridgeTest;
class DynCompiledCodeToInterpreterBridgeTest;
class JNITest;
class CFrameTest;
class StackWalkerTest;
class PandaRunner;
class CompilerQueueTest;
class ProfileSaverTest;
class DynObjectsTest;
class JSObjectTest;
class JSFunctionTest;
class BuiltinsTest;
class NameDictionaryTest;
class JSSymbolTest;
class NativePointerTest;
class JSTaggedValueTest;
class LexicalEnvTest;
class GlobalEnvTest;
class EcmaIntrinsicsTest;
class BuiltinsNumberTest;
class JSArrayTest;
class BuiltinsObjectTest;
class BuiltinsBooleanTest;
class BuiltinsErrorsTest;
class BuiltinsStringTest;
class JSDateTest;
class BuiltinsDateTest;
class BuiltinsSymbolTest;
class BuiltinsRegExpTest;
class BuiltinsFunctionTest;
class BuiltinsProxyTest;
class BuiltinsReflectTest;
class BuiltinsErrorTest;
class LinkedHashTableTest;
class BuiltinsSetTest;
class BuiltinsMapTest;
class BuiltinsWeakMapTest;
class BuiltinsWeakSetTest;
class BuiltinsMathTest;
class BuiltinsJsonTest;
class BuiltinsIteratorTest;
class JSSetTest;
class JSMapTest;
class JSProxyTest;
class JSPrimitiveRefTest;
class BuiltinsArrayTest;
class BuiltinsTypedArrayTest;
class ObjectFactoryTest;
class HistogramTest;
class JSHandleTest;
class JSForinIteratorTest;
class JSTaggedQueueTest;
class EcmaDumpTest;
class JSIteratorTest;
class BuiltinsArrayBufferTest;
class InlineCacheTest;
class WeakRefStwGCTest;
class WeakRefGenGCTest;
class DynBufferTest;
class RegExpTest;
class JSPromiseTest;
class BuiltinsDataViewTest;
class SepareteJSVMTest;
class HProfTest;
class HeapTrackerTest;
class DebuggerTypesTest;
class DebuggerEventsTest;
class DebuggerCommandsTest;
class SnapShotTest;
class JSVerificationTest;
class LargeObjectTest;
class GlueRegsTest;
}  // namespace test

namespace coretypes::test {
class ArrayTest;
class ClassTest;
class StringTest;
}  // namespace coretypes::test

namespace mem {
class FreeListAllocatorTest;
class VerificationTest;
class RegionAllocatorTest;
class HybridObjectAllocatorTest;
class PygoteSpaceAllocatorGenTest;
class PygoteSpaceAllocatorStwTest;
}  // namespace mem

namespace mem::test {
class StringTableTest;
class FileMemoryProfileTest;
class MemStatsTest;
class MemLeakTest;
class MemStatsGCTest;
class MemStatsAdditionalInfoTest;
class PandaSmartPointersTest;
class InternalAllocatorTest;
class CardTableTest;
class MultithreadedInternStringTableTest;
class RemSetTest;
}  // namespace mem::test

namespace mem::test {
class ReferenceStorageTest;
}  // namespace mem::test

namespace concurrency::test {
class MonitorTest;
class AttachThreadTest;
}  // namespace concurrency::test

namespace interpreter::test {
class InterpreterTest;
}  // namespace interpreter::test

namespace debugger::test {
class DebuggerTest;
}  // namespace debugger::test

namespace verifier::test {
class VerifierTest;
}  // namespace verifier::test

namespace time::test {
class TimeTest;
}  // namespace time::test

/**
 * \brief Class represents runtime options
 *
 * It extends Options that represents public options (that described in options.yaml) and
 * adds some private options related to runtime initialization that cannot be controlled
 * via command line tools. Now they are used in unit tests to create minimal runtime for
 * testing.
 *
 * To control private options from any class/function we need to make it friend for this class.
 */
class RuntimeOptions : public Options {
public:
    explicit RuntimeOptions(const std::string &exe_path = "") : Options(exe_path) {}
    ~RuntimeOptions() = default;
    DEFAULT_COPY_SEMANTIC(RuntimeOptions);
    DEFAULT_MOVE_SEMANTIC(RuntimeOptions);

    bool ShouldLoadBootPandaFiles() const
    {
        return should_load_boot_panda_files_;
    }

    bool ShouldInitializeIntrinsics() const
    {
        return should_initialize_intrinsics_;
    }

    void *GetMobileLog()
    {
        return mlog_buf_print_ptr_;
    }

    std::string GetFingerprint()
    {
        return fingerPrint_;
    }

    void SetFingerprint(const std::string &in)
    {
        fingerPrint_.assign(in);
    }

    void SetUnwindStack(void *in)
    {
        unwindstack_ = reinterpret_cast<char *>(in);
    }

    void *GetUnwindStack()
    {
        return unwindstack_;
    }

    void SetCrashConnect(void *in)
    {
        crash_connect_ = reinterpret_cast<char *>(in);
    }

    void *GetCrashConnect()
    {
        return crash_connect_;
    }

    void SetMobileLog(void *mlog_buf_print_ptr)
    {
        mlog_buf_print_ptr_ = mlog_buf_print_ptr;
        Logger::SetMobileLogPrintEntryPointByPtr(mlog_buf_print_ptr);
    }

    void SetForSnapShotStart()
    {
        should_load_boot_panda_files_ = false;
        should_initialize_intrinsics_ = false;
    }

    void SetShouldLoadBootPandaFiles(bool value)
    {
        should_load_boot_panda_files_ = value;
    }

    void SetShouldInitializeIntrinsics(bool value)
    {
        should_initialize_intrinsics_ = value;
    }

    bool UseMallocForInternalAllocations() const
    {
        bool use_malloc = false;
        auto option = GetInternalAllocatorType();
        if (option == "default") {
#ifdef NDEBUG
            use_malloc = true;
#else
            use_malloc = false;
#endif
        } else if (option == "malloc") {
            use_malloc = true;
        } else if (option == "panda_allocators") {
            use_malloc = false;
        } else {
            UNREACHABLE();
        }
        return use_malloc;
    }

private:
    bool should_load_boot_panda_files_ {true};
    bool should_initialize_intrinsics_ {true};
    void *mlog_buf_print_ptr_ {nullptr};
    std::string fingerPrint_ {"unknown"};
    void *unwindstack_ {nullptr};
    void *crash_connect_ {nullptr};

    friend class panda::JSNApi;
    friend class coretypes::test::ArrayTest;
    friend class coretypes::test::ClassTest;
    friend class coretypes::test::StringTest;
    friend class mem::test::StringTableTest;
    friend class mem::test::MultithreadedInternStringTableTest;
    friend class mem::FreeListAllocatorTest;
    friend class mem::RegionAllocatorTest;
    friend class mem::HybridObjectAllocatorTest;
    friend class mem::PygoteSpaceAllocatorGenTest;
    friend class mem::PygoteSpaceAllocatorStwTest;
    friend class concurrency::test::MonitorTest;
    friend class concurrency::test::AttachThreadTest;
    friend class test::MethodTest;
    friend class test::CompilerThreadPoolTest;
    friend class test::MockThreadPoolTest;
    friend class test::ThreadTest;
    friend class test::ObjectHeaderTest;
    friend class test::ClassLinkerTest;
    friend class test::CompilerQueueTest;
    friend class test::GetJniNameTest;
    friend class mem::test::FileMemoryProfileTest;
    friend class mem::test::ReferenceStorageTest;
    friend class mem::test::MemStatsTest;
    friend class mem::test::MemLeakTest;
    friend class mem::test::MemStatsGCTest;
    friend class mem::test::MemStatsAdditionalInfoTest;
    friend class mem::test::RemSetTest;
    friend class interpreter::test::InterpreterTest;
    friend class mem::test::PandaSmartPointersTest;
    friend class test::InterpreterToCompiledCodeBridgeTest;
    friend class test::CompiledCodeToInterpreterBridgeTest;
    friend class test::DynCompiledCodeToInterpreterBridgeTest;
    friend class test::JNITest;
    friend class panda::test::CFrameTest;
    friend class panda::test::StackWalkerTest;
    friend class test::ClassLinkerTest;
    friend class debugger::test::DebuggerTest;
    friend class panda::test::PandaRunner;
    friend class verifier::test::VerifierTest;
    friend class panda::Logger;
    friend class test::ProfileSaverTest;
    friend class panda::test::DynObjectsTest;
    friend class panda::test::JSObjectTest;
    friend class panda::test::JSFunctionTest;
    friend class panda::test::BuiltinsTest;
    friend class panda::test::NativePointerTest;
    friend class panda::test::JSSymbolTest;
    friend class panda::test::NameDictionaryTest;
    friend class panda::test::JSTaggedValueTest;
    friend class panda::test::LexicalEnvTest;
    friend class panda::test::GlobalEnvTest;
    friend class panda::test::EcmaIntrinsicsTest;
    friend class panda::test::BuiltinsNumberTest;
    friend class panda::test::BuiltinsObjectTest;
    friend class panda::test::JSArrayTest;
    friend class panda::test::BuiltinsBooleanTest;
    friend class time::test::TimeTest;
    friend class panda::test::BuiltinsErrorsTest;
    friend class panda::test::BuiltinsStringTest;
    friend class panda::test::JSDateTest;
    friend class panda::test::BuiltinsDateTest;
    friend class panda::test::BuiltinsSymbolTest;
    friend class panda::test::BuiltinsRegExpTest;
    friend class panda::test::BuiltinsFunctionTest;
    friend class panda::test::BuiltinsProxyTest;
    friend class panda::test::BuiltinsReflectTest;
    friend class panda::test::BuiltinsErrorTest;
    friend class panda::test::LinkedHashTableTest;
    friend class panda::test::BuiltinsSetTest;
    friend class panda::test::BuiltinsMapTest;
    friend class panda::test::BuiltinsWeakMapTest;
    friend class panda::test::BuiltinsWeakSetTest;
    friend class panda::test::BuiltinsMathTest;
    friend class panda::test::BuiltinsJsonTest;
    friend class panda::test::BuiltinsIteratorTest;
    friend class panda::test::JSSetTest;
    friend class panda::test::JSMapTest;
    friend class panda::test::JSProxyTest;
    friend class panda::test::JSPrimitiveRefTest;
    friend class panda::test::BuiltinsArrayTest;
    friend class panda::test::BuiltinsTypedArrayTest;
    friend class panda::test::ObjectFactoryTest;
    friend class panda::test::HistogramTest;
    friend class panda::mem::VerificationTest;
    friend class panda::test::JSHandleTest;
    friend class panda::test::JSForinIteratorTest;
    friend class panda::test::JSTaggedQueueTest;
    friend class panda::test::EcmaDumpTest;
    friend class panda::test::JSIteratorTest;
    friend class panda::test::BuiltinsArrayBufferTest;
    friend class panda::test::InlineCacheTest;
    friend class panda::test::WeakRefStwGCTest;
    friend class panda::test::WeakRefGenGCTest;
    friend class mem::test::InternalAllocatorTest;
    friend class panda::test::DynBufferTest;
    friend class panda::test::RegExpTest;
    friend class panda::test::JSPromiseTest;
    friend class panda::test::BuiltinsDataViewTest;
    friend class panda::test::SepareteJSVMTest;
    friend class panda::test::HProfTest;
    friend class panda::test::HeapTrackerTest;
    friend class panda::test::DebuggerTypesTest;
    friend class panda::test::DebuggerEventsTest;
    friend class panda::test::DebuggerCommandsTest;
    friend class panda::test::SnapShotTest;
    friend class panda::mem::test::CardTableTest;
    friend class panda::test::JSVerificationTest;
    friend class panda::test::LargeObjectTest;
    friend class panda::test::GlueRegsTest;
};

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_RUNTIME_OPTIONS_H_
