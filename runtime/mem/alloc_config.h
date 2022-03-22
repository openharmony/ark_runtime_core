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

#ifndef PANDA_RUNTIME_MEM_ALLOC_CONFIG_H_
#define PANDA_RUNTIME_MEM_ALLOC_CONFIG_H_

#include "runtime/arch/memory_helpers.h"
#include "runtime/mem/gc/crossing_map_singleton.h"
#include "libpandabase/mem/mem.h"
#include "runtime/mem/mem_stats_additional_info.h"
#include "runtime/mem/mem_stats_default.h"
#include "libpandabase/utils/tsan_interface.h"

namespace panda::mem {

/**
 * We want to record stats about allocations and free events. Allocators don't care about the type of allocated memory.
 * It could be raw memory for any reason or memory for object in the programming language. If it's a memory for object -
 * we can cast void* to object and get the specific size of this object, otherwise we should trust allocator and
 * can record only approximate size. Because of this we force allocators to use specific config for their needs.
 */

/**
 * Config for objects allocators with Crossing Map support.
 */
class ObjectAllocConfigWithCrossingMap {
public:
    static void OnAlloc(size_t size, SpaceType type_mem, MemStatsType *mem_stats)
    {
        mem_stats->RecordAllocateObject(size, type_mem);
    }

    static void OnFree(size_t size, SpaceType type_mem, MemStatsType *mem_stats)
    {
        mem_stats->RecordFreeObject(size, type_mem);
    }

    /**
     * \brief Initialize an object memory allocated by an allocator.
     */
    static void MemoryInit(void *mem, size_t size)
    {
        // zeroing according to newobj description in ISA
        TSAN_ANNOTATE_IGNORE_WRITES_BEGIN();
        (void)memset_s(mem, size, 0, size);
        TSAN_ANNOTATE_IGNORE_WRITES_END();
        // As per java spec, zero init should be visible from other threads even if pointer to object was fetched
        // without 'volatile' specifier so full memory barrier is required
        arch::FullMemoryBarrier();
    }

    /**
     * \brief Record new allocation of an object and add it to Crossing Map.
     */
    static void AddToCrossingMap(void *obj_addr, size_t obj_size)
    {
        CrossingMapSingleton::AddObject(obj_addr, obj_size);
    }

    /**
     * \brief Record free call of an object and remove it from Crossing Map.
     * @param obj_addr - pointer to the removing object (object header).
     * @param obj_size - size of the removing object.
     * @param next_obj_addr - pointer to the next object (object header). It can be nullptr.
     * @param prev_obj_addr - pointer to the previous object (object header). It can be nullptr.
     * @param prev_obj_size - size of the previous object.
     *        It is used to check if previous object crosses the borders of the current map.
     */
    static void RemoveFromCrossingMap(void *obj_addr, size_t obj_size, void *next_obj_addr,
                                      void *prev_obj_addr = nullptr, size_t prev_obj_size = 0)
    {
        CrossingMapSingleton::RemoveObject(obj_addr, obj_size, next_obj_addr, prev_obj_addr, prev_obj_size);
    }

    /**
     * \brief Find and return the first object, which starts in an interval inclusively
     * or an object, which crosses the interval border.
     * It is essential to check the previous object of the returned object to make sure that
     * we find the first object, which crosses the border of this interval.
     * @param start_addr - pointer to the first byte of the interval.
     * @param end_addr - pointer to the last byte of the interval.
     * @return Returns the first object which starts inside an interval,
     *  or an object which crosses a border of this interval
     *  or nullptr
     */
    static void *FindFirstObjInCrossingMap(void *start_addr, void *end_addr)
    {
        return CrossingMapSingleton::FindFirstObject(start_addr, end_addr);
    }

    /**
     * \brief Initialize a Crossing map for the corresponding memory ranges.
     * @param start_addr - pointer to the first byte of the interval.
     * @param size - size of the interval.
     */
    static void InitializeCrossingMapForMemory(void *start_addr, size_t size)
    {
        return CrossingMapSingleton::InitializeCrossingMapForMemory(start_addr, size);
    }

    /**
     * \brief Remove a Crossing map for the corresponding memory ranges.
     * @param start_addr - pointer to the first byte of the interval.
     * @param size - size of the interval.
     */
    static void RemoveCrossingMapForMemory(void *start_addr, size_t size)
    {
        return CrossingMapSingleton::RemoveCrossingMapForMemory(start_addr, size);
    }
};

/**
 * Config for objects allocators.
 */
class ObjectAllocConfig {
public:
    static void OnAlloc(size_t size, SpaceType type_mem, MemStatsType *mem_stats)
    {
        mem_stats->RecordAllocateObject(size, type_mem);
    }

