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

#ifndef PANDA_RUNTIME_MEM_OBJECT_HELPERS_INL_H_
#define PANDA_RUNTIME_MEM_OBJECT_HELPERS_INL_H_

#include "runtime/mem/object_helpers.h"
#include "runtime/include/class.h"
#include "runtime/include/coretypes/array-inl.h"
#include "runtime/include/coretypes/dyn_objects.h"
#include "runtime/include/coretypes/string.h"
#include "runtime/include/coretypes/class.h"
#include "runtime/include/hclass.h"

namespace panda::mem {

class ClassFieldVisitor {
public:
    explicit ClassFieldVisitor(const ObjectVisitorEx &visitor) : visitor_(visitor) {}
    ALWAYS_INLINE void operator()([[maybe_unused]] Class *cls, ObjectHeader *field_object,
                                  [[maybe_unused]] uint32_t offset, [[maybe_unused]] bool is_volatile) const
    {
        return visitor_(cls->GetManagedObject(), field_object);
    }

    virtual ~ClassFieldVisitor() = default;

    NO_COPY_SEMANTIC(ClassFieldVisitor);
    NO_MOVE_SEMANTIC(ClassFieldVisitor);

private:
    const ObjectVisitorEx &visitor_;
};

class ObjectFieldVisitor {
public:
    explicit ObjectFieldVisitor(const ObjectVisitorEx &visitor) : visitor_(visitor) {}
    ALWAYS_INLINE void operator()([[maybe_unused]] ObjectHeader *object, ObjectHeader *field_object,
                                  [[maybe_unused]] uint32_t offset, [[maybe_unused]] bool is_volatile) const
    {
        return visitor_(object, field_object);
    }

    virtual ~ObjectFieldVisitor() = default;

    NO_COPY_SEMANTIC(ObjectFieldVisitor);
    NO_MOVE_SEMANTIC(ObjectFieldVisitor);

private:
    const ObjectVisitorEx &visitor_;
};

class ArrayElementVisitor {
public:
    explicit ArrayElementVisitor(const ObjectVisitorEx &visitor) : visitor_(visitor) {}
    ALWAYS_INLINE void operator()([[maybe_unused]] ObjectHeader *array_object,
                                  [[maybe_unused]] array_size_t element_index, ObjectHeader *element_object) const
    {
        return visitor_(array_object, element_object);
    }

    virtual ~ArrayElementVisitor() = default;

