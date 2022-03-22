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

#ifndef PANDA_RUNTIME_INCLUDE_RUNTIME_NOTIFICATION_H_
#define PANDA_RUNTIME_INCLUDE_RUNTIME_NOTIFICATION_H_

#include <optional>
#include <string_view>

#include "libpandabase/os/mutex.h"
#include "runtime/include/locks.h"
#include "runtime/include/mem/panda_containers.h"
#include "runtime/include/mem/panda_string.h"
#include "runtime/include/runtime.h"
#include "runtime/include/managed_thread.h"

namespace panda {

class Method;
class Class;
class Rendezvous;

class RuntimeListener {
public:
    RuntimeListener() = default;
    virtual ~RuntimeListener() = default;
    DEFAULT_COPY_SEMANTIC(RuntimeListener);
    DEFAULT_MOVE_SEMANTIC(RuntimeListener);

    virtual void LoadModule([[maybe_unused]] std::string_view name) {}

    virtual void ThreadStart([[maybe_unused]] ManagedThread::ThreadId thread_id) {}
    virtual void ThreadEnd([[maybe_unused]] ManagedThread::ThreadId thread_id) {}

    virtual void BytecodePcChanged([[maybe_unused]] ManagedThread *thread, [[maybe_unused]] Method *method,
                                   [[maybe_unused]] uint32_t bc_offset)
    {
    }

    virtual void GarbageCollectorStart() {}
    virtual void GarbageCollectorFinish() {}

    virtual void ExceptionCatch([[maybe_unused]] const ManagedThread *thread, [[maybe_unused]] const Method *method,
                                [[maybe_unused]] uint32_t bc_offset)
    {
    }

    virtual void VmStart() {}
    virtual void VmInitialization([[maybe_unused]] ManagedThread::ThreadId thread_id) {}
    virtual void VmDeath() {}

    virtual void MethodEntry([[maybe_unused]] ManagedThread *thread, [[maybe_unused]] Method *method) {}
    virtual void MethodExit([[maybe_unused]] ManagedThread *thread, [[maybe_unused]] Method *method) {}

    virtual void ClassLoad([[maybe_unused]] Class *klass) {}
    virtual void ClassPrepare([[maybe_unused]] Class *klass) {}

    virtual void MonitorWait([[maybe_unused]] ObjectHeader *object, [[maybe_unused]] int64_t timeout) {}
    virtual void MonitorWaited([[maybe_unused]] ObjectHeader *object, [[maybe_unused]] bool timed_out) {}
    virtual void MonitorContendedEnter([[maybe_unused]] ObjectHeader *object) {}
    virtual void MonitorContendedEntered([[maybe_unused]] ObjectHeader *object) {}

    virtual void ObjectAlloc([[maybe_unused]] BaseClass *klass, [[maybe_unused]] ObjectHeader *object,
                             [[maybe_unused]] ManagedThread *thread, [[maybe_unused]] size_t size)
    {
    }
};

class DdmListener {
public:
    DdmListener() = default;
    virtual ~DdmListener() = default;
    DEFAULT_COPY_SEMANTIC(DdmListener);
    DEFAULT_MOVE_SEMANTIC(DdmListener);

    virtual void DdmPublishChunk(uint32_t type, const Span<const uint8_t> &data) = 0;
};

class DebuggerListener {
public:
    DebuggerListener() = default;
    virtual ~DebuggerListener() = default;
    DEFAULT_COPY_SEMANTIC(DebuggerListener);
    DEFAULT_MOVE_SEMANTIC(DebuggerListener);

    virtual void StartDebugger() = 0;
    virtual void StopDebugger() = 0;
    virtual bool IsDebuggerConfigured() = 0;
};

class RuntimeNotificationManager {
public:
    enum Event : uint32_t {
        BYTECODE_PC_CHANGED = 0x01,
        LOAD_MODULE = 0x02,
        THREAD_EVENTS = 0x04,
        GARBAGE_COLLECTOR_EVENTS = 0x08,
        EXCEPTION_EVENTS = 0x10,
        VM_EVENTS = 0x20,
        METHOD_EVENTS = 0x40,
        CLASS_EVENTS = 0x80,
        MONITOR_EVENTS = 0x100,
        ALLOCATION_EVENTS = 0x200,
        ALL = 0xFFFFFFFF
    };