    static void OnFree(size_t size, SpaceType type_mem, MemStatsType *mem_stats)
    {
        mem_stats->RecordFreeObject(size, type_mem);
    }

    /**
     * \brief Initialize an object memory allocated by an allocator.
     */
    static void MemoryInit(void *mem, size_t size)
    {
        // zeroing according to newobj description in ISA
        TSAN_ANNOTATE_IGNORE_WRITES_BEGIN();
        (void)memset_s(mem, size, 0, size);
        TSAN_ANNOTATE_IGNORE_WRITES_END();
        // As per java spec, zero init should be visible from other threads even if pointer to object was fetched
        // without 'volatile' specifier so full memory barrier is required
        arch::FullMemoryBarrier();
    }

    // We don't use crossing map in this config.
    static void AddToCrossingMap([[maybe_unused]] void *obj_addr, [[maybe_unused]] size_t obj_size) {}

    // We don't use crossing map in this config.
    static void RemoveFromCrossingMap([[maybe_unused]] void *obj_addr, [[maybe_unused]] size_t obj_size,
                                      [[maybe_unused]] void *next_obj_addr = nullptr,
                                      [[maybe_unused]] void *prev_obj_addr = nullptr,
                                      [[maybe_unused]] size_t prev_obj_size = 0)
    {
    }

    // We don't use crossing map in this config.
    static void *FindFirstObjInCrossingMap([[maybe_unused]] void *start_addr, [[maybe_unused]] void *end_addr)
    {
        // We can't call CrossingMap when we don't use it
        ASSERT(start_addr == nullptr);
        return nullptr;
    }

    // We don't use crossing map in this config.
    static void InitializeCrossingMapForMemory([[maybe_unused]] void *start_addr, [[maybe_unused]] size_t size) {}

    // We don't use crossing map in this config.
    static void RemoveCrossingMapForMemory([[maybe_unused]] void *start_addr, [[maybe_unused]] size_t size) {}
};

/**
 * Config for raw memory allocators.
 */
class RawMemoryConfig {
public:
    static void OnAlloc(size_t size, SpaceType type_mem, MemStatsType *mem_stats)
    {
        ASSERT(type_mem == SpaceType::SPACE_TYPE_INTERNAL);
        mem_stats->RecordAllocateRaw(size, type_mem);
    }

    static void OnFree(size_t size, SpaceType type_mem, MemStatsType *mem_stats)
    {
        ASSERT(type_mem == SpaceType::SPACE_TYPE_INTERNAL);
        mem_stats->RecordFreeRaw(size, type_mem);
    }

    /**
     * \brief We don't need it for raw memory.
     */
    static void MemoryInit([[maybe_unused]] void *mem, [[maybe_unused]] size_t size) {}

    // We don't use crossing map for raw memory allocations.
    static void AddToCrossingMap([[maybe_unused]] void *obj_addr, [[maybe_unused]] size_t obj_size) {}

    // We don't use crossing map for raw memory allocations.
    static void RemoveFromCrossingMap([[maybe_unused]] void *obj_addr, [[maybe_unused]] size_t obj_size,
                                      [[maybe_unused]] void *next_obj_addr = nullptr,
                                      [[maybe_unused]] void *prev_obj_addr = nullptr,
                                      [[maybe_unused]] size_t prev_obj_size = 0)
    {
    }

    // We don't use crossing map for raw memory allocations.
    static void *FindFirstObjInCrossingMap([[maybe_unused]] void *start_addr, [[maybe_unused]] void *end_addr)
    {
        // We can't call CrossingMap when we don't use it
        ASSERT(start_addr == nullptr);
        return nullptr;
    }

    // We don't use crossing map for raw memory allocations.
    static void InitializeCrossingMapForMemory([[maybe_unused]] void *start_addr, [[maybe_unused]] size_t size) {}

    // We don't use crossing map for raw memory allocations.
    static void RemoveCrossingMapForMemory([[maybe_unused]] void *start_addr, [[maybe_unused]] size_t size) {}
};

/**
 * Debug config with empty MemStats calls and with Crossing Map support.
 */
class EmptyAllocConfigWithCrossingMap {
public:
    static void OnAlloc([[maybe_unused]] size_t size, [[maybe_unused]] SpaceType type_mem,
                        [[maybe_unused]] MemStatsType *mem_stats)
    {
    }

    static void OnFree([[maybe_unused]] size_t size, [[maybe_unused]] SpaceType type_mem,
                       [[maybe_unused]] MemStatsType *mem_stats)
    {
    }

