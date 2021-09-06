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

#ifndef PANDA_RUNTIME_MEM_GC_GC_H_
#define PANDA_RUNTIME_MEM_GC_GC_H_

#include <atomic>
#include <map>
#include <string_view>
#include <vector>

#include "libpandabase/os/mutex.h"
#include "libpandabase/os/thread.h"
#include "libpandabase/trace/trace.h"
#include "libpandabase/utils/expected.h"
#include "runtime/include/gc_task.h"
#include "runtime/include/language_config.h"
#include "runtime/include/locks.h"
#include "runtime/include/mem/panda_containers.h"
#include "runtime/include/mem/panda_smart_pointers.h"
#include "runtime/include/mem/panda_string.h"
#include "runtime/mem/allocator_adapter.h"
#include "runtime/mem/gc/gc_barrier_set.h"
#include "runtime/mem/gc/gc_phase.h"
#include "runtime/mem/gc/gc_root.h"
#include "runtime/mem/gc/gc_scoped_phase.h"
#include "runtime/mem/gc/gc_stats.h"
#include "runtime/mem/gc/gc_types.h"
#include "runtime/mem/refstorage/reference.h"
#include "runtime/mem/gc/bitmap.h"
#include "runtime/mem/object_helpers.h"
#include "runtime/timing.h"

namespace panda {

class BaseClass;
class Class;
class HClass;
class PandaVM;
class Timing;
namespace java {
class JClass;
class JReference;
}  // namespace java
namespace mem {
class GlobalObjectStorage;
class ReferenceProcessor;
namespace test {
class ReferenceStorageTest;
class RemSetTest;
}  // namespace test
namespace java {
class ReferenceQueue;
class JavaReferenceProcessor;
namespace test {
class ReferenceProcessorBaseTest;
}  // namespace test
}  // namespace java
}  // namespace mem
}  // namespace panda

namespace panda::coretypes {
class Array;
class DynClass;
}  // namespace panda::coretypes

namespace panda::mem {

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_DEBUG_GC LOG(DEBUG, GC) << this->GetLogPrefix()
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_INFO_GC LOG(INFO, GC) << this->GetLogPrefix()

// forward declarations:
class GCListener;
class HybridObjectAllocator;
class GCScopedPhase;
class GCQueueInterface;
class GCDynamicObjectHelpers;

enum class GCError { GC_ERROR_NO_ROOTS, GC_ERROR_NO_FRAMES, GC_ERROR_LAST = GC_ERROR_NO_FRAMES };

enum ClassRootsVisitFlag : bool {
    ENABLED = true,
    DISABLED = false,
};

enum CardTableVisitFlag : bool {
    VISIT_ENABLED = true,
    VISIT_DISABLED = false,
};

enum class NativeGcTriggerType { INVALID_NATIVE_GC_TRIGGER, NO_NATIVE_GC_TRIGGER, SIMPLE_STRATEGY };
inline NativeGcTriggerType NativeGcTriggerTypeFromString(std::string_view native_gc_trigger_type_str)
{
    if (native_gc_trigger_type_str == "no-native-gc-trigger") {
        return NativeGcTriggerType::NO_NATIVE_GC_TRIGGER;
    }
    if (native_gc_trigger_type_str == "simple-strategy") {
        return NativeGcTriggerType::SIMPLE_STRATEGY;
    }
    return NativeGcTriggerType::INVALID_NATIVE_GC_TRIGGER;
}

class GCListener {
public:
    GCListener() = default;
    NO_COPY_SEMANTIC(GCListener);
    DEFAULT_MOVE_SEMANTIC(GCListener);
    virtual ~GCListener() = 0;
    virtual void GCStarted(size_t heap_size) = 0;
    virtual void GCFinished(const GCTask &task, size_t heap_size_before_gc, size_t heap_size) = 0;
};

struct GCSettings {
    bool is_gc_enable_tracing = false;  /// tracing via systrace
    NativeGcTriggerType native_gc_trigger_type = {
        NativeGcTriggerType::INVALID_NATIVE_GC_TRIGGER};  /// type of native trigger
    bool is_dump_heap = false;                            /// dump heap at the beginning and the end of GC
    bool is_concurrency_enabled = true;                   /// true if concurrency enabled
    bool run_gc_in_place = false;                         /// true if GC should be running in place
    bool pre_gc_heap_verification = false;                /// true if heap verification before GC enabled
    bool post_gc_heap_verification = false;               /// true if heap verification after GC enabled
    bool fail_on_heap_verification = false;  /// if true then fail execution if heap verifier found heap corruption
    uint64_t young_space_size = 0;           /// size of young-space for gen-gc
};

class GCExtensionData;

using UpdateRefInObject = std::function<void(ObjectHeader *)>;
using UpdateRefInAllocator = std::function<void(const UpdateRefInObject &)>;

class GCMarker {
public:
    template <bool reversed_mark = false, bool atomic_mark = true>
    void MarkObjectHeader(ObjectHeader *object) const
    {
        // NOLINTNEXTLINE(readability-braces-around-statements)
        if constexpr (reversed_mark) {  // NOLINT(bugprone-suspicious-semicolon)
            object->SetUnMarkedForGC<atomic_mark>();
            return;
        }
        object->SetMarkedForGC<atomic_mark>();
    }