    explicit RuntimeNotificationManager(mem::AllocatorPtr<mem::AllocatorPurpose::ALLOCATOR_PURPOSE_INTERNAL> allocator)
        : bytecode_pc_listeners_(allocator->Adapter()),
          load_module_listeners_(allocator->Adapter()),
          thread_events_listeners_(allocator->Adapter()),
          garbage_collector_listeners_(allocator->Adapter()),
          exception_listeners_(allocator->Adapter()),
          vm_events_listeners_(allocator->Adapter()),
          method_listeners_(allocator->Adapter()),
          class_listeners_(allocator->Adapter()),
          monitor_listeners_(allocator->Adapter()),
          ddm_listeners_(allocator->Adapter())
    {
    }
    ~RuntimeNotificationManager() = default;
    NO_COPY_SEMANTIC(RuntimeNotificationManager);
    NO_MOVE_SEMANTIC(RuntimeNotificationManager);

    void AddListener(RuntimeListener *listener, uint32_t event_mask)
    {
        ScopedSuspendAllThreads ssat(rendezvous_);
        AddListenerIfMatches(listener, event_mask, &bytecode_pc_listeners_, Event::BYTECODE_PC_CHANGED,
                             &has_bytecode_pc_listeners_);

        AddListenerIfMatches(listener, event_mask, &load_module_listeners_, Event::LOAD_MODULE,
                             &has_load_module_listeners_);

        AddListenerIfMatches(listener, event_mask, &thread_events_listeners_, Event::THREAD_EVENTS,
                             &has_thread_events_listeners_);

        AddListenerIfMatches(listener, event_mask, &garbage_collector_listeners_, Event::GARBAGE_COLLECTOR_EVENTS,
                             &has_garbage_collector_listeners_);

        AddListenerIfMatches(listener, event_mask, &exception_listeners_, Event::EXCEPTION_EVENTS,
                             &has_exception_listeners_);

        AddListenerIfMatches(listener, event_mask, &vm_events_listeners_, Event::VM_EVENTS, &has_vm_events_listeners_);

        AddListenerIfMatches(listener, event_mask, &method_listeners_, Event::METHOD_EVENTS, &has_method_listeners_);

        AddListenerIfMatches(listener, event_mask, &class_listeners_, Event::CLASS_EVENTS, &has_class_listeners_);

        AddListenerIfMatches(listener, event_mask, &monitor_listeners_, Event::MONITOR_EVENTS, &has_monitor_listeners_);

        AddListenerIfMatches(listener, event_mask, &allocation_listeners_, Event::ALLOCATION_EVENTS,
                             &has_allocation_listeners_);
    }

    void RemoveListener(RuntimeListener *listener, uint32_t event_mask)
    {
        ScopedSuspendAllThreads ssat(rendezvous_);
        RemoveListenerIfMatches(listener, event_mask, &bytecode_pc_listeners_, Event::BYTECODE_PC_CHANGED,
                                &has_bytecode_pc_listeners_);

        RemoveListenerIfMatches(listener, event_mask, &load_module_listeners_, Event::LOAD_MODULE,
                                &has_load_module_listeners_);

        RemoveListenerIfMatches(listener, event_mask, &thread_events_listeners_, Event::THREAD_EVENTS,
                                &has_thread_events_listeners_);

        RemoveListenerIfMatches(listener, event_mask, &garbage_collector_listeners_, Event::GARBAGE_COLLECTOR_EVENTS,
                                &has_garbage_collector_listeners_);

        RemoveListenerIfMatches(listener, event_mask, &exception_listeners_, Event::EXCEPTION_EVENTS,
                                &has_exception_listeners_);

        RemoveListenerIfMatches(listener, event_mask, &vm_events_listeners_, Event::VM_EVENTS,
                                &has_vm_events_listeners_);

        RemoveListenerIfMatches(listener, event_mask, &method_listeners_, Event::METHOD_EVENTS, &has_method_listeners_);

        RemoveListenerIfMatches(listener, event_mask, &class_listeners_, Event::CLASS_EVENTS, &has_class_listeners_);

        RemoveListenerIfMatches(listener, event_mask, &monitor_listeners_, Event::MONITOR_EVENTS,
                                &has_monitor_listeners_);

        RemoveListenerIfMatches(listener, event_mask, &allocation_listeners_, Event::ALLOCATION_EVENTS,
                                &has_allocation_listeners_);
    }

