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

#include "debugger.h"
#include "include/stack_walker.h"
#include "include/tooling/pt_location.h"
#include "include/tooling/pt_thread.h"
#include "pt_method_private.h"
#include "pt_scoped_managed_code.h"
#include "pt_lang_ext_private.h"
#include "pt_object_private.h"
#include "interpreter/frame.h"
#include "include/mem/panda_smart_pointers.h"
#include "tooling/pt_object_private.h"
#include "tooling/pt_reference_private.h"
#include "pt_thread_info.h"

#include "libpandabase/macros.h"
#include "libpandabase/os/mem.h"
#include "libpandabase/utils/expected.h"
#include "libpandabase/utils/span.h"
#include "libpandafile/bytecode_instruction.h"
#include "runtime/include/mem/panda_smart_pointers.h"
#include "runtime/include/stack_walker.h"
#include "runtime/interpreter/frame.h"
#include "runtime/tooling/pt_method_private.h"
#include "runtime/tooling/pt_value_private.h"

namespace panda::tooling {
static PtLangExtPrivate *GetPtLangExtPrivate()
{
    return reinterpret_cast<PtLangExtPrivate *>(Runtime::GetCurrent()->GetPtLangExt());
}

std::optional<Error> Debugger::SetNotification(PtThread thread, bool enable, PtHookType hookType)
{
    if (thread == PtThread::NONE) {
        if (enable) {
            hooks_.EnableGlobalHook(hookType);
        } else {
            hooks_.DisableGlobalHook(hookType);
        }
    } else {
        MTManagedThread *mt_managed_thread = GetManagedThreadByPtThread(thread);
        if (enable) {
            mt_managed_thread->GetPtThreadInfo()->GetHookTypeInfo().Enable(hookType);
        } else {
            mt_managed_thread->GetPtThreadInfo()->GetHookTypeInfo().Disable(hookType);
        }
    }

    return {};
}

std::optional<Error> Debugger::SetBreakpoint(const PtLocation &location)
{
    Method *method = runtime_->GetClassLinker()->GetMethod(location.GetPandaFile(), location.GetMethodId());
    if (method == nullptr) {
        return Error(Error::Type::METHOD_NOT_FOUND,
                     std::string("Cannot find method with id ") + std::to_string(location.GetMethodId().GetOffset()) +
                         " in panda file '" + std::string(location.GetPandaFile()) + "'");
    }

    if (location.GetBytecodeOffset() >= method->GetCodeSize()) {
        return Error(Error::Type::INVALID_BREAKPOINT, std::string("Invalid breakpoint location: bytecode offset (") +
                                                          std::to_string(location.GetBytecodeOffset()) +
                                                          ") >= method code size (" +
                                                          std::to_string(method->GetCodeSize()) + ")");
    }

    if (!breakpoints_.emplace(method, location.GetBytecodeOffset()).second) {
        return Error(Error::Type::BREAKPOINT_ALREADY_EXISTS,
                     std::string("Breakpoint already exists: bytecode offset ") +
                         std::to_string(location.GetBytecodeOffset()));
    }

    return {};
}

std::optional<Error> Debugger::RemoveBreakpoint(const PtLocation &location)
{
    Method *method = runtime_->GetClassLinker()->GetMethod(location.GetPandaFile(), location.GetMethodId());
    if (method == nullptr) {
        return Error(Error::Type::METHOD_NOT_FOUND,
                     std::string("Cannot find method with id ") + std::to_string(location.GetMethodId().GetOffset()) +
                         " in panda file '" + std::string(location.GetPandaFile()) + "'");
    }

    if (!RemoveBreakpoint(method, location.GetBytecodeOffset())) {
        return Error(Error::Type::BREAKPOINT_NOT_FOUND, "Breakpoint not found");
    }

    return {};
}

static panda::Frame *GetPandaFrame(ManagedThread *thread, uint32_t frameDepth = 0, bool *outIsNative = nullptr)
{
    StackWalker stack(thread);

    while (stack.HasFrame() && frameDepth != 0) {
        stack.NextFrame();
        --frameDepth;
    }

    bool isNative = false;
    panda::Frame *frame = nullptr;
    if (stack.HasFrame()) {
        if (!stack.IsCFrame()) {
            frame = stack.GetIFrame();
        } else {
            isNative = true;
        }
    }

    if (outIsNative != nullptr) {
        *outIsNative = isNative;
    }

    return frame;
}

Expected<panda::Frame::VRegister *, Error> Debugger::GetVRegByPtThread(PtThread thread, uint32_t frameDepth,
                                                                       int32_t regNumber) const
{
    MTManagedThread *mt_managed_thread = GetManagedThreadByPtThread(thread);
    if (MTManagedThread::GetCurrent() != mt_managed_thread && !mt_managed_thread->IsUserSuspended()) {
        return Unexpected(Error(Error::Type::THREAD_NOT_SUSPENDED,
                                std::string("Thread " + std::to_string(thread.GetId()) + " is not suspended")));
    }

    bool isNative = false;
    panda::Frame *frame = GetPandaFrame(mt_managed_thread, frameDepth, &isNative);
    if (frame == nullptr) {
        if (isNative) {
            return Unexpected(Error(Error::Type::OPAQUE_FRAME,
                                    std::string("Frame is native, threadId=" + std::to_string(thread.GetId()) +
                                                " frameDepth=" + std::to_string(frameDepth))));
        }

        return Unexpected(Error(Error::Type::FRAME_NOT_FOUND,
                                std::string("Frame not found or native, threadId=" + std::to_string(thread.GetId()) +
                                            " frameDepth=" + std::to_string(frameDepth))));
    }

    if (regNumber == -1) {
        return &frame->GetAcc();
    }

    if (regNumber >= 0 && uint32_t(regNumber) < frame->GetSize()) {
        return &frame->GetVReg(uint32_t(regNumber));
    }

    return Unexpected(
        Error(Error::Type::INVALID_REGISTER, std::string("Invalid register number: ") + std::to_string(regNumber)));
}

std::optional<Error> Debugger::GetVariable(PtThread thread, uint32_t frameDepth, int32_t regNumber,
                                           PtValue *result) const
{
    ASSERT_NATIVE_CODE();
    auto ret = GetVRegByPtThread(thread, frameDepth, regNumber);
    if (!ret) {
        return ret.Error();
    }

    Frame::VRegister *reg = ret.Value();
    PtScopedManagedCode smc;
    return GetPtLangExtPrivate()->GetPtValueFromManaged(*reg, result);
}

std::optional<Error> Debugger::SetVariable(PtThread thread, uint32_t frameDepth, int32_t regNumber,
                                           const PtValue &value) const
{
    ASSERT_NATIVE_CODE();
    auto ret = GetVRegByPtThread(thread, frameDepth, regNumber);
    if (!ret) {
        return ret.Error();
    }

    Frame::VRegister *reg = ret.Value();
    PtScopedManagedCode smc;
    return GetPtLangExtPrivate()->StorePtValueFromManaged(value, reg);
}

Expected<std::unique_ptr<PtFrame>, Error> Debugger::GetCurrentFrame(PtThread thread) const
{
    MTManagedThread *mt_managed_thread = GetManagedThreadByPtThread(thread);
    if (mt_managed_thread == nullptr) {
        return Unexpected(Error(Error::Type::THREAD_NOT_FOUND,
                                std::string("Thread ") + std::to_string(thread.GetId()) + " not found"));
    }

    StackWalker stack(mt_managed_thread);

    Method *method = stack.GetMethod();
    Frame *interpreterFrame = nullptr;

    if (!stack.IsCFrame()) {
        interpreterFrame = stack.GetIFrame();
    }

    return {std::make_unique<PtDebugFrame>(method, interpreterFrame)};
}

std::optional<Error> Debugger::EnumerateFrames(PtThread thread, std::function<bool(const PtFrame &)> callback) const
{
    MTManagedThread *mt_managed_thread = GetManagedThreadByPtThread(thread);
    if (mt_managed_thread == nullptr) {
        return Error(Error::Type::THREAD_NOT_FOUND,
                     std::string("Thread ") + std::to_string(thread.GetId()) + " not found");
    }

    StackWalker stack(mt_managed_thread);
    while (stack.HasFrame()) {
        Method *method = stack.GetMethod();
        Frame *frame = stack.IsCFrame() ? nullptr : stack.GetIFrame();
        PtDebugFrame debug_frame(method, frame);
        if (!callback(debug_frame)) {
            break;
        }
        stack.NextFrame();
    }

    return {};
}

std::optional<Error> Debugger::SuspendThread(PtThread thread) const
{
    auto managedThread = GetManagedThreadByPtThread(thread);
    if (managedThread == nullptr) {
        return Error(Error::Type::THREAD_NOT_FOUND,
                     std::string("MT thread ") + std::to_string(thread.GetId()) + " not found");
    }
    managedThread->SuspendImpl();

    return {};
}

std::optional<Error> Debugger::ResumeThread(PtThread thread) const
{
    auto managedThread = GetManagedThreadByPtThread(thread);
    if (managedThread == nullptr) {
        return Error(Error::Type::THREAD_NOT_FOUND,
                     std::string("MT thread ") + std::to_string(thread.GetId()) + " not found");
    }
    managedThread->ResumeImpl();

    return {};
}

Expected<PtMethod, Error> Debugger::GetPtMethod(const PtLocation &location) const
{
    panda_file::File::EntityId methodId = location.GetMethodId();
    const char *pandaFile = location.GetPandaFile();
    Method *method = runtime_->GetClassLinker()->GetMethod(pandaFile, methodId);
    if (method == nullptr) {
        return Unexpected(Error(Error::Type::METHOD_NOT_FOUND, std::string("Cannot find method with id ") +
                                                                   std::to_string(methodId.GetOffset()) +
                                                                   " in panda file '" + std::string(pandaFile) + "'"));
    }
    return MethodToPtMethod(method);
}

std::optional<Error> Debugger::RestartFrame(PtThread thread, uint32_t frameNumber) const
{
    MTManagedThread *mt_managed_thread = GetManagedThreadByPtThread(thread);
    if (mt_managed_thread == nullptr) {
        return Error(Error::Type::THREAD_NOT_FOUND,
                     std::string("Thread ") + std::to_string(thread.GetId()) + " not found");
    }
    if (!mt_managed_thread->IsUserSuspended() && mt_managed_thread->IsJavaThread()) {
        return Error(Error::Type::THREAD_NOT_SUSPENDED,
                     std::string("Thread ") + std::to_string(thread.GetId()) + " is not suspended");
    }

    StackWalker stack(mt_managed_thread);
    panda::Frame *popFrame = nullptr;
    panda::Frame *retryFrame = nullptr;
    uint32_t currentFrameNumber = 0;

    while (stack.HasFrame()) {
        if (stack.IsCFrame()) {
            return Error(Error::Type::OPAQUE_FRAME, std::string("Thread ") + std::to_string(thread.GetId()) +
                                                        ", frame at depth is executing a native method");
        }
        if (currentFrameNumber == frameNumber) {
            popFrame = stack.GetIFrame();
        } else if (currentFrameNumber == (frameNumber + 1)) {
            retryFrame = stack.GetIFrame();
            break;
        }
        ++currentFrameNumber;
        stack.NextFrame();
    }

    if (popFrame == nullptr) {
        return Error(Error::Type::FRAME_NOT_FOUND, std::string("Thread ") + std::to_string(thread.GetId()) +
                                                       " doesn't have managed frame with number " +
                                                       std::to_string(frameNumber));
    }

    if (retryFrame == nullptr) {
        return Error(Error::Type::NO_MORE_FRAMES, std::string("Thread ") + std::to_string(thread.GetId()) +
                                                      " does not have more than one frame on the call stack");
    }

    // Set force pop frames from top to target
    stack.Reset(mt_managed_thread);
    while (stack.HasFrame()) {
        panda::Frame *frame = stack.GetIFrame();
        frame->SetForcePop();
        if (frame == popFrame) {
            break;
        }
        stack.NextFrame();
    }
    retryFrame->SetRetryInstruction();

    return {};
}

std::optional<Error> Debugger::NotifyFramePop(PtThread thread, uint32_t depth) const
{
    MTManagedThread *mt_managed_thread = GetManagedThreadByPtThread(thread);
    if (mt_managed_thread == nullptr) {
        return Error(Error::Type::THREAD_NOT_FOUND,
                     std::string("Thread ") + std::to_string(thread.GetId()) + " not found");
    }

    bool isNative = false;
    panda::Frame *popFrame = GetPandaFrame(mt_managed_thread, depth, &isNative);
    if (popFrame == nullptr) {
        if (isNative) {
            return Error(Error::Type::OPAQUE_FRAME, std::string("Thread ") + std::to_string(thread.GetId()) +
                                                        ", frame at depth is executing a native method");
        }

        return Error(Error::Type::NO_MORE_FRAMES,
                     std::string("Thread ") + std::to_string(thread.GetId()) +
                         ", are no stack frames at the specified depth: " + std::to_string(depth));
    }

    popFrame->SetNotifyPop();
    return {};
}

void Debugger::BytecodePcChanged(ManagedThread *thread, Method *method, uint32_t bcOffset)
{
    ASSERT(bcOffset < method->GetCodeSize() && "code size of current method less then bcOffset");

    HandleExceptionThrowEvent(thread, method, bcOffset);

    // Step event is reported before breakpoint, according to the spec.
    HandleStep(thread, method, bcOffset);
    HandleBreakpoint(thread, method, bcOffset);

    if (IsPropertyWatchActive()) {
        if (!HandlePropertyAccess(thread, method, bcOffset)) {
            HandlePropertyModify(thread, method, bcOffset);
        }
    }
}

void Debugger::ObjectAlloc(BaseClass *klass, ObjectHeader *object, ManagedThread *thread, size_t size)
{
    if (!vm_started_) {
        return;
    }
    if (thread == nullptr) {
        thread = ManagedThread::GetCurrent();
    }
    if (thread == nullptr) {
        return;
    }

    PtThread ptThread(thread->GetId());
    PtLangExtPrivate *ext = GetPtLangExtPrivate();
    PtClass ptClass = ext->ClassToPtClass(klass);
    PtScopedObjectPrivate scopedObject(object);
    hooks_.ObjectAlloc(ptClass, scopedObject.GetObject(), ptThread, size);
}

void Debugger::MethodEntry(ManagedThread *managedThread, Method *method)
{
    uint32_t threadId = managedThread->GetId();
    PtThread ptThread(threadId);

    hooks_.MethodEntry(ptThread, MethodToPtMethod(method));
}

void Debugger::MethodExit(ManagedThread *managedThread, Method *method)
{
    bool isExceptionTriggered = managedThread->HasPendingException();
    PtThread ptThread(managedThread->GetId());
    PtValue retValue(managedThread->GetCurrentFrame()->GetAcc().GetValue());
    hooks_.MethodExit(ptThread, MethodToPtMethod(method), isExceptionTriggered, retValue);

    HandleNotifyFramePop(managedThread, method, isExceptionTriggered);
}

static bool IsSkipClassEvent()
{
    auto *thread = ManagedThread::GetCurrent();
    return (thread == nullptr || thread->IsJSThread());
}

void Debugger::ClassLoad(Class *klass)
{
    if (!vm_started_ || IsSkipClassEvent()) {
        return;
    }

    PtLangExtPrivate *ext = GetPtLangExtPrivate();
    PtThread ptThread(ManagedThread::GetCurrent()->GetId());
    PtClass ptClass = ext->ClassToPtClass(klass);

    hooks_.ClassLoad(ptThread, ptClass);
}

void Debugger::ClassPrepare(Class *klass)
{
    if (!vm_started_ || IsSkipClassEvent()) {
        return;
    }

    PtLangExtPrivate *ext = GetPtLangExtPrivate();
    PtThread ptThread(ManagedThread::GetCurrent()->GetId());
    PtClass ptClass = ext->ClassToPtClass(klass);

    hooks_.ClassPrepare(ptThread, ptClass);
}

void Debugger::MonitorWait(ObjectHeader *object, int64_t timeout)
{
    PtThread ptThread(ManagedThread::GetCurrent()->GetId());
    PtScopedObjectPrivate ptScopedObj(object);

    hooks_.MonitorWait(ptThread, ptScopedObj.GetObject(), timeout);
}

void Debugger::MonitorWaited(ObjectHeader *object, bool timedOut)
{
    PtThread ptThread(ManagedThread::GetCurrent()->GetId());
    PtScopedObjectPrivate ptScopedObj(object);

    hooks_.MonitorWaited(ptThread, ptScopedObj.GetObject(), timedOut);
}

void Debugger::MonitorContendedEnter(ObjectHeader *object)
{
    PtThread ptThread(ManagedThread::GetCurrent()->GetId());
    PtScopedObjectPrivate ptScopedObj(object);

    hooks_.MonitorContendedEnter(ptThread, ptScopedObj.GetObject());
}

void Debugger::MonitorContendedEntered(ObjectHeader *object)
{
    PtThread ptThread(ManagedThread::GetCurrent()->GetId());
    PtScopedObjectPrivate ptScopedObj(object);

    hooks_.MonitorContendedEntered(ptThread, ptScopedObj.GetObject());
}

bool Debugger::HandleBreakpoint(const ManagedThread *managedThread, const Method *method, uint32_t bcOffset)
{
    if (FindBreakpoint(method, bcOffset) == nullptr) {
        return false;
    }

    auto *pf = method->GetPandaFile();
    PtLocation location {pf->GetFilename().c_str(), method->GetFileId(), bcOffset};
    hooks_.Breakpoint(PtThread(managedThread->GetId()), location);

    return true;
}

void Debugger::HandleExceptionThrowEvent(ManagedThread *thread, Method *method, uint32_t bcOffset)
{
    if (!thread->HasPendingException() || thread->GetPtThreadInfo()->GetPtActiveExceptionThrown()) {
        return;
    }

    thread->GetPtThreadInfo()->SetPtActiveExceptionThrown(true);

    auto *pf = method->GetPandaFile();
    LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(*method);
    std::pair<Method *, uint32_t> res = ctx.GetCatchMethodAndOffset(method, thread);
    auto *catchMethodFile = res.first->GetPandaFile();

    PtLocation throwLocation {pf->GetFilename().c_str(), method->GetFileId(), bcOffset};
    PtLocation catchLocation {catchMethodFile->GetFilename().c_str(), res.first->GetFileId(), res.second};

    ObjectHeader *exceptionObject = thread->GetException();
    PtScopedObjectPrivate ptScopedExObj(exceptionObject);

    thread->GetPtThreadInfo()->SetCurrentException(ptScopedExObj.GetObject());

    hooks_.Exception(PtThread(thread->GetId()), throwLocation, ptScopedExObj.GetObject(), catchLocation);
}

void Debugger::ExceptionCatch(const ManagedThread *thread, const Method *method, uint32_t bcOffset)
{
    ASSERT(!thread->HasPendingException() && thread->GetPtThreadInfo()->GetPtActiveExceptionThrown());

    thread->GetPtThreadInfo()->SetPtActiveExceptionThrown(false);

    auto *pf = method->GetPandaFile();
    PtLocation catchLocation {pf->GetFilename().c_str(), method->GetFileId(), bcOffset};

    PtObject exceptionObject = thread->GetPtThreadInfo()->GetCurrentException();
    hooks_.ExceptionCatch(PtThread(thread->GetId()), catchLocation, exceptionObject);
    thread->GetPtThreadInfo()->ResetCurrentException();
}

bool Debugger::HandleStep(const ManagedThread *managedThread, const Method *method, uint32_t bcOffset)
{
    auto *pf = method->GetPandaFile();
    PtLocation location {pf->GetFilename().c_str(), method->GetFileId(), bcOffset};
    hooks_.SingleStep(PtThread(managedThread->GetId()), location);
    return true;
}

void Debugger::HandleNotifyFramePop(ManagedThread *managedThread, Method *method, bool wasPoppedByException)
{
    panda::Frame *frame = GetPandaFrame(managedThread);
    if (frame != nullptr && frame->IsNotifyPop()) {
        hooks_.FramePop(PtThread(managedThread->GetId()), MethodToPtMethod(method), wasPoppedByException);
        frame->ClearNotifyPop();
    }
}

bool Debugger::HandlePropertyAccess(const ManagedThread *thread, const Method *method, uint32_t bcOffset)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    BytecodeInstruction inst(method->GetInstructions() + bcOffset);
    auto opcode = inst.GetOpcode();
    bool isStatic = false;

