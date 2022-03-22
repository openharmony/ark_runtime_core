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

#ifndef PANDA_RUNTIME_CORE_CORE_VM_H_
#define PANDA_RUNTIME_CORE_CORE_VM_H_

#include "libpandabase/macros.h"
#include "libpandabase/utils/expected.h"
#include "runtime/include/mem/panda_smart_pointers.h"
#include "runtime/include/mem/panda_string.h"
#include "runtime/include/panda_vm.h"
#include "runtime/mem/gc/gc_phase.h"
#include "runtime/mem/refstorage/reference.h"

namespace panda {

class Method;
class Runtime;

namespace core {

class PandaCoreVM : public PandaVM {
public:
    static Expected<PandaCoreVM *, PandaString> Create(Runtime *runtime, const RuntimeOptions &options);
    ~PandaCoreVM() override;

    static PandaCoreVM *GetCurrent();

    bool Initialize() override;
    bool InitializeFinish() override;
    void UninitializeThreads() override;

    void PreStartup() override;
    void PreZygoteFork() override;
    void PostZygoteFork() override;
    void InitializeGC() override;
    void StartGC() override;
    void StopGC() override;

    void HandleReferences(const GCTask &task) override;
    void HandleEnqueueReferences() override;
    void HandleGCFinished() override;

    void VisitVmRoots(const GCRootVisitor &visitor) override;
    void UpdateVmRefs() override {}

    mem::HeapManager *GetHeapManager() const override
    {
        return mm_->GetHeapManager();
    }

    mem::GC *GetGC() const override
    {
        return mm_->GetGC();
    }

    mem::GCTrigger *GetGCTrigger() const override
    {
        return mm_->GetGCTrigger();
    }

    mem::GCStats *GetGCStats() const override
    {
        return mm_->GetGCStats();
    }

    ManagedThread *GetAssociatedThread() const override
    {
        return ManagedThread::GetCurrent();
    }

    StringTable *GetStringTable() const override
    {
        return string_table_;
    }

    mem::MemStatsType *GetMemStats() const override
    {
        return mm_->GetMemStats();
    }

    const RuntimeOptions &GetOptions() const override
    {
        return Runtime::GetOptions();
    }

    ThreadManager *GetThreadManager() const override
    {
        return thread_manager_;
    }

    MonitorPool *GetMonitorPool() const override
    {
        return monitor_pool_;
    }

    mem::GlobalObjectStorage *GetGlobalObjectStorage() const override
    {
        return mm_->GetGlobalObjectStorage();
    }

    panda::mem::ReferenceProcessor *GetReferenceProcessor() const override
    {
        ASSERT(reference_processor_ != nullptr);
        return reference_processor_;
    }

    void DumpForSigQuit(std::ostream &os);

    PandaVMType GetPandaVMType() const override
    {
        return PandaVMType::CORE_VM;
    }

    LanguageContext GetLanguageContext() const override
    {
        return Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
    }

    CompilerInterface *GetCompiler() const override
    {
        return nullptr;
    }

    Rendezvous *GetRendezvous() const override
    {
        return rendezvous_;
    }

    ObjectHeader *GetOOMErrorObject() override;

protected:
    bool CheckEntrypointSignature(Method *entrypoint) override;
    Expected<int, Runtime::Error> InvokeEntrypointImpl(Method *entrypoint,
                                                       const std::vector<std::string> &args) override;
    void HandleUncaughtException(ObjectHeader *exception) override;

private:
    explicit PandaCoreVM(Runtime *runtime, const RuntimeOptions &options, mem::MemoryManager *mm);

    Runtime *runtime_ {nullptr};
    mem::MemoryManager *mm_ {nullptr};
    mem::ReferenceProcessor *reference_processor_ {nullptr};
    PandaVector<ObjectHeader *> gc_roots_;
    Rendezvous *rendezvous_ {nullptr};
    CompilerInterface *compiler_ {nullptr};
    MTManagedThread *main_thread_ {nullptr};
    StringTable *string_table_ {nullptr};
    MonitorPool *monitor_pool_ {nullptr};
    ThreadManager *thread_manager_ {nullptr};

    NO_MOVE_SEMANTIC(PandaCoreVM);
    NO_COPY_SEMANTIC(PandaCoreVM);

    friend class mem::Allocator;
};

}  // namespace core
}  // namespace panda

#endif  // PANDA_RUNTIME_CORE_CORE_VM_H_