    template <bool reversed_mark = false, bool atomic_mark = true>
    bool IsObjectHeaderMarked(ObjectHeader *object) const
    {
        // NOLINTNEXTLINE(readability-braces-around-statements)
        if constexpr (reversed_mark) {  // NOLINT(bugprone-suspicious-semicolon)
            return !object->IsMarkedForGC<atomic_mark>();
        }
        return object->IsMarkedForGC<atomic_mark>();
    }

    template <bool reversed_mark = false>
    bool MarkIfNotMarked(ObjectHeader *object) const
    {
        MarkBitmap *bitmap = GetMarkBitMap(object);
        if (bitmap != nullptr) {
            if (bitmap->Test(object)) {
                return false;
            }
            bitmap->Set(object);
            return true;
        }
        if (atomic_mark_flag_) {
            if (IsObjectHeaderMarked<reversed_mark, true>(object)) {
                return false;
            }
            MarkObjectHeader<reversed_mark, true>(object);
        } else {
            if (IsObjectHeaderMarked<reversed_mark, false>(object)) {
                return false;
            }
            MarkObjectHeader<reversed_mark, false>(object);
        }
        return true;
    }

    template <bool reversed_mark = false>
    void Mark(ObjectHeader *object) const
    {
        MarkBitmap *bitmap = GetMarkBitMap(object);
        if (bitmap != nullptr) {
            bitmap->Set(object);
            return;
        }
        if constexpr (reversed_mark) {  // NOLINTNEXTLINE(readability-braces-around-statements)
            if (atomic_mark_flag_) {
                object->SetUnMarkedForGC<true>();
            } else {
                object->SetUnMarkedForGC<false>();
            }
            return;
        }
        if (atomic_mark_flag_) {
            object->SetMarkedForGC<true>();
        } else {
            object->SetMarkedForGC<false>();
        }
    }

    template <bool reversed_mark = false>
    void UnMark(ObjectHeader *object) const
    {
        MarkBitmap *bitmap = GetMarkBitMap(object);
        if (bitmap != nullptr) {
            return;  // no need for bitmap
        }
        if constexpr (reversed_mark) {  // NOLINTNEXTLINE(readability-braces-around-statements)
            if (atomic_mark_flag_) {
                object->SetMarkedForGC<true>();
            } else {
                object->SetMarkedForGC<false>();
            }
            return;
        }
        if (atomic_mark_flag_) {
            object->SetUnMarkedForGC<true>();
        } else {
            object->SetUnMarkedForGC<false>();
        }
    }

