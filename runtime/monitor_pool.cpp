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

#include "runtime/monitor_pool.h"

#include "runtime/include/object_header.h"
#include "runtime/include/runtime.h"
#include "runtime/include/panda_vm.h"
#include "runtime/mark_word.h"
#include "runtime/monitor.h"

namespace panda {

Monitor *MonitorPool::CreateMonitor(PandaVM *vm, ObjectHeader *obj)
{
    MonitorPool *pool = vm->GetMonitorPool();

    os::memory::LockHolder lock(pool->pool_lock_);
    for (Monitor::MonitorId i = 0; i < MAX_MONITOR_ID; i++) {
        pool->last_id_ = (pool->last_id_ + 1) % MAX_MONITOR_ID;
        if (pool->monitors_.count(pool->last_id_) == 0) {
            auto monitor = pool->allocator_->New<Monitor>(pool->last_id_);
            if (monitor == nullptr) {
                return nullptr;
            }
            (pool->monitors_)[pool->last_id_] = monitor;
            monitor->SetObject(obj);
            return monitor;
        }
    }
    LOG(FATAL, RUNTIME) << "Out of MonitorPool indexes";
    UNREACHABLE();
}

Monitor *MonitorPool::LookupMonitor(PandaVM *vm, Monitor::MonitorId id)
{
    MonitorPool *pool = vm->GetMonitorPool();
    os::memory::LockHolder lock(pool->pool_lock_);
    if (pool->monitors_.count(id) == 0) {
        return nullptr;
    }
    return (pool->monitors_)[id];
}

void MonitorPool::FreeMonitor(PandaVM *vm, Monitor::MonitorId id)
{
    MonitorPool *pool = vm->GetMonitorPool();
    os::memory::LockHolder lock(pool->pool_lock_);
    auto monitor = (pool->monitors_)[id];
    pool->monitors_.erase(id);
    pool->allocator_->Delete(monitor);
}

void MonitorPool::DeflateMonitors()
{
    os::memory::LockHolder lock(pool_lock_);
    for (auto monitor_iter = monitors_.begin(); monitor_iter != monitors_.end();) {
        auto monitor = monitor_iter->second;
        if (monitor->DeflateInternal()) {
            monitor_iter = monitors_.erase(monitor_iter);
            allocator_->Delete(monitor);
        } else {
            monitor_iter++;
        }
    }
}

}  // namespace panda
