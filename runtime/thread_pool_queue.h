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

#ifndef PANDA_RUNTIME_THREAD_POOL_QUEUE_H_
#define PANDA_RUNTIME_THREAD_POOL_QUEUE_H_

static constexpr size_t QUEUE_SIZE_MAX_SIZE = 1000;

namespace panda {

class TaskInterface {
public:
    bool IsEmpty();
};

template <typename Task>
class TaskQueueInterface {
public:
    // All methods (except for Finalize) require an acquired lock from a thread pool.
    explicit TaskQueueInterface(size_t queue_max_size = QUEUE_SIZE_MAX_SIZE) : queue_max_size_(queue_max_size) {}
    virtual ~TaskQueueInterface() = default;

    NO_COPY_SEMANTIC(TaskQueueInterface);
    NO_MOVE_SEMANTIC(TaskQueueInterface);

    virtual Task GetTask() = 0;

    // NOLINTNEXTLINE(google-default-arguments)
    virtual void AddTask(Task task, size_t priority = 0) = 0;
    virtual void Finalize() = 0;

    bool TryAddTask(Task task, size_t priority = 0)
    {
        if (IsFull()) {
            return false;
        }
        AddTask(task, priority);
        return true;
    }

    bool IsEmpty()
    {
        return GetQueueSize() == 0;
    }

    bool IsFull()
    {
        return GetQueueSize() >= queue_max_size_;
    }

protected:
    virtual size_t GetQueueSize() = 0;

private:
    const size_t queue_max_size_;
};

}  // namespace panda

#endif  // PANDA_RUNTIME_THREAD_POOL_QUEUE_H_