    template <bool reversed_mark = false>
    bool IsMarked(const ObjectHeader *object) const
    {
        MarkBitmap *bitmap = GetMarkBitMap(object);
        if (bitmap != nullptr) {
            return bitmap->Test(object);
        }
        bool is_marked = atomic_mark_flag_ ? object->IsMarkedForGC<true>() : object->IsMarkedForGC<false>();
        if constexpr (reversed_mark) {  // NOLINTNEXTLINE(readability-braces-around-statements)
            return !is_marked;
        }
        return is_marked;
    }

    template <bool reversed_mark = false>
    ObjectStatus MarkChecker(const ObjectHeader *object) const
    {
        if constexpr (!reversed_mark) {  // NOLINTNEXTLINE(readability-braces-around-statements)
            // If ClassAddr is not setted - it means object header initialization is in progress now
            if (object->AtomicClassAddr<Class>() == nullptr) {
                return ObjectStatus::ALIVE_OBJECT;
            }
        }
        ObjectStatus object_status =
            IsMarked<reversed_mark>(object) ? ObjectStatus::ALIVE_OBJECT : ObjectStatus::DEAD_OBJECT;
        LOG(DEBUG, GC) << " Mark check for " << std::hex << object << std::dec
                       << " object is alive: " << static_cast<bool>(object_status);
        return object_status;
    }

    MarkBitmap *GetMarkBitMap(const void *object) const
    {
        for (auto bitmap : mark_bitmaps_) {
            if (bitmap->IsAddrInRange(object)) {
                return bitmap;
            }
        }
        return nullptr;
    }

    void ClearMarkBitMaps()
    {
        mark_bitmaps_.clear();
    }

    template <typename It>
    void AddMarkBitMaps(It start, It end)
    {
        mark_bitmaps_.insert(mark_bitmaps_.end(), start, end);
    }

    void SetAtomicMark(bool flag)
    {
        atomic_mark_flag_ = flag;
    }

    bool GetAtomicMark() const
    {
        return atomic_mark_flag_;
    }

private:
    // Bitmaps for mark object
    PandaVector<MarkBitmap *> mark_bitmaps_;
    bool atomic_mark_flag_ = true;
};

class NoAtomicGCMarkerScope {
public:
    explicit NoAtomicGCMarkerScope(GCMarker *marker)
    {
        ASSERT(marker != nullptr);
        gc_marker_ = marker;
        old_state_ = gc_marker_->GetAtomicMark();
        if (old_state_) {
            gc_marker_->SetAtomicMark(false);
        }
    }

    NO_COPY_SEMANTIC(NoAtomicGCMarkerScope);
    NO_MOVE_SEMANTIC(NoAtomicGCMarkerScope);

    ~NoAtomicGCMarkerScope()
    {
        if (old_state_) {
            gc_marker_->SetAtomicMark(old_state_);
        }
    }

private:
    GCMarker *gc_marker_;
    bool old_state_ = false;
};

// base class for all GCs
class GC {
public:
    explicit GC(ObjectAllocatorBase *object_allocator, const GCSettings &settings);
    NO_COPY_SEMANTIC(GC);
    NO_MOVE_SEMANTIC(GC);
    virtual ~GC() = 0;

    GCType GetType();

    /**
     * \brief Initialize GC
     */
    void Initialize();

    /**
     * \brief Starts GC after initialization
     * Creates worker thread, sets gc_running_ to true
     */
    virtual void StartGC();

    /**
     * \brief Stops GC for runtime destruction
     * Joins GC thread, clears queue
     */
    virtual void StopGC();

    /**
     * Should be used to wait while GC should work exlusively
     * Note: for non-mt STW GC can be used to run GC
     */
    virtual void WaitForGC(const GCTask &task) = 0;