    void LoadModuleEvent(std::string_view name)
    {
        if (UNLIKELY(has_load_module_listeners_)) {
            for (auto *listener : load_module_listeners_) {
                if (listener != nullptr) {
                    listener->LoadModule(name);
                }
            }
        }
    }

    void ThreadStartEvent(ManagedThread::ThreadId thread_id)
    {
        if (UNLIKELY(has_thread_events_listeners_)) {
            for (auto *listener : thread_events_listeners_) {
                if (listener != nullptr) {
                    listener->ThreadStart(thread_id);
                }
            }
        }
    }

    void ThreadEndEvent(ManagedThread::ThreadId thread_id)
    {
        if (UNLIKELY(has_thread_events_listeners_)) {
            for (auto *listener : thread_events_listeners_) {
                if (listener != nullptr) {
                    listener->ThreadEnd(thread_id);
                }
            }
        }
    }

    void BytecodePcChangedEvent(ManagedThread *thread, Method *method, uint32_t bc_offset)
    {
        if (UNLIKELY(has_bytecode_pc_listeners_)) {
            for (auto *listener : bytecode_pc_listeners_) {
                if (listener != nullptr) {
                    listener->BytecodePcChanged(thread, method, bc_offset);
                }
            }
        }
    }

    void GarbageCollectorStartEvent()
    {
        if (UNLIKELY(has_garbage_collector_listeners_)) {
            for (auto *listener : garbage_collector_listeners_) {
                if (listener != nullptr) {
                    listener->GarbageCollectorStart();
                }
            }
        }
    }

    void GarbageCollectorFinishEvent()
    {
        if (UNLIKELY(has_garbage_collector_listeners_)) {
            for (auto *listener : garbage_collector_listeners_) {
                if (listener != nullptr) {
                    listener->GarbageCollectorFinish();
                }
            }
        }
    }

    void ExceptionCatchEvent(ManagedThread *thread, Method *method, uint32_t bc_offset)
    {
        if (UNLIKELY(has_exception_listeners_)) {
            for (auto *listener : exception_listeners_) {
                if (listener != nullptr) {
                    listener->ExceptionCatch(thread, method, bc_offset);
                }
            }
        }
    }

    void VmStartEvent()
    {
        if (UNLIKELY(has_vm_events_listeners_)) {
            for (auto *listener : vm_events_listeners_) {
                if (listener != nullptr) {
                    listener->VmStart();
                }
            }
        }
    }

    void VmInitializationEvent(ManagedThread::ThreadId thread_id)
    {
        if (UNLIKELY(has_vm_events_listeners_)) {
            for (auto *listener : vm_events_listeners_) {
                if (listener != nullptr) {
                    listener->VmInitialization(thread_id);
                }
            }
        }
    }

    void VmDeathEvent()
    {
        if (UNLIKELY(has_vm_events_listeners_)) {
            for (auto *listener : vm_events_listeners_) {
                if (listener != nullptr) {
                    listener->VmDeath();
                }
            }
        }
    }

    void MethodEntryEvent(ManagedThread *thread, Method *method)
    {
        if (UNLIKELY(has_method_listeners_)) {
            for (auto *listener : method_listeners_) {
                if (listener != nullptr) {
                    listener->MethodEntry(thread, method);
                }
            }
        }
    }

