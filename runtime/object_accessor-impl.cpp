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

#ifndef PANDA_RUNTIME_OBJECT_ACCESSOR_IMPL_CPP_
#define PANDA_RUNTIME_OBJECT_ACCESSOR_IMPL_CPP_
// This file is included by interpreter to inline methods that defined in it.
// We don't move them to object_accessor-inl.h because including runtime.h
// will lead to circular header dependencies.

#include "runtime/include/object_accessor-inl.h"
#include "runtime/include/runtime.h"
#include "runtime/include/panda_vm.h"

namespace panda {

/* static */
#ifndef PANDA_RUNTIME_OBJECT_ACCESSOR_CPP_
inline
#endif
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION__RETURNTYPE_QUALIFIER_NAME_ONELINE)
    mem::GCBarrierSet *
    ObjectAccessor::GetBarrierSet()
{
    return Thread::GetCurrent()->GetVM()->GetGC()->GetBarrierSet();
}

/* static */
#ifndef PANDA_RUNTIME_OBJECT_ACCESSOR_CPP_
inline
#endif
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION__RETURNTYPE_QUALIFIER_NAME_ONELINE)
    mem::GCBarrierSet *
    ObjectAccessor::GetBarrierSet(const ManagedThread *thread)
{
    return thread->GetVM()->GetGC()->GetBarrierSet();
}

/* static */
#ifndef PANDA_RUNTIME_OBJECT_ACCESSOR_CPP_
inline
#endif
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION__RETURNTYPE_QUALIFIER_NAME_ONELINE)
    mem::BarrierType
    ObjectAccessor::GetPreBarrierType(const ManagedThread *thread)
{
    return thread->GetPreBarrierType();
}

/* static */
#ifndef PANDA_RUNTIME_OBJECT_ACCESSOR_CPP_
inline
#endif
    // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION__RETURNTYPE_QUALIFIER_NAME_ONELINE)
    mem::BarrierType
    ObjectAccessor::GetPostBarrierType(const ManagedThread *thread)
{
    return thread->GetPostBarrierType();
}

}  // namespace panda

#endif  // PANDA_RUNTIME_OBJECT_ACCESSOR_IMPL_CPP_
