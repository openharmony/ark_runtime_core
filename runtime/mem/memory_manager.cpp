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

#include "memory_manager.h"

#include "runtime/include/runtime_options.h"
#include "runtime/mem/refstorage/global_object_storage.h"
#include "runtime/mem/gc/gc.h"
#include "runtime/mem/gc/gc_trigger.h"
#include "runtime/mem/gc/gc_stats.h"
#include "runtime/mem/heap_manager.h"

namespace panda::mem {

static HeapManager *CreateHeapManager(InternalAllocatorPtr internal_allocator,
                                      const MemoryManager::HeapOptions &options, GCType gc_type,
                                      MemStatsType *mem_stats)
{
    // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
    auto *heap_manager = new (std::nothrow) HeapManager();
    if (heap_manager == nullptr) {
        LOG(ERROR, RUNTIME) << "Failed to allocate HeapManager";
        return nullptr;
    }

    if (!heap_manager->Initialize(gc_type, options.is_single_thread, options.is_use_tlab_for_allocations, mem_stats,
                                  internal_allocator, options.is_start_as_zygote)) {
        LOG(ERROR, RUNTIME) << "Failed to initialize HeapManager";
        delete heap_manager;
        return nullptr;
    }
    heap_manager->SetIsFinalizableFunc(options.is_object_finalizeble_func);
    heap_manager->SetRegisterFinalizeReferenceFunc(options.register_finalize_reference_func);

    return heap_manager;
}

/* static */
MemoryManager *MemoryManager::Create(LanguageContext ctx, InternalAllocatorPtr internal_allocator, GCType gc_type,
                                     const GCSettings &gc_settings, const GCTriggerConfig &gc_trigger_config,
                                     const HeapOptions &heap_options)
{
    std::unique_ptr<MemStatsType> mem_stats = std::make_unique<MemStatsType>();

    HeapManager *heap_manager = CreateHeapManager(internal_allocator, heap_options, gc_type, mem_stats.get());
    if (heap_manager == nullptr) {
        return nullptr;
    }

    InternalAllocatorPtr allocator = heap_manager->GetInternalAllocator();
    PandaUniquePtr<GCStats> gc_stats = MakePandaUnique<GCStats>(mem_stats.get(), gc_type, allocator);
    PandaUniquePtr<GC> gc(ctx.CreateGC(gc_type, heap_manager->GetObjectAllocator().AsObjectAllocator(), gc_settings));
    PandaUniquePtr<GCTrigger> gc_trigger(CreateGCTrigger(mem_stats.get(), gc_trigger_config, allocator));
    PandaUniquePtr<GlobalObjectStorage> global_object_storage = MakePandaUnique<GlobalObjectStorage>(
        internal_allocator, heap_options.max_global_ref_size, heap_options.is_global_reference_size_check_enabled);
    if (global_object_storage.get() == nullptr) {
        LOG(ERROR, RUNTIME) << "Failed to allocate GlobalObjectStorage";
        delete heap_manager;
        return nullptr;
    }

    // CODECHECK-NOLINTNEXTLINE(CPP_RULE_ID_SMARTPOINTER_INSTEADOF_ORIGINPOINTER)
    return new MemoryManager(internal_allocator, heap_manager, gc.release(), gc_trigger.release(), gc_stats.release(),
                             mem_stats.release(), global_object_storage.release());
}

/* static */
void MemoryManager::Destroy(MemoryManager *mm)
{
    delete mm;
}

MemoryManager::~MemoryManager()
{
    heap_manager_->GetInternalAllocator()->Delete(gc_);
    heap_manager_->GetInternalAllocator()->Delete(gc_trigger_);
    heap_manager_->GetInternalAllocator()->Delete(gc_stats_);
    heap_manager_->GetInternalAllocator()->Delete(global_object_storage_);

    delete heap_manager_;

    // One more check that we don't have memory leak in internal allocator.
    ASSERT(mem_stats_->GetFootprint(SpaceType::SPACE_TYPE_INTERNAL) == 0);
    delete mem_stats_;
}

void MemoryManager::Finalize()
{
    heap_manager_->Finalize();
}

void MemoryManager::InitializeGC() const
{
    gc_->Initialize();
    gc_->AddListener(gc_trigger_);
}

void MemoryManager::PreStartup() const
{
    gc_->PreStartup();
}

void MemoryManager::PreZygoteFork() const
{
    gc_->PreZygoteFork();
    heap_manager_->PreZygoteFork();
}

void MemoryManager::PostZygoteFork() const
{
    gc_->PostZygoteFork();
}

void MemoryManager::StartGC() const
{
    gc_->StartGC();
}

void MemoryManager::StopGC() const
{
    gc_->StopGC();
}

}  // namespace panda::mem
