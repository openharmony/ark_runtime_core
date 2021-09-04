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

#include "runtime/mem/gc/crossing_map.h"

#include <cstring>

namespace panda::mem {

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_CROSSING_MAP(level) LOG(level, GC) << "CrossingMap: "

CrossingMap::CrossingMap(InternalAllocatorPtr internal_allocator, uintptr_t start_addr, size_t size)
    : start_addr_(start_addr), internal_allocator_(internal_allocator)
{
    ASSERT(size % CROSSING_MAP_GRANULARITY == 0);
    map_elements_count_ = size / CROSSING_MAP_GRANULARITY;
    static_array_elements_count_ =
        AlignUp(size, CROSSING_MAP_STATIC_ARRAY_GRANULARITY) / CROSSING_MAP_STATIC_ARRAY_GRANULARITY;
    ASSERT((start_addr & (PAGE_SIZE - 1)) == 0U);
    LOG_CROSSING_MAP(DEBUG) << "Create CrossingMap with start_addr 0x" << std::hex << start_addr_;
}

CrossingMap::~CrossingMap()
{
    ASSERT(static_array_ == nullptr);
}

void CrossingMap::Initialize()
{
    if (static_array_ != nullptr) {
        LOG_CROSSING_MAP(FATAL) << "Try to initialize already initialized CrossingMap";
    }
    size_t static_array_size_in_bytes = static_array_elements_count_ * sizeof(StaticArrayPtr);
    static_array_ = static_cast<StaticArrayPtr>(internal_allocator_->Alloc(static_array_size_in_bytes));
    ASSERT(static_array_ != nullptr);
    for (size_t i = 0; i < static_array_elements_count_; i++) {
        SetStaticArrayElement(i, nullptr);
    }
}

void CrossingMap::Destroy()
{
    for (size_t i = 0; i < static_array_elements_count_; i++) {
        void *element = GetStaticArrayElement(i);
        if (element != nullptr) {
            internal_allocator_->Free(element);
        }
    }
    ASSERT(static_array_ != nullptr);
    internal_allocator_->Free(static_array_);
    static_array_ = nullptr;
}

void CrossingMap::AddObject(const void *obj_addr, size_t obj_size)
{
    LOG_CROSSING_MAP(DEBUG) << "Try to AddObject with addr " << std::hex << obj_addr << " and size " << std::dec
                            << obj_size;
    size_t first_map_num = GetMapNumFromAddr(obj_addr);
    size_t obj_offset = GetOffsetFromAddr(obj_addr);
    CrossingMapElement::STATE state = GetMapElement(first_map_num)->GetState();
    switch (state) {
        case CrossingMapElement::STATE::STATE_UNINITIALIZED:
            LOG_CROSSING_MAP(DEBUG) << "AddObject - state of the map num " << first_map_num
                                    << " wasn't INITIALIZED. Initialize it with offset " << obj_offset;
            GetMapElement(first_map_num)->SetInitialized(obj_offset);
            break;
        case CrossingMapElement::STATE::STATE_CROSSED_BORDER:
            LOG_CROSSING_MAP(DEBUG) << "AddObject - state of the map num " << first_map_num
                                    << " was CROSSED BORDER. Initialize it with offset " << obj_offset;
            GetMapElement(first_map_num)->SetInitializedAndCrossedBorder(obj_offset);
            break;
        case CrossingMapElement::STATE::STATE_INITIALIZED_AND_CROSSED_BORDERS:
            if (GetMapElement(first_map_num)->GetOffset() > obj_offset) {
                LOG_CROSSING_MAP(DEBUG) << "AddObject - state of the map num " << first_map_num
                                        << " is INITIALIZED and CROSSED BORDERS, but this object is the first in it."
                                        << " Initialize it with new offset " << obj_offset;
                GetMapElement(first_map_num)->SetInitializedAndCrossedBorder(obj_offset);
            }
            break;
        case CrossingMapElement::STATE::STATE_INITIALIZED:
            if (GetMapElement(first_map_num)->GetOffset() > obj_offset) {
                LOG_CROSSING_MAP(DEBUG) << "AddObject - state of the map num " << first_map_num
                                        << " is INITIALIZED, but this object is the first in it."
                                        << " Initialize it with new offset " << obj_offset;
                GetMapElement(first_map_num)->SetInitialized(obj_offset);
            }
            break;
        default:
            LOG_CROSSING_MAP(FATAL) << "Unknown state!";
    }
    void *last_obj_byte = ToVoidPtr(ToUintPtr(obj_addr) + obj_size - 1U);
    size_t final_map_num = GetMapNumFromAddr(last_obj_byte);
    if (CROSSING_MAP_MANAGE_CROSSED_BORDER && (final_map_num != first_map_num)) {
        UpdateCrossedBorderOnAdding(first_map_num + 1U, final_map_num);
    }
}

void CrossingMap::UpdateCrossedBorderOnAdding(const size_t first_crossed_border_map,
                                              const size_t last_crossed_border_map)
{
    ASSERT(last_crossed_border_map >= first_crossed_border_map);
    // Iterate over maps which are fully covered by this object
    // i.e. from second to last minus one map
    size_t map_offset = 1U;
    for (size_t i = first_crossed_border_map; i + 1U <= last_crossed_border_map; i++) {
        LOG_CROSSING_MAP(DEBUG) << "AddObject - set CROSSED_BORDER to map num " << i << " with offset " << map_offset;
        GetMapElement(i)->SetCrossedBorder(map_offset);
        // If map_offset exceeds the limit, we will set max value to each map after that.
        // When we want to find the element, which crosses the borders of a map,
        // we will iterate before we meet a map with non-CROSSED_BORDER state.
        if (map_offset < CrossingMapElement::GetMaxOffsetValue()) {
            map_offset++;
        }
    }
    // Set up last map:
    switch (GetMapElement(last_crossed_border_map)->GetState()) {
        case CrossingMapElement::STATE::STATE_UNINITIALIZED:
            GetMapElement(last_crossed_border_map)->SetCrossedBorder(map_offset);
            break;
        case CrossingMapElement::STATE::STATE_INITIALIZED:
            GetMapElement(last_crossed_border_map)
                ->SetInitializedAndCrossedBorder(GetMapElement(last_crossed_border_map)->GetOffset());
            break;
        default:
            LOG_CROSSING_MAP(FATAL) << "Unknown state!";
    }
    LOG_CROSSING_MAP(DEBUG) << "AddObject - set CROSSED_BORDER or INITIALIZED_AND_CROSSED_BORDERS to final map num "
                            << last_crossed_border_map << " with offset " << map_offset;
}

void CrossingMap::RemoveObject(const void *obj_addr, size_t obj_size, const void *next_obj_addr,
                               const void *prev_obj_addr, size_t prev_obj_size)
{
    LOG_CROSSING_MAP(DEBUG) << "Try to RemoveObject with addr " << std::hex << obj_addr << " and size " << std::dec
                            << obj_size;
    ASSERT(obj_addr != nullptr);
    // Let's set all maps, which are related to this object, as uninitialized
    size_t first_map_num = GetMapNumFromAddr(obj_addr);
    size_t obj_offset = GetOffsetFromAddr(obj_addr);
    ASSERT(GetMapElement(first_map_num)->GetState() == CrossingMapElement::STATE::STATE_INITIALIZED ||
           GetMapElement(first_map_num)->GetState() ==
               CrossingMapElement::STATE::STATE_INITIALIZED_AND_CROSSED_BORDERS);
    // We need to check that first object in this map is a pointer to this object
    size_t map_offset = GetMapElement(first_map_num)->GetOffset();
    ASSERT(map_offset <= obj_offset);
    if (map_offset == obj_offset) {
        LOG_CROSSING_MAP(DEBUG) << "RemoveObject - it is the first object in map num " << first_map_num
                                << ". So, just uninitialize it.";
        GetMapElement(first_map_num)->SetUninitialized();
    }

    if (CROSSING_MAP_MANAGE_CROSSED_BORDER) {
        void *last_obj_byte = ToVoidPtr(ToUintPtr(obj_addr) + obj_size - 1U);
        size_t final_map_num = GetMapNumFromAddr(last_obj_byte);
        ASSERT(final_map_num >= first_map_num);
        // Set all pages, which fully covered by this object, as Uninitialized;
        // and for last map (we will set it up correctly later)
        for (size_t i = first_map_num + 1U; i <= final_map_num; i++) {
            LOG_CROSSING_MAP(DEBUG) << "RemoveObject - Set uninitialized to map num " << i;
            GetMapElement(i)->SetUninitialized();
        }
    }

    // Set up map for next element (because we could set it as uninitialized)
    if (next_obj_addr != nullptr) {
        size_t next_obj_map_num = GetMapNumFromAddr(next_obj_addr);
        if (GetMapElement(next_obj_map_num)->GetState() == CrossingMapElement::STATE::STATE_UNINITIALIZED) {
            LOG_CROSSING_MAP(DEBUG) << "RemoveObject - Set up map " << next_obj_map_num << " for next object with addr "
                                    << std::hex << next_obj_addr << " as INITIALIZED with offset " << std::dec
                                    << GetOffsetFromAddr(next_obj_addr);
            GetMapElement(next_obj_map_num)->SetInitialized(GetOffsetFromAddr(next_obj_addr));
        }
    }
    // Set up map for last byte of prev element (because it can cross the page borders)
    if (CROSSING_MAP_MANAGE_CROSSED_BORDER && (prev_obj_addr != nullptr)) {
        void *prev_obj_last_byte = ToVoidPtr(ToUintPtr(prev_obj_addr) + prev_obj_size - 1U);
        size_t prev_obj_last_map = GetMapNumFromAddr(prev_obj_last_byte);
        size_t prev_obj_first_map = GetMapNumFromAddr(prev_obj_addr);
        if ((prev_obj_last_map == first_map_num) && (prev_obj_first_map != first_map_num)) {
            UpdateCrossedBorderOnRemoving(prev_obj_last_map);
        }
    }
}

void CrossingMap::UpdateCrossedBorderOnRemoving(const size_t crossed_border_map)
{
    switch (GetMapElement(crossed_border_map)->GetState()) {
        case CrossingMapElement::STATE::STATE_UNINITIALIZED: {
            // This situation can only happen when removed object was the first object in a corresponding page map
            // and next_obj_addr is not located in the same page map.
            ASSERT(crossed_border_map > 0);
            // Calculate offset for crossed border map
            size_t offset = GetMapElement(crossed_border_map - 1U)->GetOffset();
            CrossingMapElement::STATE prev_map_state = GetMapElement(crossed_border_map - 1U)->GetState();
            switch (prev_map_state) {
                case CrossingMapElement::STATE::STATE_INITIALIZED:
                case CrossingMapElement::STATE::STATE_INITIALIZED_AND_CROSSED_BORDERS:
                    offset = 1U;
                    break;
                case CrossingMapElement::STATE::STATE_CROSSED_BORDER:
                    if (offset < CrossingMapElement::GetMaxOffsetValue()) {
                        offset++;
                    }
                    break;
                default:
                    LOG_CROSSING_MAP(FATAL) << "Incorrect state!";
            }
            GetMapElement(crossed_border_map)->SetCrossedBorder(offset);
            break;
        }
        case CrossingMapElement::STATE::STATE_INITIALIZED: {
            GetMapElement(crossed_border_map)
                ->SetInitializedAndCrossedBorder(GetMapElement(crossed_border_map)->GetOffset());
            break;
        }
        default:
            LOG_CROSSING_MAP(FATAL) << "Incorrect state!";
    }
}

void *CrossingMap::FindFirstObject(const void *start_addr, const void *end_addr)
{
    LOG_CROSSING_MAP(DEBUG) << "FindFirstObject for interval [" << std::hex << start_addr << ", " << end_addr << "]";
    size_t first_map = GetMapNumFromAddr(start_addr);
    size_t last_map = GetMapNumFromAddr(end_addr);
    LOG_CROSSING_MAP(DEBUG) << "FindFirstObject for maps [" << std::dec << first_map << ", " << last_map << "]";
    for (size_t i = first_map; i <= last_map; i++) {
        void *obj_offset = FindObjInMap(i);
        if (obj_offset != nullptr) {
            LOG_CROSSING_MAP(DEBUG) << "Found first object in this interval with addr " << std::hex << obj_offset;
            return obj_offset;
        }
    }
    LOG_CROSSING_MAP(DEBUG) << "There is no object in this interval, return nullptr";
    return nullptr;
}

void CrossingMap::InitializeCrossingMapForMemory(const void *start_addr, size_t size)
{
    LOG_CROSSING_MAP(DEBUG) << "InitializeCrossingMapForMemory for addr " << std::hex << start_addr << " with size "
                            << size;
    size_t start_map = GetStaticArrayNumFromAddr(start_addr);
    size_t end_map = GetStaticArrayNumFromAddr(ToVoidPtr(ToUintPtr(start_addr) + size - 1));
    ASSERT(start_map <= end_map);
    size_t static_map_size_in_bytes = CROSSING_MAP_COUNT_IN_STATIC_ARRAY_ELEMENT * sizeof(CROSSING_MAP_TYPE);
    for (size_t i = start_map; i <= end_map; i++) {
        ASSERT(GetStaticArrayElement(i) == nullptr);
        void *mem = internal_allocator_->Alloc(static_map_size_in_bytes);
        (void)memset_s(mem, static_map_size_in_bytes, 0x0, static_map_size_in_bytes);
        SetStaticArrayElement(i, static_cast<CrossingMapElement *>(mem));
        ASSERT(GetStaticArrayElement(i) != nullptr);
    }
}

void CrossingMap::RemoveCrossingMapForMemory(const void *start_addr, size_t size)
{
    LOG_CROSSING_MAP(DEBUG) << "RemoveCrossingMapForMemory for addr " << std::hex << start_addr << " with size "
                            << size;
    size_t start_map = GetStaticArrayNumFromAddr(start_addr);
    size_t end_map = GetStaticArrayNumFromAddr(ToVoidPtr(ToUintPtr(start_addr) + size - 1));
    ASSERT(start_map <= end_map);
    for (size_t i = start_map; i <= end_map; i++) {
        ASSERT(GetStaticArrayElement(i) != nullptr);
        internal_allocator_->Free(GetStaticArrayElement(i));
        SetStaticArrayElement(i, nullptr);
    }
}

void *CrossingMap::FindObjInMap(size_t map_num)
{
    LOG_CROSSING_MAP(DEBUG) << "Try to find object for map_num - " << map_num;
    CrossingMapElement::STATE state = GetMapElement(map_num)->GetState();
    switch (state) {
        case CrossingMapElement::STATE::STATE_UNINITIALIZED:
            LOG_CROSSING_MAP(DEBUG) << "STATE_UNINITIALIZED, return nullptr";
            return nullptr;
        case CrossingMapElement::STATE::STATE_INITIALIZED:
            LOG_CROSSING_MAP(DEBUG) << "STATE_INITIALIZED, obj addr = " << std::hex
                                    << GetAddrFromOffset(map_num, GetMapElement(map_num)->GetOffset());
            return GetAddrFromOffset(map_num, GetMapElement(map_num)->GetOffset());
        case CrossingMapElement::STATE::STATE_INITIALIZED_AND_CROSSED_BORDERS: {
            LOG_CROSSING_MAP(DEBUG)
                << "STATE_INITIALIZED_AND_CROSSED_BORDERS, try to find object which crosses the borders";
            ASSERT(map_num > 0);
            size_t current_map = map_num - 1;
            while (GetMapElement(current_map)->GetState() == CrossingMapElement::STATE::STATE_CROSSED_BORDER) {
                ASSERT(current_map >= GetMapElement(current_map)->GetOffset());
                current_map = current_map - GetMapElement(current_map)->GetOffset();
            }
            ASSERT(GetMapElement(current_map)->GetState() != CrossingMapElement::STATE::STATE_UNINITIALIZED);
            LOG_CROSSING_MAP(DEBUG) << "Found object in map " << current_map << " with object addr = " << std::hex
                                    << GetAddrFromOffset(current_map, GetMapElement(current_map)->GetOffset());
            return GetAddrFromOffset(current_map, GetMapElement(current_map)->GetOffset());
        }
        case CrossingMapElement::STATE::STATE_CROSSED_BORDER: {
            LOG_CROSSING_MAP(DEBUG) << "STATE_CROSSED_BORDER, try to find object which crosses the borders";
            ASSERT(map_num >= GetMapElement(map_num)->GetOffset());
            size_t current_map = map_num - GetMapElement(map_num)->GetOffset();
            while (GetMapElement(current_map)->GetState() == CrossingMapElement::STATE::STATE_CROSSED_BORDER) {
                ASSERT(current_map >= GetMapElement(current_map)->GetOffset());
                current_map = current_map - GetMapElement(current_map)->GetOffset();
            }
            ASSERT(GetMapElement(current_map)->GetState() != CrossingMapElement::STATE::STATE_UNINITIALIZED);
            LOG_CROSSING_MAP(DEBUG) << "Found object in map " << current_map << " with object addr = " << std::hex
                                    << GetAddrFromOffset(current_map, GetMapElement(current_map)->GetOffset());
            return GetAddrFromOffset(current_map, GetMapElement(current_map)->GetOffset());
        }
        default:
            LOG_CROSSING_MAP(ERROR) << "Undefined map state";
            return nullptr;
    }
}

}  // namespace panda::mem
