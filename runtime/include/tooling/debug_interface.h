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

#ifndef PANDA_RUNTIME_INCLUDE_TOOLING_DEBUG_INTERFACE_H_
#define PANDA_RUNTIME_INCLUDE_TOOLING_DEBUG_INTERFACE_H_

#include <cstdint>
#include <list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "libpandabase/macros.h"
#include "libpandabase/utils/expected.h"
#include "libpandafile/file.h"
#include "runtime/include/mem/panda_containers.h"
#include "runtime/include/thread.h"
#include "runtime/include/tooling/pt_class.h"
#include "runtime/include/tooling/pt_location.h"
#include "runtime/include/tooling/pt_method.h"
#include "runtime/include/tooling/pt_object.h"
#include "runtime/include/tooling/pt_property.h"
#include "runtime/include/tooling/pt_thread.h"
#include "runtime/include/tooling/pt_value.h"
#include "runtime/interpreter/frame.h"

namespace panda::tooling {
class PtLangExt;

class Error {
public:
    enum class Type {
        BREAKPOINT_NOT_FOUND,
        BREAKPOINT_ALREADY_EXISTS,
        ENTRY_POINT_RESOLVE_ERROR,
        FRAME_NOT_FOUND,
        NO_MORE_FRAMES,
        OPAQUE_FRAME,
        INVALID_BREAKPOINT,
        INVALID_ENTRY_POINT,
        METHOD_NOT_FOUND,
        PANDA_FILE_LOAD_ERROR,
        THREAD_NOT_FOUND,
        THREAD_NOT_SUSPENDED,
        INVALID_REGISTER,
        INVALID_VALUE,
        INVALID_EXPRESSION,
        PROPERTY_ACCESS_WATCH_NOT_FOUND,
        INVALID_PROPERTY_ACCESS_WATCH,
        PROPERTY_MODIFY_WATCH_NOT_FOUND,
        INVALID_PROPERTY_MODIFY_WATCH
    };

    Error(Type type, std::string msg) : type_(type), msg_(std::move(msg)) {}

    Type GetType() const
    {
        return type_;
    }

    std::string GetMessage() const
    {
        return msg_;
    }

    ~Error() = default;

    DEFAULT_COPY_SEMANTIC(Error);
    DEFAULT_MOVE_SEMANTIC(Error);

private:
    Type type_;
    std::string msg_;
};

class PtFrame {
public:
    PtFrame() = default;

    virtual bool IsInterpreterFrame() const = 0;

    virtual PtMethod GetPtMethod() const = 0;

    virtual uint64_t GetVReg(size_t i) const = 0;

    virtual size_t GetVRegNum() const = 0;

    virtual uint64_t GetArgument(size_t i) const = 0;

    virtual size_t GetArgumentNum() const = 0;

    virtual uint64_t GetAccumulator() const = 0;

    virtual panda_file::File::EntityId GetMethodId() const = 0;

    virtual uint32_t GetBytecodeOffset() const = 0;

    virtual std::string GetPandaFile() const = 0;

    // mock API
    virtual uint32_t GetFrameId() const = 0;

    virtual ~PtFrame() = default;

