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
#include "runtime/include/language_context.h"
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

// See issue 4100, js thread always true
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define ASSERT_MANAGED_CODE() ASSERT(::panda::MTManagedThread::GetCurrent()->IsManagedCode())
#define ASSERT_NATIVE_CODE() ASSERT(::panda::MTManagedThread::GetCurrent()->IsInNativeCode())  // NOLINT

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

enum ThreadFlag {
    NO_FLAGS = 0,
    GC_SAFEPOINT_REQUEST = 1,
    SUSPEND_REQUEST = 2,
    RUNTIME_TERMINATION_REQUEST = 4,
};

/**
 * \brief Class represents managed thread
 *
 * When the thread is created it registers itself in the runtime, so
 * runtime knows about all managed threads at any given time.
 *
 * This class should be used to store thread specitic information that
 * is necessary to execute managed code:
 *  - Frame
 *  - Exception
 *  - Interpreter cache
 *  - etc.
 *
 *  Now it's used by interpreter to store current frame only.
 */
class ManagedThread : public Thread {
public:
    using ThreadId = uint32_t;
    using native_handle_type = os::thread::native_handle_type;
    static constexpr ThreadId NON_INITIALIZED_THREAD_ID = 0;
    static constexpr ThreadId MAX_INTERNAL_THREAD_ID = MarkWord::LIGHT_LOCK_THREADID_MAX_COUNT;

    void SetLanguageContext(LanguageContext ctx)
    {
        ctx_ = ctx;
    }

    LanguageContext GetLanguageContext() const
    {
        return ctx_;
    }

    void SetCurrentFrame(Frame *f)
    {
        stor_ptr_.frame_ = f;
    }

    tooling::PtThreadInfo *GetPtThreadInfo() const
    {
        return pt_thread_info_.get();
    }

    Frame *GetCurrentFrame() const
    {
        return stor_ptr_.frame_;
    }

    void *GetFrame() const
    {
        void *fp = GetCurrentFrame();
        if (IsCurrentFrameCompiled()) {
            return StackWalker::IsBoundaryFrame<FrameKind::INTERPRETER>(fp)
                       ? StackWalker::GetPrevFromBoundary<FrameKind::COMPILER>(fp)
                       : fp;
        }
        return fp;
    }

    bool IsCurrentFrameCompiled() const
    {
        return stor_32_.is_compiled_frame_;
    }

    void SetCurrentFrameIsCompiled(bool value)
    {
        stor_32_.is_compiled_frame_ = value;
    }

    void SetException(ObjectHeader *exception)
    {
        stor_ptr_.exception_ = exception;
    }

    ObjectHeader *GetException() const
    {
        return stor_ptr_.exception_;
    }

    bool HasPendingException() const
    {
        return stor_ptr_.exception_ != nullptr;
    }

    void ClearException()
    {
        stor_ptr_.exception_ = nullptr;
    }

    static bool ThreadIsManagedThread(Thread *thread)
    {
        ASSERT(thread != nullptr);
        Thread::ThreadType thread_type = thread->GetThreadType();
        return thread_type == Thread::ThreadType::THREAD_TYPE_MANAGED ||
               thread_type == Thread::ThreadType::THREAD_TYPE_MT_MANAGED;
    }

    static ManagedThread *CastFromThread(Thread *thread)
    {
        ASSERT(thread != nullptr);
        ASSERT(ThreadIsManagedThread(thread));
        return static_cast<ManagedThread *>(thread);
    }

    /**
     * @brief GetCurrentRaw Unsafe method to get current ManagedThread.
     * It can be used in hotspots to get the best performance.
     * We can only use this method in places where the ManagedThread exists.
     * @return pointer to ManagedThread
     */
    static ManagedThread *GetCurrentRaw()
    {
        return CastFromThread(Thread::GetCurrent());
    }

    /**
     * @brief GetCurrent Safe method to gets current ManagedThread.
     * @return pointer to ManagedThread or nullptr (if current thread is not a managed thread)
     */
    static ManagedThread *GetCurrent()
    {
        Thread *thread = Thread::GetCurrent();
        ASSERT(thread != nullptr);
        if (ThreadIsManagedThread(thread)) {
            return CastFromThread(thread);
        }
        return nullptr;
    }

    static bool Initialize();

    static bool Shutdown();

    bool IsThreadAlive() const
    {
        return GetStatus() != FINISHED;
    }

