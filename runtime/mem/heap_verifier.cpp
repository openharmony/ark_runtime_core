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
#include "runtime/include/class_root.h"
#include "runtime/include/runtime.h"
#include "runtime/include/panda_vm.h"
#include "runtime/mem/gc/gc_root.h"
#include "runtime/mem/heap_manager.h"
#include "runtime/mem/heap_verifier.h"

namespace panda::mem {

// Should be called only with MutatorLock held
template <class LanguageConfig>
size_t HeapVerifier<LanguageConfig>::VerifyAllPaused() const
{
    Rendezvous *rendezvous = Runtime::GetCurrent()->GetPandaVM()->GetRendezvous();
    rendezvous->SafepointBegin();
    size_t fail_count = VerifyAll();
    rendezvous->SafepointEnd();
    return fail_count;
}

template <LangTypeT LangType>
void HeapObjectVerifier<LangType>::operator()(ObjectHeader *obj)
{
    HeapReferenceVerifier ref_verifier(HEAP, FAIL_COUNT);
    ObjectHelpers<LangType>::TraverseAllObjects(obj, ref_verifier);
}

void HeapReferenceVerifier::operator()([[maybe_unused]] ObjectHeader *object_header, ObjectHeader *referent)
{
    auto obj_allocator = HEAP->GetObjectAllocator().AsObjectAllocator();
    if (!obj_allocator->IsLive(referent)) {
        LOG_HEAP_VERIFIER(ERROR) << "Heap corruption found! Heap references a dead object at " << referent;
        ++(*FAIL_COUNT);
    }
}

void HeapReferenceVerifier::operator()(const GCRoot &root)
{
    auto obj_allocator = HEAP->GetObjectAllocator().AsObjectAllocator();
    auto referent = root.GetObjectHeader();
    if (!obj_allocator->IsLive(referent)) {
        LOG_HEAP_VERIFIER(ERROR) << "Heap corruption found! Root references a dead object at " << referent;
        ++(*FAIL_COUNT);
    }
}

template <class LanguageConfig>
bool HeapVerifier<LanguageConfig>::IsValidObjectAddress(void *addr) const
{
    return IsAligned<DEFAULT_ALIGNMENT_IN_BYTES>(ToUintPtr(addr)) && IsHeapAddress(addr);
}

template <class LanguageConfig>
bool HeapVerifier<LanguageConfig>::IsHeapAddress(void *addr) const
{
    return heap_->GetObjectAllocator().AsObjectAllocator()->ContainObject(reinterpret_cast<ObjectHeader *>(addr));
}

template <class LanguageConfig>
size_t HeapVerifier<LanguageConfig>::VerifyHeap() const
{
    return heap_->VerifyHeapReferences();
}

template <class LanguageConfig>
size_t HeapVerifier<LanguageConfig>::VerifyRoot() const
{
    RootManager<LanguageConfig> root_manager;
    size_t fail_count = 0;
    root_manager.SetPandaVM(heap_->GetPandaVM());
    root_manager.VisitNonHeapRoots([this, &fail_count](const GCRoot &root) {
        if (root.GetType() == RootType::ROOT_FRAME || root.GetType() == RootType::ROOT_THREAD) {
            auto base_cls = root.GetObjectHeader()->ClassAddr<BaseClass>();
            if (!(!base_cls->IsDynamicClass() && static_cast<Class *>(base_cls)->IsClassClass())) {
                HeapReferenceVerifier(heap_, &fail_count)(root);
            }
        }
    });

    return fail_count;
}

template class HeapVerifier<PandaAssemblyLanguageConfig>;

}  // namespace panda::mem