    switch (opcode) {
        case BytecodeInstruction::Opcode::LDOBJ_V8_ID16:
        case BytecodeInstruction::Opcode::LDOBJ_64_V8_ID16:
        case BytecodeInstruction::Opcode::LDOBJ_OBJ_V8_ID16:
            break;
        case BytecodeInstruction::Opcode::LDSTATIC_ID16:
        case BytecodeInstruction::Opcode::LDSTATIC_64_ID16:
        case BytecodeInstruction::Opcode::LDSTATIC_OBJ_ID16:
            isStatic = true;
            break;
        default:
            return false;
    }

    auto propertyIndex = inst.GetId().AsIndex();
    auto propertyId = method->GetClass()->ResolveFieldIndex(propertyIndex);
    auto *classLinker = Runtime::GetCurrent()->GetClassLinker();
    ASSERT(classLinker);
    auto *field = classLinker->GetField(*method, propertyId);
    ASSERT(field);
    auto *klass = field->GetClass();
    ASSERT(klass);

    if (FindPropertyWatch(klass->GetFileId(), field->GetFileId(), PropertyWatch::Type::ACCESS) == nullptr) {
        return false;
    }

    PtLocation location {method->GetPandaFile()->GetFilename().c_str(), method->GetFileId(), bcOffset};
    PtThread ptThread(thread->GetId());
    PtLangExtPrivate *ext = GetPtLangExtPrivate();
    PtProperty ptProperty = ext->FieldToPtProperty(field);

