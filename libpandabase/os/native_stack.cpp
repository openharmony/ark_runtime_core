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

#include "native_stack.h"

namespace panda::os::native_stack {
#if !defined(PANDA_TARGET_UNIX)
void DumpUnattachedThread::AddTid([[maybe_unused]] pid_t tid_thread) {}

bool DumpUnattachedThread::InitKernelTidLists()
{
    return true;
}

void DumpUnattachedThread::Dump([[maybe_unused]] std::ostream &os, [[maybe_unused]] bool dump_native_crash,
                                [[maybe_unused]] FUNC_UNWINDSTACK call_unwindstack)
{
}
void DumpKernelStack([[maybe_unused]] std::ostream &os, [[maybe_unused]] pid_t tid, [[maybe_unused]] const char *tag,
                     [[maybe_unused]] bool count)
{
}

std::string GetNativeThreadNameForFile([[maybe_unused]] pid_t tid)
{
    return "<unknown>";
}

bool ReadOsFile([[maybe_unused]] const std::string &file_name, [[maybe_unused]] std::string *result)
{
    return false;
}

bool WriterOsFile([[maybe_unused]] const void *buffer, [[maybe_unused]] size_t count, [[maybe_unused]] int fd)
{
    return false;
}

std::string ChangeJaveStackFormat([[maybe_unused]] const char *descriptor)
{
    return "unknown";
}
#endif  // PANDA_TARGET_UNIX

}  // namespace panda::os::native_stack