    /**
     * Should be used to wait while GC should be executed in managed scope
     */
    void WaitForGCInManaged(const GCTask &task) NO_THREAD_SAFETY_ANALYSIS;

    /**
     * Only be used to at first pygote fork
     */
    void WaitForGCOnPygoteFork(const GCTask &task);

    bool IsOnPygoteFork();

    /**
     * Initialize GC bits on object creation.
     * Required only for GCs with switched bits
     */
    virtual void InitGCBits(panda::ObjectHeader *obj_header) = 0;

    /**
     * Initialize GC bits on object creation for the TLAB allocation.
     */
    virtual void InitGCBitsForAllocationInTLAB(panda::ObjectHeader *obj_header) = 0;

    bool IsTLABsSupported()
    {
        return tlabs_supported_;
    }

    /**
     * Triggers GC
     */
    virtual void Trigger() = 0;

    /**
     * Return true if gc has generations, false otherwise
     */
    bool IsGenerational() const;

    PandaString DumpStatistics()
    {
        return instance_stats_.GetDump(gc_type_);
    }

    void AddListener(GCListener *listener)
    {
        ASSERT(gc_listeners_ptr_ != nullptr);
        gc_listeners_ptr_->push_back(listener);
    }

    GCBarrierSet *GetBarrierSet()
    {
        ASSERT(gc_barrier_set_ != nullptr);
        return gc_barrier_set_;
    }

    // Additional NativeGC
    void NotifyNativeAllocations();

    void RegisterNativeAllocation(size_t bytes);

    void RegisterNativeFree(size_t bytes);

    int32_t GetNotifyNativeInterval()
    {
        return NOTIFY_NATIVE_INTERVAL;
    }

    // Calling CheckGCForNative immediately for every NOTIFY_NATIVE_INTERVAL allocations
    static constexpr int32_t NOTIFY_NATIVE_INTERVAL = 32;

    // Calling CheckGCForNative immediately if size exceeds the following
    static constexpr size_t CHECK_IMMEDIATELY_THRESHOLD = 300000;

    inline GCPhase GetGCPhase() const
    {
        return phase_;
    }

    inline bool IsGCRunning()
    {
        return gc_running_.load();
    }

    void PreStartup();

    InternalAllocatorPtr GetInternalAllocator() const
    {
        return internal_allocator_;
    }

    /**
     * Enqueue all references in ReferenceQueue. Should be done after GC to avoid deadlock (lock in
     * ReferenceQueue.class)
     */
    void EnqueueReferences();

    /**
     * Process all references which GC found in marking phase.
     */
    void ProcessReferences(GCPhase gc_phase, const GCTask &task);

    size_t GetNativeBytesRegistered()
    {
        return native_bytes_registered_.load(std::memory_order_relaxed);
    }

    virtual void SetPandaVM(PandaVM *vm);

    PandaVM *GetPandaVm() const
    {
        return vm_;
    }

    virtual void PreZygoteFork()
    {
        JoinWorker();
    }

    virtual void PostZygoteFork()
    {
        CreateWorker();
    }

    void SetCanAddGCTask(bool can_add_task)
    {
        can_add_gc_task_.store(can_add_task, std::memory_order_relaxed);
    }

    void SetGCAtomicFlag(bool atomic_flag)
    {
        marker_.SetAtomicMark(atomic_flag);
    }

    GCExtensionData *GetExtensionData() const
    {
        return extension_data_;
    }

    void SetExtensionData(GCExtensionData *data)
    {
        extension_data_ = data;
    }

    virtual void PostForkCallback() {}

    uint64_t GetLastGCReclaimedBytes();

    /**
     * Check if the object addr is in the GC sweep range
     */
    virtual bool InGCSweepRange([[maybe_unused]] uintptr_t addr) const
    {
        return true;
    }

protected:
    /**
     * \brief Runs all phases
     */
    void RunPhases(const GCTask &task);

