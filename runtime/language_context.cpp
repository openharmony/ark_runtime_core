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

#include "runtime/include/language_context.h"

#include "runtime/handle_scope-inl.h"
#include "runtime/include/method.h"
#include "runtime/include/runtime.h"
#include "runtime/include/stack_walker.h"
#include "runtime/include/thread.h"
#include "runtime/mem/vm_handle.h"

namespace panda {
std::pair<Method *, uint32_t> LanguageContextBase::GetCatchMethodAndOffset(Method *method, ManagedThread *thread) const
{
    uint32_t catchOffset = 0;
    Method *catchMethod = method;
    StackWalker stack(thread);
    while (stack.HasFrame()) {
        catchMethod = stack.GetMethod();
        if (catchMethod->GetPandaFile() == nullptr) {
            stack.NextFrame();
            continue;
        }
        if (stack.IsCFrame()) {
            stack.NextFrame();
            continue;
        }
        catchOffset = catchMethod->FindCatchBlock(thread->GetException()->ClassAddr<Class>(), stack.GetBytecodePc());

        if (catchOffset != panda_file::INVALID_OFFSET) {
            break;
        }
        stack.NextFrame();
    }

    return std::make_pair(catchMethod, catchOffset);
}

std::unique_ptr<ClassLinkerExtension> LanguageContextBase::CreateClassLinkerExtension() const
{
    return nullptr;
}

void LanguageContextBase::ThrowException([[maybe_unused]] ManagedThread *thread,
                                         [[maybe_unused]] const uint8_t *mutf8_name,
                                         [[maybe_unused]] const uint8_t *mutf8_msg) const
{
}

const uint8_t *LanguageContextBase::GetErrorClassDescriptor() const
{
    return nullptr;
}

PandaUniquePtr<ITableBuilder> LanguageContextBase::CreateITableBuilder() const
{
    return nullptr;
}

PandaUniquePtr<VTableBuilder> LanguageContextBase::CreateVTableBuilder() const
{
    return nullptr;
}

PandaUniquePtr<tooling::PtLangExt> LanguageContextBase::CreatePtLangExt() const
{
    return nullptr;
}

void LanguageContextBase::SetExceptionToVReg(
    [[maybe_unused]] Frame::VRegister &vreg,  // NOLINTNEXTLINE(google-runtime-references)
    [[maybe_unused]] ObjectHeader *obj) const
{
}

}  // namespace panda
