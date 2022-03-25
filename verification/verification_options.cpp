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

#include "utils/logger.h"
#include "verification_options.h"
#include "utils/hash.h"
#include "runtime/include/method.h"
#include "runtime/include/mem/allocator.h"

#include "macros.h"

#include <cstdint>
#include <string>

namespace panda::verifier {

void VerificationOptions::Initialize(const panda::RuntimeOptions &runtime_options)
{
    // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
    Debug.MethodOptions = new (mem::AllocatorAdapter<MethodOptionsConfig>().allocate(1)) MethodOptionsConfig {};

    Enable = runtime_options.IsVerificationEnabled();

    auto check_option = [](const auto &lst, const char *flag) {
        for (const auto &item : lst) {
            if (item == flag) {
                return true;
            }
        }
        return false;
    };

    auto &&options = runtime_options.GetVerificationOptions();

    Show.Status = check_option(options, "show-status");

    Cflow[CflowCheckOptions::ALLOW_JMP_BODY_TO_HANDLER] = check_option(options, "cflow-allow-jumps-body-to-handler");
    Cflow[CflowCheckOptions::ALLOW_JMP_BODY_INTO_HANDLER] =
        check_option(options, "cflow-allow-jumps-body-into-handler");
    Cflow[CflowCheckOptions::ALLOW_JMP_HANDLER_INTO_BODY] =
        check_option(options, "cflow-allow-jumps-handler-into-body");
    Cflow[CflowCheckOptions::ALLOW_JMP_HANDLER_TO_HANDLER] =
        check_option(options, "cflow-allow-jumps-handler-to-handler");
    Cflow[CflowCheckOptions::ALLOW_JMP_HANDLER_INTO_HANDLER] =
        check_option(options, "cflow-allow-jumpe-handler-into-handler");

    Cache.File = runtime_options.GetVerificationCacheFile();
    Cache.UpdateOnExit = check_option(options, "update-cache");

    Mode.OnlyBuildTypeSystem = check_option(options, "only-build-typesystem");
    Mode.VerifyAllRuntimeLibraryMethods = check_option(options, "verify-all-runtime-library-methods");
    Mode.VerifyOnlyEntryPoint = check_option(options, "verify-only-entry-point");
    Mode.VerifierDoesNotFail = check_option(options, "verifier-does-not-fail");
    Mode.OnlyVerify = check_option(options, "only-verify");
    Mode.DoNotAssumeLibraryMethodsVerified = check_option(options, "do-not-assume-library-methods-verified");
    Mode.SyncOnClassInitialization = check_option(options, "sync-on-class-initialization");
    Mode.VerificationThreads = runtime_options.GetVerificationThreads();

    if (Mode.DebugEnable) {
        Debug.ConfigFile = runtime_options.GetVerificationDebugConfigFile();
    }
}

void VerificationOptions::Destroy()
{
    if (Debug.MethodOptions != nullptr) {
        Debug.MethodOptions->~MethodOptionsConfig();
        mem::AllocatorAdapter<MethodOptionsConfig>().deallocate(Debug.MethodOptions, 1);
    }
    Debug.MethodOptions = nullptr;
}

}  // namespace panda::verifier