    enum ThreadStatus GetStatus() const
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        uint32_t res_int = stor_32_.fts_.as_atomic.load(std::memory_order_acquire);
        return static_cast<enum ThreadStatus>(res_int >> THREAD_STATUS_OFFSET);
    }

    panda::mem::StackFrameAllocator *GetStackFrameAllocator() const
    {
        return stack_frame_allocator_;
    }

    panda::mem::InternalAllocator<>::LocalSmallObjectAllocator *GetLocalInternalAllocator() const
    {
        return internal_local_allocator_;
    }

    mem::TLAB *GetTLAB() const
    {
        ASSERT(stor_ptr_.tlab_ != nullptr);
        return stor_ptr_.tlab_;
    }

    void UpdateTLAB(mem::TLAB *tlab);

    void ClearTLAB();

    void SetStringClassPtr(void *p)
    {
        stor_ptr_.string_class_ptr_ = p;
    }

    static ManagedThread *Create(Runtime *runtime, PandaVM *vm);
    ~ManagedThread() override;

    explicit ManagedThread(ThreadId id, mem::InternalAllocatorPtr allocator, PandaVM *vm,
                           Thread::ThreadType thread_type);

    // Here methods which are just proxy or cache for runtime interface
    ALWAYS_INLINE mem::BarrierType GetPreBarrierType() const
    {
        return pre_barrier_type_;
    }

    ALWAYS_INLINE mem::BarrierType GetPostBarrierType() const
    {
        return post_barrier_type_;
    }

    // Methods to access thread local storage
    InterpreterCache *GetInterpreterCache()
    {
        return &interpreter_cache_;
    }

    uintptr_t GetNativePc() const
    {
        return stor_ptr_.native_pc_;
    }

    bool IsJavaThread() const
    {
        return is_java_thread_;
    }

    bool IsJSThread() const
    {
        return is_js_thread_;
    }

    LanguageContext GetLanguageContext();

    inline bool IsSuspended() const
    {
        return ReadFlag(SUSPEND_REQUEST);
    }

    inline bool IsRuntimeTerminated() const
    {
        return ReadFlag(RUNTIME_TERMINATION_REQUEST);
    }

    inline void SetRuntimeTerminated()
    {
        SetFlag(RUNTIME_TERMINATION_REQUEST);
    }

    static constexpr size_t GetPtrStorageOffset(Arch arch, size_t offset)
    {
        return MEMBER_OFFSET(ManagedThread, stor_ptr_) + StoragePackedPtr::ConvertOffset(PointerSize(arch), offset);
    }

    static constexpr uint32_t GetFlagOffset()
    {
        return MEMBER_OFFSET(ManagedThread, stor_32_) + MEMBER_OFFSET(StoragePacked32, fts_);
    }

    static constexpr uint32_t GetNativePcOffset(Arch arch)
    {
        return GetPtrStorageOffset(arch, MEMBER_OFFSET(StoragePackedPtr, native_pc_));
    }

    static constexpr uint32_t GetFrameKindOffset()
    {
        return MEMBER_OFFSET(ManagedThread, stor_32_) + MEMBER_OFFSET(StoragePacked32, is_compiled_frame_);
    }

    static constexpr uint32_t GetFrameOffset(Arch arch)
    {
        return GetPtrStorageOffset(arch, MEMBER_OFFSET(StoragePackedPtr, frame_));
    }

    static constexpr uint32_t GetExceptionOffset(Arch arch)
    {
        return GetPtrStorageOffset(arch, MEMBER_OFFSET(StoragePackedPtr, exception_));
    }

    static constexpr uint32_t GetTLABOffset(Arch arch)
    {
        return GetPtrStorageOffset(arch, MEMBER_OFFSET(StoragePackedPtr, tlab_));
    }

    static constexpr uint32_t GetObjectOffset(Arch arch)
    {
        return GetPtrStorageOffset(arch, MEMBER_OFFSET(StoragePackedPtr, object_));
    }

    static constexpr uint32_t GetTlsCardTableAddrOffset(Arch arch)
    {
        return GetPtrStorageOffset(arch, MEMBER_OFFSET(StoragePackedPtr, card_table_addr_));
    }

    static constexpr uint32_t GetTlsCardTableMinAddrOffset(Arch arch)
    {
        return GetPtrStorageOffset(arch, MEMBER_OFFSET(StoragePackedPtr, card_table_min_addr_));
    }

    static constexpr uint32_t GetTlsConcurrentMarkingAddrOffset(Arch arch)
    {
        return GetPtrStorageOffset(arch, MEMBER_OFFSET(StoragePackedPtr, concurrent_marking_addr_));
    }

    virtual void VisitGCRoots(const ObjectVisitor &cb);

    virtual void UpdateGCRoots();

    void PushLocalObject(ObjectHeader **object_header);

    void PopLocalObject();

    void SetThreadPriority(int32_t prio);

    uint32_t GetThreadPriority() const;

    inline bool IsGcRequired() const
    {
        return ReadFlag(GC_SAFEPOINT_REQUEST);
    }

    // NO_THREAD_SANITIZE for invalid TSAN data race report
    NO_THREAD_SANITIZE bool ReadFlag(ThreadFlag flag) const
    {
        return (stor_32_.fts_.as_struct.flags & flag) != 0;  // NOLINT(cppcoreguidelines-pro-type-union-access)
    }

    NO_THREAD_SANITIZE bool TestAllFlags() const
    {
        return (stor_32_.fts_.as_struct.flags) != NO_FLAGS;  // NOLINT(cppcoreguidelines-pro-type-union-access)
    }

    void SetFlag(ThreadFlag flag)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        stor_32_.fts_.as_atomic.fetch_or(flag, std::memory_order_seq_cst);
    }

    void ClearFlag(ThreadFlag flag)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        stor_32_.fts_.as_atomic.fetch_and(UINT32_MAX ^ flag, std::memory_order_seq_cst);
    }

    // Separate functions for NO_THREAD_SANITIZE to suppress TSAN data race report
    NO_THREAD_SANITIZE uint32_t ReadFlagsAndThreadStatusUnsafe() const
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        return stor_32_.fts_.as_int;
    }

    void StoreStatus(ThreadStatus status)
    {
        while (true) {
            union FlagsAndThreadStatus old_fts {
            };
            union FlagsAndThreadStatus new_fts {
            };
            old_fts.as_int = ReadFlagsAndThreadStatusUnsafe();  // NOLINT(cppcoreguidelines-pro-type-union-access)
            new_fts.as_struct.flags = old_fts.as_struct.flags;  // NOLINT(cppcoreguidelines-pro-type-union-access)
            new_fts.as_struct.status = status;                  // NOLINT(cppcoreguidelines-pro-type-union-access)
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
            if (stor_32_.fts_.as_atomic.compare_exchange_weak(old_fts.as_nonvolatile_int, new_fts.as_nonvolatile_int,
                                                              std::memory_order_release)) {
                // If CAS succeeded, we set new status and no request occured here, safe to proceed.
                break;
            }
        }
    }

    bool IsManagedCodeAllowed() const
    {
        return is_managed_code_allowed_;
    }

    void SetManagedCodeAllowed(bool allowed)
    {
        is_managed_code_allowed_ = allowed;
    }

    // TaggedType has been specialized for js, Other types are empty implementation
    template <typename T>
    inline HandleScope<T> *PopHandleScope()
    {
        return nullptr;
    }

    // TaggedType has been specialized for js, Other types are empty implementation
    template <typename T>
    inline void PushHandleScope([[maybe_unused]] HandleScope<T> *handle_scope)
    {
    }

    // TaggedType has been specialized for js, Other types are empty implementation
    template <typename T>
    inline HandleScope<T> *GetTopScope() const
    {
        return nullptr;
    }

    // TaggedType has been specialized for js, Other types are empty implementation
    template <typename T>
    inline HandleStorage<T> *GetHandleStorage() const
    {
        return nullptr;
    }

    // TaggedType has been specialized for js, Other types are empty implementation
    template <typename T>
    inline GlobalHandleStorage<T> *GetGlobalHandleStorage() const
    {
        return nullptr;
    }

    CustomTLSData *GetCustomTLSData(const char *key);
    void SetCustomTLSData(const char *key, CustomTLSData *data);