    if (isStatic) {
        hooks_.PropertyAccess(ptThread, location, PtObject(), ptProperty);
    } else {
        Frame::VRegister &reg = thread->GetCurrentFrame()->GetVReg(inst.GetVReg());
        ASSERT(reg.HasObject());
        PtScopedObjectPrivate slo(reg.GetReference());
        hooks_.PropertyAccess(ptThread, location, slo.GetObject(), ptProperty);
    }

    return true;
}

bool Debugger::HandlePropertyModify(const ManagedThread *thread, const Method *method, uint32_t bcOffset)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    BytecodeInstruction inst(method->GetInstructions() + bcOffset);
    auto opcode = inst.GetOpcode();
    bool isStatic = false;

    switch (opcode) {
        case BytecodeInstruction::Opcode::STOBJ_V8_ID16:
        case BytecodeInstruction::Opcode::STOBJ_64_V8_ID16:
        case BytecodeInstruction::Opcode::STOBJ_OBJ_V8_ID16:
            break;
        case BytecodeInstruction::Opcode::STSTATIC_ID16:
        case BytecodeInstruction::Opcode::STSTATIC_64_ID16:
        case BytecodeInstruction::Opcode::STSTATIC_OBJ_ID16:
            isStatic = true;
            break;
        default:
            return false;
    }

    auto propertyIdx = inst.GetId().AsIndex();
    auto propertyId = method->GetClass()->ResolveFieldIndex(propertyIdx);
    auto *classLinker = Runtime::GetCurrent()->GetClassLinker();
    ASSERT(classLinker);
    auto *field = classLinker->GetField(*method, propertyId);
    ASSERT(field);
    auto *klass = field->GetClass();
    ASSERT(klass);

    if (FindPropertyWatch(klass->GetFileId(), field->GetFileId(), PropertyWatch::Type::MODIFY) == nullptr) {
        return false;
    }

    PtThread ptThread(thread->GetId());
    PtLangExtPrivate *ext = GetPtLangExtPrivate();
    PtLocation location {method->GetPandaFile()->GetFilename().c_str(), method->GetFileId(), bcOffset};
    PtProperty ptProperty = ext->FieldToPtProperty(field);

    PtValuePrivate svfm(ext, &thread->GetCurrentFrame()->GetAcc());
    if (isStatic) {
        hooks_.PropertyModification(ptThread, location, PtObject(), ptProperty, svfm.GetValue());
    } else {
        Frame::VRegister &reg = thread->GetCurrentFrame()->GetVReg(inst.GetVReg());
        ASSERT(reg.HasObject());
        PtScopedObjectPrivate slo(reg.GetReference());
        hooks_.PropertyModification(ptThread, location, slo.GetObject(), ptProperty, svfm.GetValue());
    }

    return true;
}