    /**
     * \brief Initialize memory for correct test execution.
     */
    static void MemoryInit(void *mem, size_t size)
    {
        TSAN_ANNOTATE_IGNORE_WRITES_BEGIN();
        (void)memset_s(mem, size, 0, size);
        TSAN_ANNOTATE_IGNORE_WRITES_END();
        // As per java spec, zero init should be visible from other threads even if pointer to object was fetched
        // without 'volatile' specifier so full memory barrier is required
        arch::FullMemoryBarrier();
    }

    /**
     * \brief Record new allocation of an object and add it to Crossing Map.
     */
    static void AddToCrossingMap(void *obj_addr, size_t obj_size)
    {
        CrossingMapSingleton::AddObject(obj_addr, obj_size);
    }

    /**
     * \brief Record free call of an object and remove it from Crossing Map.
     * @param obj_addr - pointer to the removing object (object header).
     * @param obj_size - size of the removing object.
     * @param next_obj_addr - pointer to the next object (object header). It can be nullptr.
     * @param prev_obj_addr - pointer to the previous object (object header). It can be nullptr.
     * @param prev_obj_size - size of the previous object.
     *        It is used check if previous object crosses the borders of the current map.
     */
    static void RemoveFromCrossingMap(void *obj_addr, size_t obj_size, void *next_obj_addr,
                                      void *prev_obj_addr = nullptr, size_t prev_obj_size = 0)
    {
        CrossingMapSingleton::RemoveObject(obj_addr, obj_size, next_obj_addr, prev_obj_addr, prev_obj_size);
    }

    /**
     * \brief Find and return the first object, which starts in an interval inclusively
     * or an object, which crosses the interval border.
     * It is essential to check the previous object of the returned object to make sure that
     * we find the first object, which crosses the border of this interval.
     * @param start_addr - pointer to the first byte of the interval.
     * @param end_addr - pointer to the last byte of the interval.
     * @return Returns the first object which starts inside an interval,
     *  or an object which crosses a border of this interval
     *  or nullptr
     */
    static void *FindFirstObjInCrossingMap(void *start_addr, void *end_addr)
    {
        return CrossingMapSingleton::FindFirstObject(start_addr, end_addr);
    }

    /**
     * \brief Initialize a Crossing map for the corresponding memory ranges.
     * @param start_addr - pointer to the first byte of the interval.
     * @param size - size of the interval.
     */
    static void InitializeCrossingMapForMemory(void *start_addr, size_t size)
    {
        return CrossingMapSingleton::InitializeCrossingMapForMemory(start_addr, size);
    }

    /**
     * \brief Remove a Crossing map for the corresponding memory ranges.
     * @param start_addr - pointer to the first byte of the interval.
     * @param size - size of the interval.
     */
    static void RemoveCrossingMapForMemory(void *start_addr, size_t size)
    {
        return CrossingMapSingleton::RemoveCrossingMapForMemory(start_addr, size);
    }
};

/*
 * Config for disuse of stats for memory allocators
 */
class EmptyMemoryConfig {
public:
    ALWAYS_INLINE static void OnAlloc([[maybe_unused]] size_t size, [[maybe_unused]] SpaceType type_mem,
                                      [[maybe_unused]] MemStatsType *mem_stats)
    {
    }
    ALWAYS_INLINE static void OnFree([[maybe_unused]] size_t size, [[maybe_unused]] SpaceType type_mem,
                                     [[maybe_unused]] MemStatsType *mem_stats)
    {
    }
    ALWAYS_INLINE static void MemoryInit([[maybe_unused]] void *mem, [[maybe_unused]] size_t size) {}
    ALWAYS_INLINE static void AddToCrossingMap([[maybe_unused]] void *obj_addr, [[maybe_unused]] size_t obj_size) {}
    ALWAYS_INLINE static void RemoveFromCrossingMap([[maybe_unused]] void *obj_addr, [[maybe_unused]] size_t obj_size,
                                                    [[maybe_unused]] void *next_obj_addr = nullptr,
                                                    [[maybe_unused]] void *prev_obj_addr = nullptr,
                                                    [[maybe_unused]] size_t prev_obj_size = 0)
    {
    }

    ALWAYS_INLINE static void *FindFirstObjInCrossingMap([[maybe_unused]] void *start_addr,
                                                         [[maybe_unused]] void *end_addr)
    {
        // We can't call CrossingMap when we don't use it
        ASSERT(start_addr == nullptr);
        return nullptr;
    }

    static void InitializeCrossingMapForMemory([[maybe_unused]] void *start_addr, [[maybe_unused]] size_t size) {}

    static void RemoveCrossingMapForMemory([[maybe_unused]] void *start_addr, [[maybe_unused]] size_t size) {}
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_ALLOC_CONFIG_H_
