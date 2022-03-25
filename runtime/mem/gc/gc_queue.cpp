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

#include "mem/gc/gc_queue.h"

#include "include/runtime.h"
#include "libpandabase/utils/time.h"
#include "runtime/mem/gc/gc.h"

namespace panda::mem {

const int64_t NANOSECONDS_PER_MILLISEC = 1000000;

GCTask *GCQueueWithTime::GetTask()
{
    os::memory::LockHolder lock(lock_);
    while (queue_.empty()) {
        if (!gc_->IsGCRunning()) {
            LOG(DEBUG, GC) << "GetTask() Return INVALID_CAUSE";
            return nullptr;
        }
        LOG(DEBUG, GC) << "Empty " << queue_name_ << ", waiting...";
        cond_var_.Wait(&lock_);
    }
    GCTask *task = queue_.top();
    auto current_time = time::GetCurrentTimeInNanos();
    while (gc_->IsGCRunning() && (task->GetTargetTime() >= current_time)) {
        auto delta = task->GetTargetTime() - current_time;
        uint64_t ms = delta / NANOSECONDS_PER_MILLISEC;
        uint64_t ns = delta % NANOSECONDS_PER_MILLISEC;
        LOG(DEBUG, GC) << "GetTask TimedWait";
        cond_var_.TimedWait(&lock_, ms, ns);
        task = queue_.top();
        current_time = time::GetCurrentTimeInNanos();
    }
    queue_.pop();
    LOG(DEBUG, GC) << "Extract a task from a " << queue_name_;
    return task;
}

void GCQueueWithTime::AddTask(GCTask *task)
{
    os::memory::LockHolder lock(lock_);
    if (finalized) {
        LOG(DEBUG, GC) << "Skip AddTask to queue: " << queue_name_ << " cause it's finalized already";
        task->Release(gc_->GetInternalAllocator());
        return;
    }
    LOG(DEBUG, GC) << "Add task to a " << queue_name_;
    if (!queue_.empty()) {
        auto last_elem = queue_.top();
        if (last_elem->reason_ == task->reason_) {
            // do not duplicate GC task with the same reason.
            task->Release(gc_->GetInternalAllocator());
            return;
        }
    }
    queue_.push(task);
    cond_var_.Signal();
}

void GCQueueWithTime::Finalize()
{
    os::memory::LockHolder lock(lock_);
    finalized = true;
    LOG(DEBUG, GC) << "Clear a " << queue_name_;
    InternalAllocatorPtr allocator = gc_->GetInternalAllocator();
    while (!queue_.empty()) {
        auto task = queue_.top();
        task->Release(allocator);
        queue_.pop();
    }
}

}  // namespace panda::mem