    NO_COPY_SEMANTIC(PtFrame);
    NO_MOVE_SEMANTIC(PtFrame);
};

struct PtStepRange {
    uint32_t start_bc_offset {0};
    uint32_t end_bc_offset {0};
};

// * * * * *
// Mock API helpers
// * * * * *

using ExceptionID = panda_file::File::EntityId;
using ExecutionContextId = panda_file::File::EntityId;
using threadGroup = uint32_t;

using ExpressionWrapper = std::string;
using ExceptionWrapper = std::string;

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
struct ThreadInfo {
    char *name;
    size_t name_lenght;
    int32_t priority;
    bool is_daemon;
    threadGroup thread_group;
    PtObject context_class_loader;
};

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
struct PandaClassDefinition {
    PtClass klass;
    uint32_t class_byte_count;
    const unsigned char *class_bytes;
};

enum PauseReason {
    AMBIGUOUS,
    ASSERT,
    DEBUGCOMMAND,
    DOM,
    EVENTLISTENER,
    EXCEPTION,
    INSTRUMENTATION,
    OOM,
    OTHER,
    PROMISEREJECTION,
    XHR,
    BREAK_ON_START
};

struct ExecutionContextWrapper {
    ExecutionContextId id;
    std::string origin;
    std::string name;
};

enum class PtHookType {
    PT_HOOK_TYPE_BREAKPOINT,
    PT_HOOK_TYPE_LOAD_MODULE,
    PT_HOOK_TYPE_PAUSED,
    PT_HOOK_TYPE_EXCEPTION,
    PT_HOOK_TYPE_EXCEPTION_CATCH,
    PT_HOOK_TYPE_PROPERTY_ACCESS,
    PT_HOOK_TYPE_PROPERTY_MODIFICATION,
    PT_HOOK_TYPE_FRAME_POP,
    PT_HOOK_TYPE_GARBAGE_COLLECTION_START,
    PT_HOOK_TYPE_GARBAGE_COLLECTION_FINISH,
    PT_HOOK_TYPE_METHOD_ENTRY,
    PT_HOOK_TYPE_METHOD_EXIT,
    PT_HOOK_TYPE_SINGLE_STEP,
    PT_HOOK_TYPE_THREAD_START,
    PT_HOOK_TYPE_THREAD_END,
    PT_HOOK_TYPE_VM_DEATH,
    PT_HOOK_TYPE_VM_INITIALIZATION,
    PT_HOOK_TYPE_VM_START,
    PT_HOOK_TYPE_EXCEPTION_REVOKED,
    PT_HOOK_TYPE_EXECUTION_CONTEXT_CREATEED,
    PT_HOOK_TYPE_EXECUTION_CONTEXT_DESTROYED,
    PT_HOOK_TYPE_EXECUTION_CONTEXTS_CLEARED,
    PT_HOOK_TYPE_INSPECT_REQUESTED,
    PT_HOOK_TYPE_CLASS_LOAD,
    PT_HOOK_TYPE_CLASS_PREPARE,
    PT_HOOK_TYPE_MONITOR_WAIT,
    PT_HOOK_TYPE_MONITOR_WAITED,
    PT_HOOK_TYPE_MONITOR_CONTENDED_ENTER,
    PT_HOOK_TYPE_MONITOR_CONTENDED_ENTERED,
    PT_HOOK_TYPE_OBJECT_ALLOC,
    // The count of hooks. Don't move
    PT_HOOK_TYPE_COUNT
};

// * * * * *
// Mock API helpers ends
// * * * * *

class PtHooks {
public:
    PtHooks() = default;

    /**
     * \brief Method is called by the runtime when breakpoint hits. Thread where breakpoint hits is stopped until
     * continue or step event will be received
     * @param thread Identifier of the thread where breakpoint hits. Now the callback is called in the same
     * thread
     * @param location Breakpoint location
     */
    virtual void Breakpoint(PtThread thread, const PtLocation &location) = 0;

    /**
     * \brief Method is called by the runtime when panda file is loaded
     * @param pandaFileName Path to panda file that is loaded
     */
    virtual void LoadModule(std::string_view pandaFileName) = 0;

    /**
     * \brief Method is called by the runtime when managed thread is attached to it
     * @param thread The attached thread
     */
    virtual void ThreadStart(PtThread thread) = 0;

    /**
     * \brief Method is called by the runtime when managed thread is detached
     * @param thread The detached thread
     */
    virtual void ThreadEnd(PtThread thread) = 0;

    /**
     * \brief Method is called by the runtime when virtual machine start initialization
     */
    virtual void VmStart() = 0;

    /**
     * \brief Method is called by the runtime when virtual machine finish initialization
     * @param thread The initial thread
     */
    virtual void VmInitialization(PtThread thread) = 0;

    /**
     * \brief Method is called by the runtime when virtual machine death
     */
    virtual void VmDeath() = 0;

    /**
     * \brief Method is called by the runtime when a class is first loaded
     * @param thread Thread loading the class
     * @param klass Class being loaded
     */
    virtual void ClassLoad(PtThread thread, PtClass klass) = 0;

    /**
     * \brief Method is called by the runtime when class preparation is complete
     * @param thread Thread generating the class prepare
     * @param klass Class being prepared
     */
    virtual void ClassPrepare(PtThread thread, PtClass klass) = 0;

    /**
     * \brief Method is called by the runtime when a thread is about to wait on an object
     * @param thread The thread about to wait
     * @param object Reference to the monitor
     * @param timeout The number of milliseconds the thread will wait
     */
    virtual void MonitorWait(PtThread thread, PtObject object, int64_t timeout) = 0;

