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

#ifndef PANDA_RUNTIME_TOOLING_PT_THREAD_INFO_H_
#define PANDA_RUNTIME_TOOLING_PT_THREAD_INFO_H_

#include <cstdint>

#include "runtime/include/tooling/pt_object.h"
#include "pt_reference_private.h"
#include "pt_hook_type_info.h"

namespace panda::tooling {
class PtThreadInfo {
public:
    PtThreadInfo() = default;

    ~PtThreadInfo()
    {
        ASSERT(pt_exception_ref_ == nullptr);
        ASSERT(managed_thread_ref_ == nullptr);
    }

    PtHookTypeInfo &GetHookTypeInfo()
    {
        return hook_type_info_;
    }

    bool GetPtIsEnteredFlag() const
    {
        return pt_is_entered_flag_;
    }

    void SetPtIsEnteredFlag(bool flag)
    {
        pt_is_entered_flag_ = flag;
    }

    bool GetPtActiveExceptionThrown() const
    {
        return pt_active_exception_thrown_;
    }

    void SetPtActiveExceptionThrown(bool value)
    {
        pt_active_exception_thrown_ = value;
    }

    void SetCurrentException(PtObject object)
    {
        ASSERT(pt_exception_ref_ == nullptr);
        pt_exception_ref_ = PtCreateGlobalReference(object.GetReference());
    }

    void ResetCurrentException()
    {
        ASSERT(pt_exception_ref_ != nullptr);
        PtDestroyGlobalReference(pt_exception_ref_);
        pt_exception_ref_ = nullptr;
    }

    PtObject GetCurrentException() const
    {
        return PtObject(pt_exception_ref_);
    }

    void SetThreadObjectHeader(const ObjectHeader *threadObjectHeader)
    {
        ASSERT(managed_thread_ref_ == nullptr);
        managed_thread_ref_ = PtCreateGlobalReference(threadObjectHeader);
    }

    void Destroy()
    {
        if (managed_thread_ref_ != nullptr) {
            PtDestroyGlobalReference(managed_thread_ref_);
            managed_thread_ref_ = nullptr;
        }

        if (pt_exception_ref_ != nullptr) {
            PtDestroyGlobalReference(pt_exception_ref_);
            pt_exception_ref_ = nullptr;
        }
    }

    PtReference *GetThreadRef() const
    {
        return managed_thread_ref_;
    }

private:
    PtHookTypeInfo hook_type_info_ {false};
    bool pt_is_entered_flag_ {false};
    bool pt_active_exception_thrown_ {false};
    PtGlobalReference *pt_exception_ref_ {nullptr};
    PtGlobalReference *managed_thread_ref_ {nullptr};

    NO_COPY_SEMANTIC(PtThreadInfo);
    NO_MOVE_SEMANTIC(PtThreadInfo);
};

}  // namespace panda::tooling

#endif  // PANDA_RUNTIME_TOOLING_PT_THREAD_INFO_H_
