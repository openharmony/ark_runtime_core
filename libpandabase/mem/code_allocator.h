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

#ifndef PANDA_LIBPANDABASE_MEM_CODE_ALLOCATOR_H_
#define PANDA_LIBPANDABASE_MEM_CODE_ALLOCATOR_H_

#include "utils/arena_containers.h"
#include "os/mem.h"
#include "os/mutex.h"

namespace panda {

class BaseMemStats;

class CodeAllocator {
public:
    explicit CodeAllocator(BaseMemStats *mem_stats);
    ~CodeAllocator();
    NO_COPY_SEMANTIC(CodeAllocator);
    NO_MOVE_SEMANTIC(CodeAllocator);

    /**
     * \brief Allocates \param size bytes, copies \param codeBuff to allocated memory and makes this memory executable
     * @param size
     * @param codeBuff
     * @return
     */
    [[nodiscard]] void *AllocateCode(size_t size, const void *code_buff);

    /**
     * \brief Allocates \param size bytes of non-protected memory
     * @param size to be allocated
     * @return MapRange of the allocated code
     */
    [[nodiscard]] os::mem::MapRange<std::byte> AllocateCodeUnprotected(size_t size);

    /**
     * Make memory \mem_range executable
     * @param mem_range
     */
    static void ProtectCode(os::mem::MapRange<std::byte> mem_range);

    /**
     * Fast checks if the given program counter belongs to JIT code
     */
    bool InAllocatedCodeRange(const void *pc);

private:
    void CodeRangeUpdate(void *ptr, size_t size);

private:
    static const Alignment PAGE_LOG_ALIGN;

    ArenaAllocator arenaAllocator_;
    BaseMemStats *memStats_;
    os::memory::RWLock code_range_lock_;
    void *codeRangeStart_;
    void *codeRangeEnd_;
};

}  // namespace panda

#endif  // PANDA_LIBPANDABASE_MEM_CODE_ALLOCATOR_H_