    /**
     * \brief Method is called by the runtime when a thread finishes waiting on an object
     * @param thread The thread about to wait
     * @param object Reference to the monitor
     * @param timedOut True if the monitor timed out
     */
    virtual void MonitorWaited(PtThread thread, PtObject object, bool timedOut) = 0;

    /**
     * \brief Method is called by the runtime when a thread is attempting to enter a monitor already acquired by another
     * thread
     * @param thread The thread about to wait
     * @param object Reference to the monitor
     */
    virtual void MonitorContendedEnter(PtThread thread, PtObject object) = 0;

    /**
     * \brief Method is called by the runtime when a thread enters a monitor after waiting for it to be released by
     * another thread
     * @param thread The thread about to wait
     * @param object Reference to the monitor
     */
    virtual void MonitorContendedEntered(PtThread thread, PtObject object) = 0;

    // * * * * *
    // mock API for panda debugger events
    // * * * * *

    virtual void Paused(PauseReason reason) = 0;

    virtual void Exception(PtThread thread, const PtLocation &location, PtObject exceptionObject,
                           const PtLocation &catchLocation) = 0;

    virtual void ExceptionCatch(PtThread thread, const PtLocation &location, PtObject exceptionObject) = 0;

    virtual void PropertyAccess(PtThread thread, const PtLocation &location, PtObject object, PtProperty property) = 0;

    virtual void PropertyModification(PtThread thread, const PtLocation &location, PtObject object, PtProperty property,
                                      PtValue newValue) = 0;

    virtual void FramePop(PtThread thread, PtMethod method, bool wasPoppedByException) = 0;

    virtual void GarbageCollectionFinish() = 0;

    virtual void GarbageCollectionStart() = 0;

    virtual void ObjectAlloc(PtClass klass, PtObject object, PtThread thread, size_t size) = 0;

    virtual void MethodEntry(PtThread thread, PtMethod method) = 0;

    virtual void MethodExit(PtThread thread, PtMethod method, bool wasPoppedByException, PtValue returnValue) = 0;

    virtual void SingleStep(PtThread thread, const PtLocation &location) = 0;

    virtual void ExceptionRevoked(ExceptionWrapper reason, ExceptionID exceptionId) = 0;

    virtual void ExecutionContextCreated(ExecutionContextWrapper context) = 0;

    virtual void ExecutionContextDestroyed(ExecutionContextWrapper context) = 0;

    virtual void ExecutionContextsCleared() = 0;

    virtual void InspectRequested(PtObject object, PtObject hints) = 0;

    // * * * * *
    // mock API ends
    // * * * * *

    virtual ~PtHooks() = default;

    NO_COPY_SEMANTIC(PtHooks);
    NO_MOVE_SEMANTIC(PtHooks);
};

class DebugInterface {
public:
    DebugInterface() = default;

    /**
     * \brief Register debug hooks in the runtime
     * @param hooks Pointer to object that implements PtHooks interface
     * @return Error if any errors occur
     */
    virtual std::optional<Error> RegisterHooks(PtHooks *hooks) = 0;

    /**
     * \brief Unregister debug hooks in the runtime
     * @return Error if any errors occur
     */
    virtual std::optional<Error> UnregisterHooks() = 0;

    /**
     * \brief Enable all debug hooks in the runtime
     * @return Error if any errors occur
     */
    virtual std::optional<Error> EnableAllGlobalHook() = 0;

    /**
     * \brief Disable all debug hooks in the runtime
     * @return Error if any errors occur
     */
    virtual std::optional<Error> DisableAllGlobalHook() = 0;

    /**
     * \brief Set notification to hook (enable/disable).
     * @param thread If thread is NONE, the notification is enabled or disabled globally
     * @param enable Enable or disable notifications (true - enable, false - disable)
     * @param hook_type Type of hook that must be enabled or disabled
     * @return Error if any errors occur
     */
    virtual std::optional<Error> SetNotification(PtThread thread, bool enable, PtHookType hookType) = 0;

    /**
     * \brief Set breakpoint to \param location
     * @param location Breakpoint location
     * @return Error if any errors occur
     */
    virtual std::optional<Error> SetBreakpoint(const PtLocation &location) = 0;

