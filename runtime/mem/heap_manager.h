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

#ifndef PANDA_RUNTIME_MEM_HEAP_MANAGER_H_
#define PANDA_RUNTIME_MEM_HEAP_MANAGER_H_

#include <cstddef>
#include <memory>

#include "libpandabase/utils/logger.h"
#include "runtime/include/class.h"
#include "runtime/include/mem/allocator.h"
#include "runtime/include/mem/panda_containers.h"
#include "runtime/include/object_header.h"
#include "runtime/include/thread.h"
#include "runtime/mem/frame_allocator-inl.h"
#include "runtime/mem/heap_verifier.h"
#include "runtime/mem/tlab.h"
#include "runtime/mem/gc/crossing_map_singleton.h"

namespace panda {
// Forward declaration
class Runtime;
class PandaVM;
class RuntimeNotificationManager;
}  //  namespace panda

namespace panda::mem {

class HeapManager {
public:
    bool Initialize(GCType gc_type, bool single_threaded, bool use_tlab, MemStatsType *mem_stats,
                    InternalAllocatorPtr internal_allocator, bool create_pygote_space);

    bool Finalize();

    [[nodiscard]] ObjectHeader *AllocateObject(BaseClass *cls, size_t size, Alignment align = DEFAULT_ALIGNMENT,
                                               MTManagedThread *thread = nullptr);

    template <bool IsFirstClassClass = false>
    [[nodiscard]] ObjectHeader *AllocateNonMovableObject(BaseClass *cls, size_t size,
                                                         Alignment align = DEFAULT_ALIGNMENT,
                                                         ManagedThread *thread = nullptr);

    /**
     * \brief Allocates memory for Frame, but do not construct it
     * @param size - size in bytes
     * @return pointer to allocated memory
     */
    [[nodiscard]] Frame *AllocateFrame(size_t size);

    /**
     * \brief Frees memory occupied by Frame
     * @param frame_ptr - pointer to Frame
     */
    void FreeFrame(Frame *frame_ptr);

    CodeAllocator *GetCodeAllocator() const;

    InternalAllocatorPtr GetInternalAllocator();

    ObjectAllocatorPtr GetObjectAllocator();

    bool UseTLABForAllocations() const
    {
        return use_tlab_for_allocations_;
    }

    bool CreateNewTLAB(ManagedThread *thread);

    size_t GetTLABMaxAllocSize()
    {
        return objectAllocator_.AsObjectAllocator()->GetTLABMaxAllocSize();
    }

    /**
     * Register TLAB information in MemStats during changing TLAB in a thread
     * or during thread destroying.
     */
    void RegisterTLAB(TLAB *tlab);

    /**
     * Prepare the heap before the fork process, The main function is to compact zygote space for fork subprocess
     *
     * @param
     * @return void
     */
    void PreZygoteFork();

    /**
     *  To implement the getTargetHeapUtilization and nativeSetTargetHeapUtilization,
     *  I set two functions and a fixed initial value here. They may need to be rewritten
     */
    float GetTargetHeapUtilization() const;

    void SetTargetHeapUtilization(float target);

    void DumpHeap(PandaOStringStream *o_string_stream);

    size_t VerifyHeapReferences()
    {
        trace::ScopedTrace scoped_trace(__FUNCTION__);
        size_t fail_count = 0;
        HeapObjectVerifier verifier(this, &fail_count);
        objectAllocator_->IterateOverObjects(verifier);
        return verifier.GetFailCount();
    }

    // Implements java.lang.Runtime.maxMemory.
    // Returns the maximum amount of memory a program can consume.
    size_t GetMaxMemory() const
    {
        return MemConfig::GetObjectPoolSize();
    }

    // Implements java.lang.Runtime.totalMemory.
    // Returns approximate amount of memory currently consumed by an application.
    size_t GetTotalMemory() const;

    // Implements java.lang.Runtime.freeMemory.
    // Returns how much free memory we have until we need to grow the heap to perform an allocation.
    size_t GetFreeMemory() const;

