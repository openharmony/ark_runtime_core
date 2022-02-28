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

#ifndef PANDA_RUNTIME_INTERPRETER_RUNTIME_INTERFACE_H_
#define PANDA_RUNTIME_INTERPRETER_RUNTIME_INTERFACE_H_

#include <memory>

#include "libpandabase/utils/logger.h"
#include "libpandafile/file_items.h"
#include "runtime/entrypoints/entrypoints.h"
#include "runtime/include/class_linker-inl.h"
#include "runtime/include/coretypes/array.h"
#include "runtime/include/coretypes/string.h"
#include "runtime/include/exceptions.h"
#include "runtime/include/field.h"
#include "runtime/include/managed_thread.h"
#include "runtime/include/method.h"
#include "runtime/include/runtime.h"
#include "runtime/mem/gc/gc.h"
#include "runtime/tooling/pt_thread_info.h"

namespace panda::interpreter {

class RuntimeInterface {
public:
    static constexpr bool NEED_READ_BARRIER = true;
    static constexpr bool NEED_WRITE_BARRIER = true;

    static coretypes::String *ResolveString(PandaVM *vm, const Method &caller, BytecodeId id)
    {
        return Runtime::GetCurrent()->ResolveString(vm, caller, id.AsFileId());
    }

    static Method *ResolveMethod(ManagedThread *thread, const Method &caller, BytecodeId id)
    {
        auto resolved_id = caller.GetClass()->ResolveMethodIndex(id.AsIndex());
        auto *class_linker = Runtime::GetCurrent()->GetClassLinker();
        auto *method = class_linker->GetMethod(caller, resolved_id);
        if (method == nullptr) {
            return nullptr;
        }

        auto *klass = method->GetClass();
        if (!klass->IsInitialized() && !class_linker->InitializeClass(thread, klass)) {
            return nullptr;
        }

        return method;
    }

    static const uint8_t *GetMethodName(const Method *caller, BytecodeId method_id)
    {
        auto resolved_id = caller->GetClass()->ResolveMethodIndex(method_id.AsIndex());
        const auto *pf = caller->GetPandaFile();
        const panda_file::MethodDataAccessor MDA(*pf, resolved_id);
        return pf->GetStringData(MDA.GetNameId()).data;
    }

    static Class *GetMethodClass(const Method *caller, BytecodeId method_id)
    {
        auto resolved_id = caller->GetClass()->ResolveMethodIndex(method_id.AsIndex());
        const auto *pf = caller->GetPandaFile();
        const panda_file::MethodDataAccessor MDA(*pf, resolved_id);
        auto class_id = MDA.GetClassId();

        auto *class_linker = Runtime::GetCurrent()->GetClassLinker();
        return class_linker->GetClass(*caller, class_id);
    }

    static uint32_t GetMethodArgumentsCount(Method *caller, BytecodeId method_id)
    {
        auto resolved_id = caller->GetClass()->ResolveMethodIndex(method_id.AsIndex());
        auto *pf = caller->GetPandaFile();
        panda_file::MethodDataAccessor mda(*pf, resolved_id);
        panda_file::ProtoDataAccessor pda(*pf, mda.GetProtoId());
        return pda.GetNumArgs();
    }

    static Field *ResolveField(ManagedThread *thread, const Method &caller, BytecodeId id)
    {
        auto resolved_id = caller.GetClass()->ResolveFieldIndex(id.AsIndex());
        auto *class_linker = Runtime::GetCurrent()->GetClassLinker();
        auto *field = class_linker->GetField(caller, resolved_id);
        if (field == nullptr) {
            return nullptr;
        }

        auto *klass = field->GetClass();
        if (!klass->IsInitialized() && !class_linker->InitializeClass(thread, field->GetClass())) {
            return nullptr;
        }

        return field;
    }

    template <bool need_init>
    static Class *ResolveClass(ManagedThread *thread, const Method &caller, BytecodeId id)
    {
        auto resolved_id = caller.GetClass()->ResolveClassIndex(id.AsIndex());
        ClassLinker *class_linker = Runtime::GetCurrent()->GetClassLinker();
        Class *klass = class_linker->GetClass(caller, resolved_id);

        if (klass == nullptr) {
            return nullptr;
        }

        if (need_init) {
            auto *klass_linker = Runtime::GetCurrent()->GetClassLinker();
            if (!klass->IsInitialized() && !klass_linker->InitializeClass(thread, klass)) {
                return nullptr;
            }
        }

        return klass;
    }

    static coretypes::Array *ResolveLiteralArray(PandaVM *vm, const Method &caller, BytecodeId id)
    {
        return Runtime::GetCurrent()->ResolveLiteralArray(vm, caller, id.AsFileId());
    }

