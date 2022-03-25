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

#ifndef PANDA_LIBPANDABASE_OS_NATIVE_STACK_H_
#define PANDA_LIBPANDABASE_OS_NATIVE_STACK_H_

#if defined(PANDA_TARGET_UNIX)
#include "os/unix/native_stack.h"
#endif  // PANDA_TARGET_UNIX
#include <string>
#include <set>
#include <signal.h>  // NOLINTNEXTLINE(modernize-deprecated-headers)

namespace panda::os::native_stack {

const auto g_PandaThreadSigmask = pthread_sigmask;  // NOLINT(readability-identifier-naming)
#if defined(PANDA_TARGET_UNIX)
using DumpUnattachedThread = panda::os::unix::native_stack::DumpUnattachedThread;
const auto DumpKernelStack = panda::os::unix::native_stack::DumpKernelStack;  // NOLINT(readability-identifier-naming)
const auto GetNativeThreadNameForFile =                                       // NOLINT(readability-identifier-naming)
    panda::os::unix::native_stack::GetNativeThreadNameForFile;
const auto ReadOsFile = panda::os::unix::native_stack::ReadOsFile;      // NOLINT(readability-identifier-naming)
const auto WriterOsFile = panda::os::unix::native_stack::WriterOsFile;  // NOLINT(readability-identifier-naming)
const auto ChangeJaveStackFormat =                                      // NOLINT(readability-identifier-naming)
    panda::os::unix::native_stack::ChangeJaveStackFormat;
#else
using FUNC_UNWINDSTACK = bool (*)(pid_t, std::ostream &, int);
class DumpUnattachedThread {
public:
    void AddTid(pid_t tid_thread);
    bool InitKernelTidLists();
    void Dump(std::ostream &os, bool dump_native_crash, FUNC_UNWINDSTACK call_unwindstack);

private:
    std::set<pid_t> kernel_tid_;
    std::set<pid_t> thread_manager_tids_;
};
void DumpKernelStack(std::ostream &os, pid_t tid, const char *tag, bool count);
std::string GetNativeThreadNameForFile(pid_t tid);
bool ReadOsFile(const std::string &file_name, std::string *result);
bool WriterOsFile(const void *buffer, size_t count, int fd);
std::string ChangeJaveStackFormat(const char *descriptor);
#endif  // PANDA_TARGET_UNIX

}  // namespace panda::os::native_stack

#endif  // PANDA_LIBPANDABASE_OS_NATIVE_STACK_H_
