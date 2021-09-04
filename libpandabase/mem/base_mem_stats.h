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

#ifndef PANDA_LIBPANDABASE_MEM_BASE_MEM_STATS_H_
#define PANDA_LIBPANDABASE_MEM_BASE_MEM_STATS_H_

#include "os/mutex.h"
#include <cstdio>
#include "macros.h"
#include "space.h"

#include <array>
#include <atomic>
namespace panda {

class BaseMemStats {
public:
    NO_COPY_SEMANTIC(BaseMemStats);
    NO_MOVE_SEMANTIC(BaseMemStats);

    BaseMemStats() = default;
    virtual ~BaseMemStats() = default;

    void RecordAllocateRaw(size_t size, SpaceType type_mem);

    void RecordFreeRaw(size_t size, SpaceType type_mem);

    // getters
    [[nodiscard]] uint64_t GetAllocated(SpaceType type_mem) const;
    [[nodiscard]] uint64_t GetFreed(SpaceType type_mem) const;
    [[nodiscard]] uint64_t GetFootprint(SpaceType type_mem) const;

    [[nodiscard]] uint64_t GetAllocatedHeap() const;
    [[nodiscard]] uint64_t GetFreedHeap() const;
    [[nodiscard]] uint64_t GetFootprintHeap() const;
    [[nodiscard]] uint64_t GetTotalFootprint() const;

protected:
    void RecordAllocate(size_t size, SpaceType type_mem);
    void RecordMoved(size_t size, SpaceType type_mem);
    void RecordFree(size_t size, SpaceType type_mem);

private:
    std::array<std::atomic_uint64_t, SPACE_TYPE_SIZE> allocated_ {0};
    std::array<std::atomic_uint64_t, SPACE_TYPE_SIZE> freed_ {0};
};

}  // namespace panda

#endif  // PANDA_LIBPANDABASE_MEM_BASE_MEM_STATS_H_
