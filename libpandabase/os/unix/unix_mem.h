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

#ifndef PANDA_LIBPANDABASE_OS_UNIX_UNIX_MEM_H_
#define PANDA_LIBPANDABASE_OS_UNIX_UNIX_MEM_H_

#include <sys/mman.h>

namespace panda::os::mem {

static constexpr uint32_t MMAP_PROT_NONE = PROT_NONE;
static constexpr uint32_t MMAP_PROT_READ = PROT_READ;
static constexpr uint32_t MMAP_PROT_WRITE = PROT_WRITE;
static constexpr uint32_t MMAP_PROT_EXEC = PROT_EXEC;

static constexpr uint32_t MMAP_FLAG_SHARED = MAP_SHARED;
static constexpr uint32_t MMAP_FLAG_PRIVATE = MAP_PRIVATE;
static constexpr uint32_t MMAP_FLAG_FIXED = MAP_FIXED;
static constexpr uint32_t MMAP_FLAG_ANONYMOUS = MAP_ANONYMOUS;

}  // namespace panda::os::mem

#endif  // PANDA_LIBPANDABASE_OS_UNIX_UNIX_MEM_H_
