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

#ifndef PANDA_LIBPANDABASE_OS_UNIX_NATIVE_STACK_H_
#define PANDA_LIBPANDABASE_OS_UNIX_NATIVE_STACK_H_

#include "os/thread.h"
#include <string>
#include <set>

namespace panda::os::unix::native_stack {
using FUNC_UNWINDSTACK = bool (*)(pid_t, std::ostream &, int);

void DumpKernelStack(std::ostream &os, pid_t tid, const char *tag, bool count);

std::string GetNativeThreadNameForFile(pid_t tid);

class DumpUnattachedThread {
public:
    void AddTid(pid_t tid_thread);
    bool InitKernelTidLists();
    void Dump(std::ostream &os, bool dump_native_crash, FUNC_UNWINDSTACK call_unwindstack);

private:
    std::set<pid_t> kernel_tid_;
    std::set<pid_t> thread_manager_tids_;
};

bool ReadOsFile(const std::string &file_name, std::string *result);
bool WriterOsFile(const void *buffer, size_t count, int fd);
std::string ChangeJaveStackFormat(const char *descriptor);

}  // namespace panda::os::unix::native_stack

#endif  // PANDA_LIBPANDABASE_OS_UNIX_NATIVE_STACK_H_
