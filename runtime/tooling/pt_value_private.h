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

#ifndef PANDA_RUNTIME_TOOLING_PT_VALUE_PRIVATE_H_
#define PANDA_RUNTIME_TOOLING_PT_VALUE_PRIVATE_H_

#include "macros.h"
#include "include/thread.h"
#include "runtime/interpreter/frame.h"
#include "runtime/include/tooling/pt_value.h"
#include "pt_lang_ext_private.h"

namespace panda::tooling {
class PtValuePrivate {
public:
    PtValuePrivate(PtLangExtPrivate *ext, Frame::VRegister *vreg) : ext_(ext)
    {
        ASSERT_MANAGED_CODE();
        [[maybe_unused]] auto ret = ext_->GetPtValueFromManaged(*vreg, &value_);
        ASSERT(!ret);
    }

    ~PtValuePrivate()
    {
        ASSERT_MANAGED_CODE();
        ext_->ReleasePtValueFromManaged(&value_);
    }

    PtValue GetValue() const
    {
        return value_;
    }

    NO_COPY_SEMANTIC(PtValuePrivate);
    NO_MOVE_SEMANTIC(PtValuePrivate);

private:
    PtLangExtPrivate *ext_;
    PtValue value_;
};

}  // namespace panda::tooling

#endif  // PANDA_RUNTIME_TOOLING_PT_VALUE_PRIVATE_H_