    void MethodExitEvent(ManagedThread *thread, Method *method)
    {
        if (UNLIKELY(has_method_listeners_)) {
            for (auto *listener : method_listeners_) {
                if (listener != nullptr) {
                    listener->MethodExit(thread, method);
                }
            }
        }
    }

    void ClassLoadEvent(Class *klass)
    {
        if (UNLIKELY(has_class_listeners_)) {
            for (auto *listener : class_listeners_) {
                if (listener != nullptr) {
                    listener->ClassLoad(klass);
                }
            }
        }
    }

    void ClassPrepareEvent(Class *klass)
    {
        if (UNLIKELY(has_class_listeners_)) {
            for (auto *listener : class_listeners_) {
                if (listener != nullptr) {
                    listener->ClassPrepare(klass);
                }
            }
        }
    }

    void MonitorWaitEvent(ObjectHeader *object, int64_t timeout)
    {
        if (UNLIKELY(has_monitor_listeners_)) {
            // If we need to support multiple monitor listeners,
            // the object must be wrapped to ObjectHandle to protect from GC move
            ASSERT(monitor_listeners_.size() == 1);
            auto *listener = monitor_listeners_.front();
            if (listener != nullptr) {
                listener->MonitorWait(object, timeout);
            }
        }
    }

    void MonitorWaitedEvent(ObjectHeader *object, bool timed_out)
    {
        if (UNLIKELY(has_monitor_listeners_)) {
            // If we need to support multiple monitor listeners,
            // the object must be wrapped to ObjectHandle to protect from GC move
            ASSERT(monitor_listeners_.size() == 1);
            auto *listener = monitor_listeners_.front();
            if (listener != nullptr) {
                monitor_listeners_.front()->MonitorWaited(object, timed_out);
            }
        }
    }

    void MonitorContendedEnterEvent(ObjectHeader *object)
    {
        if (UNLIKELY(has_monitor_listeners_)) {
            // If we need to support multiple monitor listeners,
            // the object must be wrapped to ObjectHandle to protect from GC move
            ASSERT(monitor_listeners_.size() == 1);
            auto *listener = monitor_listeners_.front();
            if (listener != nullptr) {
                monitor_listeners_.front()->MonitorContendedEnter(object);
            }
        }
    }

    void MonitorContendedEnteredEvent(ObjectHeader *object)
    {
        if (UNLIKELY(has_monitor_listeners_)) {
            // If we need to support multiple monitor listeners,
            // the object must be wrapped to ObjectHandle to protect from GC move
            ASSERT(monitor_listeners_.size() == 1);
            auto *listener = monitor_listeners_.front();
            if (listener != nullptr) {
                monitor_listeners_.front()->MonitorContendedEntered(object);
            }
        }
    }

    bool HasAllocationListeners() const
    {
        return has_allocation_listeners_;
    }

    void ObjectAllocEvent(BaseClass *klass, ObjectHeader *object, ManagedThread *thread, size_t size) const
    {
        if (UNLIKELY(has_allocation_listeners_)) {
            // If we need to support multiple allocation listeners,
            // the object must be wrapped to ObjectHandle to protect from GC move
            ASSERT(allocation_listeners_.size() == 1);
            auto *listener = allocation_listeners_.front();
            if (listener != nullptr) {
                allocation_listeners_.front()->ObjectAlloc(klass, object, thread, size);
            }
        }
    }

    void DdmPublishChunk(uint32_t type, const Span<const uint8_t> &data)
    {
        os::memory::ReadLockHolder holder(ddm_lock_);
        for (auto *listener : ddm_listeners_) {
            listener->DdmPublishChunk(type, data);
        }
    }

    void StartDebugger()
    {
        os::memory::ReadLockHolder holder(debugger_lock_);
        for (auto *listener : debugger_listeners_) {
            listener->StartDebugger();
        }
    }

    void StopDebugger()
    {
        os::memory::ReadLockHolder holder(debugger_lock_);
        for (auto *listener : debugger_listeners_) {
            listener->StopDebugger();
        }
    }

