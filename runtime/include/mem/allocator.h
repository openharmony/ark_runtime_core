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

#ifndef PANDA_RUNTIME_INCLUDE_MEM_ALLOCATOR_H_
#define PANDA_RUNTIME_INCLUDE_MEM_ALLOCATOR_H_

#include <functional>

#include "libpandabase/mem/code_allocator.h"
#include "libpandabase/mem/mem.h"
#include "libpandabase/mem/pool_map.h"
#include "libpandabase/utils/logger.h"
#include "libpandabase/macros.h"
#include "runtime/mem/bump-allocator.h"
#include "runtime/mem/freelist_allocator.h"
#include "runtime/mem/gc/gc_types.h"
#include "runtime/mem/humongous_obj_allocator.h"
#include "runtime/mem/internal_allocator.h"
#include "runtime/mem/runslots_allocator.h"
#include "runtime/mem/pygote_space_allocator.h"

namespace panda {
class ObjectHeader;
}  // namespace panda

namespace panda {
class ManagedThread;
}  // namespace panda

namespace panda {
class BaseClass;
}  // namespace panda

namespace panda::mem {

class ObjectAllocConfigWithCrossingMap;
class ObjectAllocConfig;
class TLAB;

/**
 * AllocatorPurpose and GCCollectMode provide info when we should collect from some allocator or not
 */
enum class AllocatorPurpose {
    ALLOCATOR_PURPOSE_OBJECT,    // Allocator for objects
    ALLOCATOR_PURPOSE_INTERNAL,  // Space for runtime internal needs
};

template <AllocatorType>
class AllocatorTraits {
};

template <>
class AllocatorTraits<AllocatorType::RUNSLOTS_ALLOCATOR> {
    using AllocType = RunSlotsAllocator<ObjectAllocConfig>;
    static constexpr bool HAS_FREE {true};  // indicates allocator can free
};

template <typename T, AllocScope AllocScopeT>
class AllocatorAdapter;

class Allocator {
public:
    template <typename T, AllocScope AllocScopeT = AllocScope::GLOBAL>
    using AdapterType = AllocatorAdapter<T, AllocScopeT>;

    NO_COPY_SEMANTIC(Allocator);
    NO_MOVE_SEMANTIC(Allocator);
    explicit Allocator(MemStatsType *mem_stats, AllocatorPurpose purpose, GCCollectMode gc_collect_mode)
        : mem_stats_(mem_stats), allocator_purpose_(purpose), gc_collect_mode_(gc_collect_mode)
    {
    }
    virtual ~Allocator() = 0;

    ALWAYS_INLINE AllocatorPurpose GetPurpose() const
    {
        return allocator_purpose_;
    }

    ALWAYS_INLINE GCCollectMode GetCollectMode() const
    {
        return gc_collect_mode_;
    }

    ALWAYS_INLINE MemStatsType *GetMemStats() const
    {
        return mem_stats_;
    }

    [[nodiscard]] void *Alloc(size_t size)
    {
        return Allocate(size, DEFAULT_ALIGNMENT, nullptr);
    }

    [[nodiscard]] void *AllocLocal(size_t size)
    {
        return AllocateLocal(size, DEFAULT_ALIGNMENT, nullptr);
    }

    [[nodiscard]] virtual void *Allocate(size_t size, Alignment align,
                                         [[maybe_unused]] panda::ManagedThread *thread) = 0;

    [[nodiscard]] virtual void *AllocateLocal(size_t size, Alignment align,
                                              [[maybe_unused]] panda::ManagedThread *thread) = 0;

    [[nodiscard]] virtual void *AllocateNonMovable(size_t size, Alignment align, panda::ManagedThread *thread) = 0;

    virtual void *AllocateTenured([[maybe_unused]] size_t size)
    {
        LOG(FATAL, ALLOC) << "AllocTenured not implemented";
        UNREACHABLE();
    }

    template <class T>
    [[nodiscard]] T *AllocArray(size_t size)
    {
        return static_cast<T *>(this->Allocate(sizeof(T) * size, DEFAULT_ALIGNMENT, nullptr));
    }

