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

namespace panda::mem {

template <>
void GC::HandleObject<LANG_TYPE_STATIC, false>(PandaStackTL<ObjectHeader *> *objects_stack, const ObjectHeader *object,
                                               BaseClass *base_cls)
{
    ASSERT(!base_cls->IsDynamicClass());
    auto cls = static_cast<Class *>(base_cls);
    while (cls != nullptr) {
        // Iterate over instance fields
        uint32_t ref_num = cls->GetRefFieldsNum<false>();
        if (ref_num > 0) {
            uint32_t offset = cls->GetRefFieldsOffset<false>();
            uint32_t ref_volatile_num = cls->GetVolatileRefFieldsNum<false>();
            for (uint32_t i = 0; i < ref_num; i++, offset += ClassHelper::OBJECT_POINTER_SIZE) {
                auto *field_object = (i < ref_volatile_num) ? object->GetFieldObject<true>(offset)
                                                            : object->GetFieldObject<false>(offset);
                if (field_object != nullptr && MarkObjectIfNotMarked(field_object)) {
                    AddToStack(objects_stack, field_object);
                }
            }
        }
        cls = cls->GetBase();
    }
}
template <>
void GC::HandleClass<LANG_TYPE_STATIC, false>(PandaStackTL<ObjectHeader *> *objects_stack, Class *cls)
{
    // Iterate over static fields
    uint32_t ref_num = cls->GetRefFieldsNum<true>();
    if (ref_num > 0) {
        uint32_t offset = cls->GetRefFieldsOffset<true>();
        uint32_t ref_volatile_num = cls->GetVolatileRefFieldsNum<true>();
        for (uint32_t i = 0; i < ref_num; i++, offset += ClassHelper::OBJECT_POINTER_SIZE) {
            auto *field_object =
                (i < ref_volatile_num) ? cls->GetFieldObject<true>(offset) : cls->GetFieldObject<false>(offset);
            if (field_object != nullptr && MarkObjectIfNotMarked(field_object)) {
                AddToStack(objects_stack, field_object);
            }
        }
    }
}

template <>
void GC::HandleArrayClass<LANG_TYPE_STATIC, false>(PandaStackTL<ObjectHeader *> *objects_stack,
                                                   const coretypes::Array *array_object,
                                                   [[maybe_unused]] const BaseClass *cls)
{
    LOG_DEBUG_GC << "Array object: " << GetDebugInfoAboutObject(array_object);
    auto array_length = array_object->GetLength();

    ASSERT(!cls->IsDynamicClass());
    ASSERT(static_cast<const Class *>(cls)->IsObjectArrayClass());

    LOG_DEBUG_GC << "Iterate over: " << array_length << " elements in array";
    for (coretypes::array_size_t i = 0; i < array_length; i++) {
        auto *array_element = array_object->Get<ObjectHeader *>(i);
        if (array_element == nullptr) {
            continue;
        }
        if (MarkObjectIfNotMarked(array_element)) {
            LOG_DEBUG_GC << "Array element is not marked, add to the stack";
            AddToStack(objects_stack, array_element);
        }
    }
}

template <>
void GC::MarkInstance<LANG_TYPE_STATIC, false>(PandaStackTL<ObjectHeader *> *objects_stack, const ObjectHeader *object,
                                               BaseClass *base_cls)
{
    ASSERT(!base_cls->IsDynamicClass());
    auto cls = static_cast<Class *>(base_cls);
    if (IsReference(cls, object)) {
        ProcessReference(objects_stack, cls, object);
    } else if (cls->IsObjectArrayClass()) {
        auto *array_object = static_cast<const panda::coretypes::Array *>(object);
        HandleArrayClass<LANG_TYPE_STATIC, false>(objects_stack, array_object, cls);
    } else if (cls->IsClassClass()) {
        // Handle Class handles static fields only, so we need to Handle regular fields explicitly too
        auto object_cls = panda::Class::FromClassObject(object);
        if (object_cls->IsInitializing() || object_cls->IsInitialized()) {
            HandleClass<LANG_TYPE_STATIC, false>(objects_stack, object_cls);
        }
        HandleObject<LANG_TYPE_STATIC, false>(objects_stack, object, cls);
    } else if (cls->IsInstantiable()) {
        HandleObject<LANG_TYPE_STATIC, false>(objects_stack, object, cls);
    } else {
        if (!cls->IsPrimitive()) {
            LOG(FATAL, GC) << "Wrong handling, missed type: " << cls->GetDescriptor();
        }
    }
}

}  // namespace panda::mem
