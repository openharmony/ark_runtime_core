/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#ifndef PANDA_LIBPANDABASE_OS_WINDOWS_MEM_HOOKS_H_
#define PANDA_LIBPANDABASE_OS_WINDOWS_MEM_HOOKS_H_

#include <crtdbg.h>
#include <iostream>

namespace panda::os::windows::mem_hooks {
class PandaHooks {
public:
    static void Enable();

    static void Disable();

private:
    /*
     * "PandaAllocHook" is an allocation hook function, following a prototype described in
     * https://docs.microsoft.com/en-us/visualstudio/debugger/allocation-hook-functions.
     * Installed it using "_CrtSetAllocHook", then it will be called every time memory is
     * allocated, reallocated, or freed.
     */
    static int PandaAllocHook(int alloctype, void *data, std::size_t size, int blocktype, long request,
                              const unsigned char *filename, int linenumber);

    static _CrtMemState begin, end, out;
};
}  // namespace panda::os::windows::mem_hooks

namespace panda::os::mem_hooks {
using PandaHooks = panda::os::windows::mem_hooks::PandaHooks;
}  // namespace panda::os::mem_hooks

#endif  // PANDA_LIBPANDABASE_OS_WINDOWS_MEM_HOOKS_H_
