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

#ifndef PANDA_RUNTIME_HANDLE_BASE_H_
#define PANDA_RUNTIME_HANDLE_BASE_H_

#include "runtime/include/coretypes/tagged_value.h"

namespace panda {
template <typename T>
class EscapeHandleScope;

/*
 * HandleBase: A HandleBase provides a reference to an object that survives relocation by the garbage collector.
 *
 * HandleScope: Handles are only valid within a HandleScope. When a Basehandle is created for an object a cell is
 * allocated in the current HandleScope.
 *
 * HandleStorage: HandleStorage is the storage structure of the object pointer. GC will use the stored pointer as root
 * and update the stored value after the object is moved
 *
 *  HandleBase ---- HandleStorage -----  heap
 *    |               |               |
 * address-----> store: T*  ------> object
 *
 *    {
 *      HandleScope scope2(thread);
 *      JHandle<T> jhandle(thread, obj4);
 *      JHandle<T> jhandle(thread, obj5);
 *      JHandle<T> jhandle(thread, obj6);
 *      JHandle<T> jhandle(thread, obj7);
 *    }
 *
 *  // out of scope, The obj pointer in node will be free (obj7, obj6, obj5, obj4) and PopTopNode(top_node = prev_node)
 *
 *      |        |          |  obj5   |
 *      |        | scope2-> |  obj4   |
 *      |        |          |  obj3   |
 *      |  obj7  |          |  obj2   |
 *      |__obj6__| scope1-> |__obj1___|
 *       top_node --------->  prev_node------>nullptr
 *
 *  example:
 *      JSHandle<T> handle;
 *      {
 *          HandleScope(thread);
 *          JSHandle<T> jshandle(thread, T*); // JSHandle extend Handle;
 *          JHandle<T> jhandle(thread, T*);
 *          jshandle->method();  // to invoke method of T
 *          handle = jshandle;
 *      }
 *      handle->method(); // error! do not used handle out of scope
 */
class HandleBase {
public:
    HandleBase() : address_(reinterpret_cast<uintptr_t>(nullptr)) {}
    ~HandleBase() = default;
    DEFAULT_NOEXCEPT_MOVE_SEMANTIC(HandleBase);
    DEFAULT_COPY_SEMANTIC(HandleBase);

    inline uintptr_t GetAddress() const
    {
        return address_;
    }

    template <typename T>
    explicit HandleBase(ManagedThread *thread, T value);

protected:
    explicit HandleBase(uintptr_t addr) : address_(addr) {}

    uintptr_t address_;  // NOLINT(misc-non-private-member-variables-in-classes)

    template <typename T>
    friend class EscapeHandleScope;
};

}  // namespace panda

#endif  // PANDA_RUNTIME_HANDLE_BASE_H_
