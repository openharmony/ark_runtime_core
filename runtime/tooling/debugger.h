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

#ifndef PANDA_RUNTIME_TOOLING_DEBUGGER_H_
#define PANDA_RUNTIME_TOOLING_DEBUGGER_H_

#include <atomic>
#include <functional>
#include <memory>
#include <string_view>

#include "include/method.h"
#include "include/runtime.h"
#include "pt_hooks_wrapper.h"
#include "include/mem/panda_smart_pointers.h"
#include "include/mem/panda_containers.h"
#include "include/runtime_notification.h"
#include "include/tooling/debug_interface.h"
#include "libpandabase/utils/span.h"
#include "runtime/include/mem/panda_containers.h"
#include "runtime/include/method.h"
#include "runtime/include/runtime.h"
#include "runtime/include/panda_vm.h"
#include "runtime/include/tooling/debug_interface.h"
#include "runtime/thread_manager.h"

namespace panda::tooling {
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
class Breakpoint {
public:
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    Breakpoint(Method *method, uint32_t bcOffset) : method_(method), bc_offset_(bcOffset) {}
    ~Breakpoint() = default;

    Method *GetMethod() const
    {
        return method_;
    }

    uint32_t GetBytecodeOffset() const
    {
        return bc_offset_;
    }

    bool operator==(const Breakpoint &bpoint) const
    {
        return GetMethod() == bpoint.GetMethod() && GetBytecodeOffset() == bpoint.GetBytecodeOffset();
    }

    DEFAULT_COPY_SEMANTIC(Breakpoint);
    DEFAULT_MOVE_SEMANTIC(Breakpoint);

private:
    Method *method_;
    uint32_t bc_offset_;
};

class HashBreakpoint {
public:
    size_t operator()(const Breakpoint &bpoint) const
    {
        return (std::hash<Method *>()(bpoint.GetMethod())) ^ (std::hash<uint32_t>()(bpoint.GetBytecodeOffset()));
    }
};

class PropertyWatch {
public:
    enum class Type { ACCESS, MODIFY };

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    PropertyWatch(panda_file::File::EntityId classId, panda_file::File::EntityId fieldId, Type type)
        : class_id_(classId), field_id_(fieldId), type_(type)
    {
    }

    ~PropertyWatch() = default;

    panda_file::File::EntityId GetClassId() const
    {
        return class_id_;
    }

    panda_file::File::EntityId GetFieldId() const
    {
        return field_id_;
    }

    Type GetType() const
    {
        return type_;
    }

private:
    NO_COPY_SEMANTIC(PropertyWatch);
    NO_MOVE_SEMANTIC(PropertyWatch);

    panda_file::File::EntityId class_id_;
    panda_file::File::EntityId field_id_;
    Type type_;
};

class Debugger : public DebugInterface, RuntimeListener {
public:
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    explicit Debugger(const Runtime *runtime)
        : runtime_(runtime),
          breakpoints_(GetInternalAllocatorAdapter(runtime)),
          property_watches_(GetInternalAllocatorAdapter(runtime)),
          vm_started_(runtime->IsInitialized())
    {
        runtime_->GetNotificationManager()->AddListener(this, DEBUG_EVENT_MASK);
    }

    ~Debugger() override
    {
        runtime_->GetNotificationManager()->RemoveListener(this, DEBUG_EVENT_MASK);
    }

    PtLangExt *GetLangExtension() const override
    {
        return runtime_->GetPtLangExt();
    }

    Expected<PtMethod, Error> GetPtMethod(const PtLocation &location) const override;

    std::optional<Error> RegisterHooks(PtHooks *hooks) override
    {
        hooks_.SetHooks(hooks);
        return {};
    }

    std::optional<Error> UnregisterHooks() override
    {
        hooks_.SetHooks(nullptr);
        return {};
    }

    std::optional<Error> EnableAllGlobalHook() override
    {
        hooks_.EnableAllGlobalHook();
        return {};
    }

    std::optional<Error> DisableAllGlobalHook() override
    {
        hooks_.DisableAllGlobalHook();
        return {};
    }

