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

#ifndef PANDA_VERIFICATION_VERIFICATION_OPTIONS_H_
#define PANDA_VERIFICATION_VERIFICATION_OPTIONS_H_

#include "utils/pandargs.h"
#include "runtime/include/runtime_options.h"
#include "runtime/include/mem/panda_containers.h"
#include "runtime/include/mem/panda_string.h"
#include "verification/cflow/cflow_check_options.h"
#include "verification/debug/options/method_options_config.h"
#include "verifier_messages.h"

#include <string>
#include <unordered_map>

namespace panda::verifier {

struct VerificationOptions {
    using MethodOptionsConfig =
        VerifierMethodOptionsConfig<PandaString, VerifierMessage, PandaUnorderedMap, PandaVector>;
    bool Enable = true;
    struct {
        bool Status = true;
    } Show;
    CflowCheckFlags Cflow;
    struct {
        bool OnlyBuildTypeSystem = false;
        bool VerifyAllRuntimeLibraryMethods = false;
        bool VerifyOnlyEntryPoint = false;
        bool VerifierDoesNotFail = false;
        bool OnlyVerify = false;
        bool DebugEnable = true;
        bool DoNotAssumeLibraryMethodsVerified = false;
        bool SyncOnClassInitialization = false;
        size_t VerificationThreads = 1;
    } Mode;
    struct {
        std::string File;
        bool UpdateOnExit = false;
    } Cache;
    struct {
        std::string ConfigFile = "default";
        struct {
            bool RegChanges = false;
            bool Context = false;
            bool TypeSystem = false;
        } Show;
        struct {
            bool UndefinedClass = false;
            bool UndefinedMethod = false;
            bool UndefinedField = false;
            bool UndefinedType = false;
            bool UndefinedString = false;
            bool MethodAccessViolation = false;
            bool ErrorInExceptionHandler = false;
            bool PermanentRuntimeException = false;
            bool FieldAccessViolation = false;
            bool WrongSubclassingInMethodArgs = false;
        } Allow;
        MethodOptionsConfig *MethodOptions = nullptr;
        MethodOptionsConfig &GetMethodOptions()
        {
            return *MethodOptions;
        }
        const MethodOptionsConfig &GetMethodOptions() const
        {
            return *MethodOptions;
        }
    } Debug;
    void Initialize(const panda::RuntimeOptions &runtime_options);
    void Destroy();
};

}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_VERIFICATION_OPTIONS_H_