    template <class T>
    void Delete(T *ptr)
    {
        if (ptr == nullptr) {
            return;
        }
        // NOLINTNEXTLINE(readability-braces-around-statements,bugprone-suspicious-semicolon)
        if constexpr (std::is_class_v<T>) {
            ptr->~T();
        }
        Free(ptr);
    }

    template <typename T>
    void DeleteArray(T *data)
    {
        if (data == nullptr) {
            return;
        }
        static constexpr size_t SIZE_BEFORE_DATA_OFFSET = AlignUp(sizeof(size_t), DEFAULT_ALIGNMENT_IN_BYTES);
        void *p = ToVoidPtr(ToUintPtr(data) - SIZE_BEFORE_DATA_OFFSET);
        size_t size = *static_cast<size_t *>(p);
        // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
        if constexpr (std::is_class_v<T>) {
            for (size_t i = 0; i < size; ++i, ++data) {
                data->~T();
            }
        }
        Free(p);
    }

    virtual void Free(void *mem) = 0;

    virtual void VisitAndRemoveAllPools(const MemVisitor &mem_visitor) = 0;

    virtual void VisitAndRemoveFreePools(const MemVisitor &mem_visitor) = 0;

    virtual void IterateOverYoungObjects([[maybe_unused]] const ObjectVisitor &object_visitor)
    {
        LOG(FATAL, ALLOC) << "Allocator::IterateOverYoungObjects" << std::endl;
    }

    virtual void IterateOverTenuredObjects([[maybe_unused]] const ObjectVisitor &object_visitor)
    {
        LOG(FATAL, ALLOC) << "Allocator::IterateOverTenuredObjects" << std::endl;
    }

    /**
     * \brief iterates all objects in object allocator
     */
    virtual void IterateRegularSizeObjects([[maybe_unused]] const ObjectVisitor &object_visitor)
    {
        LOG(FATAL, ALLOC) << "Allocator::IterateRegularSizeObjects";
    }

    /**
     * \brief iterates objects in all allocators except object allocator
     */
    virtual void IterateNonRegularSizeObjects([[maybe_unused]] const ObjectVisitor &object_visitor)
    {
        LOG(FATAL, ALLOC) << "Allocator::IterateNonRegularSizeObjects";
    }

    virtual void FreeObjectsMovedToPygoteSpace()
    {
        LOG(FATAL, ALLOC) << "Allocator::FreeObjectsMovedToPygoteSpace";
    }

    virtual void IterateOverObjectsInRange(MemRange mem_range, const ObjectVisitor &object_visitor) = 0;

    virtual void IterateOverObjects(const ObjectVisitor &object_visitor) = 0;

    template <AllocScope AllocScopeT = AllocScope::GLOBAL>
    AllocatorAdapter<void, AllocScopeT> Adapter();

    template <typename T, typename... Args>
    std::enable_if_t<!std::is_array_v<T>, T *> New(Args &&... args)
    {
        void *p = Alloc(sizeof(T));
        if (UNLIKELY(p == nullptr)) {
            return nullptr;
        }
        new (p) T(std::forward<Args>(args)...);  // NOLINT(bugprone-throw-keyword-missing)
        return reinterpret_cast<T *>(p);
    }

    template <typename T>
    std::enable_if_t<is_unbounded_array_v<T>, std::remove_extent_t<T> *> New(size_t size)
    {
        static constexpr size_t SIZE_BEFORE_DATA_OFFSET = AlignUp(sizeof(size_t), DEFAULT_ALIGNMENT_IN_BYTES);
        using element_type = std::remove_extent_t<T>;
        void *p = Alloc(SIZE_BEFORE_DATA_OFFSET + sizeof(element_type) * size);
        *static_cast<size_t *>(p) = size;
        auto *data = ToNativePtr<element_type>(ToUintPtr(p) + SIZE_BEFORE_DATA_OFFSET);
        element_type *current_element = data;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        for (size_t i = 0; i < size; ++i, ++current_element) {
            new (current_element) element_type();
        }
        return data;
    }

