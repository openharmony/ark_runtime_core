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

#include "runtime/include/gc_task.h"

#include "runtime/mem/gc/gc.h"

namespace panda {

void GCTask::Run(mem::GC &gc)
{
    gc.WaitForGC(*this);
    gc.SetCanAddGCTask(true);
}

void GCTask::Release(mem::InternalAllocatorPtr allocator)
{
    allocator->Delete(this);
}

std::ostream &operator<<(std::ostream &os, const GCTaskCause &cause)
{
    switch (cause) {
        case GCTaskCause::INVALID_CAUSE:
            os << "Invalid";
            break;
        case GCTaskCause::PYGOTE_FORK_CAUSE:
            os << "PygoteFork";
            break;
        case GCTaskCause::STARTUP_COMPLETE_CAUSE:
            os << "StartupComplete";
            break;
        case GCTaskCause::NATIVE_ALLOC_CAUSE:
            os << "NativeAlloc";
            break;
        case GCTaskCause::EXPLICIT_CAUSE:
            os << "Explicit";
            break;
        case GCTaskCause::HEAP_USAGE_THRESHOLD_CAUSE:
            os << "Threshold";
            break;
        case GCTaskCause::YOUNG_GC_CAUSE:
            os << "Young";
            break;
        case GCTaskCause::OOM_CAUSE:
            os << "OOM";
            break;
        default:
            LOG(FATAL, GC) << "Unknown gc cause";
            break;
    }
    return os;
}

}  // namespace panda
