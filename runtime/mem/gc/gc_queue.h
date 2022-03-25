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

#ifndef PANDA_RUNTIME_MEM_GC_GC_QUEUE_H_
#define PANDA_RUNTIME_MEM_GC_GC_QUEUE_H_

#include <memory>

#include "runtime/include/locks.h"
#include "runtime/include/gc_task.h"
#include "runtime/include/mem/panda_containers.h"
#include "runtime/include/mem/panda_smart_pointers.h"

namespace panda::mem {

constexpr uint64_t GC_WAIT_TIMEOUT = 500U;

class GCQueueInterface {
public:
    GCQueueInterface() = default;
    virtual ~GCQueueInterface() = default;

    virtual GCTask *GetTask() = 0;

    virtual void AddTask(GCTask *task) = 0;

    virtual void Finalize() = 0;

    virtual void Signal() = 0;

    virtual bool WaitForGCTask() = 0;

    NO_COPY_SEMANTIC(GCQueueInterface);
    NO_MOVE_SEMANTIC(GCQueueInterface);
};

/**
 * GCQueueWithTime is an ascending priority queue ordered by target time.
 *
 */
class GCQueueWithTime : public GCQueueInterface {
public:
    explicit GCQueueWithTime(GC *gc) : gc_(gc) {}
    ~GCQueueWithTime() override = default;
    NO_MOVE_SEMANTIC(GCQueueWithTime);
    NO_COPY_SEMANTIC(GCQueueWithTime);

    GCTask *GetTask() override;

    void AddTask(GCTask *task) override;

    void Finalize() override;

    void Signal() override
    {
        os::memory::LockHolder lock(lock_);
        cond_var_.Signal();
    }

    bool WaitForGCTask() override
    {
        os::memory::LockHolder lock(lock_);
        return cond_var_.TimedWait(&lock_, GC_WAIT_TIMEOUT);
    }

private:
    class CompareByTime {
    public:
        bool operator()(const GCTask *left, const GCTask *right) const
        {
            return left->GetTargetTime() > right->GetTargetTime();
        }
    };

    GC *gc_;
    os::memory::Mutex lock_;
    PandaPriorityQueue<GCTask *, PandaVector<GCTask *>, CompareByTime> queue_ GUARDED_BY(lock_);
    os::memory::ConditionVariable cond_var_;
    const char *queue_name_ = "GC queue ordered by time";
    bool finalized = false;
};

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_GC_GC_QUEUE_H_