    template <LangTypeT LANG_TYPE, bool HAS_VALUE_OBJECT_TYPES>
    void MarkInstance(PandaStackTL<ObjectHeader *> *objects_stack, const ObjectHeader *object, BaseClass *cls);

    /**
     * Add task to GC Queue to be run by GC thread (or run in place)
     */
    void AddGCTask(bool is_managed, PandaUniquePtr<GCTask> task, bool triggered_by_threshold);

    virtual void InitializeImpl() = 0;
    virtual void PreRunPhasesImpl() = 0;
    virtual void RunPhasesImpl(const GCTask &task) = 0;
    virtual void PreStartupImp() {}

    void BindBitmaps(bool clear_pygote_space_bitmaps);

    inline bool IsTracingEnabled() const
    {
        return gc_settings_.is_gc_enable_tracing;
    }

    inline void BeginTracePoint(const PandaString &trace_point_name) const
    {
        if (IsTracingEnabled()) {
            trace::BeginTracePoint(trace_point_name.c_str());
        }
    }

    inline void EndTracePoint() const
    {
        if (IsTracingEnabled()) {
            trace::EndTracePoint();
        }
    }

    virtual void VisitRoots(const GCRootVisitor &gc_root_visitor, VisitGCRootFlags flags) = 0;
    virtual void VisitClassRoots(const GCRootVisitor &gc_root_visitor) = 0;
    virtual void VisitCardTableRoots(CardTable *card_table, const GCRootVisitor &gc_root_visitor,
                                     const MemRangeChecker &range_checker, const ObjectChecker &range_object_checker,
                                     const ObjectChecker &from_object_checker, uint32_t processed_flag) = 0;

    inline void SetGCPhase(GCPhase gc_phase)
    {
        phase_ = gc_phase;
    }

    inline bool CASGCPhase(GCPhase expected, GCPhase set)
    {
        return phase_.compare_exchange_strong(expected, set);
    }

    GCInstanceStats *GetStats()
    {
        return &instance_stats_;
    }

    inline void SetType(GCType gc_type)
    {
        gc_type_ = gc_type;
    }

    inline void SetTLABsSupported()
    {
        tlabs_supported_ = true;
    }

    void SetGCBarrierSet(GCBarrierSet *barrier_set)
    {
        ASSERT(gc_barrier_set_ == nullptr);
        gc_barrier_set_ = barrier_set;
    }

    /**
     * Mark object.
     * Note: for some GCs it is not necessary to set GC bit to 1.
     * @param object_header
     */
    virtual void MarkObject(ObjectHeader *object_header);

    /**
     * Mark object.
     * Note: for some GCs it is not necessary to set GC bit to 1.
     * @param object_header
     * @return true if object old state is not marked
     */
    virtual bool MarkObjectIfNotMarked(ObjectHeader *object_header);

    /**
     * UnMark object
     * @param object_header
     */
    virtual void UnMarkObject(ObjectHeader *object_header);

    /**
     * Check if the object is marked for GC(alive)
     * @param object
     * @return true if object marked for GC
     */
    virtual bool IsMarked(const ObjectHeader *object) const;

    /**
     * Return true of ref is an instance of reference or it's ancestor, false otherwise
     */
    bool IsReference(BaseClass *cls, const ObjectHeader *ref);

    void ProcessReference(PandaStackTL<ObjectHeader *> *objects_stack, BaseClass *cls, const ObjectHeader *object);

    /**
     * Add reference for later processing in marking phase
     * @param object - object from which we start to mark
     */
    void AddReference(ObjectHeader *object);

    /**
     * Mark all references which we added by AddReference method
     */
    virtual void MarkReferences(PandaStackTL<ObjectHeader *> *references, GCPhase gc_phase) = 0;

    ObjectAllocatorBase *GetObjectAllocator() const
    {
        return object_allocator_;
    }

    friend class HeapRootVisitor;

