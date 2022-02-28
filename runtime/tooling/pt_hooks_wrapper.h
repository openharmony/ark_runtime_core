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

#ifndef PANDA_RUNTIME_TOOLING_PT_HOOKS_WRAPPER_H_
#define PANDA_RUNTIME_TOOLING_PT_HOOKS_WRAPPER_H_

#include "runtime/include/tooling/debug_interface.h"
#include "os/mutex.h"
#include "runtime/include/managed_thread.h"
#include "pt_thread_info.h"
#include "pt_hook_type_info.h"

// NOLINTNEXTLINE
#define ASSERT_PT_HOOK_NATIVE_CONTEXT() \
    ASSERT(MTManagedThread::GetCurrent() != nullptr && MTManagedThread::GetCurrent()->IsInNativeCode())

namespace panda::tooling {
class ScopedNativePtHook {
public:
    ScopedNativePtHook()
    {
        ManagedThread *managed_thread = ManagedThread::GetCurrent();
        ASSERT(managed_thread != nullptr);
        thread_type_ = managed_thread->GetThreadType();
        if (thread_type_ == Thread::ThreadType::THREAD_TYPE_MANAGED) {
            return;
        }

        ASSERT(thread_type_ == Thread::ThreadType::THREAD_TYPE_MT_MANAGED);
        MTManagedThread *mt_managed_thread = MTManagedThread::CastFromThread(managed_thread);
        if (!mt_managed_thread->IsInNativeCode()) {
            mt_managed_thread_ = mt_managed_thread;
            mt_managed_thread_->NativeCodeBegin();
        }

        PtPushLocalFrameFromNative();
    }

    ~ScopedNativePtHook()
    {
        if (thread_type_ == Thread::ThreadType::THREAD_TYPE_MANAGED) {
            return;
        }
        ASSERT(thread_type_ == Thread::ThreadType::THREAD_TYPE_MT_MANAGED);

        PtPopLocalFrameFromNative();
        if (mt_managed_thread_ != nullptr) {
            mt_managed_thread_->NativeCodeEnd();
        }
    }

    NO_COPY_SEMANTIC(ScopedNativePtHook);
    NO_MOVE_SEMANTIC(ScopedNativePtHook);

private:
    MTManagedThread *mt_managed_thread_ = nullptr;
    Thread::ThreadType thread_type_ = Thread::ThreadType::THREAD_TYPE_NONE;
};

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
class PtHooksWrapper : public PtHooks {
public:
    void SetHooks(PtHooks *hooks)
    {
        os::memory::WriteLockHolder wholder(hooks_rwlock_);
        hooks_ = hooks;
    }

    void EnableGlobalHook(PtHookType hookType)
    {
        global_hook_type_info_.Enable(hookType);
    }

    void DisableGlobalHook(PtHookType hookType)
    {
        global_hook_type_info_.Disable(hookType);
    }

    void EnableAllGlobalHook()
    {
        global_hook_type_info_.EnableAll();
    }

    void DisableAllGlobalHook()
    {
        global_hook_type_info_.DisableAll();
    }

    // Wrappers for hooks
    void Breakpoint(PtThread thread, const PtLocation &location) override
    {
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !HookIsEnabled(PtHookType::PT_HOOK_TYPE_BREAKPOINT)) {
            return;
        }
        ScopedNativePtHook nativeScope;
        ASSERT_PT_HOOK_NATIVE_CONTEXT();
        hooks_->Breakpoint(thread, location);
    }