    bool IsDebuggerConfigured()
    {
        os::memory::ReadLockHolder holder(debugger_lock_);
        for (auto *listener : debugger_listeners_) {
            if (!listener->IsDebuggerConfigured()) {
                return false;
            }
        }
        return true;
    }

    void AddDdmListener(DdmListener *listener)
    {
        os::memory::WriteLockHolder holder(ddm_lock_);
        ddm_listeners_.push_back(listener);
    }

    void SetRendezvous(Rendezvous *rendezvous)
    {
        rendezvous_ = rendezvous;
    }

    void RemoveDdmListener(DdmListener *listener)
    {
        os::memory::WriteLockHolder holder(ddm_lock_);
        RemoveListener(ddm_listeners_, listener);
    }

    void AddDebuggerListener(DebuggerListener *listener)
    {
        os::memory::WriteLockHolder holder(debugger_lock_);
        debugger_listeners_.push_back(listener);
    }

    void RemoveDebuggerListener(DebuggerListener *listener)
    {
        os::memory::WriteLockHolder holder(debugger_lock_);
        RemoveListener(debugger_listeners_, listener);
    }

private:
    static void AddListenerIfMatches(RuntimeListener *listener, uint32_t event_mask,
                                     PandaList<RuntimeListener *> *listener_group, Event event_modifier,
                                     bool *event_flag)
    {
        if ((event_mask & event_modifier) != 0) {
            // If a free group item presents, use it, otherwise push back a new item
            auto it = std::find(listener_group->begin(), listener_group->end(), nullptr);
            if (it != listener_group->end()) {
                *it = listener;
            } else {
                listener_group->push_back(listener);
            }
            *event_flag = true;
        }
    }

    template <typename Container, typename T>
    ALWAYS_INLINE void RemoveListener(Container &c, T &listener)
    {
        c.erase(std::remove_if(c.begin(), c.end(), [&listener](const T &elem) { return listener == elem; }));
    }

    static void RemoveListenerIfMatches(RuntimeListener *listener, uint32_t event_mask,
                                        PandaList<RuntimeListener *> *listener_group, Event event_modifier,
                                        bool *event_flag)
    {
        if ((event_mask & event_modifier) != 0) {
            auto it = std::find(listener_group->begin(), listener_group->end(), listener);
            if (it == listener_group->end()) {
                return;
            }
            // Removing a listener is not safe, because the iteration can not be completed in another thread.
            // We just set the item to null in the group
            *it = nullptr;

            // Check if any listener presents and updates the flag if not
            if (std::find_if(listener_group->begin(), listener_group->end(),
                             [](RuntimeListener *item) { return item != nullptr; }) == listener_group->end()) {
                *event_flag = false;
            }
        }
    }

    PandaList<RuntimeListener *> bytecode_pc_listeners_;
    PandaList<RuntimeListener *> load_module_listeners_;
    PandaList<RuntimeListener *> thread_events_listeners_;
    PandaList<RuntimeListener *> garbage_collector_listeners_;
    PandaList<RuntimeListener *> exception_listeners_;
    PandaList<RuntimeListener *> vm_events_listeners_;
    PandaList<RuntimeListener *> method_listeners_;
    PandaList<RuntimeListener *> class_listeners_;
    PandaList<RuntimeListener *> monitor_listeners_;
    PandaList<RuntimeListener *> allocation_listeners_;

    PandaList<DdmListener *> ddm_listeners_;
    os::memory::RWLock ddm_lock_;
    bool has_bytecode_pc_listeners_ = false;
    bool has_load_module_listeners_ = false;
    bool has_thread_events_listeners_ = false;
    bool has_garbage_collector_listeners_ = false;
    bool has_exception_listeners_ = false;
    bool has_vm_events_listeners_ = false;
    bool has_method_listeners_ = false;
    bool has_class_listeners_ = false;
    bool has_monitor_listeners_ = false;
    bool has_allocation_listeners_ = false;
    Rendezvous *rendezvous_ {nullptr};

    os::memory::RWLock debugger_lock_;
    PandaList<DebuggerListener *> debugger_listeners_;
};

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_RUNTIME_NOTIFICATION_H_
