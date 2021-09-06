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

#include "runtime/monitor_object_lock.h"

#include "libpandabase/os/thread.h"
#include "runtime/include/thread.h"
#include "runtime/handle_scope-inl.h"

namespace panda {

ObjectLock::ObjectLock(ObjectHeader *obj)
    : scope_(HandleScope<ObjectHeader *>(ManagedThread::GetCurrent())),
      obj_handler_(VMHandle<ObjectHeader>(ManagedThread::GetCurrent(), obj))
{
    [[maybe_unused]] auto res = Monitor::MonitorEnter(obj_handler_.GetPtr());
    ASSERT(res == Monitor::State::OK);
}

void ObjectLock::Wait(bool ignore_interruption)
{
    Monitor::State state = Monitor::Wait(obj_handler_.GetPtr(), ThreadStatus::IS_WAITING, 0, 0, ignore_interruption);
    LOG_IF(state == Monitor::State::ILLEGAL, FATAL, RUNTIME) << "Monitor::Wait() failed";
}

void ObjectLock::TimedWait(uint64_t timeout)
{
    Monitor::State state = Monitor::Wait(obj_handler_.GetPtr(), ThreadStatus::IS_TIMED_WAITING, timeout, 0);
    LOG_IF(state == Monitor::State::ILLEGAL, FATAL, RUNTIME) << "Monitor::Wait() failed";
}

void ObjectLock::Notify()
{
    Monitor::State state = Monitor::Notify(obj_handler_.GetPtr());
    LOG_IF(state != Monitor::State::OK, FATAL, RUNTIME) << "Monitor::Notify() failed";
}

void ObjectLock::NotifyAll()
{
    Monitor::State state = Monitor::NotifyAll(obj_handler_.GetPtr());
    LOG_IF(state != Monitor::State::OK, FATAL, RUNTIME) << "Monitor::NotifyAll() failed";
}

ObjectLock::~ObjectLock()
{
    [[maybe_unused]] auto res = Monitor::MonitorExit(obj_handler_.GetPtr());
    ASSERT(res == Monitor::State::OK);
}

}  // namespace panda
