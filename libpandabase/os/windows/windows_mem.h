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

#ifndef PANDA_LIBPANDABASE_OS_WINDOWS_WINDOWS_MEM_H_
#define PANDA_LIBPANDABASE_OS_WINDOWS_WINDOWS_MEM_H_

namespace panda::os::mem {

static constexpr uint32_t MMAP_PROT_NONE = 0;
static constexpr uint32_t MMAP_PROT_READ = 1;
static constexpr uint32_t MMAP_PROT_WRITE = 2;
static constexpr uint32_t MMAP_PROT_EXEC = 4;

static constexpr uint32_t MMAP_FLAG_SHARED = 1;
static constexpr uint32_t MMAP_FLAG_PRIVATE = 2;
static constexpr uint32_t MMAP_FLAG_FIXED = 0x10;
static constexpr uint32_t MMAP_FLAG_ANONYMOUS = 0x20;

void *mmap([[maybe_unused]] void *addr, size_t len, int prot, uint32_t flags, int fildes, off_t off);

int munmap(void *addr, [[maybe_unused]] size_t len);

}  // namespace panda::os::mem

#endif  // PANDA_LIBPANDABASE_OS_WINDOWS_WINDOWS_MEM_H_
