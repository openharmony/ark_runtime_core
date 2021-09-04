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
#include "verification/job_queue/job_queue.h"
#include "verification/job_queue/cache.h"

#include "verification/absint/panda_types.h"
#include "verification/absint/absint.h"

#include "runtime/include/thread_scopes.h"
#include "runtime/include/runtime.h"
#include "libpandabase/utils/logger.h"

#include "verification/debug/breakpoint/breakpoint.h"

namespace panda::verifier {

bool UpdateTypes(PandaTypes *panda_types_ptr, const Job &job)
{
    auto &panda_types = *panda_types_ptr;
    bool result = true;
    job.ForAllCachedClasses([&panda_types, &result](const CacheOfRuntimeThings::CachedClass &klass) {
        auto type = panda_types.TypeOf(klass);
        result = result && type.IsValid();
    });
    job.ForAllCachedMethods([&panda_types](const CacheOfRuntimeThings::CachedMethod &method) {
        panda_types.NormalizedMethodSignature(method);
    });
    job.ForAllCachedFields([&panda_types, &result](const CacheOfRuntimeThings::CachedField &field) {
        auto &klass_ref = CacheOfRuntimeThings::GetRef(field.klass);
        auto &type_ref = CacheOfRuntimeThings::GetRef(field.type);
        if (Valid(klass_ref) && Valid(type_ref)) {
            auto class_type = panda_types.TypeOf(klass_ref);
            auto field_type = panda_types.TypeOf(type_ref);
            result = result && class_type.IsValid() && field_type.IsValid();
        } else {
            result = false;
        }
    });
    return result;
}

bool Verify(PandaTypes *panda_types, const Job &job)
{
    auto verif_context = PrepareVerificationContext(panda_types, job);
    auto result = VerifyMethod(VerificationLevel::LEVEL0, &verif_context);
    return result != VerificationStatus::ERROR;
}

void SetResult(Method *method, bool result)
{
    auto &runtime = *Runtime::GetCurrent();
    auto &verif_options = runtime.GetVerificationOptions();
    if (verif_options.Mode.VerifierDoesNotFail) {
        result = true;
    }
    method->SetVerified(result);
}

void VerifierThread(size_t n)
{
    while (true) {
        bool result = true;

        auto *job_ptr = JobQueue::GetJob();
        if (job_ptr == nullptr) {
            break;
        }
        auto &job = *job_ptr;
        auto &method = job.JobMethod();

        if (method.IsVerified()) {
            // method may be marked as verified during marking all
            // methods in runtime libraries
            JobQueue::DisposeJob(&job);
            continue;
        }

        LOG(DEBUG, VERIFIER) << "Verification of method '" << method.GetFullName() << std::hex << "' ( 0x"
                             << method.GetUniqId() << ", 0x" << reinterpret_cast<uintptr_t>(&method) << ")";

        auto &panda_types = JobQueue::GetPandaTypes(n);

        DBG_MANAGED_BRK(panda::verifier::debug::Component::VERIFIER, job.JobCachedMethod().id, 0xFFFF);

        ASSERT(method.GetInstructions() == job.JobCachedMethod().bytecode);

        if (job.Options().Check()[MethodOption::CheckType::TYPING]) {
            result = UpdateTypes(&panda_types, job);
            if (!result) {
                LOG(DEBUG, VERIFIER) << "Cannot update types from cached classes";
            }
        }

        if (job.Options().Check()[MethodOption::CheckType::ABSINT]) {
            result = result && Verify(&panda_types, job);
        }

        LOG(INFO, VERIFIER) << "Verification result for method '" << method.GetFullName() << std::hex << "' ( 0x"
                            << method.GetUniqId() << ", 0x" << reinterpret_cast<uintptr_t>(&method)
                            << "): " << (result ? "OK" : "FAIL");

        SetResult(&method, result);
        JobQueue::DisposeJob(&job);
    }
    bool show_subtyping =
        Runtime::GetCurrentSync([](auto &instance) { return instance.GetVerificationOptions().Debug.Show.TypeSystem; });
    if (show_subtyping) {
        LOG(DEBUG, VERIFIER) << "Typesystem of verifier thread #" << n;
        auto &panda_types = JobQueue::GetPandaTypes(n);
        panda_types.DisplayTypeSystem([](auto str) { LOG(DEBUG, VERIFIER) << str; });
    }
}

}  // namespace panda::verifier