    std::optional<Error> SetNotification(PtThread thread, bool enable, PtHookType hookType) override;
    std::optional<Error> SetBreakpoint(const PtLocation &location) override;

    std::optional<Error> RemoveBreakpoint(const PtLocation &location) override;

    Expected<std::unique_ptr<PtFrame>, Error> GetCurrentFrame(PtThread thread) const override;

    std::optional<Error> EnumerateFrames(PtThread thread, std::function<bool(const PtFrame &)> callback) const override;

    // RuntimeListener methods

    void LoadModule(std::string_view filename) override
    {
        hooks_.LoadModule(filename);
    }

    void ThreadStart(ManagedThread::ThreadId threadId) override
    {
        hooks_.ThreadStart(PtThread(threadId));
    }

    void ThreadEnd(ManagedThread::ThreadId threadId) override
    {
        hooks_.ThreadEnd(PtThread(threadId));
    }

    void BytecodePcChanged(ManagedThread *thread, Method *method, uint32_t bcOffset) override;

    void VmStart() override
    {
        vm_started_ = true;
        hooks_.VmStart();
    }

    void VmInitialization(ManagedThread::ThreadId threadId) override
    {
        hooks_.VmInitialization(PtThread(threadId));
    }

    void VmDeath() override
    {
        hooks_.VmDeath();
    }

    void GarbageCollectorStart() override
    {
        hooks_.GarbageCollectionStart();
    }

    void GarbageCollectorFinish() override
    {
        hooks_.GarbageCollectionFinish();
    }

    void ObjectAlloc(BaseClass *klass, ObjectHeader *object, ManagedThread *thread, size_t size) override;

    void ExceptionCatch(const ManagedThread *thread, const Method *method, uint32_t bcOffset) override;

    void MethodEntry(ManagedThread *thread, Method *method) override;
    void MethodExit(ManagedThread *thread, Method *method) override;

    void ClassLoad(Class *klass) override;
    void ClassPrepare(Class *klass) override;

    void MonitorWait(ObjectHeader *object, int64_t timeout) override;
    void MonitorWaited(ObjectHeader *object, bool timedOut) override;
    void MonitorContendedEnter(ObjectHeader *object) override;
    void MonitorContendedEntered(ObjectHeader *object) override;

    /*
     * Mock API for debug interphase starts:
     *
     * API's function should be revorked and input parameters should be added
     */
    std::optional<Error> GetThreadList(PandaVector<PtThread> *threadList) const override
    {
        runtime_->GetPandaVM()->GetThreadManager()->EnumerateThreads(
            [threadList](MTManagedThread *mt_managed_thread) {
                ASSERT(mt_managed_thread && "thread is null");
                threadList->push_back(PtThread(mt_managed_thread->GetId()));
                return true;
            },
            static_cast<unsigned int>(panda::EnumerationFlag::ALL),
            static_cast<unsigned int>(panda::EnumerationFlag::VM_THREAD));

        return {};
    }

    std::optional<Error> GetThreadInfo(PtThread thread, ThreadInfo *infoPtr) const override
    {
        MTManagedThread *mt_managed_thread = GetManagedThreadByPtThread(thread);

        if (mt_managed_thread == nullptr) {
            return Error(Error::Type::THREAD_NOT_FOUND,
                         std::string("Thread ") + std::to_string(thread.GetId()) + " not found");
        }

        infoPtr->is_daemon = mt_managed_thread->IsDaemon();
        infoPtr->priority = mt_managed_thread->GetThreadPriority();
        /* fields that didn't still implemented (we don't support it):
         * infoPtr->thread_group
         * infoPtr->context_class_loader
         */
        return {};
    }

    std::optional<Error> SuspendThread(PtThread thread) const override;

    std::optional<Error> ResumeThread(PtThread thread) const override;

    std::optional<Error> SetVariable(PtThread thread, uint32_t frameDepth, int32_t regNumber,
                                     const PtValue &value) const override;

