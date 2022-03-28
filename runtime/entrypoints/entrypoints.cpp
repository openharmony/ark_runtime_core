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

#include "runtime/entrypoints/entrypoints.h"

#include "libpandabase/events/events.h"
#include "runtime/include/class_linker-inl.h"
#include "runtime/include/coretypes/array.h"
#include "runtime/include/exceptions.h"
#include "runtime/include/method-inl.h"
#include "runtime/include/object_header-inl.h"
#include "runtime/include/runtime.h"
#include "runtime/include/value-inl.h"
#include "runtime/include/panda_vm.h"
#include "runtime/interpreter/frame.h"
#include "runtime/interpreter/interpreter.h"
#include "runtime/interpreter/runtime_interface.h"
#include "runtime/mem/tlab.h"
#include "runtime/handle_base-inl.h"
#include "libpandabase/utils/asan_interface.h"
#include "libpandabase/utils/tsan_interface.h"

namespace panda {

#undef LOG_ENTRYPOINTS

class ScopedLog {
public:
    ScopedLog() = delete;
    explicit ScopedLog(const char *function) : function_(function)
    {
        LOG(DEBUG, INTEROP) << "ENTRYPOINT: " << function;
    }
    ~ScopedLog()
    {
        LOG(DEBUG, INTEROP) << "EXIT ENTRYPOINT: " << function_;
    }
    NO_COPY_SEMANTIC(ScopedLog);
    NO_MOVE_SEMANTIC(ScopedLog);

private:
    std::string function_;
};

#ifdef LOG_ENTRYPOINTS
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_ENTRYPOINT() ScopedLog __log(__FUNCTION__)
#else
#define LOG_ENTRYPOINT()
#endif

// enable stack walker dry run on each entrypoint to discover stack issues early
#if false
#define CHECK_STACK_WALKER StackWalker(ManagedThread::GetCurrent()).Verify();
#else
#define CHECK_STACK_WALKER
#endif

static void HandlePendingException()
{
    auto *thread = ManagedThread::GetCurrent();
    ASSERT(thread->HasPendingException());

    StackWalker stack(thread);
    ASSERT(stack.IsCFrame());

    FindCatchBlockInCFrames(thread->GetException(), &stack, nullptr);
}

extern "C" bool IncrementHotnessCounter(Method *method)
{
    method->IncrementHotnessCounter(0, nullptr);
    return method->GetCompiledEntryPoint() != GetCompiledCodeToInterpreterBridge(method);
}

extern "C" NO_ADDRESS_SANITIZE void InterpreterEntryPoint(Method *method, Frame *frame)
{
    auto pc = method->GetInstructions();
    Method *callee = frame->GetMethod();
    ASSERT(callee != nullptr);

    if (callee->IsAbstract()) {
        ASSERT(pc == nullptr);
        panda::ThrowAbstractMethodError(callee);
        HandlePendingException();
        UNREACHABLE();
    }

    ManagedThread *thread = ManagedThread::GetCurrent();
    Frame *prev_frame = thread->GetCurrentFrame();
    thread->SetCurrentFrame(frame);

    auto is_compiled_code = thread->IsCurrentFrameCompiled();
    thread->SetCurrentFrameIsCompiled(false);
    interpreter::Execute(thread, pc, frame);
    thread->SetCurrentFrameIsCompiled(is_compiled_code);
    if (prev_frame != nullptr && reinterpret_cast<uintptr_t>(prev_frame->GetMethod()) == COMPILED_CODE_TO_INTERPRETER) {
        thread->SetCurrentFrame(prev_frame->GetPrevFrame());
    } else {
        thread->SetCurrentFrame(prev_frame);
    }
}

extern "C" Frame *CreateFrame(uint32_t nregs, Method *method, Frame *prev)
{
    Frame *mem = Thread::GetCurrent()->GetVM()->GetHeapManager()->AllocateFrame(panda::Frame::GetSize(nregs));
    if (UNLIKELY(mem == nullptr)) {
        return nullptr;
    }
    // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
    return (new (mem) panda::Frame(method, prev, nregs));
}

extern "C" Frame *CreateFrameForMethod(Method *method, Frame *prev)
{
    auto nregs = method->GetNumArgs() + method->GetNumVregs();
    return CreateFrame(nregs, method, prev);
}

extern "C" Frame *CreateFrameWithActualArgsAndSize(uint32_t size, uint32_t nregs, uint32_t num_actual_args,
                                                   Method *method, Frame *prev)
{
    auto *mem =
        static_cast<Frame *>(ManagedThread::GetCurrent()->GetStackFrameAllocator()->Alloc(panda::Frame::GetSize(size)));
    if (UNLIKELY(mem == nullptr)) {
        return nullptr;
    }
    // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
    return (new (mem) panda::Frame(method, prev, nregs, num_actual_args));
}

extern "C" Frame *CreateFrameWithActualArgs(uint32_t nregs, uint32_t num_actual_args, Method *method, Frame *prev)
{
    return CreateFrameWithActualArgsAndSize(nregs, nregs, num_actual_args, method, prev);
}

extern "C" Frame *CreateFrameForMethodWithActualArgs(uint32_t num_actual_args, Method *method, Frame *prev)
{
    auto nargs = std::max(num_actual_args, method->GetNumArgs());
    auto nregs = nargs + method->GetNumVregs();
    return CreateFrameWithActualArgs(nregs, num_actual_args, method, prev);
}

extern "C" void FreeFrame(Frame *frame)
{
    ManagedThread::GetCurrent()->GetStackFrameAllocator()->Free(frame);
}

extern "C" DecodedTaggedValue GetInitialTaggedValue(Method *method)
{
    CHECK_STACK_WALKER;
    LOG_ENTRYPOINT();
    return Runtime::GetCurrent()->GetLanguageContext(*method).GetInitialDecodedValue();
}

}  // namespace panda