    static uint32_t GetCompilerHotnessThreshold()
    {
        return 0;
    }

    static bool IsCompilerEnableJit()
    {
        return false;
    }

    static void SetCurrentFrame(ManagedThread *thread, Frame *frame)
    {
        thread->SetCurrentFrame(frame);
    }

    static RuntimeNotificationManager *GetNotificationManager()
    {
        return Runtime::GetCurrent()->GetNotificationManager();
    }

    static coretypes::Array *CreateArray(Class *klass, coretypes::array_size_t length)
    {
        return coretypes::Array::Create(klass, length);
    }

    static ObjectHeader *CreateObject(Class *klass);

    static Value InvokeMethod(ManagedThread *thread, Method *method, Value *args)
    {
        return method->Invoke(thread, args);
    }

    static uint32_t FindCatchBlock(const Method &method, ObjectHeader *exception, uint32_t pc)
    {
        return method.FindCatchBlock(exception->ClassAddr<Class>(), pc);
    }

    static void ThrowNullPointerException()
    {
        return panda::ThrowNullPointerException();
    }

    static void ThrowArrayIndexOutOfBoundsException(coretypes::array_ssize_t idx, coretypes::array_size_t length)
    {
        panda::ThrowArrayIndexOutOfBoundsException(idx, length);
    }

    static void ThrowNegativeArraySizeException(coretypes::array_ssize_t size)
    {
        panda::ThrowNegativeArraySizeException(size);
    }

    static void ThrowArithmeticException()
    {
        panda::ThrowArithmeticException();
    }

    static void ThrowClassCastException(Class *dst_type, Class *src_type)
    {
        panda::ThrowClassCastException(dst_type, src_type);
    }

    static void ThrowAbstractMethodError(Method *method)
    {
        panda::ThrowAbstractMethodError(method);
    }

    static void ThrowOutOfMemoryError(const PandaString &msg)
    {
        panda::ThrowOutOfMemoryError(msg);
    }

    static void ThrowArrayStoreException(Class *array_class, Class *elem_class)
    {
        panda::ThrowArrayStoreException(array_class, elem_class);
    }

    static void ThrowIllegalAccessException(const PandaString &msg)
    {
        panda::ThrowIllegalAccessException(msg);
    }

    static void ThrowVerificationException(const PandaString &msg)
    {
        panda::ThrowVerificationException(msg);
    }

    static void ThrowTypedErrorDyn(const std::string &msg)
    {
        panda::ThrowTypedErrorDyn(msg);
    }

    static void ThrowReferenceErrorDyn(const std::string &msg)
    {
        panda::ThrowReferenceErrorDyn(msg);
    }

    static Frame *CreateFrame(size_t nregs, Method *method, Frame *prev)
    {
        return panda::CreateFrame(nregs, method, prev);
    }

    static Frame *CreateFrameWithActualArgs(uint32_t nregs, uint32_t num_actual_args, Method *method, Frame *prev)
    {
        return panda::CreateFrameWithActualArgs(nregs, num_actual_args, method, prev);
    }

    static Frame *CreateFrameWithActualArgs(uint32_t size, uint32_t nregs, uint32_t num_actual_args, Method *method,
                                            Frame *prev)
    {
        return panda::CreateFrameWithActualArgsAndSize(size, nregs, num_actual_args, method, prev);
    }

    static void FreeFrame(Frame *frame)
    {
        panda::FreeFrame(frame);
    }

    static void ThreadSuspension(MTManagedThread *thread)
    {
        thread->WaitSuspension();
    }

    static void ThreadRuntimeTermination(MTManagedThread *thread)
    {
        thread->TerminationLoop();
    }

    static panda_file::SourceLang GetLanguageContext(Method *method_ptr)
    {
        LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(*method_ptr);
        return ctx.GetLanguage();
    }

    /**
     * \brief Executes external implementation of safepoint
     *
     * It is not-inlined version of safepoint.
     * Shouldn't be used in production in the JIT.
     */
    static void Safepoint()
    {
        MTManagedThread *thread = MTManagedThread::GetCurrent();
        if (UNLIKELY(thread->IsRuntimeTerminated())) {
            ThreadRuntimeTermination(thread);
        }
        if (thread->IsSuspended()) {
            ThreadSuspension(thread);
        }
    }

    static LanguageContext GetLanguageContext(const Method &method)
    {
        return Runtime::GetCurrent()->GetLanguageContext(method);
    }
};

}  // namespace panda::interpreter

#endif  // PANDA_RUNTIME_INTERPRETER_RUNTIME_INTERFACE_H_
