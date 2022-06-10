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

#include "os/mem_hooks.h"

#include <iostream>
#include <iomanip>

namespace panda::os::windows::mem_hooks {
volatile bool enable = false;
static bool first = true;

static const char *GetAllocTypeName(int at)
{
    switch (at) {
        case _HOOK_ALLOC:
            return "_HOOK_ALLOC";
        case _HOOK_REALLOC:
            return "_HOOK_REALLOC";
        case _HOOK_FREE:
            return "_HOOK_FREE";
        default:
            return "unknown AllocType";
    }
}

static const char *GetBlockTypeName(int bt)
{
    switch (bt) {
        case _CRT_BLOCK:
            return "_CRT_BLOCK";
        case _NORMAL_BLOCK:
            return "_NORMAL_BLOCK";
        case _FREE_BLOCK:
            return "_FREE_BLOCK";
        default:
            return "unknown BlockType";
    }
}

int PandaHooks::PandaAllocHook(int alloctype, [[maybe_unused]] void *data, std::size_t size, int blocktype,
                               [[maybe_unused]] long request, const unsigned char *filename, int linenumber)
{
    if (!enable) {
        return true;
    }

    /* Ignoring internal allocations made by C run-time library functions,
     * or else it may trap the program in an endless loop.
     */
    if (blocktype == _CRT_BLOCK) {
        return true;
    }

    constexpr int ALIGN_SIZE = 32;

    if (first) {
        std::cout << std::left << std::setfill(' ') << std::setw(ALIGN_SIZE) << "alloc type";
        std::cout << std::left << std::setfill(' ') << std::setw(ALIGN_SIZE) << "block type";
        std::cout << std::left << std::setfill(' ') << std::setw(ALIGN_SIZE) << "size";
        std::cout << std::left << std::setfill(' ') << std::setw(ALIGN_SIZE) << "filename";
        std::cout << std::left << std::setfill(' ') << std::setw(ALIGN_SIZE) << "linenumber" << std::endl;
        first = false;
    }

    const char *alloctype_name = GetAllocTypeName(alloctype);
    const char *blocktype_name = GetBlockTypeName(blocktype);

    std::cout << std::left << std::setfill(' ') << std::setw(ALIGN_SIZE) << alloctype_name;
    std::cout << std::left << std::setfill(' ') << std::setw(ALIGN_SIZE) << blocktype_name;
    std::cout << std::left << std::setfill(' ') << std::setw(ALIGN_SIZE) << (int)size;
    std::cout << std::left << std::setfill(' ') << std::setw(ALIGN_SIZE) << filename;
    std::cout << std::left << std::setfill(' ') << std::setw(ALIGN_SIZE) << linenumber << std::endl;

    return true;
}

/* static */
void PandaHooks::Enable()
{
    enable = true;

    _CrtSetAllocHook(PandaAllocHook);
    _CrtMemCheckpoint(&begin);
    _CrtMemDumpAllObjectsSince(&begin);
}

/* static */
void PandaHooks::Disable()
{
    enable = false;

    _CrtMemCheckpoint(&end);
    _CrtMemDumpAllObjectsSince(&end);

    if (_CrtMemDifference(&out, &begin, &end)) {
        std::cerr << "Memory leak detected:" << std::endl;
        _CrtDumpMemoryLeaks();
    }
}
}  // namespace panda::os::windows::mem_hooks
