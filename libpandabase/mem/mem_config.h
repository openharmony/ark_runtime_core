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

#ifndef PANDA_LIBPANDABASE_MEM_MEM_CONFIG_H_
#define PANDA_LIBPANDABASE_MEM_MEM_CONFIG_H_

#include "macros.h"
#include "mem/mem.h"
#include "utils/asan_interface.h"

#include <cstddef>

namespace panda::mem {

/**
 * class for global memory parameters
 */
class MemConfig {
public:
    static void Initialize(size_t object_pool_size, size_t internal_size, size_t compiler_size, size_t code_size)
    {
        ASSERT(!is_initialized);
        heap_pool_size = object_pool_size;
        internal_pool_size = internal_size;
        compiler_pool_size = compiler_size;
        code_pool_size = code_size;
        is_initialized = true;
    }

    static void Finalize()
    {
        is_initialized = false;
        heap_pool_size = 0;
        internal_pool_size = 0;
        code_pool_size = 0;
    }

    static size_t GetObjectPoolSize()
    {
        ASSERT(is_initialized);
        return heap_pool_size;
    }

    static size_t GetInternalPoolSize()
    {
        ASSERT(is_initialized);
        return internal_pool_size;
    }

    static size_t GetCodePoolSize()
    {
        ASSERT(is_initialized);
        return code_pool_size;
    }

    static size_t GetCompilerPoolSize()
    {
        ASSERT(is_initialized);
        return compiler_pool_size;
    }

    MemConfig() = delete;

    ~MemConfig() = delete;

    NO_COPY_SEMANTIC(MemConfig);
    NO_MOVE_SEMANTIC(MemConfig);

private:
    static bool is_initialized;
    static size_t heap_pool_size;      // Pool size used for object storage
    static size_t internal_pool_size;  // Pool size used for internal storage
    static size_t code_pool_size;      // Pool size used for compiled code storage
    static size_t compiler_pool_size;  // Pool size used for internal compiler storage
};

}  // namespace panda::mem

#endif  // PANDA_LIBPANDABASE_MEM_MEM_CONFIG_H_