std::optional<Error> Debugger::SetPropertyAccessWatch(PtClass klass, PtProperty property)
{
    PtLangExtPrivate *langExt = GetPtLangExtPrivate();
    panda_file::File::EntityId classId = langExt->PtClassToClass(klass)->GetFileId();
    panda_file::File::EntityId propertyId = langExt->PtPropertyToField(property)->GetFileId();
    if (FindPropertyWatch(classId, propertyId, PropertyWatch::Type::ACCESS) != nullptr) {
        return Error(Error::Type::INVALID_PROPERTY_ACCESS_WATCH,
                     std::string("Invalid property access watch, already exist, ClassID: ") +
                         std::to_string(classId.GetOffset()) +
                         ", PropertyID: " + std::to_string(propertyId.GetOffset()));
    }
    property_watches_.emplace_back(classId, propertyId, PropertyWatch::Type::ACCESS);
    return {};
}

std::optional<Error> Debugger::ClearPropertyAccessWatch(PtClass klass, PtProperty property)
{
    PtLangExtPrivate *langExt = GetPtLangExtPrivate();
    panda_file::File::EntityId classId = langExt->PtClassToClass(klass)->GetFileId();
    panda_file::File::EntityId propertyId = langExt->PtPropertyToField(property)->GetFileId();
    if (!RemovePropertyWatch(classId, propertyId, PropertyWatch::Type::ACCESS)) {
        return Error(Error::Type::PROPERTY_ACCESS_WATCH_NOT_FOUND,
                     std::string("Property access watch not found, ClassID: ") + std::to_string(classId.GetOffset()) +
                         ", PropertyID: " + std::to_string(propertyId.GetOffset()));
    }
    return {};
}