#if EVENT_METHOD_ENTER_ENABLED || EVENT_METHOD_EXIT_ENABLED
    uint32_t RecordMethodEnter()
    {
        return call_depth_++;
    }

    uint32_t RecordMethodExit()
    {
        return --call_depth_;
    }
#endif

    bool IsAttached() const
    {
        return is_attached_.load(std::memory_order_relaxed);
    }

    void SetAttached()
    {
        is_attached_.store(true, std::memory_order_relaxed);
    }

    void SetDetached()
    {
        is_attached_.store(false, std::memory_order_relaxed);
    }

    bool IsVMThread() const
    {
        return is_vm_thread_;
    }

    void SetVMThread()
    {
        is_vm_thread_ = true;
    }

    bool IsThrowingOOM() const
    {
        return throwing_oom_count_ > 0;
    }

    void SetThrowingOOM(bool is_throwing_oom)
    {
        if (is_throwing_oom) {
            throwing_oom_count_++;
            return;
        }
        ASSERT(throwing_oom_count_ > 0);
        throwing_oom_count_--;
    }

    bool IsUsePreAllocObj() const
    {
        return use_prealloc_obj_;
    }

    void SetUsePreAllocObj(bool use_prealloc_obj)
    {
        use_prealloc_obj_ = use_prealloc_obj;
    }

    void PrintSuspensionStackIfNeeded();

    ThreadId GetId() const
    {
        return id_.load(std::memory_order_relaxed);
    }

    virtual void FreeInternalMemory();