    // added for VMDebug::countInstancesOfClass and countInstancesOfClasses
    void CountInstances(const PandaVector<Class *> &classes, bool assignable, uint64_t *counts);

    using IsObjectFinalizebleFunc = bool (*)(BaseClass *);
    using RegisterFinalizeReferenceFunc = void (*)(ObjectHeader *, BaseClass *);
    void SetIsFinalizableFunc(IsObjectFinalizebleFunc func);
    void SetRegisterFinalizeReferenceFunc(RegisterFinalizeReferenceFunc func);

    bool IsObjectFinalized(BaseClass *cls);
    void RegisterFinalizedObject(ObjectHeader *object, BaseClass *cls, bool is_object_finalizable);

    void SetPandaVM(PandaVM *vm);

    PandaVM *GetPandaVM() const
    {
        return vm_;
    }

    mem::GC *GetGC() const
    {
        return gc_;
    }

    RuntimeNotificationManager *GetNotificationManager() const
    {
        return notification_manager_;
    }

    MemStatsType *GetMemStats() const
    {
        return mem_stats_;
    }

    HeapManager() : target_utilization_(DEFAULT_TARGET_UTILIZATION) {}

    ~HeapManager() = default;

    NO_COPY_SEMANTIC(HeapManager);
    NO_MOVE_SEMANTIC(HeapManager);

private:
    template <GCType gc_type, MTModeT MTMode = MT_MODE_MULTI>
    bool Initialize(MemStatsType *mem_stats, bool create_pygote_space)
    {
        ASSERT(!isInitialized_);
        isInitialized_ = true;

        codeAllocator_ = new (std::nothrow) CodeAllocator(mem_stats);
        if (!CrossingMapSingleton::IsCreated()) {
            CrossingMapSingleton::Create();
        }
        objectAllocator_ = new (std::nothrow)
            typename AllocConfig<gc_type, MTMode>::ObjectAllocatorType(mem_stats, create_pygote_space);
        return (codeAllocator_ != nullptr) && (internalAllocator_ != nullptr) && (objectAllocator_ != nullptr);
    }

    /***
     * Initialize GC bits and also zeroing memory for the whole Object memory
     * @param cls - class
     * @param mem - pointer to the ObjectHeader
     * @return pointer to the ObjectHeader
     */
    ObjectHeader *InitObjectHeaderAtMem(BaseClass *cls, void *mem);

    /***
     * Triggers GC if needed
     */
    void TriggerGCIfNeeded();

    void *TryGCAndAlloc(size_t size, Alignment align, panda::MTManagedThread *thread);

    void *AllocByTLAB(size_t size, ManagedThread *thread);

    void *AllocateMemoryForObject(size_t size, Alignment align, ManagedThread *thread);

    static constexpr float DEFAULT_TARGET_UTILIZATION = 0.5;

    bool isInitialized_ = false;
    bool use_runtime_internal_allocator_ {true};
    CodeAllocator *codeAllocator_ = nullptr;
    InternalAllocatorPtr internalAllocator_ = nullptr;
    ObjectAllocatorPtr objectAllocator_ = nullptr;

    bool use_tlab_for_allocations_ = false;

    /**
     * StackFrameAllocator is per thread
     */
    StackFrameAllocator *GetCurrentStackFrameAllocator();

    friend class ::panda::Runtime;

    /**
     * To implement the getTargetHeapUtilization and nativeSetTargetHeapUtilization, I set a variable here.
     * It may need to be initialized, but now I give it a fixed initial value 0.5
     */
    float target_utilization_;

    IsObjectFinalizebleFunc IsObjectFinalizebleFunc_ = nullptr;
    RegisterFinalizeReferenceFunc RegisterFinalizeReferenceFunc_ = nullptr;
    PandaVM *vm_ {nullptr};
    MemStatsType *mem_stats_ {nullptr};
    mem::GC *gc_ = nullptr;
    RuntimeNotificationManager *notification_manager_ = nullptr;
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_HEAP_MANAGER_H_
