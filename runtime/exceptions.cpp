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

#include "runtime/include/exceptions.h"

#include <libpandabase/utils/cframe_layout.h>

#include "runtime/bridge/bridge.h"
#include "runtime/entrypoints/entrypoints.h"
#include "runtime/include/coretypes/string.h"
#include "runtime/include/object_header-inl.h"
#include "runtime/include/runtime.h"
#include "runtime/include/stack_walker.h"
#include "runtime/mem/vm_handle.h"
#include "libpandabase/utils/asan_interface.h"
#include "libpandabase/utils/logger.h"
#include "libpandabase/utils/utf.h"
#include "libpandafile/class_data_accessor-inl.h"
#include "libpandafile/method_data_accessor-inl.h"
#include "events/events.h"
#include "runtime/handle_base-inl.h"

namespace panda {

void ThrowException(LanguageContext ctx, ManagedThread *thread, const uint8_t *mutf8_name, const uint8_t *mutf8_msg)
{
    ctx.ThrowException(thread, mutf8_name, mutf8_msg);
}

static LanguageContext GetLanguageContext(ManagedThread *thread)
{
    ASSERT(thread != nullptr);

    StackWalker stack(thread);
    ASSERT(stack.HasFrame());

    auto *method = stack.GetMethod();
    ASSERT(method != nullptr);

    return Runtime::GetCurrent()->GetLanguageContext(*method);
}

void ThrowNullPointerException()
{
    auto *thread = ManagedThread::GetCurrent();
    auto ctx = GetLanguageContext(thread);
    ThrowNullPointerException(ctx, thread);
}

void ThrowNullPointerException(LanguageContext ctx, ManagedThread *thread)
{
    ThrowException(ctx, thread, ctx.GetNullPointerExceptionClassDescriptor(), nullptr);
}

void ThrowArrayIndexOutOfBoundsException(coretypes::array_ssize_t idx, coretypes::array_size_t length)
{
    auto *thread = ManagedThread::GetCurrent();
    auto ctx = GetLanguageContext(thread);
    ThrowArrayIndexOutOfBoundsException(idx, length, ctx, thread);
}

void ThrowArrayIndexOutOfBoundsException(coretypes::array_ssize_t idx, coretypes::array_size_t length,
                                         LanguageContext ctx, ManagedThread *thread)
{
    PandaString msg;
    msg = "idx = " + ToPandaString(idx) + "; length = " + ToPandaString(length);

    ThrowException(ctx, thread, ctx.GetArrayIndexOutOfBoundsExceptionClassDescriptor(),
                   utf::CStringAsMutf8(msg.c_str()));
}

void ThrowIndexOutOfBoundsException(coretypes::array_ssize_t idx, coretypes::array_ssize_t length)
{
    auto *thread = ManagedThread::GetCurrent();
    auto ctx = GetLanguageContext(thread);

    PandaString msg;
    msg = "idx = " + ToPandaString(idx) + "; length = " + ToPandaString(length);

    ThrowException(ctx, thread, ctx.GetIndexOutOfBoundsExceptionClassDescriptor(), utf::CStringAsMutf8(msg.c_str()));
}

void ThrowIllegalStateException(const PandaString &msg)
{
    auto *thread = ManagedThread::GetCurrent();
    auto ctx = GetLanguageContext(thread);
    ThrowException(ctx, thread, ctx.GetIllegalStateExceptionClassDescriptor(), utf::CStringAsMutf8(msg.c_str()));
}

void ThrowStringIndexOutOfBoundsException(coretypes::array_ssize_t idx, coretypes::array_size_t length)
{
    auto *thread = ManagedThread::GetCurrent();
    auto ctx = GetLanguageContext(thread);

    PandaString msg;
    msg = "idx = " + ToPandaString(idx) + "; length = " + ToPandaString(length);

    ThrowException(ctx, thread, ctx.GetStringIndexOutOfBoundsExceptionClassDescriptor(),
                   utf::CStringAsMutf8(msg.c_str()));
}

void ThrowNegativeArraySizeException(coretypes::array_ssize_t size)
{
    auto *thread = ManagedThread::GetCurrent();
    auto ctx = GetLanguageContext(thread);

    PandaString msg;
    msg = "size = " + ToPandaString(size);

    ThrowException(ctx, thread, ctx.GetNegativeArraySizeExceptionClassDescriptor(), utf::CStringAsMutf8(msg.c_str()));
}

void ThrowNegativeArraySizeException(const PandaString &msg)
{
    auto *thread = ManagedThread::GetCurrent();
    auto ctx = GetLanguageContext(thread);
    ThrowException(ctx, thread, ctx.GetNegativeArraySizeExceptionClassDescriptor(), utf::CStringAsMutf8(msg.c_str()));
}

void ThrowArithmeticException()
{
    auto *thread = ManagedThread::GetCurrent();
    auto ctx = GetLanguageContext(thread);
    ThrowException(ctx, thread, ctx.GetArithmeticExceptionClassDescriptor(), utf::CStringAsMutf8("/ by zero"));
}

void ThrowClassCastException(Class *dst_type, Class *src_type)
{
    auto *thread = ManagedThread::GetCurrent();
    auto ctx = GetLanguageContext(thread);

    PandaString msg;
    msg = src_type->GetName() + " cannot be cast to " + dst_type->GetName();

    ThrowException(ctx, thread, ctx.GetClassCastExceptionClassDescriptor(), utf::CStringAsMutf8(msg.c_str()));
}

void ThrowAbstractMethodError(Method *method)
{
    auto *thread = ManagedThread::GetCurrent();
    auto ctx = GetLanguageContext(thread);

    PandaString msg;
    msg = "abstract method \"" + method->GetClass()->GetName() + ".";
    msg += utf::Mutf8AsCString(method->GetName().data);
    msg += "\"";

    ThrowException(ctx, thread, ctx.GetAbstractMethodErrorClassDescriptor(), utf::CStringAsMutf8(msg.c_str()));
}

void ThrowArrayStoreException(Class *array_class, Class *element_class)
{
    PandaStringStream ss;
    ss << element_class->GetName() << " cannot be stored in an array of type " << array_class->GetName();
    ThrowArrayStoreException(ss.str());
}

void ThrowArrayStoreException(const PandaString &msg)
{
    auto *thread = ManagedThread::GetCurrent();
    auto ctx = GetLanguageContext(thread);

    ThrowException(ctx, thread, ctx.GetArrayStoreExceptionClassDescriptor(), utf::CStringAsMutf8(msg.c_str()));
}

void ThrowRuntimeException(const PandaString &msg)
{
    auto *thread = ManagedThread::GetCurrent();
    auto ctx = GetLanguageContext(thread);

    ThrowException(ctx, thread, ctx.GetRuntimeExceptionClassDescriptor(), utf::CStringAsMutf8(msg.c_str()));
}

void ThrowIllegalArgumentException(const PandaString &msg)
{
    auto *thread = ManagedThread::GetCurrent();
    auto ctx = GetLanguageContext(thread);

    ThrowException(ctx, thread, ctx.GetIllegalArgumentExceptionClassDescriptor(), utf::CStringAsMutf8(msg.c_str()));
}

void ThrowClassCircularityError(const PandaString &class_name, LanguageContext ctx)
{
    auto *thread = ManagedThread::GetCurrent();
    PandaString msg = "Class or interface \"" + class_name + "\" is its own superclass or superinterface";
    ThrowException(ctx, thread, ctx.GetClassCircularityErrorDescriptor(), utf::CStringAsMutf8(msg.c_str()));
}

// NOLINTNEXTLINE(google-runtime-references)
NO_ADDRESS_SANITIZE void FindCatchBlockInCFrames([[maybe_unused]] ObjectHeader *exception,
                                                 [[maybe_unused]] StackWalker *stack,
                                                 [[maybe_unused]] Frame *orig_frame)
{
}

NO_ADDRESS_SANITIZE void FindCatchBlockInCallStack(ObjectHeader *exception)
{
    StackWalker stack(ManagedThread::GetCurrent());
    auto orig_frame = stack.GetIFrame();
    ASSERT(!stack.IsCFrame());
    LOG(DEBUG, INTEROP) << "Enter in FindCatchBlockInCallStack for " << orig_frame->GetMethod()->GetFullName();
    // Exception thrown from static constructor should be wrapped by the ExceptionInInitializerError
    if (stack.GetMethod()->IsStaticConstructor()) {
        return;
    }

    stack.NextFrame();

    // JNI frames can handle exceptions as well
    if (!stack.HasFrame() || !stack.IsCFrame() || stack.GetCFrame().IsJni()) {
        return;
    }
    FindCatchBlockInCFrames(exception, &stack, orig_frame);
}

void ThrowFileNotFoundException(const PandaString &msg)
{
    auto *thread = ManagedThread::GetCurrent();
    auto ctx = GetLanguageContext(thread);

    ThrowException(ctx, thread, ctx.GetFileNotFoundExceptionClassDescriptor(), utf::CStringAsMutf8(msg.c_str()));
}

void ThrowIOException(const PandaString &msg)
{
    auto *thread = ManagedThread::GetCurrent();
    auto ctx = GetLanguageContext(thread);

    ThrowException(ctx, thread, ctx.GetIOExceptionClassDescriptor(), utf::CStringAsMutf8(msg.c_str()));
}

void ThrowIllegalAccessException(const PandaString &msg)
{
    auto *thread = ManagedThread::GetCurrent();
    auto ctx = GetLanguageContext(thread);

    ThrowException(ctx, thread, ctx.GetIllegalAccessExceptionClassDescriptor(), utf::CStringAsMutf8(msg.c_str()));
}

void ThrowOutOfMemoryError(ManagedThread *thread, const PandaString &msg)
{
    auto ctx = GetLanguageContext(thread);

    if (thread->IsThrowingOOM()) {
        thread->SetUsePreAllocObj(true);
    }

    thread->SetThrowingOOM(true);
    ThrowException(ctx, thread, ctx.GetOutOfMemoryErrorClassDescriptor(), utf::CStringAsMutf8(msg.c_str()));
    thread->SetThrowingOOM(false);
}

void ThrowOutOfMemoryError(const PandaString &msg)
{
    auto *thread = ManagedThread::GetCurrent();
    ThrowOutOfMemoryError(thread, msg);
}

void ThrowUnsupportedOperationException()
{
    auto *thread = ManagedThread::GetCurrent();
    auto ctx = GetLanguageContext(thread);
    ThrowException(ctx, thread, ctx.GetUnsupportedOperationExceptionClassDescriptor(), nullptr);
}

void ThrowVerificationException(const PandaString &msg)
{
    auto *thread = ManagedThread::GetCurrent();
    auto ctx = GetLanguageContext(thread);

    ThrowException(ctx, thread, ctx.GetVerifyErrorClassDescriptor(), utf::CStringAsMutf8(msg.c_str()));
}

void ThrowVerificationException(LanguageContext ctx, const PandaString &msg)
{
    auto *thread = ManagedThread::GetCurrent();

    ThrowException(ctx, thread, ctx.GetVerifyErrorClassDescriptor(), utf::CStringAsMutf8(msg.c_str()));
}

void ThrowInstantiationError(const PandaString &msg)
{
    auto *thread = ManagedThread::GetCurrent();
    auto ctx = GetLanguageContext(thread);

    ThrowException(ctx, thread, ctx.GetInstantiationErrorDescriptor(), utf::CStringAsMutf8(msg.c_str()));
}

void ThrowNoClassDefFoundError(const PandaString &msg)
{
    auto *thread = ManagedThread::GetCurrent();
    auto ctx = GetLanguageContext(thread);

    ThrowException(ctx, thread, ctx.GetNoClassDefFoundErrorDescriptor(), utf::CStringAsMutf8(msg.c_str()));
}

void ThrowTypedErrorDyn(const std::string &msg)
{
    auto *thread = ManagedThread::GetCurrent();
    auto ctx = GetLanguageContext(thread);
    ThrowException(ctx, thread, ctx.GetTypedErrorDescriptor(), utf::CStringAsMutf8(msg.c_str()));
}

void ThrowReferenceErrorDyn(const std::string &msg)
{
    auto *thread = ManagedThread::GetCurrent();
    auto ctx = GetLanguageContext(thread);
    ThrowException(ctx, thread, ctx.GetReferenceErrorDescriptor(), utf::CStringAsMutf8(msg.c_str()));
}

void ThrowIllegalMonitorStateException(const PandaString &msg)
{
    auto *thread = ManagedThread::GetCurrent();
    auto ctx = GetLanguageContext(thread);

    ThrowException(ctx, thread, ctx.GetIllegalMonitorStateExceptionDescriptor(), utf::CStringAsMutf8(msg.c_str()));
}

}  // namespace panda
