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

#include <gtest/gtest.h>

#include "runtime/include/runtime.h"
#include "runtime/thread_pool.h"

namespace panda::test {

class MockThreadPoolTest : public testing::Test {
public:
    static const size_t TASK_NUMBER = 32;
    MockThreadPoolTest()
    {
        RuntimeOptions options;
        options.SetShouldLoadBootPandaFiles(false);
        options.SetShouldInitializeIntrinsics(false);
        Runtime::Create(options);
        thread_ = panda::MTManagedThread::GetCurrent();
        thread_->ManagedCodeBegin();
    }

    ~MockThreadPoolTest()
    {
        thread_->ManagedCodeEnd();
        Runtime::Destroy();
    }

protected:
    panda::MTManagedThread *thread_ {nullptr};
};

class MockTask : public TaskInterface {
public:
    explicit MockTask(size_t identifier = 0) : identifier_(identifier) {}

    enum TaskStatus {
        NOT_STARTED,
        IN_QUEUE,
        PROCESSING,
        COMPLETED,
    };

    bool IsEmpty() const
    {
        return identifier_ == 0;
    }

    size_t GetId() const
    {
        return identifier_;
    }

    TaskStatus GetStatus() const
    {
        return status_;
    }

    void SetStatus(TaskStatus status)
    {
        status_ = status;
    }

private:
    size_t identifier_;
    TaskStatus status_ = NOT_STARTED;
};

class MockQueue : public TaskQueueInterface<MockTask> {
public:
    explicit MockQueue(mem::InternalAllocatorPtr allocator) : queue_(allocator->Adapter()) {}
    MockQueue(mem::InternalAllocatorPtr allocator, size_t queue_size)
        : TaskQueueInterface<MockTask>(queue_size), queue_(allocator->Adapter())
    {
    }

    MockTask GetTask() override
    {
        if (queue_.empty()) {
            LOG(DEBUG, RUNTIME) << "Cannot get an element, queue is empty";
            return MockTask();
        }
        auto task = queue_.front();
        queue_.pop_front();
        LOG(DEBUG, RUNTIME) << "Extract task " << task.GetId();
        return task;
    }

    // NOLINTNEXTLINE(google-default-arguments)
    void AddTask(MockTask task, [[maybe_unused]] size_t priority = 0) override
    {
        task.SetStatus(MockTask::IN_QUEUE);
        queue_.push_front(task);
    }

    void Finalize() override
    {
        queue_.clear();
    }

protected:
    size_t GetQueueSize() override
    {
        return queue_.size();
    }

private:
    PandaList<MockTask> queue_;
};

class MockTaskController {
public:
    explicit MockTaskController() {}

    void SolveTask(MockTask task)
    {
        task.SetStatus(MockTask::PROCESSING);
        // This is required to distribute tasks between different workers rather than solve it instantly
        // on only one worker.
        std::this_thread::sleep_for(std::chrono::milliseconds(10U));
        task.SetStatus(MockTask::COMPLETED);
        LOG(DEBUG, RUNTIME) << "Task " << task.GetId() << " has been solved";
        solved_tasks_++;
    }

    size_t GetSolvedTasks()
    {
        return solved_tasks_;
    }

private:
    std::atomic_size_t solved_tasks_ = 0;
};

class MockProcessor : public ProcessorInterface<MockTask, MockTaskController *> {
public:
    explicit MockProcessor(MockTaskController *controller) : controller_(controller) {}

    bool Process(MockTask task) override
    {
        if (task.GetStatus() == MockTask::IN_QUEUE) {
            controller_->SolveTask(task);
            return true;
        }
        return false;
    }

    bool Init() override
    {
        return true;
    }

