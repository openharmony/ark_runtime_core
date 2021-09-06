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

#ifndef PANDA_RUNTIME_INTERPRETER_ACC_VREGISTER_H_
#define PANDA_RUNTIME_INTERPRETER_ACC_VREGISTER_H_

#include <cstddef>
#include <cstdint>

#include "runtime/interpreter/frame.h"

#ifdef PANDA_ENABLE_GLOBAL_REGISTER_VARIABLES
#include "arch/global_regs.h"
#endif

namespace panda::interpreter {

#ifdef PANDA_ENABLE_GLOBAL_REGISTER_VARIABLES

class AccVRegister : public VRegisterIface<AccVRegister> {
public:
    ALWAYS_INLINE inline AccVRegister(const Frame::VRegister &other)
    {
        SetValue(other.GetValue());
        SetTag(other.GetTag());
    }
    ~AccVRegister() = default;
    DEFAULT_COPY_SEMANTIC(AccVRegister);
    DEFAULT_MOVE_SEMANTIC(AccVRegister);

    ALWAYS_INLINE inline operator panda::Frame::VRegister() const
    {
        return Frame::VRegister(GetValue(), GetTag());
    }

    ALWAYS_INLINE inline int64_t GetValue() const
    {
        return arch::regs::GetAccValue();
    }

    ALWAYS_INLINE inline void SetValue(int64_t value)
    {
        arch::regs::SetAccValue(value);
    }

    ALWAYS_INLINE inline uint64_t GetTag() const
    {
        return arch::regs::GetAccTag();
    }

    ALWAYS_INLINE inline void SetTag(uint64_t value)
    {
        arch::regs::SetAccTag(value);
    }
};

#else

using AccVRegister = Frame::VRegister;

#endif  // PANDA_ENABLE_GLOBAL_REGISTER_VARIABLES

}  // namespace panda::interpreter

#endif  // PANDA_RUNTIME_INTERPRETER_ACC_VREGISTER_H_