protected:
    static const int WAIT_INTERVAL = 10;

    void SetJavaThread()
    {
        is_java_thread_ = true;
    }

    void SetJSThread()
    {
        is_js_thread_ = true;
    }

    template <typename T = void>
    T *GetAssociatedObject() const
    {
        return reinterpret_cast<T *>(stor_ptr_.object_);
    }

    template <typename T>
    void SetAssociatedObject(T *object)
    {
        stor_ptr_.object_ = object;
    }

    virtual void InterruptPostImpl() {}

    void UpdateId(ThreadId id)
    {
        id_.store(id, std::memory_order_relaxed);
    }

private:
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    static constexpr uint32_t THREAD_STATUS_OFFSET = 16;
    static_assert(sizeof(stor_32_.fts_) == sizeof(uint32_t), "Wrong fts_ size");

    // Can cause data races if child thread's UpdateId is executed concurrently with GetNativeThreadId
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    std::atomic<ThreadId> id_;

    static mem::TLAB *zero_tlab;
    static bool is_initialized;
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    PandaVector<ObjectHeader **> local_objects_;

    // Something like custom TLS - it is faster to access via ManagedThread than via thread_local
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    InterpreterCache interpreter_cache_;

    PandaMap<const char *, PandaUniquePtr<CustomTLSData>> custom_tls_cache_ GUARDED_BY(Locks::custom_tls_lock);

    // Keep these here to speed up interpreter
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    mem::BarrierType pre_barrier_type_ {mem::BarrierType::PRE_WRB_NONE};
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    mem::BarrierType post_barrier_type_ {mem::BarrierType::POST_WRB_NONE};
    // Thread local storages to avoid locks in heap manager
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    mem::StackFrameAllocator *stack_frame_allocator_;
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    mem::InternalAllocator<>::LocalSmallObjectAllocator *internal_local_allocator_;
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    bool is_java_thread_ = false;
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    std::atomic_bool is_attached_ {false};  // Can be changed after thread is registered and can cause data race
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    bool is_vm_thread_ = false;

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    bool is_js_thread_ = false;

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    bool is_managed_code_allowed_ {true};

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    size_t throwing_oom_count_ {0};
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    bool use_prealloc_obj_ {false};

    // remove ctx in thread later
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    LanguageContext ctx_;

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    PandaUniquePtr<tooling::PtThreadInfo> pt_thread_info_;

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    PandaVector<HandleScope<coretypes::TaggedType> *> tagged_handle_scopes_ {};
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    HandleStorage<coretypes::TaggedType> *tagged_handle_storage_ {nullptr};
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    GlobalHandleStorage<coretypes::TaggedType> *tagged_global_handle_storage_ {nullptr};

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    PandaVector<HandleScope<ObjectHeader *> *> object_header_handle_scopes_ {};
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    HandleStorage<ObjectHeader *> *object_header_handle_storage_ {nullptr};

    friend class panda::test::ThreadTest;
    friend class openjdkjvmti::TiThread;
    friend class openjdkjvmti::ScopedNoUserCodeSuspension;
    friend class Offsets_Thread_Test;
    friend class panda::ThreadManager;

    // Used in method events
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    uint32_t call_depth_ {0};

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    NO_COPY_SEMANTIC(ManagedThread);
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    NO_MOVE_SEMANTIC(ManagedThread);
};