    template <typename T, typename... Args>
    std::enable_if_t<!std::is_array_v<T>, T *> NewLocal(Args &&... args)
    {
        void *p = AllocLocal(sizeof(T));
        if (UNLIKELY(p == nullptr)) {
            return nullptr;
        }
        new (p) T(std::forward<Args>(args)...);  // NOLINT(bugprone-throw-keyword-missing)
        return reinterpret_cast<T *>(p);
    }

    template <typename T>
    std::enable_if_t<is_unbounded_array_v<T>, std::remove_extent_t<T> *> NewLocal(size_t size)
    {
        static constexpr size_t SIZE_BEFORE_DATA_OFFSET = AlignUp(sizeof(size_t), DEFAULT_ALIGNMENT_IN_BYTES);
        using element_type = std::remove_extent_t<T>;
        void *p = AllocLocal(SIZE_BEFORE_DATA_OFFSET + sizeof(element_type) * size);
        *static_cast<size_t *>(p) = size;
        auto *data = ToNativePtr<element_type>(ToUintPtr(p) + SIZE_BEFORE_DATA_OFFSET);
        element_type *current_element = data;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        for (size_t i = 0; i < size; ++i, ++current_element) {
            new (current_element) element_type();
        }
        return data;
    }

    virtual void *AllocateInLargeAllocator([[maybe_unused]] size_t size, [[maybe_unused]] Alignment align,
                                           [[maybe_unused]] BaseClass *cls)
    {
        return nullptr;
    }

#if defined(TRACK_INTERNAL_ALLOCATIONS)
    virtual void Dump() {}
#endif

protected:
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    MemStatsType *mem_stats_;

private:
    AllocatorPurpose allocator_purpose_;
    GCCollectMode gc_collect_mode_;
};

class ObjectAllocatorBase : public Allocator {
public:
    ObjectAllocatorBase() = delete;
    NO_COPY_SEMANTIC(ObjectAllocatorBase);
    NO_MOVE_SEMANTIC(ObjectAllocatorBase);

    explicit ObjectAllocatorBase(MemStatsType *mem_stats, GCCollectMode gc_collect_mode,
                                 bool create_pygote_space_allocator);

    ~ObjectAllocatorBase() override;

    /**
     * Iterate over all objects and reclaim memory for objects reported as true by gc_object_visitor
     * @param gc_object_visitor - function which return true for ObjectHeader if we can reclaim memory occupied by
     * object
     */
    virtual void Collect(const GCObjectVisitor &gc_object_visitor, GCCollectMode collect_mode) = 0;

    /**
     * Return max size for regular size objects
     * @return max size in bytes for regular size objects
     */
    virtual size_t GetRegularObjectMaxSize() = 0;

    /**
     * Return max size for large objects
     * @return max size in bytes for large objects
     */
    virtual size_t GetLargeObjectMaxSize() = 0;

    /**
     * Checks if address in the young space
     * @param address
     * @return true if \param address is in young space
     */
    virtual bool IsAddressInYoungSpace(uintptr_t address) = 0;

    /**
     * Checks if object in the non-movable space
     * @param obj
     * @return true if \param obj is in non-movable space
     */
    virtual bool IsObjectInNonMovableSpace(const ObjectHeader *obj) = 0;

    /**
     * @return true if allocator has a young space
     */
    virtual bool HasYoungSpace() = 0;

    /**
     * Get young space memory range
     * @return young space memory range
     */
    virtual MemRange GetYoungSpaceMemRange() = 0;

    virtual void ResetYoungAllocator() = 0;

    virtual TLAB *CreateNewTLAB(panda::ManagedThread *thread) = 0;

    virtual size_t GetTLABMaxAllocSize() = 0;

    virtual bool IsTLABSupported() = 0;

    /**
     * \brief Check if the object allocator contains the object starting at address obj
     */
    virtual bool ContainObject([[maybe_unused]] const ObjectHeader *obj) const = 0;

    /**
     * \brief Check if the object obj is live: obj is allocated already and
     * not collected yet.
     */
    virtual bool IsLive([[maybe_unused]] const ObjectHeader *obj) = 0;

    /**
     * \brief Check if current allocators' allocation state is valid.
     */
    virtual size_t VerifyAllocatorStatus() = 0;

