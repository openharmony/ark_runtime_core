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

#ifndef PANDA_RUNTIME_HANDLE_SCOPE_INL_H_
#define PANDA_RUNTIME_HANDLE_SCOPE_INL_H_

#include "runtime/handle_scope.h"
#include "runtime/include/thread-inl.h"

namespace panda {
template <typename T>
inline HandleScope<T>::HandleScope(ManagedThread *thread) : thread_(thread)
{
    HandleScope<T> *topScope = thread->GetTopScope<T>();
    if (topScope != nullptr) {
        beginIndex_ = topScope->GetBeginIndex() + topScope->GetHandleCount();
    }
    thread->PushHandleScope<T>(this);
}

template <typename T>
inline HandleScope<T>::HandleScope(ManagedThread *thread, T value) : thread_(thread)
{
    HandleScope<T> *topScope = thread->GetTopScope<T>();
    ASSERT(topScope != nullptr);
    topScope->NewHandle(value);
    beginIndex_ = topScope->GetBeginIndex() + topScope->GetHandleCount();
    thread->PushHandleScope<T>(this);
}

template <typename T>
inline HandleScope<T>::~HandleScope()
{
    thread_->PopHandleScope<T>();
    thread_->GetHandleStorage<T>()->FreeHandles(beginIndex_);
}

template <typename T>
inline ManagedThread *HandleScope<T>::GetThread() const
{
    return thread_;
}

template <typename T>
inline EscapeHandleScope<T>::EscapeHandleScope(ManagedThread *thread)
    : HandleScope<T>(thread, 0),
      escapeHandle_(thread->GetHandleStorage<T>()->GetNodeAddress(thread->GetTopScope<T>()->GetBeginIndex() - 1))
{
}

}  // namespace panda

#endif  // PANDA_RUNTIME_HANDLE_SCOPE_INL_H_
