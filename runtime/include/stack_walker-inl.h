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

#ifndef PANDA_RUNTIME_INCLUDE_STACK_WALKER_INL_H_
#define PANDA_RUNTIME_INCLUDE_STACK_WALKER_INL_H_

#include "macros.h"
#include "stack_walker.h"
#include "runtime/include/cframe_iterators.h"

namespace panda {

template <bool objects, bool with_reg_info, typename Func>
// NOLINTNEXTLINE(google-runtime-references)
bool InvokeCallback(Func func, [[maybe_unused]] VRegInfo reg_info, Frame::VRegister &vreg)
{
    if (objects && !vreg.HasObject()) {
        return true;
    }
    if constexpr (with_reg_info) {  // NOLINT
        if (!func(reg_info, vreg)) {
            return false;
        }
    } else {  // NOLINT
        if (!func(vreg)) {
            return false;
        }
    }
    return true;
}

template <bool objects, bool with_reg_info, typename Func>
bool StackWalker::IterateRegsForIFrame(Func func)
{
    auto frame = GetIFrame();
    for (size_t i = 0; i < frame->GetSize(); i++) {
        auto &vreg = frame->GetVReg(i);
        if (objects && !vreg.HasObject()) {
            continue;
        }
        VRegInfo reg_info(0, VRegInfo::Location::SLOT,
                          vreg.HasObject() ? VRegInfo::Type::OBJECT : VRegInfo::Type::INT64, false, i);
        if (!InvokeCallback<objects, with_reg_info>(func, reg_info, vreg)) {
            return false;
        }
    }
    auto &acc = frame->GetAcc();
    VRegInfo reg_info(0, VRegInfo::Location::SLOT, acc.HasObject() ? VRegInfo::Type::OBJECT : VRegInfo::Type::INT64,
                      true, 0);
    return InvokeCallback<objects, with_reg_info>(func, reg_info, acc);
}

template <bool objects, bool with_reg_info, typename Func>
bool StackWalker::IterateRegs(Func func)
{
    ASSERT(!IsCFrame());
    return IterateRegsForIFrame<objects, with_reg_info>(func);
}

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_STACK_WALKER_INL_H_
