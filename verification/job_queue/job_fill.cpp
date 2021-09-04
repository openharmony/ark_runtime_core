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

#include "verification/job_queue/job.h"
#include "verification/job_queue/job_fill.h"
#include "verification/cflow/cflow_check.h"
#include "verification/cache/file_entity_cache.h"

#include "runtime/include/runtime.h"
#include "runtime/include/method.h"
#include "runtime/include/field.h"
#include "runtime/include/class_linker.h"

#include "runtime/interpreter/runtime_interface.h"

namespace panda::verifier {

#include "job_fill_gen.h"

bool FillJob(Job &job)
{
    // ASSERT method.bytecode == cached_method.bytecode
    const auto &method = job.JobMethod();
    const auto &cached_method = job.JobCachedMethod();
    const uint8_t *pc_start_ptr = method.GetInstructions();
    size_t code_size = method.GetCodeSize();
    if (pc_start_ptr != cached_method.bytecode || code_size != cached_method.bytecode_size) {
        return false;
    }
    const uint8_t *pc_end_ptr =
        &pc_start_ptr[code_size - 1];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    if (job.Options().Check()[MethodOption::CheckType::RESOLVE_ID]) {
        if (!ResolveIdentifiersForJob(JobQueue::GetCache(), job, pc_start_ptr, pc_end_ptr)) {
            return false;
        }
    }
    if (job.Options().Check()[MethodOption::CheckType::CFLOW]) {
        auto cflow_check_options = Runtime::GetCurrent()->GetVerificationOptions().Cflow;
        auto cflow_info = CheckCflow(cflow_check_options, cached_method);
        if (!cflow_info) {
            return false;
        }
        job.SetMethodCflowInfo(std::move(cflow_info));
    }
    return true;
}

}  // namespace panda::verifier