    void LoadModule(std::string_view pandaFile) override
    {
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !HookIsEnabled(PtHookType::PT_HOOK_TYPE_LOAD_MODULE)) {
            return;
        }
        ScopedNativePtHook nativeScope;
        ASSERT_PT_HOOK_NATIVE_CONTEXT();
        hooks_->LoadModule(pandaFile);
    }

    void Paused(PauseReason reason) override
    {
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !HookIsEnabled(PtHookType::PT_HOOK_TYPE_PAUSED)) {
            return;
        }
        ScopedNativePtHook nativeScope;
        ASSERT_PT_HOOK_NATIVE_CONTEXT();
        hooks_->Paused(reason);
    }

    void Exception(PtThread thread, const PtLocation &location, PtObject exceptionObject,
                   const PtLocation &catchLocation) override
    {
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !HookIsEnabled(PtHookType::PT_HOOK_TYPE_EXCEPTION)) {
            return;
        }
        ScopedNativePtHook nativeScope;
        ASSERT_PT_HOOK_NATIVE_CONTEXT();
        hooks_->Exception(thread, location, exceptionObject, catchLocation);
    }

    void ExceptionCatch(PtThread thread, const PtLocation &location, PtObject exceptionObject) override
    {
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !HookIsEnabled(PtHookType::PT_HOOK_TYPE_EXCEPTION_CATCH)) {
            return;
        }
        ScopedNativePtHook nativeScope;
        ASSERT_PT_HOOK_NATIVE_CONTEXT();
        hooks_->ExceptionCatch(thread, location, exceptionObject);
    }

    void PropertyAccess(PtThread thread, const PtLocation &location, PtObject object, PtProperty property) override
    {
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !HookIsEnabled(PtHookType::PT_HOOK_TYPE_PROPERTY_ACCESS)) {
            return;
        }
        ScopedNativePtHook nativeScope;
        ASSERT_PT_HOOK_NATIVE_CONTEXT();
        hooks_->PropertyAccess(thread, location, object, property);
    }

    void PropertyModification(PtThread thread, const PtLocation &location, PtObject object, PtProperty property,
                              PtValue newValue) override
    {
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !HookIsEnabled(PtHookType::PT_HOOK_TYPE_PROPERTY_MODIFICATION)) {
            return;
        }
        ScopedNativePtHook nativeScope;
        ASSERT_PT_HOOK_NATIVE_CONTEXT();
        hooks_->PropertyModification(thread, location, object, property, newValue);
    }

    void FramePop(PtThread thread, PtMethod method, bool wasPoppedByException) override
    {
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !HookIsEnabled(PtHookType::PT_HOOK_TYPE_FRAME_POP)) {
            return;
        }
        ScopedNativePtHook nativeScope;
        ASSERT_PT_HOOK_NATIVE_CONTEXT();
        hooks_->FramePop(thread, method, wasPoppedByException);
    }

    void GarbageCollectionFinish() override
    {
        // ASSERT(ManagedThread::GetCurrent() == nullptr)
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !GlobalHookIsEnabled(PtHookType::PT_HOOK_TYPE_GARBAGE_COLLECTION_FINISH)) {
            return;
        }
        // Called in an unmanaged thread
        hooks_->GarbageCollectionFinish();
    }

    void GarbageCollectionStart() override
    {
        // ASSERT(ManagedThread::GetCurrent() == nullptr)
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !GlobalHookIsEnabled(PtHookType::PT_HOOK_TYPE_GARBAGE_COLLECTION_START)) {
            return;
        }
        // Called in an unmanaged thread
        hooks_->GarbageCollectionStart();
    }

    void MethodEntry(PtThread thread, PtMethod method) override
    {
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !HookIsEnabled(PtHookType::PT_HOOK_TYPE_METHOD_ENTRY)) {
            return;
        }
        ScopedNativePtHook nativeScope;
        ASSERT_PT_HOOK_NATIVE_CONTEXT();
        hooks_->MethodEntry(thread, method);
    }

    void MethodExit(PtThread thread, PtMethod method, bool wasPoppedByException, PtValue returnValue) override
    {
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !HookIsEnabled(PtHookType::PT_HOOK_TYPE_METHOD_EXIT)) {
            return;
        }
        ScopedNativePtHook nativeScope;
        ASSERT_PT_HOOK_NATIVE_CONTEXT();
        hooks_->MethodExit(thread, method, wasPoppedByException, returnValue);
    }

    void SingleStep(PtThread thread, const PtLocation &location) override
    {
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !HookIsEnabled(PtHookType::PT_HOOK_TYPE_SINGLE_STEP)) {
            return;
        }
        ScopedNativePtHook nativeScope;
        ASSERT_PT_HOOK_NATIVE_CONTEXT();
        hooks_->SingleStep(thread, location);
    }

    void ThreadStart(PtThread thread) override
    {
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !HookIsEnabled(PtHookType::PT_HOOK_TYPE_THREAD_START)) {
            return;
        }
        ScopedNativePtHook nativeScope;
        ASSERT_PT_HOOK_NATIVE_CONTEXT();
        hooks_->ThreadStart(thread);
    }

    void ThreadEnd(PtThread thread) override
    {
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !HookIsEnabled(PtHookType::PT_HOOK_TYPE_THREAD_END)) {
            return;
        }
        ScopedNativePtHook nativeScope;
        ASSERT_PT_HOOK_NATIVE_CONTEXT();
        hooks_->ThreadEnd(thread);
    }

    void VmStart() override
    {
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !HookIsEnabled(PtHookType::PT_HOOK_TYPE_VM_START)) {
            return;
        }
        ScopedNativePtHook nativeScope;
        ASSERT_PT_HOOK_NATIVE_CONTEXT();
        hooks_->VmStart();
    }

    void VmInitialization(PtThread thread) override
    {
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !HookIsEnabled(PtHookType::PT_HOOK_TYPE_VM_INITIALIZATION)) {
            return;
        }
        ScopedNativePtHook nativeScope;
        ASSERT_PT_HOOK_NATIVE_CONTEXT();
        hooks_->VmInitialization(thread);
    }

    void VmDeath() override
    {
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
#ifndef NDEBUG
        vmdeath_did_not_happen_ = false;
#endif
        if (hooks_ == nullptr || !HookIsEnabled(PtHookType::PT_HOOK_TYPE_VM_DEATH)) {
            return;
        }
        ManagedThread *thread = ManagedThread::GetCurrent();
        LOG_IF(thread->IsThreadAlive(), FATAL, RUNTIME) << "Main Thread should have been destroyed";
        hooks_->VmDeath();
    }

    void ExceptionRevoked(ExceptionWrapper reason, ExceptionID exceptionId) override
    {
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !HookIsEnabled(PtHookType::PT_HOOK_TYPE_EXCEPTION_REVOKED)) {
            return;
        }
        ScopedNativePtHook nativeScope;
        ASSERT_PT_HOOK_NATIVE_CONTEXT();
        hooks_->ExceptionRevoked(reason, exceptionId);
    }

    void ExecutionContextCreated(ExecutionContextWrapper context) override
    {
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !HookIsEnabled(PtHookType::PT_HOOK_TYPE_EXECUTION_CONTEXT_CREATEED)) {
            return;
        }
        ScopedNativePtHook nativeScope;
        ASSERT_PT_HOOK_NATIVE_CONTEXT();
        hooks_->ExecutionContextCreated(context);
    }

    void ExecutionContextDestroyed(ExecutionContextWrapper context) override
    {
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !HookIsEnabled(PtHookType::PT_HOOK_TYPE_EXECUTION_CONTEXT_DESTROYED)) {
            return;
        }
        ScopedNativePtHook nativeScope;
        ASSERT_PT_HOOK_NATIVE_CONTEXT();
        hooks_->ExecutionContextDestroyed(context);
    }

    void ExecutionContextsCleared() override
    {
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !HookIsEnabled(PtHookType::PT_HOOK_TYPE_EXECUTION_CONTEXTS_CLEARED)) {
            return;
        }
        ScopedNativePtHook nativeScope;
        ASSERT_PT_HOOK_NATIVE_CONTEXT();
        hooks_->ExecutionContextsCleared();
    }

    void InspectRequested(PtObject object, PtObject hints) override
    {
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !HookIsEnabled(PtHookType::PT_HOOK_TYPE_INSPECT_REQUESTED)) {
            return;
        }
        ScopedNativePtHook nativeScope;
        ASSERT_PT_HOOK_NATIVE_CONTEXT();
        hooks_->InspectRequested(object, hints);
    }

    void ClassLoad(PtThread thread, PtClass klass) override
    {
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !HookIsEnabled(PtHookType::PT_HOOK_TYPE_CLASS_LOAD)) {
            return;
        }
        ScopedNativePtHook nativeScope;
        ASSERT_PT_HOOK_NATIVE_CONTEXT();
        hooks_->ClassLoad(thread, klass);
    }

    void ClassPrepare(PtThread thread, PtClass klass) override
    {
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !HookIsEnabled(PtHookType::PT_HOOK_TYPE_CLASS_PREPARE)) {
            return;
        }
        ScopedNativePtHook nativeScope;
        ASSERT_PT_HOOK_NATIVE_CONTEXT();
        hooks_->ClassPrepare(thread, klass);
    }

    void MonitorWait(PtThread thread, PtObject object, int64_t timeout) override
    {
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !HookIsEnabled(PtHookType::PT_HOOK_TYPE_MONITOR_WAIT)) {
            return;
        }
        ScopedNativePtHook nativeScope;
        ASSERT_PT_HOOK_NATIVE_CONTEXT();
        hooks_->MonitorWait(thread, object, timeout);
    }

    void MonitorWaited(PtThread thread, PtObject object, bool timedOut) override
    {
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !HookIsEnabled(PtHookType::PT_HOOK_TYPE_MONITOR_WAITED)) {
            return;
        }
        ScopedNativePtHook nativeScope;
        ASSERT_PT_HOOK_NATIVE_CONTEXT();
        hooks_->MonitorWaited(thread, object, timedOut);
    }

    void MonitorContendedEnter(PtThread thread, PtObject object) override
    {
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !HookIsEnabled(PtHookType::PT_HOOK_TYPE_MONITOR_CONTENDED_ENTER)) {
            return;
        }
        ScopedNativePtHook nativeScope;
        ASSERT_PT_HOOK_NATIVE_CONTEXT();
        hooks_->MonitorContendedEnter(thread, object);
    }

    void MonitorContendedEntered(PtThread thread, PtObject object) override
    {
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !HookIsEnabled(PtHookType::PT_HOOK_TYPE_MONITOR_CONTENDED_ENTERED)) {
            return;
        }
        ScopedNativePtHook nativeScope;
        ASSERT_PT_HOOK_NATIVE_CONTEXT();
        hooks_->MonitorContendedEntered(thread, object);
    }

    void ObjectAlloc(PtClass klass, PtObject object, PtThread thread, size_t size) override
    {
        os::memory::ReadLockHolder rholder(hooks_rwlock_);
        ASSERT(vmdeath_did_not_happen_);
        if (hooks_ == nullptr || !HookIsEnabled(PtHookType::PT_HOOK_TYPE_OBJECT_ALLOC)) {
            return;
        }
        ScopedNativePtHook nativeScope;
        ASSERT_PT_HOOK_NATIVE_CONTEXT();
        hooks_->ObjectAlloc(klass, object, thread, size);
    }

private:
    bool GlobalHookIsEnabled(PtHookType type) const
    {
        return global_hook_type_info_.IsEnabled(type);
    }

    bool HookIsEnabled(PtHookType type) const
    {
        if (GlobalHookIsEnabled(type)) {
            return true;
        }

        MTManagedThread *mt_managed_thread = MTManagedThread::GetCurrent();
        ASSERT(mt_managed_thread != nullptr);

        // Check local value
        return mt_managed_thread->GetPtThreadInfo()->GetHookTypeInfo().IsEnabled(type);
    }

    PtHooks *hooks_ = nullptr;
    mutable os::memory::RWLock hooks_rwlock_;

    PtHookTypeInfo global_hook_type_info_ {true};

#ifndef NDEBUG
    bool vmdeath_did_not_happen_ = true;
#endif
};

}  // namespace panda::tooling

#endif  // PANDA_RUNTIME_TOOLING_PT_HOOKS_WRAPPER_H_