    bool Destroy() override
    {
        return true;
    }

private:
    MockTaskController *controller_;
};

void CreateTasks(ThreadPool<MockTask, MockProcessor, MockTaskController *> *thread_pool, size_t number_of_elements)
{
    for (size_t i = 0; i < number_of_elements; i++) {
        MockTask task(i + 1);
        thread_pool->PutTask(task);
        LOG(DEBUG, RUNTIME) << "Queue task " << task.GetId();
    }
}

void TestThreadPool(size_t initial_number_of_threads, size_t scaled_number_of_threads, float scale_threshold)
{
    auto allocator = Runtime::GetCurrent()->GetInternalAllocator();
    auto queue = allocator->New<MockQueue>(allocator);
    auto controller = allocator->New<MockTaskController>();
    auto thread_pool = allocator->New<ThreadPool<MockTask, MockProcessor, MockTaskController *>>(
        allocator, queue, controller, initial_number_of_threads, "Test thread");

    CreateTasks(thread_pool, MockThreadPoolTest::TASK_NUMBER);

    if (scale_threshold < 1.0) {
        while (controller->GetSolvedTasks() < scale_threshold * MockThreadPoolTest::TASK_NUMBER) {
        }
        thread_pool->Scale(scaled_number_of_threads);
    }

    for (;;) {
        auto solved_tasks = controller->GetSolvedTasks();
        size_t rate = static_cast<size_t>((static_cast<float>(solved_tasks) / MockThreadPoolTest::TASK_NUMBER) * 100U);
        LOG(DEBUG, RUNTIME) << "Number of solved tasks is " << solved_tasks << " (" << rate << "%)";
        if (scale_threshold == 1.0) {
            size_t dynamic_scaling = rate / 10U + 1;
            thread_pool->Scale(dynamic_scaling);
        }

        if (solved_tasks == MockThreadPoolTest::TASK_NUMBER) {
            break;
        }
    }

    allocator->Delete(thread_pool);
    allocator->Delete(controller);
    allocator->Delete(queue);
}

TEST_F(MockThreadPoolTest, SeveralThreads)
{
    constexpr size_t NUMBER_OF_THREADS_INITIAL = 8;
    constexpr size_t NUMBER_OF_THREADS_SCALED = 8;
    constexpr float SCALE_THRESHOLD = 0.0;
    TestThreadPool(NUMBER_OF_THREADS_INITIAL, NUMBER_OF_THREADS_SCALED, SCALE_THRESHOLD);
}

TEST_F(MockThreadPoolTest, ReduceThreads)
{
    constexpr size_t NUMBER_OF_THREADS_INITIAL = 8;
    constexpr size_t NUMBER_OF_THREADS_SCALED = 4;
    constexpr float SCALE_THRESHOLD = 0.25;
    TestThreadPool(NUMBER_OF_THREADS_INITIAL, NUMBER_OF_THREADS_SCALED, SCALE_THRESHOLD);
}

TEST_F(MockThreadPoolTest, IncreaseThreads)
{
    constexpr size_t NUMBER_OF_THREADS_INITIAL = 4;
    constexpr size_t NUMBER_OF_THREADS_SCALED = 8;
    constexpr float SCALE_THRESHOLD = 0.25;
    TestThreadPool(NUMBER_OF_THREADS_INITIAL, NUMBER_OF_THREADS_SCALED, SCALE_THRESHOLD);
}

TEST_F(MockThreadPoolTest, DifferentNumberOfThreads)
{
    constexpr size_t NUMBER_OF_THREADS_INITIAL = 8;
    constexpr size_t NUMBER_OF_THREADS_SCALED = 8;
    constexpr float SCALE_THRESHOLD = 1.0;
    TestThreadPool(NUMBER_OF_THREADS_INITIAL, NUMBER_OF_THREADS_SCALED, SCALE_THRESHOLD);
}

void ControllerThreadPutTask(ThreadPool<MockTask, MockProcessor, MockTaskController *> *thread_pool,
                             size_t number_of_tasks)
{
    CreateTasks(thread_pool, number_of_tasks);
}

void ControllerThreadTryPutTask(ThreadPool<MockTask, MockProcessor, MockTaskController *> *thread_pool,
                                size_t number_of_tasks)
{
    for (size_t i = 0; i < number_of_tasks; i++) {
        MockTask task(i + 1);
        for (;;) {
            if (thread_pool->TryPutTask(task) || !thread_pool->IsActive()) {
                break;
            }
        }
    }
}

void ControllerThreadScale(ThreadPool<MockTask, MockProcessor, MockTaskController *> *thread_pool,
                           size_t number_of_threads)
{
    thread_pool->Scale(number_of_threads);
}

void ControllerThreadShutdown(ThreadPool<MockTask, MockProcessor, MockTaskController *> *thread_pool, bool is_shutdown,
                              bool is_force_shutdown)
{
    if (is_shutdown) {
        thread_pool->Shutdown(is_force_shutdown);
    }
}

void TestThreadPoolWithControllers(size_t number_of_threads_initial, size_t number_of_threads_scaled, bool is_shutdown,
                                   bool is_force_shutdown)
{
    constexpr size_t NUMBER_OF_TASKS = MockThreadPoolTest::TASK_NUMBER / 4;
    constexpr size_t QUEUE_SIZE = 16;

    auto allocator = Runtime::GetCurrent()->GetInternalAllocator();
    auto queue = allocator->New<MockQueue>(allocator, QUEUE_SIZE);
    auto controller = allocator->New<MockTaskController>();
    auto thread_pool = allocator->New<ThreadPool<MockTask, MockProcessor, MockTaskController *>>(
        allocator, queue, controller, number_of_threads_initial, "Test thread");

    std::thread controller_thread_put_task_1(ControllerThreadPutTask, thread_pool, NUMBER_OF_TASKS);
    std::thread controller_thread_put_task_2(ControllerThreadPutTask, thread_pool, NUMBER_OF_TASKS);
    std::thread controller_thread_try_put_task_1(ControllerThreadTryPutTask, thread_pool, NUMBER_OF_TASKS);
    std::thread controller_thread_try_put_task_2(ControllerThreadTryPutTask, thread_pool, NUMBER_OF_TASKS);
    std::thread controller_thread_scale_1(ControllerThreadScale, thread_pool, number_of_threads_scaled);
    std::thread controller_thread_scale_2(ControllerThreadScale, thread_pool,
                                          number_of_threads_scaled + number_of_threads_initial);
    std::thread controller_thread_shutdown_1(ControllerThreadShutdown, thread_pool, is_shutdown, is_force_shutdown);
    std::thread controller_thread_shutdown_2(ControllerThreadShutdown, thread_pool, is_shutdown, is_force_shutdown);

    // Wait for tasks completion.
    for (;;) {
        auto solved_tasks = controller->GetSolvedTasks();
        size_t rate = static_cast<size_t>((static_cast<float>(solved_tasks) / MockThreadPoolTest::TASK_NUMBER) * 100U);
        LOG(DEBUG, RUNTIME) << "Number of solved tasks is " << solved_tasks << " (" << rate << "%)";
        if (solved_tasks == MockThreadPoolTest::TASK_NUMBER || !thread_pool->IsActive()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10U));
    }
    controller_thread_put_task_1.join();
    controller_thread_put_task_2.join();
    controller_thread_try_put_task_1.join();
    controller_thread_try_put_task_2.join();
    controller_thread_scale_1.join();
    controller_thread_scale_2.join();
    controller_thread_shutdown_1.join();
    controller_thread_shutdown_2.join();

