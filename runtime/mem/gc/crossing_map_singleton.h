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

#ifndef PANDA_RUNTIME_MEM_GC_CROSSING_MAP_SINGLETON_H_
#define PANDA_RUNTIME_MEM_GC_CROSSING_MAP_SINGLETON_H_

#include <cstdlib>

#include "libpandabase/macros.h"
#include "libpandabase/os/mutex.h"

namespace panda::mem {

class CrossingMap;
/**
 * Singleton for CrossingMap class.
 */
class CrossingMapSingleton {
public:
    CrossingMapSingleton() = delete;
    ~CrossingMapSingleton() = delete;
    NO_COPY_SEMANTIC(CrossingMapSingleton);
    NO_MOVE_SEMANTIC(CrossingMapSingleton);

    static bool Create();
    static bool IsCreated();

    static CrossingMap *GetCrossingMap()
    {
        ASSERT(instance != nullptr);
        return instance;
    }

    static void AddObject(void *obj_addr, size_t obj_size);
    static void RemoveObject(void *obj_addr, size_t obj_size, void *next_obj_addr, void *prev_obj_addr,
                             size_t prev_obj_size);
    static void *FindFirstObject(void *start_addr, void *end_addr);

    static void InitializeCrossingMapForMemory(void *start_addr, size_t size);
    static void RemoveCrossingMapForMemory(void *start_addr, size_t size);

    static bool Destroy();

    static size_t GetCrossingMapGranularity();

private:
    static CrossingMap *instance;
    static os::memory::Mutex mutex;
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_GC_CROSSING_MAP_SINGLETON_H_
