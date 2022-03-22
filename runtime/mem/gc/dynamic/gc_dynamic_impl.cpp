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
#include "runtime/mem/gc/gc.h"
#include "runtime/mem/object_helpers-inl.h"

namespace panda::mem {

template <>
void GC::HandleObject<LANG_TYPE_DYNAMIC, false>(PandaStackTL<ObjectHeader *> *objects_stack, const ObjectHeader *object,
                                                BaseClass *base_cls)
{
    ASSERT(base_cls->IsDynamicClass());
    auto *cls = static_cast<HClass *>(base_cls);
    // Handle dyn_class
    ObjectHeader *dyn_class = cls->GetManagedObject();
    if (MarkObjectIfNotMarked(dyn_class)) {
        AddToStack(objects_stack, dyn_class);
    }
    // mark object data
    auto obj_body_size = cls->GetObjectSize() - ObjectHeader::ObjectHeaderSize();
    ASSERT(obj_body_size % TaggedValue::TaggedTypeSize() == 0);
    int num_of_fields = static_cast<int>(obj_body_size / TaggedValue::TaggedTypeSize());
    size_t addr = reinterpret_cast<uintptr_t>(object) + ObjectHeader::ObjectHeaderSize();
    for (int i = 0; i < num_of_fields; i++) {
        auto *field_addr = reinterpret_cast<TaggedType *>(addr + i * TaggedValue::TaggedTypeSize());
        TaggedValue tagged_value(*field_addr);
        if (tagged_value.IsWeak()) {
            ObjectHelpers<LANG_TYPE_DYNAMIC>::RecordDynWeakReference(this, field_addr);
            continue;
        }
        if (tagged_value.IsHeapObject()) {
            ObjectHeader *object_header = tagged_value.GetHeapObject();
            if (MarkObjectIfNotMarked(object_header)) {
                AddToStack(objects_stack, object_header);
            }
        }
    }
}

template <>
void GC::HandleClass<LANG_TYPE_DYNAMIC, false>(PandaStackTL<ObjectHeader *> *objects_stack,
                                               const coretypes::DynClass *cls)
{
    // mark Hclass Data & Prototype
    HClass *klass = const_cast<coretypes::DynClass *>(cls)->GetHClass();

    auto dynclass_dynclass = static_cast<coretypes::DynClass *>(cls->ClassAddr<HClass>()->GetManagedObject());
    ASSERT(dynclass_dynclass != nullptr);
    // klass_size is sizeof DynClass include JSHClass, which is saved in root DynClass.
    size_t klass_size = dynclass_dynclass->GetHClass()->GetObjectSize() - sizeof(coretypes::DynClass);

    uintptr_t start_addr = reinterpret_cast<uintptr_t>(klass) + sizeof(HClass);
    int num_of_fields = static_cast<int>((klass_size - sizeof(HClass)) / TaggedValue::TaggedTypeSize());
    for (int i = 0; i < num_of_fields; i++) {
        auto *field_addr = reinterpret_cast<TaggedType *>(start_addr + i * TaggedValue::TaggedTypeSize());
        TaggedValue tagged_value(*field_addr);
        if (tagged_value.IsWeak()) {
            ObjectHelpers<LANG_TYPE_DYNAMIC>::RecordDynWeakReference(this, field_addr);
            continue;
        }
        if (!tagged_value.IsHeapObject()) {
            continue;
        }
        ObjectHeader *object_header = tagged_value.GetHeapObject();
        if (MarkObjectIfNotMarked(object_header)) {
            AddToStack(objects_stack, object_header);
        }
    }
}

template <>
void GC::HandleArrayClass<LANG_TYPE_DYNAMIC, false>(PandaStackTL<ObjectHeader *> *objects_stack,
                                                    const coretypes::Array *array_object,
                                                    [[maybe_unused]] const BaseClass *cls)
{
    LOG_DEBUG_GC << "Dyn Array object: " << GetDebugInfoAboutObject(array_object);
    auto array_length = array_object->GetLength();
    ASSERT(cls->IsDynamicClass());
    size_t array_start_addr = reinterpret_cast<uintptr_t>(array_object) + coretypes::Array::GetDataOffset();
    for (coretypes::array_size_t i = 0; i < array_length; i++) {
        TaggedValue array_element(array_object->Get<TaggedType, false, true>(i));
        if (array_element.IsWeak()) {
            auto *element_addr = reinterpret_cast<TaggedType *>(array_start_addr + i * TaggedValue::TaggedTypeSize());
            ObjectHelpers<LANG_TYPE_DYNAMIC>::RecordDynWeakReference(this, element_addr);
            continue;
        }
        if (!array_element.IsHeapObject()) {
            continue;
        }
        ObjectHeader *element_object = array_element.GetHeapObject();
        if (MarkObjectIfNotMarked(element_object)) {
            AddToStack(objects_stack, element_object);
        }
    }
}

template <>
void GC::MarkInstance<LANG_TYPE_DYNAMIC, false>(PandaStackTL<ObjectHeader *> *objects_stack, const ObjectHeader *object,
                                                BaseClass *base_cls)
{
    ASSERT(base_cls->IsDynamicClass());
    auto *cls = static_cast<HClass *>(base_cls);
    // push to stack after marked, so just return here.
    if (cls->IsNativePointer() || cls->IsString()) {
        return;
    }
    if (cls->IsHClass()) {
        auto dyn_class = static_cast<const panda::coretypes::DynClass *>(object);
        HandleClass<LANG_TYPE_DYNAMIC, false>(objects_stack, dyn_class);
    } else if (cls->IsArray()) {
        auto *array_object = static_cast<const panda::coretypes::Array *>(object);
        HandleArrayClass<LANG_TYPE_DYNAMIC, false>(objects_stack, array_object, cls);
    } else {
        HandleObject<LANG_TYPE_DYNAMIC, false>(objects_stack, object, cls);
    }
}

}  // namespace panda::mem