    using PygoteAllocator = PygoteSpaceAllocator<ObjectAllocConfig>;  // Allocator for pygote space
    PygoteAllocator *GetPygoteSpaceAllocator()
    {
        return pygote_space_allocator_;
    }

    const PygoteAllocator *GetPygoteSpaceAllocator() const
    {
        return pygote_space_allocator_;
    }

    void DisablePygoteAlloc()
    {
        pygote_alloc_enabled_ = false;
    }

    bool IsPygoteAllocEnabled() const
    {
        ASSERT(!pygote_alloc_enabled_ || pygote_space_allocator_ != nullptr);
        return pygote_alloc_enabled_;
    }

    static size_t GetObjectSpaceFreeBytes()
    {
        return PoolManager::GetMmapMemPool()->GetObjectSpaceFreeBytes();
    }

protected:
    /**
     * \brief Add new memory pools to object_allocator and allocate memory in them
     */
    template <typename AllocT>
    inline void *AddPoolsAndAlloc(size_t size, Alignment align, AllocT *object_allocator, size_t pool_size,
                                  SpaceType space_type);

    /**
     * Try to allocate memory for the object and if failed add new memory pools and allocate again
     * @param size - size of the object in bytes
     * @param align - alignment
     * @param object_allocator - allocator for the object
     * @param pool_size - size of a memory pool for specified allocator
     * @param space_type - SpaceType of the object
     * @return pointer to allocated memory or nullptr if failed
     */
    template <typename AllocT>
    inline void *AllocateSafe(size_t size, Alignment align, AllocT *object_allocator, size_t pool_size,
                              SpaceType space_type);

    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    PygoteAllocator *pygote_space_allocator_ = nullptr;
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    bool pygote_alloc_enabled_ = false;

private:
    void Free([[maybe_unused]] void *mem) final
    {
        LOG(FATAL, ALLOC) << "ObjectAllocatorBase shouldn't have Free";
    }
};

/**
 * Template wrapper for single underlying allocator
 * @tparam AllocT
 */
template <typename AllocT, AllocatorPurpose allocatorPurpose>
class AllocatorSingleT final : public Allocator {
public:
    // NOLINTNEXTLINE(readability-magic-numbers)
    explicit AllocatorSingleT(MemStatsType *mem_stats)
        : Allocator(mem_stats, allocatorPurpose, GCCollectMode::GC_NONE), allocator_(mem_stats)
    {
    }
    ~AllocatorSingleT() final = default;
    NO_COPY_SEMANTIC(AllocatorSingleT);
    DEFAULT_NOEXCEPT_MOVE_SEMANTIC(AllocatorSingleT);

    [[nodiscard]] void *Allocate(size_t size, Alignment align, [[maybe_unused]] panda::ManagedThread *thread) final
    {
        return allocator_.Alloc(size, align);
    }

    [[nodiscard]] void *AllocateLocal(size_t size, Alignment align, [[maybe_unused]] panda::ManagedThread *thread) final
    {
        return allocator_.AllocLocal(size, align);
    }

    [[nodiscard]] void *AllocateNonMovable([[maybe_unused]] size_t size, [[maybe_unused]] Alignment align,
                                           [[maybe_unused]] panda::ManagedThread *thread) final
    {
        LOG(FATAL, ALLOC) << "AllocatorSingleT shouldn't have AllocateNonMovable";
        return nullptr;
    }

    void Free(void *mem) final
    {
        allocator_.Free(mem);
    }

    void VisitAndRemoveAllPools(const MemVisitor &mem_visitor) final
    {
        allocator_.VisitAndRemoveAllPools(mem_visitor);
    }

    void VisitAndRemoveFreePools(const MemVisitor &mem_visitor) final
    {
        allocator_.VisitAndRemoveFreePools(mem_visitor);
    }

    void IterateOverObjectsInRange([[maybe_unused]] MemRange mem_range,
                                   [[maybe_unused]] const ObjectVisitor &object_visitor) final
    {
        LOG(FATAL, ALLOC) << "IterateOverObjectsInRange not implemented for AllocatorSinglet";
    }