    /**
     * Update all refs to moved objects
     */
    virtual void CommonUpdateRefsToMovedObjects(const UpdateRefInAllocator &update_allocator) = 0;

    virtual void UpdateVmRefs() = 0;

    virtual void UpdateGlobalObjectStorage() = 0;

    virtual void UpdateClassLinkerContextRoots() = 0;

    void UpdateRefsInVRegs(ManagedThread *thread);

    void AddToStack(PandaStackTL<ObjectHeader *> *objects_stack, ObjectHeader *object);

    ObjectHeader *PopObjectFromStack(PandaStackTL<ObjectHeader *> *objects_stack);

    Timing *GetTiming()
    {
        return &timing_;
    }

    void SetForwardAddress(ObjectHeader *src, ObjectHeader *dst);

    // vector here because we can add some references on young-gc and get new refs on old-gc
    // it's possible if we make 2 GCs for one safepoint
    // max length of this vector - is 2
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    PandaVector<panda::mem::Reference *> *cleared_references_ GUARDED_BY(cleared_references_lock_) {nullptr};

    os::memory::Mutex *cleared_references_lock_ {nullptr};  // NOLINT(misc-non-private-member-variables-in-classes)

    std::atomic<size_t> gc_counter_ {0};                // NOLINT(misc-non-private-member-variables-in-classes)
    std::atomic<uint64_t> last_gc_reclaimed_bytes {0};  // NOLINT(misc-non-private-member-variables-in-classes)
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    std::atomic<GCTaskCause> last_cause_ {GCTaskCause::INVALID_CAUSE};

    GCSettings *GetSettings()
    {
        return &gc_settings_;
    }

    /**
     * @return true if GC can work in concurrent mode
     */
    bool IsConcurrencyAllowed() const
    {
        return gc_settings_.is_concurrency_enabled;
    }

    PandaString GetLogPrefix() const
    {
        PandaOStringStream ss;
        ss << "[" << gc_counter_.load(std::memory_order_acquire) << ", " << GCScopedPhase::GetPhaseAbbr(GetGCPhase())
           << "]: ";
        return ss.str();
    }

    GCMarker marker_;  // NOLINT(misc-non-private-member-variables-in-classes)
    Timing timing_;    // NOLINT(misc-non-private-member-variables-in-classes)

private:
    /**
     * Entrypoint for GC worker thread
     * @param gc pointer to GC structure
     * @param vm pointer to VM structure
     */
    static void GCWorkerEntry(GC *gc, PandaVM *vm);

    /**
     * Iterate over all fields with references of object and add all not null object references to the objects_stack
     * @param objects_stack - stack with objects
     * @param object
     * @param base_cls - class of object(used for perf in case if class for the object already was obtained)
     */
    template <LangTypeT LANG_TYPE, bool HAS_VALUE_OBJECT_TYPES>
    void HandleObject(PandaStackTL<ObjectHeader *> *objects_stack, const ObjectHeader *object, BaseClass *base_cls);

    /**
     * Iterate over class data and add all found not null object references to the objects_stack
     * @param objects_stack - stack with objects
     * @param cls - class
     */
    template <LangTypeT LANG_TYPE, bool HAS_VALUE_OBJECT_TYPES, class ClassT>
    void HandleClass(PandaStackTL<ObjectHeader *> *objects_stack, ClassT *cls);

    /**
     * For arrays of objects add all not null object references to the objects_stack
     * @param objects_stack - stack with objects
     * @param array_object - array object
     * @param cls - class of array object(used for perf)
     */
    template <LangTypeT LANG_TYPE, bool HAS_VALUE_OBJECT_TYPES>
    void HandleArrayClass(PandaStackTL<ObjectHeader *> *objects_stack, const coretypes::Array *array_object,
                          const BaseClass *cls);

    void JoinWorker();
    void CreateWorker();

    /**
     * Move small objects to pygote space at first pygote fork
     */
    void MoveObjectsToPygoteSpace();