class MTManagedThread : public ManagedThread {
public:
    enum ThreadState : uint8_t { NATIVE_CODE = 0, MANAGED_CODE = 1 };

    ThreadId GetInternalId();

    static MTManagedThread *Create(Runtime *runtime, PandaVM *vm);

    explicit MTManagedThread(ThreadId id, mem::InternalAllocatorPtr allocator, PandaVM *vm);
    ~MTManagedThread() override;

    std::unordered_set<Monitor *> &GetMonitors();
    void AddMonitor(Monitor *monitor);
    void RemoveMonitor(Monitor *monitor);
    void ReleaseMonitors();

    void PushLocalObjectLocked(ObjectHeader *obj);
    void PopLocalObjectLocked(ObjectHeader *out);
    const PandaVector<LockedObjectInfo> &GetLockedObjectInfos();

    void VisitGCRoots(const ObjectVisitor &cb) override;
    void UpdateGCRoots() override;

    ThreadStatus GetWaitingMonitorOldStatus() const
    {
        return monitor_old_status_;
    }

    void SetWaitingMonitorOldStatus(ThreadStatus status)
    {
        monitor_old_status_ = status;
    }

    static bool IsManagedScope()
    {
        auto thread = GetCurrent();
        return thread != nullptr && thread->is_managed_scope_;
    }

    void FreeInternalMemory() override;

    static bool Sleep(uint64_t ms);

    void SuspendImpl(bool internal_suspend = false);
    void ResumeImpl(bool internal_resume = false);

    Monitor *GetWaitingMonitor() const
    {
        return waiting_monitor_;
    }

    void SetWaitingMonitor(Monitor *monitor)
    {
        ASSERT(waiting_monitor_ == nullptr || monitor == nullptr);
        waiting_monitor_ = monitor;
    }

    virtual void StopDaemonThread();

    bool IsDaemon()
    {
        return is_daemon_;
    }

    void SetDaemon();

    virtual void Destroy();

    static void Yield();

    static void Interrupt(MTManagedThread *thread);

    [[nodiscard]] bool HasManagedCodeOnStack() const;
    [[nodiscard]] bool HasClearStack() const;

    /**
     * Transition to suspended and back to runnable, re-acquire share on mutator_lock_
     */
    void SuspendCheck();

    bool IsUserSuspended()
    {
        return user_code_suspend_count_ > 0;
    }

    // Need to acquire the mutex before waiting to avoid scheduling between monitor release and clond_lock acquire
    os::memory::Mutex *GetWaitingMutex() RETURN_CAPABILITY(cond_lock_)
    {
        return &cond_lock_;
    }

    void Signal()
    {
        os::memory::LockHolder lock(cond_lock_);
        cond_var_.Signal();
    }

    bool Interrupted();

    bool IsInterrupted() const
    {
        os::memory::LockHolder lock(cond_lock_);
        return is_interrupted_;
    }

    bool IsInterruptedWithLockHeld() const REQUIRES(cond_lock_)
    {
        return is_interrupted_;
    }

    void ClearInterrupted()
    {
        os::memory::LockHolder lock(cond_lock_);
        is_interrupted_ = false;
    }

    void IncSuspended(bool is_internal) REQUIRES(suspend_lock_)
    {
        if (!is_internal) {
            user_code_suspend_count_++;
        }
        // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
        auto old_count = suspend_count_++;
        if (old_count == 0) {
            SetFlag(SUSPEND_REQUEST);
        }
    }

    void DecSuspended(bool is_internal) REQUIRES(suspend_lock_)
    {
        if (!is_internal) {
            ASSERT(user_code_suspend_count_ != 0);
            user_code_suspend_count_--;
        }
        if (suspend_count_ > 0) {
            suspend_count_--;
            if (suspend_count_ == 0) {
                ClearFlag(SUSPEND_REQUEST);
            }
        }
    }

    static bool ThreadIsMTManagedThread(Thread *thread)
    {
        ASSERT(thread != nullptr);
        return thread->GetThreadType() == Thread::ThreadType::THREAD_TYPE_MT_MANAGED;
    }

