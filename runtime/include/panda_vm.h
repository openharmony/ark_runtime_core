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

#ifndef PANDA_RUNTIME_INCLUDE_PANDA_VM_H_
#define PANDA_RUNTIME_INCLUDE_PANDA_VM_H_

#include "include/runtime_options.h"
#include "runtime/include/method.h"
#include "runtime/include/runtime.h"
#include "runtime/mem/gc/gc_phase.h"

#include "libpandabase/utils/expected.h"

namespace panda {

class ManagedThread;
class StringTable;
class ThreadManager;

namespace mem {
class HeapManager;
class GC;
class GCTrigger;
class ReferenceProcessor;
}  // namespace mem

enum class PandaVMType : size_t { CORE_VM, JAVA_VM, ECMA_VM };

class PandaVM {
public:
    static PandaVM *Create(Runtime *runtime, const RuntimeOptions &options, std::string_view runtime_type);

    PandaVM() = default;
    virtual ~PandaVM() = default;

    static PandaVM *GetCurrent()
    {
        return Thread::GetCurrent()->GetVM();
    }

    virtual bool Initialize() = 0;
    virtual bool InitializeFinish() = 0;
    virtual void PreStartup() = 0;
    virtual void PreZygoteFork() = 0;
    virtual void PostZygoteFork() = 0;
    virtual void InitializeGC() = 0;
    virtual void StartGC() = 0;
    virtual void StopGC() = 0;
    virtual void VisitVmRoots(const GCRootVisitor &visitor) = 0;
    virtual void UpdateVmRefs() = 0;
    virtual void UninitializeThreads() = 0;

    virtual Expected<int, Runtime::Error> InvokeEntrypoint(Method *entrypoint, const std::vector<std::string> &args);

    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    virtual void HandleReferences([[maybe_unused]] const GCTask &task) {}
    virtual void HandleEnqueueReferences() {}
    virtual void HandleBufferData([[maybe_unused]] bool reverse) {}
    virtual void HandleGCFinished() {}

    virtual mem::GCStats *GetGCStats() const = 0;
    virtual mem::HeapManager *GetHeapManager() const = 0;
    virtual mem::GC *GetGC() const = 0;
    virtual mem::GCTrigger *GetGCTrigger() const = 0;
    virtual const RuntimeOptions &GetOptions() const = 0;
    virtual ManagedThread *GetAssociatedThread() const = 0;
    virtual StringTable *GetStringTable() const = 0;
    virtual mem::MemStatsType *GetMemStats() const = 0;
    virtual Rendezvous *GetRendezvous() const = 0;
    virtual mem::GlobalObjectStorage *GetGlobalObjectStorage() const = 0;
    virtual MonitorPool *GetMonitorPool() const = 0;
    virtual ThreadManager *GetThreadManager() const = 0;

    // remove this method after fixing interpreter performance
    virtual PandaVMType GetPandaVMType() const = 0;
    virtual LanguageContext GetLanguageContext() const = 0;
    virtual CompilerInterface *GetCompiler() const = 0;

    virtual panda::mem::ReferenceProcessor *GetReferenceProcessor() const = 0;

    virtual ObjectHeader *GetOOMErrorObject() = 0;

    NO_MOVE_SEMANTIC(PandaVM);
    NO_COPY_SEMANTIC(PandaVM);

protected:
    virtual bool CheckEntrypointSignature(Method *entrypoint) = 0;
    virtual Expected<int, Runtime::Error> InvokeEntrypointImpl(Method *entrypoint,
                                                               const std::vector<std::string> &args) = 0;
    virtual void HandleUncaughtException(ObjectHeader *exception) = 0;
};

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_PANDA_VM_H_