    size_t GetNativeBytesFromMallinfoAndRegister() const;
    virtual void UpdateThreadLocals() = 0;
    virtual size_t VerifyHeap() = 0;
    NativeGcTriggerType GetNativeGcTriggerType();

    volatile std::atomic<GCPhase> phase_ {GCPhase::GC_PHASE_IDLE};
    GCType gc_type_ {GCType::INVALID_GC};
    GCSettings gc_settings_;
    PandaVector<GCListener *> *gc_listeners_ptr_ {nullptr};
    GCBarrierSet *gc_barrier_set_ {nullptr};
    ObjectAllocatorBase *object_allocator_ {nullptr};
    InternalAllocatorPtr internal_allocator_ {nullptr};
    GCInstanceStats instance_stats_;

    // Additional NativeGC
    std::atomic<size_t> native_bytes_registered_ = 0;
    std::atomic<size_t> native_objects_notified_ = 0;

    ReferenceProcessor *reference_processor_ {nullptr};
    std::atomic_bool allow_soft_reference_processing_ = false;

    GCQueueInterface *gc_queue_ = nullptr;
    std::thread *worker_ = nullptr;
    std::atomic_bool gc_running_ = false;
    std::atomic<bool> can_add_gc_task_ = true;
    bool tlabs_supported_ = false;

    // Additional data for extensions
    GCExtensionData *extension_data_ {nullptr};

    class PostForkGCTask;

    friend class java::ReferenceQueue;
    friend class java::JavaReferenceProcessor;
    friend class java::test::ReferenceProcessorBaseTest;
    friend class panda::mem::test::ReferenceStorageTest;
    friend class panda::mem::test::RemSetTest;
    friend class GCScopedPhase;
    friend class GlobalObjectStorage;
    friend class GCDynamicObjectHelpers;
    friend class GCStaticObjectHelpers;
    void TriggerGCForNative();
    size_t SimpleNativeAllocationGcWatermark();
    /**
     * Waits while current GC task(if any) will be processed
     */
    void WaitForIdleGC() NO_THREAD_SAFETY_ANALYSIS;

    friend class ConcurrentScope;

    PandaVM *vm_ {nullptr};
};

template <MTModeT MTMode>
class AllocConfig<GCType::STW_GC, MTMode> {
public:
    using ObjectAllocatorType = ObjectAllocatorNoGen<MTMode>;
    using CodeAllocatorType = CodeAllocator;
};

template <MTModeT MTMode>
class AllocConfig<GCType::EPSILON_GC, MTMode> {
public:
    using ObjectAllocatorType = ObjectAllocatorNoGen<MTMode>;
    using CodeAllocatorType = CodeAllocator;
};

template <MTModeT MTMode>
class AllocConfig<GCType::GEN_GC, MTMode> {
public:
    using ObjectAllocatorType = ObjectAllocatorGen<MTMode>;
    using CodeAllocatorType = CodeAllocator;
};

template <MTModeT MTMode>
class AllocConfig<GCType::HYBRID_GC, MTMode> {
public:
    using ObjectAllocatorType = HybridObjectAllocator;
    using CodeAllocatorType = CodeAllocator;
};

/**
 * \brief Create GC with \param gc_type
 * @param gc_type - type of create GC
 * @return pointer to created GC on success, nullptr on failure
 */
template <class LanguageConfig>
GC *CreateGC(GCType gc_type, ObjectAllocatorBase *object_allocator, const GCSettings &settings);

/**
 * Enable concurrent mode. Should be used only from STW code.
 */
class ConcurrentScope final {
public:
    explicit ConcurrentScope(GC *gc, bool auto_start = true);
    NO_COPY_SEMANTIC(ConcurrentScope);
    NO_MOVE_SEMANTIC(ConcurrentScope);
    ~ConcurrentScope();
    void Start();

private:
    GC *gc_;
    bool started_ = false;
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_GC_GC_H_
