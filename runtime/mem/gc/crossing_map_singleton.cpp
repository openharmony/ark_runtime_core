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

#include "runtime/mem/gc/crossing_map_singleton.h"

#include "libpandabase/utils/logger.h"
#include "runtime/include/runtime.h"
#include "runtime/mem/gc/crossing_map.h"

namespace panda::mem {

CrossingMap *CrossingMapSingleton ::instance = nullptr;
os::memory::Mutex CrossingMapSingleton::mutex;  // NOLINT(fuchsia-statically-constructed-objects)

bool CrossingMapSingleton::Create()
{
    {
        os::memory::LockHolder<os::memory::Mutex> lock(mutex);

        InternalAllocatorPtr allocator {InternalAllocator<>::GetInternalAllocatorFromRuntime()};
        if (instance != nullptr) {
            LOG(FATAL, RUNTIME) << "CrossingMap is created already";
            return false;
        }
        instance = allocator->New<CrossingMap>(allocator, PoolManager::GetMmapMemPool()->GetMinObjectAddress(),
                                               PoolManager::GetMmapMemPool()->GetTotalObjectSize());
        instance->Initialize();
    }
    return true;
}

/* static */
bool CrossingMapSingleton::IsCreated()
{
    return instance != nullptr;
}

/* static */
bool CrossingMapSingleton::Destroy()
{
    CrossingMap *temp_instance;
    {
        os::memory::LockHolder<os::memory::Mutex> lock(mutex);

        if (instance == nullptr) {
            return false;
        }
        temp_instance = instance;
        instance = nullptr;
    }
    temp_instance->Destroy();
    InternalAllocatorPtr allocator {InternalAllocator<>::GetInternalAllocatorFromRuntime()};
    allocator->Delete(temp_instance);
    return true;
}

/* static */
void CrossingMapSingleton::AddObject(void *obj_addr, size_t obj_size)
{
    GetCrossingMap()->AddObject(obj_addr, obj_size);
}

/* static */
void CrossingMapSingleton::RemoveObject(void *obj_addr, size_t obj_size, void *next_obj_addr, void *prev_obj_addr,
                                        size_t prev_obj_size)
{
    GetCrossingMap()->RemoveObject(obj_addr, obj_size, next_obj_addr, prev_obj_addr, prev_obj_size);
}

/* static */
void *CrossingMapSingleton::FindFirstObject(void *start_addr, void *end_addr)
{
    return GetCrossingMap()->FindFirstObject(start_addr, end_addr);
}

/* static */
void CrossingMapSingleton::InitializeCrossingMapForMemory(void *start_addr, size_t size)
{
    return GetCrossingMap()->InitializeCrossingMapForMemory(start_addr, size);
}

/* static */
void CrossingMapSingleton::RemoveCrossingMapForMemory(void *start_addr, size_t size)
{
    return GetCrossingMap()->RemoveCrossingMapForMemory(start_addr, size);
}

/* static */
size_t CrossingMapSingleton::GetCrossingMapGranularity()
{
    return PANDA_CROSSING_MAP_GRANULARITY;
}

}  // namespace panda::mem