    void IterateOverObjects([[maybe_unused]] const ObjectVisitor &object_visitor) final
    {
        LOG(FATAL, ALLOC) << "IterateOverObjects not implemented for AllocatorSinglet";
    }

#if defined(TRACK_INTERNAL_ALLOCATIONS)
    void Dump() override
    {
        allocator_.Dump();
    }
#endif

private:
    AllocT allocator_;
};

/**
 * Class is pointer wrapper. It checks if type of allocator matches expected.
 * @tparam allocatorType - type of allocator
 */
template <AllocatorPurpose allocatorPurpose>
class AllocatorPtr {
public:
    AllocatorPtr() = default;
    // NOLINTNEXTLINE(google-explicit-constructor)
    AllocatorPtr(std::nullptr_t a_nullptr) noexcept : allocator_ptr_(a_nullptr) {}

    explicit AllocatorPtr(Allocator *allocator) : allocator_ptr_(allocator) {}

    Allocator *operator->()
    {
        ASSERT((allocator_ptr_ == nullptr) || (allocator_ptr_->GetPurpose() == allocatorPurpose));
        return allocator_ptr_;
    }

    AllocatorPtr &operator=(std::nullptr_t a_nullptr) noexcept
    {
        allocator_ptr_ = a_nullptr;
        return *this;
    }

    AllocatorPtr &operator=(Allocator *allocator)
    {
        allocator_ptr_ = allocator;
        return *this;
    }

    explicit operator Allocator *()
    {
        return allocator_ptr_;
    }

    explicit operator ObjectAllocatorBase *()
    {
        ASSERT(allocator_ptr_->GetPurpose() == AllocatorPurpose::ALLOCATOR_PURPOSE_OBJECT);
        return static_cast<ObjectAllocatorBase *>(allocator_ptr_);
    }

    ALWAYS_INLINE bool operator==(const AllocatorPtr &other)
    {
        return allocator_ptr_ == static_cast<Allocator *>(other);
    }

    ALWAYS_INLINE bool operator==(std::nullptr_t) noexcept
    {
        return allocator_ptr_ == nullptr;
    }

    ALWAYS_INLINE bool operator!=(std::nullptr_t) noexcept
    {
        return allocator_ptr_ != nullptr;
    }

    ObjectAllocatorBase *AsObjectAllocator()
    {
        ASSERT(allocatorPurpose == AllocatorPurpose::ALLOCATOR_PURPOSE_OBJECT);
        return this->operator panda::mem::ObjectAllocatorBase *();
    }

    ~AllocatorPtr() = default;

    DEFAULT_COPY_SEMANTIC(AllocatorPtr);
    DEFAULT_NOEXCEPT_MOVE_SEMANTIC(AllocatorPtr);

protected:
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    Allocator *allocator_ptr_ = nullptr;
};

using InternalAllocatorPtr = AllocatorPtr<AllocatorPurpose::ALLOCATOR_PURPOSE_INTERNAL>;
using ObjectAllocatorPtr = AllocatorPtr<AllocatorPurpose::ALLOCATOR_PURPOSE_OBJECT>;

template <InternalAllocatorConfig Config>
using InternalAllocatorT = AllocatorSingleT<InternalAllocator<Config>, AllocatorPurpose::ALLOCATOR_PURPOSE_INTERNAL>;

template <MTModeT MTMode = MT_MODE_MULTI>
class ObjectAllocatorNoGen final : public ObjectAllocatorBase {
    using ObjectAllocator = RunSlotsAllocator<ObjectAllocConfig>;       // Allocator used for middle size allocations
    using LargeObjectAllocator = FreeListAllocator<ObjectAllocConfig>;  // Allocator used for large objects
    using HumongousObjectAllocator = HumongousObjAllocator<ObjectAllocConfig>;  // Allocator used for humongous objects
    
public:
    NO_MOVE_SEMANTIC(ObjectAllocatorNoGen);
    NO_COPY_SEMANTIC(ObjectAllocatorNoGen);

    explicit ObjectAllocatorNoGen(MemStatsType *mem_stats, bool create_pygote_space_allocator);

    ~ObjectAllocatorNoGen() final;

    [[nodiscard]] void *Allocate(size_t size, Alignment align, [[maybe_unused]] panda::ManagedThread *thread) final;

    [[nodiscard]] void *AllocateNonMovable(size_t size, Alignment align, panda::ManagedThread *thread) final;

