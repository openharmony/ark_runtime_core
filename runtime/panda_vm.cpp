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

#include <cstdlib>

#include "runtime/include/runtime.h"
#include "runtime/include/runtime_options.h"
#include "runtime/include/runtime_notification.h"
#include "runtime/core/core_vm.h"
#include "runtime/include/panda_vm.h"
#include "runtime/mem/gc/reference-processor/reference_processor.h"

namespace panda {

/* static */
PandaVM *PandaVM::Create(Runtime *runtime, const RuntimeOptions &options, std::string_view runtime_type)
{
    PandaVM *panda_vm = runtime->GetLanguageContext(std::string(runtime_type)).CreateVM(runtime, options);
    if (panda_vm == nullptr) {
        return nullptr;
    }

    // WORKAROUND(v.cherkashin): EcmaScript doesn't have GC and HeapManager
    if (runtime_type != "ecmascript") {
        panda_vm->GetGC()->SetPandaVM(panda_vm);
        panda_vm->GetHeapManager()->SetPandaVM(panda_vm);
    }
    return panda_vm;
}

Expected<int, Runtime::Error> PandaVM::InvokeEntrypoint(Method *entrypoint, const std::vector<std::string> &args)
{
    if (!CheckEntrypointSignature(entrypoint)) {
        LOG(ERROR, RUNTIME) << "Method '" << entrypoint << "' has invalid signature";
        return Unexpected(Runtime::Error::INVALID_ENTRY_POINT);
    }
    Expected<int, Runtime::Error> ret = InvokeEntrypointImpl(entrypoint, args);
    ManagedThread *thread = ManagedThread::GetCurrent();
    if (thread->HasPendingException()) {
        auto *exception = thread->GetException();
        HandleUncaughtException(exception);
        ret = EXIT_FAILURE;
    }

    return ret;
}

}  // namespace panda
