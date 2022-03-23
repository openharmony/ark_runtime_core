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

#ifndef PANDA_RUNTIME_INCLUDE_THREAD_H_
#define PANDA_RUNTIME_INCLUDE_THREAD_H_

#include <memory>
#include <chrono>
#include <limits>
#include <thread>
#include <atomic>

#include "libpandabase/mem/gc_barrier.h"
#include "libpandabase/os/mutex.h"
#include "libpandabase/os/thread.h"
#include "libpandabase/utils/aligned_storage.h"
#include "libpandabase/utils/arch.h"
#include "libpandabase/utils/list.h"
#include "runtime/include/mem/panda_containers.h"
#include "runtime/include/mem/panda_smart_pointers.h"
#include "runtime/include/object_header-inl.h"
#include "runtime/include/stack_walker.h"
#include "runtime/include/language_context.h"
#include "runtime/include/locks.h"
#include "runtime/include/thread_status.h"
#include "runtime/interpreter/cache.h"
#include "runtime/interpreter/frame.h"
#include "runtime/mem/frame_allocator-inl.h"
#include "runtime/mem/gc/gc.h"
#include "runtime/mem/internal_allocator.h"
#include "runtime/mem/tlab.h"
#include "runtime/mem/refstorage/reference_storage.h"
#include "runtime/entrypoints/entrypoints.h"
#include "events/events.h"

#define ASSERT_HAVE_ACCESS_TO_MANAGED_OBJECTS()

namespace openjdkjvmti {
class TiThread;
class ScopedNoUserCodeSuspension;
}  // namespace openjdkjvmti

namespace panda {

template <class TYPE>
class HandleStorage;
template <class TYPE>
class GlobalHandleStorage;
template <class TYPE>
class HandleScope;

namespace test {
class ThreadTest;
}  // namespace test

class ThreadManager;
class Runtime;
class PandaVM;

namespace mem {
class GCBarrierSet;
}  // namespace mem

namespace tooling {
class PtThreadInfo;
}  // namespace tooling

struct CustomTLSData {
    CustomTLSData() = default;
    virtual ~CustomTLSData() = default;

    NO_COPY_SEMANTIC(CustomTLSData);
    NO_MOVE_SEMANTIC(CustomTLSData);
};

class LockedObjectInfo {
public:
    LockedObjectInfo(ObjectHeader *obj, void *fp) : object(obj), stack(fp) {}
    ~LockedObjectInfo() = default;
    DEFAULT_COPY_SEMANTIC(LockedObjectInfo);
    DEFAULT_MOVE_SEMANTIC(LockedObjectInfo);
    inline ObjectHeader *GetObject() const
    {
        return object;
    }

    inline void SetObject(ObjectHeader *obj_new)
    {
        object = obj_new;
    }

    inline void *GetStack() const
    {
        return stack;
    }

    inline void SetStack(void *stack_new)
    {
        stack = stack_new;
    }

private:
    ObjectHeader *object;
    void *stack;
};

/**
 *  Hierarchy of thread classes
 *
 *         +--------+
 *         | Thread |
 *         +--------+
 *             |
 *      +---------------+
 *      | ManagedThread |
 *      +---------------+
 *             |
 *     +-----------------+
 *     | MTManagedThread |
 *     +-----------------+
 *
 *
 *  Thread - is the most low-level entity. This class contains pointers to VM with which this thread is associated.
 *  ManagedThread - stores runtime context to run managed code in single-threaded environment
 *  MTManagedThread - extends ManagedThread to be able to run code in multi-threaded environment
 */

/**
 * \brief Class represents arbitrary runtime thread
 */
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
class Thread {
public:
    enum class ThreadType {
        THREAD_TYPE_NONE,
        THREAD_TYPE_GC,
        THREAD_TYPE_COMPILER,
        THREAD_TYPE_MANAGED,
        THREAD_TYPE_MT_MANAGED,
    };

    explicit Thread(PandaVM *vm, ThreadType thread_type) : vm_(vm), thread_type_(thread_type) {}
    virtual ~Thread() = default;
    NO_COPY_SEMANTIC(Thread);
    NO_MOVE_SEMANTIC(Thread);

    static Thread *GetCurrent();
    static void SetCurrent(Thread *thread);

    PandaVM *GetVM() const
    {
        return vm_;
    }

    void SetVM(PandaVM *vm)
    {
        vm_ = vm;
    }

    ThreadType GetThreadType() const
    {
        return thread_type_;
    }

protected:
    union __attribute__((__aligned__(4))) FlagsAndThreadStatus {
        FlagsAndThreadStatus() = default;
        ~FlagsAndThreadStatus() = default;
        struct __attribute__((packed)) {
            volatile uint16_t flags;
            volatile enum ThreadStatus status;
        } as_struct;
        volatile uint32_t as_int;
        uint32_t as_nonvolatile_int;
        std::atomic_uint32_t as_atomic;

        NO_COPY_SEMANTIC(FlagsAndThreadStatus);
        NO_MOVE_SEMANTIC(FlagsAndThreadStatus);
    };

    static constexpr size_t STORAGE_32_NUM = 2;
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    struct StoragePacked32 : public AlignedStorage<sizeof(uint64_t), sizeof(uint32_t), STORAGE_32_NUM> {
        Aligned<bool> is_compiled_frame_ {false};
        Aligned<union FlagsAndThreadStatus> fts_ {};
    } stor_32_;  // NOLINT(misc-non-private-member-variables-in-classes)
    static_assert(sizeof(stor_32_) == StoragePacked32::GetSize());

    static constexpr size_t STORAGE_PTR_NUM = 9;

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    struct StoragePackedPtr : public AlignedStorage<sizeof(uintptr_t), sizeof(uintptr_t), STORAGE_PTR_NUM> {
        Aligned<void *> object_ {nullptr};
        Aligned<Frame *> frame_ {nullptr};
        Aligned<ObjectHeader *> exception_ {nullptr};
        Aligned<uintptr_t> native_pc_ {};
        Aligned<mem::TLAB *> tlab_ {nullptr};
        Aligned<void *> card_table_addr_ {nullptr};
        Aligned<void *> card_table_min_addr_ {nullptr};
        Aligned<void *> concurrent_marking_addr_ {nullptr};
        Aligned<void *> string_class_ptr_ {nullptr};
    } stor_ptr_;  // NOLINT(misc-non-private-member-variables-in-classes)
    static_assert(sizeof(stor_ptr_) == StoragePackedPtr::GetSize());

private:
    PandaVM *vm_ {nullptr};
    ThreadType thread_type_ {ThreadType::THREAD_TYPE_NONE};
};

template <typename ThreadT>
class ScopedCurrentThread {
public:
    explicit ScopedCurrentThread(ThreadT *thread) : thread_(thread)
    {
        ASSERT(Thread::GetCurrent() == nullptr);

        // Set current thread
        Thread::SetCurrent(thread_);
    }

    ~ScopedCurrentThread()
    {
        // Reset current thread
        Thread::SetCurrent(nullptr);
    }

    NO_COPY_SEMANTIC(ScopedCurrentThread);
    NO_MOVE_SEMANTIC(ScopedCurrentThread);

private:
    ThreadT *thread_;
};

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_THREAD_H_