    allocator->Delete(thread_pool);
    allocator->Delete(controller);
    allocator->Delete(queue);
}

TEST_F(MockThreadPoolTest, Controllers)
{
    constexpr size_t NUMBER_OF_THREADS_INITIAL = 8;
    constexpr size_t NUMBER_OF_THREADS_SCALED = 4;
    constexpr bool IS_SHUTDOWN = false;
    constexpr bool IS_FORCE_SHUTDOWN = false;
    TestThreadPoolWithControllers(NUMBER_OF_THREADS_INITIAL, NUMBER_OF_THREADS_SCALED, IS_SHUTDOWN, IS_FORCE_SHUTDOWN);
}

TEST_F(MockThreadPoolTest, ControllersShutdown)
{
    constexpr size_t NUMBER_OF_THREADS_INITIAL = 8;
    constexpr size_t NUMBER_OF_THREADS_SCALED = 4;
    constexpr bool IS_SHUTDOWN = true;
    constexpr bool IS_FORCE_SHUTDOWN = false;
    TestThreadPoolWithControllers(NUMBER_OF_THREADS_INITIAL, NUMBER_OF_THREADS_SCALED, IS_SHUTDOWN, IS_FORCE_SHUTDOWN);
}

TEST_F(MockThreadPoolTest, ControllersForceShutdown)
{
    constexpr size_t NUMBER_OF_THREADS_INITIAL = 8;
    constexpr size_t NUMBER_OF_THREADS_SCALED = 4;
    constexpr bool IS_SHUTDOWN = true;
    constexpr bool IS_FORCE_SHUTDOWN = true;
    TestThreadPoolWithControllers(NUMBER_OF_THREADS_INITIAL, NUMBER_OF_THREADS_SCALED, IS_SHUTDOWN, IS_FORCE_SHUTDOWN);
}

}  // namespace panda::test
