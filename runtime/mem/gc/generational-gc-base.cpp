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

#include "runtime/mem/gc/generational-gc-base.h"
#include "runtime/include/runtime.h"
#include "runtime/include/runtime_notification.h"
#include "runtime/include/panda_vm.h"

namespace panda::mem {

template <class LanguageConfig>
bool GenerationalGC<LanguageConfig>::ShouldRunTenuredGC([[maybe_unused]] const GCTask &task)
{
    static size_t young_gc_count = 0;
    young_gc_count++;
    bool run_tenured = false;
    if (young_gc_count >= GenerationalGC<LanguageConfig>::GetMajorPeriod()) {
        young_gc_count = 0;
        run_tenured = true;
    }
    LOG_DEBUG_GC << "GenGC::ShouldRunTenuredGC = " << run_tenured;
    return run_tenured;
}

template <class LanguageConfig>
void GenerationalGC<LanguageConfig>::WaitForGC(const GCTask &task)
{
    Runtime::GetCurrent()->GetNotificationManager()->GarbageCollectorStartEvent();
    auto old_counter = this->gc_counter_.load(std::memory_order_acquire);
    this->GetPandaVm()->GetRendezvous()->SafepointBegin();

    auto new_counter = this->gc_counter_.load(std::memory_order_acquire);
    if (new_counter > old_counter && this->last_cause_.load() >= task.reason_) {
        this->GetPandaVm()->GetRendezvous()->SafepointEnd();
        return;
    }
    this->RunPhases(task);
    this->GetPandaVm()->GetRendezvous()->SafepointEnd();
    Runtime::GetCurrent()->GetNotificationManager()->GarbageCollectorFinishEvent();
    this->GetPandaVm()->HandleGCFinished();
    this->GetPandaVm()->HandleEnqueueReferences();
}

template <class LanguageConfig>
PandaString GenerationalGC<LanguageConfig>::MemStats::Dump()
{
    PandaStringStream statistic;
    statistic << "Young freed " << young_free_object_count_ << "(" << helpers::MemoryConverter(young_free_object_size_)
              << ") Young moved " << young_move_object_count_ << "("
              << helpers::MemoryConverter(young_move_object_count_) << ")";
    if (tenured_free_object_size_ > 0U) {
        statistic << " Tenured freed " << tenured_free_object_size_ << "("
                  << helpers::MemoryConverter(tenured_free_object_size_) << ")";
    }
    return statistic.str();
}

template class GenerationalGC<PandaAssemblyLanguageConfig>;
}  // namespace panda::mem
