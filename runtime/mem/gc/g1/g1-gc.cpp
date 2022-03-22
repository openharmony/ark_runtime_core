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

#include "runtime/mem/gc/g1/g1-gc.h"
#include "runtime/include/panda_vm.h"

namespace panda::mem {

void PreStoreInBuffG1([[maybe_unused]] void *object_header) {}

template <class LanguageConfig>
G1GC<LanguageConfig>::G1GC(ObjectAllocatorBase *object_allocator, const GCSettings &settings)
    : GenerationalGC<LanguageConfig>(object_allocator, settings)
{
    post_queue_func_ = [this](const void *from, const void *to) {
        // No need to keep remsets for young->young
        if (!(Region::AddrToRegion(from)->IsEden() && Region::AddrToRegion(to)->IsEden())) {
            os::memory::LockHolder lock(this->queue_lock_);
            LOG_DEBUG_GC << "post queue add ref: " << std::hex << from << " -> " << to;
            this->updated_refs_queue_.emplace_back(from, to);
        }
    };
}

template <class LanguageConfig>
void G1GC<LanguageConfig>::InitGCBits(panda::ObjectHeader *obj_header)
{
    if (UNLIKELY(this->GetGCPhase() == GCPhase::GC_PHASE_SWEEP) && (!IsInCollectibleSet(obj_header))) {
        obj_header->SetMarkedForGC();
    } else {
        obj_header->SetUnMarkedForGC();
    }
    LOG_DEBUG_GC << "Init gc bits for object: " << std::hex << obj_header << " bit: " << obj_header->IsMarkedForGC()
                 << ", is marked = " << IsMarked(obj_header);
}

template <class LanguageConfig>
void G1GC<LanguageConfig>::PreStartupImp()
{
    LOG(FATAL, GC) << "Not implemented";
}

template <class LanguageConfig>
void G1GC<LanguageConfig>::Trigger()
{
    auto task = MakePandaUnique<GCTask>(GCTaskCause::HEAP_USAGE_THRESHOLD_CAUSE, time::GetCurrentTimeInNanos());
    this->AddGCTask(true, std::move(task), true);
}

template <class LanguageConfig>
void G1GC<LanguageConfig>::RunPhasesImpl([[maybe_unused]] panda::GCTask const &task)
{
    LOG(INFO, GC) << "GenGC start";
    LOG_DEBUG_GC << "Footprint before GC: " << this->GetPandaVm()->GetMemStats()->GetFootprintHeap();
    GCScopedPauseStats scoped_pause_stats(this->GetPandaVm()->GetGCStats());
    LOG_DEBUG_GC << "Young range: " << this->GetObjectAllocator()->GetYoungSpaceMemRange();
    uint64_t young_total_time;
    this->GetTiming()->Reset();
    {
        ScopedTiming t("Generational GC", *this->GetTiming());
        this->mem_stats_.Reset();
        {
            time::Timer timer(&young_total_time, true);
            this->GetPandaVm()->GetMemStats()->RecordGCPauseStart();
            this->BindBitmaps(false);
            this->GetPandaVm()->GetMemStats()->RecordGCPhaseEnd();
        }
        if (young_total_time > 0) {
            this->GetStats()->AddTimeValue(young_total_time, TimeTypeStats::YOUNG_TOTAL_TIME);
        }
        // we trigger a full gc at first pygote fork
        if (this->ShouldRunTenuredGC(task) || this->IsOnPygoteFork() || task.reason_ == GCTaskCause::OOM_CAUSE ||
            task.reason_ == GCTaskCause::EXPLICIT_CAUSE) {
            this->BindBitmaps(true);  // clear pygote live bitmaps, we will rebuild it
        }
    }
    LOG_DEBUG_GC << "Footprint after GC: " << this->GetPandaVm()->GetMemStats()->GetFootprintHeap();
    LOG(INFO, GC) << this->mem_stats_.Dump();
    LOG(INFO, GC) << this->GetTiming()->Dump();
    this->GetTiming()->Reset();  // Clear records.
}

template <class LanguageConfig>
void G1GC<LanguageConfig>::InitializeImpl()
{
    // GC saved the PandaVM instance, so we get allocator from the PandaVM.
    InternalAllocatorPtr allocator = this->GetInternalAllocator();
    card_table_ = MakePandaUnique<CardTable>(allocator, PoolManager::GetMmapMemPool()->GetMinObjectAddress(),
                                             PoolManager::GetMmapMemPool()->GetTotalObjectSize());
    card_table_->Initialize();
    auto barrier_set = allocator->New<GCG1BarrierSet>(allocator, &concurrent_marking_flag_, PreStoreInBuffG1,
                                                      PoolManager::GetMmapMemPool()->GetAddressOfMinObjectAddress(),
                                                      reinterpret_cast<uint8_t *>(*card_table_->begin()),
                                                      CardTable::GetCardBits(), CardTable::GetCardDirtyValue(),
                                                      post_queue_func_, this->GetG1ObjectAllocator()->GetRegionSize());
    ASSERT(barrier_set != nullptr);
    this->SetGCBarrierSet(barrier_set);
    LOG_DEBUG_GC << "GenGC initialized";
}

template <class LanguageConfig>
void G1GC<LanguageConfig>::MarkObject([[maybe_unused]] panda::ObjectHeader *object)
{
    LOG(FATAL, GC) << "Not implemented";
}

template <class LanguageConfig>
void G1GC<LanguageConfig>::InitGCBitsForAllocationInTLAB([[maybe_unused]] panda::ObjectHeader *object)
{
    LOG(FATAL, GC) << "Not implemented";
}

template <class LanguageConfig>
bool G1GC<LanguageConfig>::IsMarked([[maybe_unused]] panda::ObjectHeader const *object) const
{
    LOG(FATAL, GC) << "Not implemented";
    return true;
}

template <class LanguageConfig>
void G1GC<LanguageConfig>::MarkReferences([[maybe_unused]] PandaStackTL<ObjectHeader *> *references,
                                          [[maybe_unused]] GCPhase gc_phase)
{
    LOG(FATAL, GC) << "Not implemented";
}

template <class LanguageConfig>
void G1GC<LanguageConfig>::UnMarkObject([[maybe_unused]] panda::ObjectHeader *object)
{
    LOG(FATAL, GC) << "Not implemented";
}

template <class LanguageConfig>
bool G1GC<LanguageConfig>::MarkObjectIfNotMarked([[maybe_unused]] panda::ObjectHeader *object)
{
    LOG(FATAL, GC) << "Not implemented";
    return true;
}

template <class LanguageConfig>
bool G1GC<LanguageConfig>::IsInCollectibleSet(ObjectHeader *obj_header) const
{
    return !this->GetObjectAllocator()->IsAddressInYoungSpace(ToUintPtr(obj_header));
}

template class G1GC<PandaAssemblyLanguageConfig>;

}  // namespace panda::mem