std::optional<Error> Debugger::SetPropertyModificationWatch(PtClass klass, PtProperty property)
{
    PtLangExtPrivate *langExt = GetPtLangExtPrivate();
    panda_file::File::EntityId classId = langExt->PtClassToClass(klass)->GetFileId();
    panda_file::File::EntityId propertyId = langExt->PtPropertyToField(property)->GetFileId();
    if (FindPropertyWatch(classId, propertyId, PropertyWatch::Type::MODIFY) != nullptr) {
        return Error(Error::Type::INVALID_PROPERTY_MODIFY_WATCH,
                     std::string("Invalid property modification watch, already exist, ClassID: ") +
                         std::to_string(classId.GetOffset()) + ", PropertyID" + std::to_string(propertyId.GetOffset()));
    }
    property_watches_.emplace_back(classId, propertyId, PropertyWatch::Type::MODIFY);
    return {};
}

std::optional<Error> Debugger::ClearPropertyModificationWatch(PtClass klass, PtProperty property)
{
    PtLangExtPrivate *langExt = GetPtLangExtPrivate();
    panda_file::File::EntityId classId = langExt->PtClassToClass(klass)->GetFileId();
    panda_file::File::EntityId propertyId = langExt->PtPropertyToField(property)->GetFileId();
    if (!RemovePropertyWatch(classId, propertyId, PropertyWatch::Type::MODIFY)) {
        return Error(Error::Type::PROPERTY_MODIFY_WATCH_NOT_FOUND,
                     std::string("Property modification watch not found, ClassID: ") +
                         std::to_string(classId.GetOffset()) + ", PropertyID" + std::to_string(propertyId.GetOffset()));
    }
    return {};
}

