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

#ifndef PANDA_RUNTIME_INCLUDE_MANAGED_THREAD_H_
#define PANDA_RUNTIME_INCLUDE_MANAGED_THREAD_H_

#include "thread.h"

namespace panda {
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
                // If CAS succeeded, we set new status and no request occurred here, safe to proceed.
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
}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_MANAGED_THREAD_H_