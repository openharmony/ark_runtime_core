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

#ifndef PANDA_VERIFICATION_UTIL_SYNCHRONIZED_H_
#define PANDA_VERIFICATION_UTIL_SYNCHRONIZED_H_

#include <utility>

#include "verification/util/callable.h"
#include "libpandabase/os/mutex.h"

#include "macros.h"

namespace panda::verifier {

template <class C, class Friend1 = C, class Friend2 = C>
class Synchronized {
    struct ConstProxy {
        ConstProxy() = delete;
        ConstProxy(const ConstProxy &) = delete;

        ConstProxy(ConstProxy &&other)
        {
            obj = other.obj;
            other.obj = nullptr;
        }

        ConstProxy(const Synchronized *param_obj) : obj {param_obj} {}

        ~ConstProxy() NO_THREAD_SAFETY_ANALYSIS
        {
            if (obj != nullptr) {
                obj->rw_lock_.Unlock();
            }
        }

        const C *operator->()
        {
            ASSERT(obj != nullptr);
            return &obj->c;
        }

        const Synchronized *obj;
    };

    struct Proxy {
        Proxy() = delete;
        Proxy(const Proxy &) = delete;

        Proxy(Proxy &&other)
        {
            obj = other.obj;
            other.obj = nullptr;
        }

        Proxy(Synchronized *param_obj) : obj {param_obj} {}

        ~Proxy() NO_THREAD_SAFETY_ANALYSIS
        {
            if (obj != nullptr) {
                obj->rw_lock_.Unlock();
            }
        }

        C *operator->()
        {
            ASSERT(obj != nullptr);
            return &obj->c;
        }

        Synchronized *obj;
    };

    C &GetObj()
    {
        return c;
    }

    const C &GetObj() const
    {
        return c;
    }

    void WriteLock() NO_THREAD_SAFETY_ANALYSIS
    {
        rw_lock_.WriteLock();
    }

    void ReadLock() NO_THREAD_SAFETY_ANALYSIS
    {
        rw_lock_.WriteLock();
    }

    void Unlock() NO_THREAD_SAFETY_ANALYSIS
    {
        rw_lock_.Unlock();
    }

    C c;

    friend Friend1;
    friend Friend2;

public:
    template <typename... Args>
    Synchronized(Args &&... args) : c(std::forward<Args>(args)...)
    {
    }

    ~Synchronized() = default;
    DEFAULT_MOVE_SEMANTIC(Synchronized);
    DEFAULT_COPY_SEMANTIC(Synchronized);

    auto operator-> () const NO_THREAD_SAFETY_ANALYSIS
    {
        rw_lock_.ReadLock();
        return ConstProxy {this};
    }

    auto operator-> () NO_THREAD_SAFETY_ANALYSIS
    {
        rw_lock_.WriteLock();
        return Proxy {this};
    }

    template <typename Handler>
    void operator()(Handler &&handler) NO_THREAD_SAFETY_ANALYSIS
    {
        rw_lock_.WriteLock();
        handler(Proxy {this});
    }

    template <typename Handler>
    void operator()(Handler &&handler) const NO_THREAD_SAFETY_ANALYSIS
    {
        rw_lock_.ReadLock();
        handler(ConstProxy {this});
    }

private:
    mutable panda::os::memory::RWLock rw_lock_;
};
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_UTIL_SYNCHRONIZED_H_