    /**
     * \brief Remove breakpoint from \param location
     * @param location Breakpoint location
     * @return Error if any errors occur
     */
    virtual std::optional<Error> RemoveBreakpoint(const PtLocation &location) = 0;

    /**
     * \brief Get Frame
     * @param thread Identifier of the thread
     * @return Frame object that implements PtFrame or Error if any errors occur
     */
    virtual Expected<std::unique_ptr<PtFrame>, Error> GetCurrentFrame(PtThread thread) const = 0;

    /**
     * \brief Enumerates managed frames in the thread \param threadId
     * @param thread Identifier of the thread
     * @param callback Callback that is called for each frame. Should return true to continue and false to stop
     * enumerating
     * @return Error if any errors occur
     */
    virtual std::optional<Error> EnumerateFrames(PtThread thread,
                                                 std::function<bool(const PtFrame &)> callback) const = 0;

    /**
     * \brief Suspend thread
     * @param thread Identifier of the thread
     * @return Error if any errors occur
     */
    virtual std::optional<Error> SuspendThread(PtThread thread) const = 0;

    /**
     * \brief Resume thread
     * @param thread Identifier of the thread
     * @return Error if any errors occur
     */
    virtual std::optional<Error> ResumeThread(PtThread thread) const = 0;

    virtual ~DebugInterface() = default;

    virtual PtLangExt *GetLangExtension() const = 0;

    virtual Expected<PtMethod, Error> GetPtMethod(const PtLocation &location) const = 0;

    /*
     * Mock API for debug interphase starts:
     *
     * add get_thread_list
     * what variables and IDs we can't get
     */
    virtual std::optional<Error> GetThreadList(PandaVector<PtThread> *threadList) const = 0;

    virtual std::optional<Error> SetVariable(PtThread thread, uint32_t frameDepth, int32_t regNumber,
                                             const PtValue &value) const = 0;

    virtual std::optional<Error> GetVariable(PtThread thread, uint32_t frameDepth, int32_t regNumber,
                                             PtValue *value) const = 0;

    virtual std::optional<Error> GetProperty(PtObject thisObject, PtProperty property, PtValue *value) const = 0;

    virtual std::optional<Error> SetProperty(PtObject thisObject, PtProperty property, const PtValue &value) const = 0;

    virtual std::optional<Error> EvaluateExpression(PtThread thread, uint32_t frameNumber, ExpressionWrapper expr,
                                                    PtValue *result) const = 0;

    virtual std::optional<Error> RetransformClasses(int classCount, const PtClass *classes) const = 0;

    virtual std::optional<Error> RedefineClasses(int classCount, const PandaClassDefinition *classes) const = 0;

    virtual std::optional<Error> GetThreadInfo(PtThread thread, ThreadInfo *infoPtr) const = 0;

    virtual std::optional<Error> RestartFrame(PtThread thread, uint32_t frameNumber) const = 0;

    virtual std::optional<Error> SetAsyncCallStackDepth(uint32_t maxDepth) const = 0;

    virtual std::optional<Error> AwaitPromise(PtObject promiseObject, PtValue *result) const = 0;

    virtual std::optional<Error> CallFunctionOn(PtObject object, PtMethod method, const PandaVector<PtValue> &arguments,
                                                PtValue *result) const = 0;

    virtual std::optional<Error> GetProperties(uint32_t *countPtr, char ***propertyPtr) const = 0;

    virtual std::optional<Error> NotifyFramePop(PtThread thread, uint32_t depth) const = 0;

    virtual std::optional<Error> SetPropertyAccessWatch(PtClass klass, PtProperty property) = 0;

    virtual std::optional<Error> ClearPropertyAccessWatch(PtClass klass, PtProperty property) = 0;

    virtual std::optional<Error> SetPropertyModificationWatch(PtClass klass, PtProperty property) = 0;

    virtual std::optional<Error> GetThisVariableByFrame(PtThread thread, uint32_t frameDepth, PtValue *value) = 0;

    virtual std::optional<Error> ClearPropertyModificationWatch(PtClass klass, PtProperty property) = 0;

    // * * * * *
    // Mock API ends
    // * * * * *

    NO_COPY_SEMANTIC(DebugInterface);
    NO_MOVE_SEMANTIC(DebugInterface);
};

}  // namespace panda::tooling

#endif  // PANDA_RUNTIME_INCLUDE_TOOLING_DEBUG_INTERFACE_H_