    void VisitAndRemoveAllPools(const MemVisitor &mem_visitor) final;

    void VisitAndRemoveFreePools(const MemVisitor &mem_visitor) final;

    void IterateOverObjects(const ObjectVisitor &object_visitor) final;

    /**
     * \brief iterates all objects in object allocator
     */
    void IterateRegularSizeObjects(const ObjectVisitor &object_visitor) final;

    /**
     * \brief iterates objects in all allocators except object allocator
     */
    void IterateNonRegularSizeObjects(const ObjectVisitor &object_visitor) final;

    void FreeObjectsMovedToPygoteSpace() final;

    void Collect(const GCObjectVisitor &gc_object_visitor, GCCollectMode collect_mode) final;

    size_t GetRegularObjectMaxSize() final;

    size_t GetLargeObjectMaxSize() final;

    bool IsAddressInYoungSpace([[maybe_unused]] uintptr_t address) final
    {
        LOG(FATAL, ALLOC) << "ObjectAllocatorNoGen: IsAddressInYoungSpace not applicable";
        return false;
    }

    bool IsObjectInNonMovableSpace([[maybe_unused]] const ObjectHeader *obj) final
    {
        return true;
    }

    bool HasYoungSpace() final
    {
        return false;
    }

    MemRange GetYoungSpaceMemRange() final
    {
        LOG(FATAL, ALLOC) << "ObjectAllocatorNoGen: GetYoungSpaceMemRange not applicable";
        return MemRange(0, 0);
    }

    void ResetYoungAllocator() final
    {
        LOG(FATAL, ALLOC) << "ObjectAllocatorNoGen: ResetYoungAllocator not applicable";
    }

    TLAB *CreateNewTLAB(panda::ManagedThread *thread) final;

    size_t GetTLABMaxAllocSize() final;

    bool IsTLABSupported() final
    {
        return false;
    }

    void IterateOverObjectsInRange([[maybe_unused]] MemRange mem_range,
                                   [[maybe_unused]] const ObjectVisitor &object_visitor) final
    {
        LOG(FATAL, ALLOC) << "ObjectAllocatorNoGen: IterateOverObjectsInRange not implemented";
    }

    bool ContainObject(const ObjectHeader *obj) const final;

    bool IsLive(const ObjectHeader *obj) final;

    size_t VerifyAllocatorStatus() final
    {
        size_t fail_count = 0;
        fail_count += object_allocator_->VerifyAllocator();
        return fail_count;
    }

    [[nodiscard]] void *AllocateLocal(size_t /* size */, Alignment /* align */,
                                      panda::ManagedThread * /* thread */) final
    {
        LOG(FATAL, ALLOC) << "ObjectAllocatorNoGen: AllocateLocal not supported";
        return nullptr;
    }

private:
    ObjectAllocator *object_allocator_ = nullptr;
    LargeObjectAllocator *large_object_allocator_ = nullptr;
    HumongousObjectAllocator *humongous_object_allocator_ = nullptr;
};

// Base class for all generational GCs
class ObjectAllocatorGenBase : public ObjectAllocatorBase {
public:
    explicit ObjectAllocatorGenBase(MemStatsType *mem_stats, GCCollectMode gc_collect_mode,
                                    bool create_pygote_space_allocator)
        : ObjectAllocatorBase(mem_stats, gc_collect_mode, create_pygote_space_allocator)
    {
    }

    ~ObjectAllocatorGenBase() override = default;

    NO_COPY_SEMANTIC(ObjectAllocatorGenBase);
    NO_MOVE_SEMANTIC(ObjectAllocatorGenBase);

protected:
    static constexpr size_t YOUNG_ALLOC_MAX_SIZE = PANDA_TLAB_MAX_ALLOC_SIZE;  // max size of allocation in young space
};

template <MTModeT MTMode = MT_MODE_MULTI>
class ObjectAllocatorGen final : public ObjectAllocatorGenBase {
    static constexpr size_t YOUNG_TLAB_SIZE = 4_KB;  // TLAB size for young gen