    NO_COPY_SEMANTIC(ArrayElementVisitor);
    NO_MOVE_SEMANTIC(ArrayElementVisitor);

private:
    const ObjectVisitorEx &visitor_;
};

template <typename FieldVisitor>
void TraverseFields(const Span<Field> &fields, Class *cls, ObjectHeader *object_header,
                    const FieldVisitor &field_visitor)
{
    for (const Field &field : fields) {
        LOG(DEBUG, GC) << " current field \"" << GetFieldName(field) << "\"";
        size_t offset = field.GetOffset();
        panda_file::Type::TypeId type_id = field.GetType().GetId();
        if (type_id == panda_file::Type::TypeId::REFERENCE) {
            ObjectHeader *field_object = object_header->GetFieldObject(offset);
            if (field_object != nullptr) {
                LOG(DEBUG, GC) << " field val = " << std::hex << field_object;
                field_visitor(cls, object_header, &field, field_object);
            } else {
                LOG(DEBUG, GC) << " field val = nullptr";
            }
        }
    }
}

template <typename FieldVisitor>
void TraverseClass(Class *cls, const FieldVisitor &field_visitor)
{
    // Iterate over static fields
    uint32_t ref_num = cls->GetRefFieldsNum<true>();
    if (ref_num > 0) {
        uint32_t offset = cls->GetRefFieldsOffset<true>();
        uint32_t ref_volatile_num = cls->GetVolatileRefFieldsNum<true>();
        for (uint32_t i = 0; i < ref_num; i++, offset += ClassHelper::OBJECT_POINTER_SIZE) {
            bool is_volatile = (i < ref_volatile_num);
            auto *field_object = is_volatile ? cls->GetFieldObject<true>(offset) : cls->GetFieldObject<false>(offset);
            if (field_object != nullptr) {
                field_visitor(cls, field_object, offset, is_volatile);
            }
        }
    }
}

template <typename FieldVisitor>
void GCStaticObjectHelpers::TraverseObject(ObjectHeader *object, BaseClass *base_cls, const FieldVisitor &field_visitor)
{
    ASSERT(!base_cls->IsDynamicClass());
    auto *cls = static_cast<Class *>(base_cls);
    while (cls != nullptr) {
        // Iterate over instance fields
        uint32_t ref_num = cls->GetRefFieldsNum<false>();
        if (ref_num > 0) {
            uint32_t offset = cls->GetRefFieldsOffset<false>();
            uint32_t ref_volatile_num = cls->GetVolatileRefFieldsNum<false>();
            for (uint32_t i = 0; i < ref_num; i++, offset += ClassHelper::OBJECT_POINTER_SIZE) {
                bool is_volatile = (i < ref_volatile_num);
                auto *field_object =
                    is_volatile ? object->GetFieldObject<true>(offset) : object->GetFieldObject<false>(offset);
                if (field_object != nullptr) {
                    field_visitor(object, field_object, offset, is_volatile);
                }
            }
        }
        cls = cls->GetBase();
    }
}

template <typename ElementVisitor>
void GCStaticObjectHelpers::TraverseArray(ObjectHeader *object, BaseClass *base_cls,
                                          const ElementVisitor &array_element_visitor)
{
    ASSERT(!base_cls->IsDynamicClass());
    [[maybe_unused]] auto *cls = static_cast<Class *>(base_cls);
    ASSERT(cls != nullptr);
    ASSERT(cls->IsObjectArrayClass());
    auto *array_object = static_cast<coretypes::Array *>(object);
    auto array_length = array_object->GetLength();
    for (coretypes::array_size_t i = 0; i < array_length; i++) {
        auto *array_element = array_object->Get<ObjectHeader *>(i);
        if (array_element != nullptr) {
            array_element_visitor(object, i, array_element);
        }
    }
}

template <typename FieldVisitor>
void GCDynamicObjectHelpers::TraverseObject(ObjectHeader *object, BaseClass *base_cls,
                                            const FieldVisitor &field_visitor)
{
    ASSERT(base_cls->IsDynamicClass());
    auto *cls = static_cast<HClass *>(base_cls);
    ASSERT(cls != nullptr);
    LOG(DEBUG, GC) << "TraverseObject Current object: " << GetDebugInfoAboutObject(object);
    // check dynclass
    if (cls->IsHClass()) {
        auto dyn_class = coretypes::DynClass::Cast(object);
        auto klass = dyn_class->GetHClass();

        auto dynclass_dynclass = static_cast<coretypes::DynClass *>(cls->GetManagedObject());
        ASSERT(dynclass_dynclass != nullptr);
        size_t klass_size = dynclass_dynclass->GetHClass()->GetObjectSize() - sizeof(coretypes::DynClass);

        uintptr_t start_addr = reinterpret_cast<uintptr_t>(klass) + sizeof(HClass);
        int num_of_fields = static_cast<int>((klass_size - sizeof(HClass)) / TaggedValue::TaggedTypeSize());
        for (int i = 0; i < num_of_fields; i++) {
            auto *field_addr = reinterpret_cast<TaggedType *>(start_addr + i * TaggedValue::TaggedTypeSize());
            TaggedValue tagged_value(*field_addr);
            if (tagged_value.IsHeapObject()) {
                ObjectHeader *ref_object_header = tagged_value.GetRawHeapObject();
                size_t offset = ObjectHeader::ObjectHeaderSize() + sizeof(HClass) + i * TaggedValue::TaggedTypeSize();
                field_visitor(object, offset, ref_object_header, false);
            }
        }
    } else {
        // handle dynobject dyn_class
        size_t offset_class_word = ObjectHeader::GetClassOffset();
        ObjectHeader *dyn_class = cls->GetManagedObject();
        field_visitor(object, offset_class_word, dyn_class, true);

        // handle object data
        auto obj_body_size = cls->GetObjectSize() - ObjectHeader::ObjectHeaderSize();
        ASSERT(obj_body_size % TaggedValue::TaggedTypeSize() == 0);
        int num_of_fields = static_cast<int>(obj_body_size / TaggedValue::TaggedTypeSize());
        size_t addr = reinterpret_cast<uintptr_t>(object) + ObjectHeader::ObjectHeaderSize();
        for (int i = 0; i < num_of_fields; i++) {
            auto *field_addr = reinterpret_cast<TaggedType *>(addr + i * TaggedValue::TaggedTypeSize());
            TaggedValue tagged_value(*field_addr);
            if (tagged_value.IsHeapObject()) {
                ObjectHeader *ref_object_header = tagged_value.GetRawHeapObject();
                size_t offset = ObjectHeader::ObjectHeaderSize() + i * TaggedValue::TaggedTypeSize();
                field_visitor(object, offset, ref_object_header, false);
            }
        }
    }
}

template <typename ElementVisitor>
void GCDynamicObjectHelpers::TraverseArray(ObjectHeader *object, BaseClass *base_cls,
                                           const ElementVisitor &array_element_visitor)
{
    ASSERT(base_cls->IsDynamicClass());
    [[maybe_unused]] auto *cls = static_cast<HClass *>(base_cls);
    ASSERT(cls != nullptr);

    ASSERT(cls->IsArray());
    auto *array_object = static_cast<coretypes::Array *>(object);
    auto array_length = array_object->GetLength();
    for (coretypes::array_size_t i = 0; i < array_length; i++) {
        TaggedValue array_element(array_object->Get<TaggedType, false, true>(i));
        if (array_element.IsHeapObject()) {
            array_element_visitor(object, i, array_element.GetRawHeapObject());
        }
    }
}

}  // namespace panda::mem

#endif  // PANDA_RUNTIME_MEM_OBJECT_HELPERS_INL_H_