    std::optional<Error> GetVariable(PtThread thread, uint32_t frameDepth, int32_t regNumber,
                                     PtValue *result) const override;

    std::optional<Error> GetProperty([[maybe_unused]] PtObject object, [[maybe_unused]] PtProperty property,
                                     PtValue *value) const override
    {
        std::cout << "GetProperty called " << std::endl;
        const int64_t anydata = 0x123456789;
        value->SetValue(anydata);
        return {};
    }

    std::optional<Error> SetProperty([[maybe_unused]] PtObject object, [[maybe_unused]] PtProperty property,
                                     [[maybe_unused]] const PtValue &value) const override
    {
        std::cout << "SetProperty called " << std::endl;
        return {};
    }

    std::optional<Error> EvaluateExpression([[maybe_unused]] PtThread thread, [[maybe_unused]] uint32_t frameNumber,
                                            ExpressionWrapper expr, PtValue *result) const override
    {
        std::cout << "EvaluateExpression called " << std::endl;
        if (expr.empty()) {
            return Error(Error::Type::INVALID_EXPRESSION, "invalid expression");
        }
        const int64_t anydata = 0x123456789;
        result->SetValue(anydata);
        return {};
    }

    std::optional<Error> RetransformClasses([[maybe_unused]] int classCount,
                                            [[maybe_unused]] const PtClass *classes) const override
    {
        std::cout << "RetransformClasses called " << std::endl;
        return {};
    }

    std::optional<Error> RedefineClasses([[maybe_unused]] int classCount,
                                         [[maybe_unused]] const PandaClassDefinition *classes) const override
    {
        std::cout << "RedefineClasses called " << std::endl;
        return {};
    }

    std::optional<Error> RestartFrame([[maybe_unused]] PtThread thread,
                                      [[maybe_unused]] uint32_t frameNumber) const override;

    std::optional<Error> SetAsyncCallStackDepth([[maybe_unused]] uint32_t maxDepth) const override
    {
        std::cout << "SetAsyncCallStackDepth called " << std::endl;
        return {};
    }

    std::optional<Error> AwaitPromise([[maybe_unused]] PtObject promiseObject, PtValue *result) const override
    {
        const uint32_t anyobj = 123456789;
        result->SetValue(anyobj);

        std::cout << "AwaitPromise called " << std::endl;
        return {};
    }

    std::optional<Error> CallFunctionOn([[maybe_unused]] PtObject object, [[maybe_unused]] PtMethod method,
                                        [[maybe_unused]] const PandaVector<PtValue> &arguments,
                                        PtValue *returnValue) const override
    {
        const int64_t anydata = 0x123456789;
        returnValue->SetValue(anydata);
        std::cout << "CallFunctionOn called " << std::endl;
        return {};
    }

    std::optional<Error> GetProperties(uint32_t *countPtr, [[maybe_unused]] char ***propertyPtr) const override
    {
        *countPtr = 0;
        std::cout << "GetProperties called " << std::endl;
        return {};
    }

    std::optional<Error> NotifyFramePop(PtThread thread, uint32_t depth) const override;

    std::optional<Error> SetPropertyAccessWatch(PtClass klass, PtProperty property) override;

    std::optional<Error> ClearPropertyAccessWatch(PtClass klass, PtProperty property) override;

    std::optional<Error> SetPropertyModificationWatch(PtClass klass, PtProperty property) override;

    std::optional<Error> ClearPropertyModificationWatch(PtClass klass, PtProperty property) override;

    std::optional<Error> GetThisVariableByFrame(PtThread thread, uint32_t frameDepth, PtValue *result) override;

private:
    Expected<panda::Frame::VRegister *, Error> GetVRegByPtThread(PtThread thread, uint32_t frameDepth,
                                                                 int32_t regNumber) const;
    const tooling::Breakpoint *FindBreakpoint(const Method *method, uint32_t bcOffset) const;
    bool RemoveBreakpoint(Method *method, uint32_t bcOffset);

    MTManagedThread *GetManagedThreadByPtThread(PtThread thread) const;