const tooling::Breakpoint *Debugger::FindBreakpoint(const Method *method, uint32_t bcOffset) const
{
    for (const auto &bp : breakpoints_) {
        if (bp.GetBytecodeOffset() == bcOffset && bp.GetMethod()->GetPandaFile() == method->GetPandaFile() &&
            bp.GetMethod()->GetFileId() == method->GetFileId()) {
            return &bp;
        }
    }

    return nullptr;
}

bool Debugger::RemoveBreakpoint(Method *method, uint32_t bcOffset)
{
    auto it = breakpoints_.begin();
    while (it != breakpoints_.end()) {
        const auto &bp = *it;
        if (bp.GetBytecodeOffset() == bcOffset && bp.GetMethod() == method) {
            it = breakpoints_.erase(it);
            return true;
        }

        it++;
    }

    return false;
}

const tooling::PropertyWatch *Debugger::FindPropertyWatch(panda_file::File::EntityId classId,
                                                          panda_file::File::EntityId fieldId,
                                                          tooling::PropertyWatch::Type type) const
{
    for (const auto &pw : property_watches_) {
        if (pw.GetClassId() == classId && pw.GetFieldId() == fieldId && pw.GetType() == type) {
            return &pw;
        }
    }

    return nullptr;
}

bool Debugger::RemovePropertyWatch(panda_file::File::EntityId classId, panda_file::File::EntityId fieldId,
                                   tooling::PropertyWatch::Type type)
{
    auto it = property_watches_.begin();
    while (it != property_watches_.end()) {
        const auto &pw = *it;
        if (pw.GetClassId() == classId && pw.GetFieldId() == fieldId && pw.GetType() == type) {
            property_watches_.erase(it);
            return true;
        }

        it++;
    }

    return false;
}

