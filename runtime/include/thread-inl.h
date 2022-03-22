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

#ifndef PANDA_RUNTIME_INCLUDE_THREAD_INL_H_
#define PANDA_RUNTIME_INCLUDE_THREAD_INL_H_

#include "runtime/handle_base.h"
#include "runtime/global_handle_storage-inl.h"
#include "runtime/handle_storage-inl.h"
#include "runtime/include/thread.h"

namespace panda {

template <>
inline void ManagedThread::PushHandleScope<coretypes::TaggedType>(HandleScope<coretypes::TaggedType> *handle_scope)
{
    tagged_handle_scopes_.push_back(handle_scope);
}

template <>
inline HandleScope<coretypes::TaggedType> *ManagedThread::PopHandleScope<coretypes::TaggedType>()
{
    HandleScope<coretypes::TaggedType> *scope = tagged_handle_scopes_.back();
    tagged_handle_scopes_.pop_back();
    return scope;
}

template <>
inline HandleScope<coretypes::TaggedType> *ManagedThread::GetTopScope<coretypes::TaggedType>() const
{
    if (tagged_handle_scopes_.empty()) {
        return nullptr;
    }
    return tagged_handle_scopes_.back();
}

template <>
inline HandleStorage<coretypes::TaggedType> *ManagedThread::GetHandleStorage<coretypes::TaggedType>() const
{
    return tagged_handle_storage_;
}

template <>
inline GlobalHandleStorage<coretypes::TaggedType> *ManagedThread::GetGlobalHandleStorage<coretypes::TaggedType>() const
{
    return tagged_global_handle_storage_;
}

template <>
inline void ManagedThread::PushHandleScope<ObjectHeader *>(HandleScope<ObjectHeader *> *handle_scope)
{
    object_header_handle_scopes_.push_back(handle_scope);
}

template <>
inline HandleScope<ObjectHeader *> *ManagedThread::PopHandleScope<ObjectHeader *>()
{
    HandleScope<ObjectHeader *> *scope = object_header_handle_scopes_.back();
    object_header_handle_scopes_.pop_back();
    return scope;
}

template <>
inline HandleScope<ObjectHeader *> *ManagedThread::GetTopScope<ObjectHeader *>() const
{
    if (object_header_handle_scopes_.empty()) {
        return nullptr;
    }
    return object_header_handle_scopes_.back();
}

template <>
inline HandleStorage<ObjectHeader *> *ManagedThread::GetHandleStorage<ObjectHeader *>() const
{
    return object_header_handle_storage_;
}

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_THREAD_INL_H_
