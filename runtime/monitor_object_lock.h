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

#ifndef PANDA_RUNTIME_MONITOR_OBJECT_LOCK_H_
#define PANDA_RUNTIME_MONITOR_OBJECT_LOCK_H_

#include "runtime/monitor.h"
#include "runtime/handle_scope.h"
#include "runtime/mem/vm_handle.h"

namespace panda {

class ObjectLock {
public:
    explicit ObjectLock(ObjectHeader *obj);

    void Wait(bool ignore_interruption = false);

    void TimedWait(uint64_t timeout);

    void Notify();

    void NotifyAll();

    ~ObjectLock();

    NO_COPY_SEMANTIC(ObjectLock);
    NO_MOVE_SEMANTIC(ObjectLock);

private:
    HandleScope<ObjectHeader *> scope_;
    VMHandle<ObjectHeader> obj_handler_;
};

}  // namespace panda

#endif  // PANDA_RUNTIME_MONITOR_OBJECT_LOCK_H_
