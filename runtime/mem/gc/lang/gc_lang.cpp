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

#include "runtime/include/panda_vm.h"
#include "runtime/mem/gc/lang/gc_lang.h"
#include "runtime/mem/object_helpers-inl.h"
#include "runtime/mem/gc/dynamic/gc_dynamic_data.h"

namespace panda::mem {

template <class LanguageConfig>
GCLang<LanguageConfig>::GCLang(ObjectAllocatorBase *object_allocator, const GCSettings &settings)
    : GC(object_allocator, settings)
{
    if constexpr (LanguageConfig::LANG_TYPE == LANG_TYPE_DYNAMIC) {  // NOLINT
        auto allocator = GetInternalAllocator();
        GCDynamicData *data = allocator->template New<GCDynamicData>(allocator);
        SetExtensionData(data);
    }
}

template <class LanguageConfig>
GCLang<LanguageConfig>::~GCLang()
{
    GCExtensionData *data = GetExtensionData();
    if (GetExtensionData() != nullptr) {
        InternalAllocatorPtr allocator = GetInternalAllocator();
        allocator->Delete(data);
    }
}

template <class LanguageConfig>
size_t GCLang<LanguageConfig>::VerifyHeap()
{
    return HeapVerifier<LanguageConfig>(GetPandaVm()->GetHeapManager()).VerifyAll();
}

template <class LanguageConfig>
void GCLang<LanguageConfig>::CommonUpdateRefsToMovedObjects(const UpdateRefInAllocator &update_allocator)
{
    trace::ScopedTrace scoped_trace(__FUNCTION__);

    UpdateRefInObject update_refs_in_object([this](ObjectHeader *obj) {
        auto base_cls = obj->ClassAddr<BaseClass>();
        ObjectHelpers<LanguageConfig::LANG_TYPE>::UpdateRefsToMovedObjects(GetPandaVm(), obj, base_cls);
    });

    // Update objects in allocator
    update_allocator(update_refs_in_object);

    // Update refs in vregs
    if constexpr (LanguageConfig::MT_MODE == MT_MODE_SINGLE) {  // NOLINT
        UpdateRefsInVRegs(GetPandaVm()->GetAssociatedThread());
    } else {  // NOLINT
        GetPandaVm()->GetThreadManager()->EnumerateThreads(
            [this](ManagedThread *thread) {
                UpdateRefsInVRegs(thread);
                return true;
            },  // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE)
            static_cast<unsigned int>(EnumerationFlag::ALL));
        // Update refs inside monitors
        GetPandaVm()->GetMonitorPool()->EnumerateMonitors([this](Monitor *monitor) {
            ObjectHeader *object_header = monitor->GetObject();
            if (object_header != nullptr) {
                MarkWord mark_word = object_header->AtomicGetMark();
                if (mark_word.GetState() == MarkWord::ObjectState::STATE_GC) {
                    MarkWord::markWordSize addr = mark_word.GetForwardingAddress();
                    LOG_DEBUG_GC << "Update monitor " << std::hex << monitor << " object, old val = 0x" << std::hex
                                 << object_header << ", new val = 0x" << addr;
                    monitor->SetObject(reinterpret_cast<ObjectHeader *>(addr));
                }
            }
            return true;
        });
    }
    // Update string table
    GetPandaVm()->GetStringTable()->UpdateMoved();

    // Update thread locals
    UpdateThreadLocals();
    // Update refs in vm
    UpdateVmRefs();
    // Update refs in class linker contexts
    UpdateClassLinkerContextRoots();
    // Update global refs
    UpdateGlobalObjectStorage();
}

template <class LanguageConfig>
void GCLang<LanguageConfig>::PreRunPhasesImpl()
{
    // NOLINTNEXTLINE(readability-braces-around-statements, bugprone-suspicious-semicolon)
    if constexpr (LanguageConfig::MT_MODE == MT_MODE_MULTI) {
        // Run monitor deflation first
        GetPandaVm()->GetMonitorPool()->DeflateMonitors();
        // Delete unused thread structures
        GetPandaVm()->GetThreadManager()->DeleteFinishedThreads();
    }
}

template class GCLang<PandaAssemblyLanguageConfig>;

}  // namespace panda::mem