    static MTManagedThread *CastFromThread(Thread *thread)
    {
        ASSERT(thread != nullptr);
        ASSERT(ThreadIsMTManagedThread(thread));
        return static_cast<MTManagedThread *>(thread);
    }

    /**
     * @brief GetCurrentRaw Unsafe method to get current MTManagedThread.
     * It can be used in hotspots to get the best performance.
     * We can only use this method in places where the MTManagedThread exists.
     * @return pointer to MTManagedThread
     */
    static MTManagedThread *GetCurrentRaw()
    {
        return CastFromThread(Thread::GetCurrent());
    }

    /**
     * @brief GetCurrent Safe method to gets current MTManagedThread.
     * @return pointer to MTManagedThread or nullptr (if current thread is not a managed thread)
     */
    static MTManagedThread *GetCurrent()
    {
        Thread *thread = Thread::GetCurrent();
        ASSERT(thread != nullptr);
        if (ThreadIsMTManagedThread(thread)) {
            return CastFromThread(thread);
        }
        // no guarantee that we will return nullptr here in the future
        return nullptr;
    }

    void SafepointPoll();

    /**
     * From NativeCode you can call ManagedCodeBegin.
     * From ManagedCode you can call NativeCodeBegin.
     * Call the same type is forbidden.
     */
    virtual void NativeCodeBegin();
    virtual void NativeCodeEnd();
    [[nodiscard]] virtual bool IsInNativeCode() const;

    virtual void ManagedCodeBegin();
    virtual void ManagedCodeEnd();
    [[nodiscard]] virtual bool IsManagedCode() const;

    void WaitWithLockHeld(ThreadStatus wait_status) REQUIRES(cond_lock_)
    {
        ASSERT(wait_status == IS_WAITING);
        // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
        auto old_status = GetStatus();
        // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
        UpdateStatus(wait_status);
        WaitWithLockHeldInternal();
        // Unlock before setting status RUNNING to handle MutatorReadLock without inversed lock order.
        cond_lock_.Unlock();
        UpdateStatus(old_status);
        cond_lock_.Lock();
    }

    static void WaitForSuspension(ManagedThread *thread)
    {
        static constexpr uint32_t YIELD_ITERS = 500;
        uint32_t loop_iter = 0;
        while (thread->GetStatus() == RUNNING) {
            if (!thread->IsSuspended()) {
                LOG(WARNING, RUNTIME) << "No request for suspension, do not wait thread " << thread->GetId();
                break;
            }

            loop_iter++;
            if (loop_iter < YIELD_ITERS) {
                MTManagedThread::Yield();
            } else {
                // Use native sleep over ManagedThread::Sleep to prevent potentially time consuming
                // mutator_lock locking and unlocking
                static constexpr uint32_t SHORT_SLEEP_MS = 1;
                os::thread::NativeSleep(SHORT_SLEEP_MS);
            }
        }
    }

    void Wait(ThreadStatus wait_status)
    {
        ASSERT(wait_status == IS_WAITING);
        auto old_status = GetStatus();
        {
            os::memory::LockHolder lock(cond_lock_);
            UpdateStatus(wait_status);
            WaitWithLockHeldInternal();
        }
        UpdateStatus(old_status);
    }

    bool TimedWaitWithLockHeld(ThreadStatus wait_status, uint64_t timeout, uint64_t nanos, bool is_absolute = false)
        REQUIRES(cond_lock_)
    {
        ASSERT(wait_status == IS_TIMED_WAITING || wait_status == IS_SLEEPING || wait_status == IS_BLOCKED ||
               wait_status == IS_SUSPENDED || wait_status == IS_COMPILER_WAITING ||
               wait_status == IS_WAITING_INFLATION);
        // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
        auto old_status = GetStatus();
        // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
        UpdateStatus(wait_status);
        // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
        bool res = TimedWaitWithLockHeldInternal(timeout, nanos, is_absolute);
        // Unlock before setting status RUNNING to handle MutatorReadLock without inversed lock order.
        cond_lock_.Unlock();
        UpdateStatus(old_status);
        cond_lock_.Lock();
        return res;
    }