MTManagedThread *Debugger::GetManagedThreadByPtThread(PtThread thread) const
{
    if (thread.GetId() == 0) {
        MTManagedThread *curr_thread = MTManagedThread::GetCurrent();
        ASSERT(curr_thread && "Current thread is nullptr!");
        if (curr_thread->IsJSThread()) {
            return curr_thread;
        }
    }

    MTManagedThread *res = nullptr;
    runtime_->GetPandaVM()->GetThreadManager()->EnumerateThreads(
        [&res, thread](MTManagedThread *mt_managed_thread) {
            if (mt_managed_thread->GetId() == thread.GetId()) {
                res = mt_managed_thread;
                return false;
            }

            return true;
        },
        static_cast<unsigned int>(panda::EnumerationFlag::ALL),
        static_cast<unsigned int>(panda::EnumerationFlag::VM_THREAD));

    return res;
}

static uint64_t GetVRegValue(const Frame::VRegister &reg)
{
    return reg.HasObject() ? reinterpret_cast<uintptr_t>(reg.GetReference()) : reg.GetLong();
}

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
PtDebugFrame::PtDebugFrame(Method *method, const Frame *interpreterFrame) : method_(MethodToPtMethod(method))
{
    panda_file_ = method->GetPandaFile()->GetFilename();
    method_id_ = method->GetFileId();

    is_interpreter_frame_ = interpreterFrame != nullptr;
    if (!is_interpreter_frame_) {
        return;
    }

    size_t nregs = method->GetNumVregs();
    size_t nargs = method->GetNumArgs();

    for (size_t i = 0; i < nregs; i++) {
        vregs_.push_back(GetVRegValue(interpreterFrame->GetVReg(i)));
    }

    for (size_t i = 0; i < nargs; i++) {
        args_.push_back(GetVRegValue(interpreterFrame->GetVReg(i + nregs)));
    }

    acc_ = GetVRegValue(interpreterFrame->GetAcc());
    bc_offset_ = interpreterFrame->GetBytecodeOffset();
}
}  // namespace panda::tooling