    using YoungGenAllocator = BumpPointerAllocator<ObjectAllocConfigWithCrossingMap,
                                                   BumpPointerAllocatorLockConfig::ParameterizedLock<MTMode>, true>;
    using ObjectAllocator =
        RunSlotsAllocator<ObjectAllocConfigWithCrossingMap>;  // Allocator used for middle size allocations
    using LargeObjectAllocator =
        FreeListAllocator<ObjectAllocConfigWithCrossingMap>;  // Allocator used for large objects
    using HumongousObjectAllocator =
        HumongousObjAllocator<ObjectAllocConfigWithCrossingMap>;  // Allocator used for humongous objects

public:
    NO_MOVE_SEMANTIC(ObjectAllocatorGen);
    NO_COPY_SEMANTIC(ObjectAllocatorGen);

    explicit ObjectAllocatorGen(MemStatsType *mem_stats, bool create_pygote_space_allocator);

    ~ObjectAllocatorGen() final;

    void *Allocate(size_t size, Alignment align, [[maybe_unused]] panda::ManagedThread *thread) final;

    void *AllocateNonMovable(size_t size, Alignment align, [[maybe_unused]] panda::ManagedThread *thread) final;

    void VisitAndRemoveAllPools(const MemVisitor &mem_visitor) final;

    void VisitAndRemoveFreePools(const MemVisitor &mem_visitor) final;

    void IterateOverYoungObjects(const ObjectVisitor &object_visitor) final;

    void IterateOverTenuredObjects(const ObjectVisitor &object_visitor) final;

    void IterateOverObjects(const ObjectVisitor &object_visitor) final;

    /**
     * \brief iterates all objects in object allocator
     */
    void IterateRegularSizeObjects(const ObjectVisitor &object_visitor) final;

    /**
     * \brief iterates objects in all allocators except object allocator
     */
    void IterateNonRegularSizeObjects(const ObjectVisitor &object_visitor) final;

    void FreeObjectsMovedToPygoteSpace() final;

    void Collect(const GCObjectVisitor &gc_object_visitor, GCCollectMode collect_mode) final;

    size_t GetRegularObjectMaxSize() final;

    size_t GetLargeObjectMaxSize() final;

    bool IsAddressInYoungSpace(uintptr_t address) final;

    bool IsObjectInNonMovableSpace(const ObjectHeader *obj) final;

    bool HasYoungSpace() final;

    MemRange GetYoungSpaceMemRange() final;

    void ResetYoungAllocator() final;

    TLAB *CreateNewTLAB([[maybe_unused]] panda::ManagedThread *thread) final;

    size_t GetTLABMaxAllocSize() final;

    bool IsTLABSupported() final
    {
        return true;
    }

    void IterateOverObjectsInRange(MemRange mem_range, const ObjectVisitor &object_visitor) final;

    bool ContainObject(const ObjectHeader *obj) const final;

    bool IsLive(const ObjectHeader *obj) final;

    size_t VerifyAllocatorStatus() final
    {
        size_t fail_count = 0;
        fail_count += object_allocator_->VerifyAllocator();
        return fail_count;
    }

    [[nodiscard]] void *AllocateLocal(size_t /* size */, Alignment /* align */,
                                      panda::ManagedThread * /* thread */) final
    {
        LOG(FATAL, ALLOC) << "ObjectAllocatorGen: AllocateLocal not supported";
        return nullptr;
    }

    static constexpr size_t GetYoungAllocMaxSize()
    {
        return YOUNG_ALLOC_MAX_SIZE;
    }

private:
    YoungGenAllocator *young_gen_allocator_ = nullptr;
    ObjectAllocator *object_allocator_ = nullptr;
    LargeObjectAllocator *large_object_allocator_ = nullptr;
    HumongousObjectAllocator *humongous_object_allocator_ = nullptr;
    MemStatsType *mem_stats_ = nullptr;
    ObjectAllocator *non_movable_object_allocator_ = nullptr;
    LargeObjectAllocator *large_non_movable_object_allocator_ = nullptr;

    void *AllocateTenured(size_t size) final;
};

template <GCType gcType, MTModeT MTMode = MT_MODE_MULTI>
class AllocConfig {
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_INCLUDE_MEM_ALLOCATOR_H_