    bool TimedWait(ThreadStatus wait_status, uint64_t timeout, uint64_t nanos = 0, bool is_absolute = false)
    {
        ASSERT(wait_status == IS_TIMED_WAITING || wait_status == IS_SLEEPING || wait_status == IS_BLOCKED ||
               wait_status == IS_SUSPENDED || wait_status == IS_COMPILER_WAITING ||
               wait_status == IS_WAITING_INFLATION);
        auto old_status = GetStatus();
        bool res = false;
        {
            os::memory::LockHolder lock(cond_lock_);
            UpdateStatus(wait_status);
            res = TimedWaitWithLockHeldInternal(timeout, nanos, is_absolute);
        }
        UpdateStatus(old_status);
        return res;
    }

    void WaitSuspension()
    {
        constexpr int TIMEOUT = 100;
        auto old_status = GetStatus();
        UpdateStatus(IS_SUSPENDED);
        {
            PrintSuspensionStackIfNeeded();
            os::memory::LockHolder lock(suspend_lock_);
            while (suspend_count_ > 0) {
                suspend_var_.TimedWait(&suspend_lock_, TIMEOUT);
                // In case runtime is being terminated, we should abort suspension and release monitors
                if (UNLIKELY(IsRuntimeTerminated())) {
                    suspend_lock_.Unlock();
                    TerminationLoop();
                }
            }
            ASSERT(!IsSuspended());
        }
        UpdateStatus(old_status);
    }

    void TerminationLoop()
    {
        ASSERT(IsRuntimeTerminated());
        // Free all monitors first in case we are suspending in status IS_BLOCKED
        ReleaseMonitors();
        UpdateStatus(IS_TERMINATED_LOOP);
        while (true) {
            static constexpr unsigned int LONG_SLEEP_MS = 1000000;
            os::thread::NativeSleep(LONG_SLEEP_MS);
        }
    }

    // NO_THREAD_SAFETY_ANALYSIS due to TSAN not being able to determine lock status
    void TransitionFromRunningToSuspended(enum ThreadStatus status) NO_THREAD_SAFETY_ANALYSIS
    {
        // Workaround: We masked the assert for 'ManagedThread::GetCurrent() == null' condition,
        //             because JSThread updates status_ not from current thread.
        //             (Remove it when issue 5183 is resolved)
        ASSERT(ManagedThread::GetCurrent() == this || ManagedThread::GetCurrent() == nullptr);

        Locks::mutator_lock->Unlock();
        StoreStatus(status);
    }

    // NO_THREAD_SAFETY_ANALYSIS due to TSAN not being able to determine lock status
    void TransitionFromSuspendedToRunning(enum ThreadStatus status) NO_THREAD_SAFETY_ANALYSIS
    {
        // Workaround: We masked the assert for 'ManagedThread::GetCurrent() == null' condition,
        //             because JSThread updates status_ not from current thread.
        //             (Remove it when issue 5183 is resolved)
        ASSERT(ManagedThread::GetCurrent() == this || ManagedThread::GetCurrent() == nullptr);

        // NB! This thread is treated as suspended so when we transition from suspended state to
        // running we need to check suspension flag and counter so SafepointPoll has to be done before
        // acquiring mutator_lock.
        StoreStatusWithSafepoint(status);
        Locks::mutator_lock->ReadLock();
    }

    void UpdateStatus(enum ThreadStatus status)
    {
        // Workaround: We masked the assert for 'ManagedThread::GetCurrent() == null' condition,
        //             because JSThread updates status_ not from current thread.
        //             (Remove it when issue 5183 is resolved)
        ASSERT(ManagedThread::GetCurrent() == this || ManagedThread::GetCurrent() == nullptr);

        ThreadStatus old_status = GetStatus();
        if (old_status == RUNNING && status != RUNNING) {
            TransitionFromRunningToSuspended(status);
        } else if (old_status != RUNNING && status == RUNNING) {
            TransitionFromSuspendedToRunning(status);
        } else if (status == TERMINATING) {
            // Using Store with safepoint to be sure that main thread didn't suspend us while trying to update status
            StoreStatusWithSafepoint(status);
        } else {
            // NB! Status is not a simple bit, without atomics it can produce faulty GetStatus.
            StoreStatus(status);
        }
    }

    MTManagedThread *GetNextWait() const
    {
        return next_;
    }

    void SetWaitNext(MTManagedThread *next)
    {
        next_ = next;
    }

    mem::ReferenceStorage *GetPtReferenceStorage() const
    {
        return pt_reference_storage_.get();
    }

protected:
    virtual void ProcessCreatedThread();

    virtual void StopDaemon0();

    void StopSuspension() REQUIRES(suspend_lock_)
    {
        // Lock before this call.
        suspend_var_.Signal();
    }