    bool IsPropertyWatchActive() const
    {
        return !property_watches_.empty();
    }
    const tooling::PropertyWatch *FindPropertyWatch(panda_file::File::EntityId classId,
                                                    panda_file::File::EntityId fieldId,
                                                    tooling::PropertyWatch::Type type) const;
    bool RemovePropertyWatch(panda_file::File::EntityId classId, panda_file::File::EntityId fieldId,
                             tooling::PropertyWatch::Type type);

    bool HandleBreakpoint(const ManagedThread *thread, const Method *method, uint32_t bcOffset);
    void HandleNotifyFramePop(ManagedThread *thread, Method *method, bool wasPoppedByException);
    void HandleExceptionThrowEvent(ManagedThread *thread, Method *method, uint32_t bcOffset);
    bool HandleStep(const ManagedThread *thread, const Method *method, uint32_t bcOffset);

    bool HandlePropertyAccess(const ManagedThread *thread, const Method *method, uint32_t bcOffset);
    bool HandlePropertyModify(const ManagedThread *thread, const Method *method, uint32_t bcOffset);

    static constexpr uint32_t DEBUG_EVENT_MASK =
        RuntimeNotificationManager::Event::LOAD_MODULE | RuntimeNotificationManager::Event::THREAD_EVENTS |
        RuntimeNotificationManager::Event::BYTECODE_PC_CHANGED | RuntimeNotificationManager::Event::EXCEPTION_EVENTS |
        RuntimeNotificationManager::Event::VM_EVENTS | RuntimeNotificationManager::Event::GARBAGE_COLLECTOR_EVENTS |
        RuntimeNotificationManager::Event::METHOD_EVENTS | RuntimeNotificationManager::Event::CLASS_EVENTS |
        RuntimeNotificationManager::Event::MONITOR_EVENTS | RuntimeNotificationManager::Event::ALLOCATION_EVENTS;

    const Runtime *runtime_;
    PtHooksWrapper hooks_;

    PandaUnorderedSet<tooling::Breakpoint, tooling::HashBreakpoint> breakpoints_;
    PandaList<tooling::PropertyWatch> property_watches_;
    bool vm_started_ {false};

    NO_COPY_SEMANTIC(Debugger);
    NO_MOVE_SEMANTIC(Debugger);
};

class PtDebugFrame : public PtFrame {
public:
    explicit PtDebugFrame(Method *method, const Frame *interpreterFrame);
    ~PtDebugFrame() override = default;

    bool IsInterpreterFrame() const override
    {
        return is_interpreter_frame_;
    }

    PtMethod GetPtMethod() const override
    {
        return method_;
    }

    uint64_t GetVReg(size_t i) const override
    {
        if (!is_interpreter_frame_) {
            return 0;
        }
        return vregs_[i];
    }

    size_t GetVRegNum() const override
    {
        return vregs_.size();
    }

    uint64_t GetArgument(size_t i) const override
    {
        if (!is_interpreter_frame_) {
            return 0;
        }
        return args_[i];
    }

    size_t GetArgumentNum() const override
    {
        return args_.size();
    }

    uint64_t GetAccumulator() const override
    {
        return acc_;
    }

    panda_file::File::EntityId GetMethodId() const override
    {
        return method_id_;
    }

    uint32_t GetBytecodeOffset() const override
    {
        return bc_offset_;
    }

    std::string GetPandaFile() const override
    {
        return panda_file_;
    }

    // mock API
    uint32_t GetFrameId() const override
    {
        return 0;
    }

private:
    NO_COPY_SEMANTIC(PtDebugFrame);
    NO_MOVE_SEMANTIC(PtDebugFrame);

    bool is_interpreter_frame_;
    PtMethod method_;
    uint64_t acc_ {0};
    PandaVector<uint64_t> vregs_;
    PandaVector<uint64_t> args_;
    panda_file::File::EntityId method_id_;
    uint32_t bc_offset_ {0};
    std::string panda_file_;
};

}  // namespace panda::tooling

#endif  // PANDA_RUNTIME_TOOLING_DEBUGGER_H_
