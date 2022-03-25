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

#include "runtime/include/tooling/pt_reference.h"

#include "pt_reference_private.h"
#include "pt_scoped_managed_code.h"
#include "runtime/include/thread_scopes.h"
#include "runtime/mem/refstorage/reference_storage.h"
#include "runtime/mem/refstorage/global_object_storage.h"

// NOLINTNEXTLINE
#define ASSERT_IS_NATIVE_CODE() \
    ASSERT(panda::MTManagedThread::GetCurrent() != nullptr && panda::MTManagedThread::GetCurrent()->IsInNativeCode())

namespace panda::tooling {

/* static */
PtGlobalReference *PtGlobalReference::Create(PtReference *ref)
{
    ASSERT_IS_NATIVE_CODE();
    ScopedManagedCodeThread smt(MTManagedThread::GetCurrent());
    return PtCreateGlobalReference(ref);
}

/* static */
void PtGlobalReference::Remove(PtGlobalReference *globalRef)
{
    ASSERT_IS_NATIVE_CODE();
    ScopedManagedCodeThread smt(MTManagedThread::GetCurrent());
    PtDestroyGlobalReference(globalRef);
}

/* static */
PtLocalReference *PtLocalReference::Create(PtReference *ref)
{
    ASSERT_IS_NATIVE_CODE();
    ScopedManagedCodeThread smt(MTManagedThread::GetCurrent());
    return PtCreateLocalReference(PtGetObjectHeaderByReference(ref));
}

/* static */
void PtLocalReference::Remove(PtLocalReference *localRef)
{
    ASSERT_IS_NATIVE_CODE();
    ScopedManagedCodeThread smt(MTManagedThread::GetCurrent());
    PtDestroyLocalReference(localRef);
}

// ====== Private API ======

void PtPushLocalFrameFromNative()
{
    ASSERT_NATIVE_CODE();
    auto *thread = MTManagedThread::GetCurrent();
    const uint32_t MAX_localRef = 4096;
    thread->GetPtReferenceStorage()->PushLocalFrame(MAX_localRef);
}

void PtPopLocalFrameFromNative()
{
    ASSERT_NATIVE_CODE();
    auto *thread = MTManagedThread::GetCurrent();
    thread->GetPtReferenceStorage()->PopLocalFrame(nullptr);
}

PtLocalReference *PtCreateLocalReference(ObjectHeader *objectHeader)
{
    ASSERT(objectHeader != nullptr);
    ASSERT_MANAGED_CODE();
    auto *thread = MTManagedThread::GetCurrent();
    auto *rs_ref = thread->GetPtReferenceStorage()->NewRef(objectHeader, mem::Reference::ObjectType::LOCAL);
    return reinterpret_cast<PtLocalReference *>(rs_ref);
}

void PtDestroyLocalReference(const PtLocalReference *localRef)
{
    ASSERT(localRef != nullptr);
    ASSERT_MANAGED_CODE();
    auto *thread = MTManagedThread::GetCurrent();
    auto *rs_ref = reinterpret_cast<const mem::Reference *>(localRef);
    thread->GetPtReferenceStorage()->RemoveRef(rs_ref);
}

ObjectHeader *PtGetObjectHeaderByReference(const PtReference *ref)
{
    ASSERT(ref != nullptr);
    ASSERT_MANAGED_CODE();
    auto *thread = MTManagedThread::GetCurrent();
    auto *rs_ref = reinterpret_cast<const mem::Reference *>(ref);
    return thread->GetPtReferenceStorage()->GetObject(rs_ref);
}

PtGlobalReference *PtCreateGlobalReference(const ObjectHeader *objectHeader)
{
    ASSERT(objectHeader != nullptr);
    ASSERT_MANAGED_CODE();
    auto newRef =
        PandaVM::GetCurrent()->GetGlobalObjectStorage()->Add(objectHeader, mem::Reference::ObjectType::GLOBAL);
    return reinterpret_cast<PtGlobalReference *>(newRef);
}

PtGlobalReference *PtCreateGlobalReference(const PtReference *ref)
{
    ASSERT(ref != nullptr);
    ASSERT_MANAGED_CODE();
    auto objectHeader = PtGetObjectHeaderByReference(ref);
    auto newRef =
        PandaVM::GetCurrent()->GetGlobalObjectStorage()->Add(objectHeader, mem::Reference::ObjectType::GLOBAL);
    return reinterpret_cast<PtGlobalReference *>(newRef);
}

void PtDestroyGlobalReference(const PtGlobalReference *globalRef)
{
    ASSERT(globalRef != nullptr);
    ASSERT_MANAGED_CODE();
    auto ref = reinterpret_cast<const mem::Reference *>(globalRef);
    PandaVM::GetCurrent()->GetGlobalObjectStorage()->Remove(ref);
}
}  // namespace panda::tooling