    os::memory::Mutex *GetSuspendMutex() RETURN_CAPABILITY(suspend_lock_)
    {
        return &suspend_lock_;
    }

    void WaitInternal()
    {
        os::memory::LockHolder lock(cond_lock_);
        WaitWithLockHeldInternal();
    }

    void WaitWithLockHeldInternal() REQUIRES(cond_lock_)
    {
        ASSERT(this == ManagedThread::GetCurrent());
        cond_var_.Wait(&cond_lock_);
    }

    bool TimedWaitInternal(uint64_t timeout, uint64_t nanos, bool is_absolute = false)
    {
        os::memory::LockHolder lock(cond_lock_);
        return TimedWaitWithLockHeldInternal(timeout, nanos, is_absolute);
    }

    bool TimedWaitWithLockHeldInternal(uint64_t timeout, uint64_t nanos, bool is_absolute = false) REQUIRES(cond_lock_)
    {
        ASSERT(this == ManagedThread::GetCurrent());
        return cond_var_.TimedWait(&cond_lock_, timeout, nanos, is_absolute);
    }

    void SignalWithLockHeld() REQUIRES(cond_lock_)
    {
        cond_var_.Signal();
    }

    void SetInterruptedWithLockHeld(bool interrupted) REQUIRES(cond_lock_)
    {
        is_interrupted_ = interrupted;
    }

private:
    PandaString LogThreadStack(ThreadState new_state) const;

    void StoreStatusWithSafepoint(ThreadStatus status)
    {
        while (true) {
            SafepointPoll();
            union FlagsAndThreadStatus old_fts {
            };
            union FlagsAndThreadStatus new_fts {
            };
            old_fts.as_int = ReadFlagsAndThreadStatusUnsafe();      // NOLINT(cppcoreguidelines-pro-type-union-access)
            new_fts.as_struct.flags = old_fts.as_struct.flags;      // NOLINT(cppcoreguidelines-pro-type-union-access)
            new_fts.as_struct.status = status;                      // NOLINT(cppcoreguidelines-pro-type-union-access)
            bool no_flags = (old_fts.as_struct.flags == NO_FLAGS);  // NOLINT(cppcoreguidelines-pro-type-union-access)

            // clang-format conflicts with CodeCheckAgent, so disable it here
            // clang-format off
            if (no_flags && stor_32_.fts_.as_atomic.compare_exchange_weak(
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
                old_fts.as_nonvolatile_int, new_fts.as_nonvolatile_int, std::memory_order_release)) {
                // If CAS succeeded, we set new status and no request occured here, safe to proceed.
                break;
            }
            // clang-format on
        }
    }

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    MTManagedThread *next_ {nullptr};

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    ThreadId internal_id_ {0};

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    PandaStack<ThreadState> thread_frame_states_;

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    PandaVector<LockedObjectInfo> local_objects_locked_;

    // Implementation of Wait/Notify
    os::memory::ConditionVariable cond_var_ GUARDED_BY(cond_lock_);
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    mutable os::memory::Mutex cond_lock_;

    bool is_interrupted_ GUARDED_BY(cond_lock_) = false;

    os::memory::ConditionVariable suspend_var_ GUARDED_BY(suspend_lock_);
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    os::memory::Mutex suspend_lock_;
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    uint32_t suspend_count_ GUARDED_BY(suspend_lock_) = 0;
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    std::atomic_uint32_t user_code_suspend_count_ {0};

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    bool is_daemon_ = false;

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    Monitor *waiting_monitor_;

    // Monitor lock is required for multithreaded AddMonitor; RecursiveMutex to allow calling RemoveMonitor
    // in ReleaseMonitors
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    os::memory::RecursiveMutex monitor_lock_;
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    std::unordered_set<Monitor *> entered_monitors_ GUARDED_BY(monitor_lock_);
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    ThreadStatus monitor_old_status_ = FINISHED;

    // Boolean which is safe to access after runtime is destroyed
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    bool is_managed_scope_ {false};

    PandaUniquePtr<mem::ReferenceStorage> pt_reference_storage_ {nullptr};

    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    NO_COPY_SEMANTIC(MTManagedThread);
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_GLOBAL_VAR_AS_INTERFACE)
    NO_MOVE_SEMANTIC(MTManagedThread);
};

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_THREAD_H_
