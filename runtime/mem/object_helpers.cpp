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

#include <algorithm>

#include "runtime/mem/object_helpers-inl.h"
#include "runtime/include/coretypes/dyn_objects.h"
#include "runtime/include/hclass.h"
#include "runtime/include/class.h"
#include "runtime/include/coretypes/array-inl.h"
#include "runtime/include/coretypes/string.h"

#include "runtime/include/coretypes/class.h"
#include "runtime/include/thread.h"
#include "runtime/include/panda_vm.h"
#include "runtime/mem/gc/dynamic/gc_dynamic_data.h"

namespace panda::mem {

using DynClass = coretypes::DynClass;
using TaggedValue = coretypes::TaggedValue;
using TaggedType = coretypes::TaggedType;

size_t GetObjectSize(const void *mem)
{
    ASSERT(mem != nullptr);
    auto *obj_header = static_cast<const ObjectHeader *>(mem);
    auto base_cls = obj_header->ClassAddr<BaseClass>();

    size_t object_size;
    if (base_cls->IsDynamicClass()) {
        auto *klass = static_cast<HClass *>(base_cls);
        if (klass->IsString()) {
            auto *string_object = static_cast<const coretypes::String *>(obj_header);
            object_size = string_object->ObjectSize();
        } else if (klass->IsArray()) {
            auto *array_object = static_cast<const coretypes::Array *>(obj_header);
            object_size = sizeof(coretypes::Array) + array_object->GetLength() * TaggedValue::TaggedTypeSize();
        } else {
            object_size = base_cls->GetObjectSize();
        }
    } else {
        object_size = obj_header->ObjectSize();
    }
    return object_size;
}

PandaString GetDebugInfoAboutObject(const ObjectHeader *header)
{
    PandaStringStream ss;
    ss << "( " << header->ClassAddr<Class>()->GetDescriptor() << " " << std::hex << header << " " << std::dec
       << GetObjectSize(header) << " bytes) mword = " << std::hex << header->AtomicGetMark().GetValue();
    return ss.str();
}

void DumpObject([[maybe_unused]] ObjectHeader *object_header,
                std::basic_ostream<char, std::char_traits<char>> *o_stream)
{
    auto *cls = object_header->ClassAddr<Class>();
    ASSERT(cls != nullptr);
    *o_stream << "Dump object object_header = " << std::hex << object_header << ", cls = " << std::hex << cls->GetName()
              << std::endl;

    if (cls->IsArrayClass()) {
        auto array = static_cast<coretypes::Array *>(object_header);
        *o_stream << "Array " << std::hex << object_header << " " << cls->GetComponentType()->GetName()
                  << " length = " << std::dec << array->GetLength() << std::endl;
        return;
    }

    while (cls != nullptr) {
        Span<Field> fields = cls->GetInstanceFields();
        *o_stream << "Dump object: " << std::hex << object_header << std::endl;
        if (cls->GetName() == "java.lang.String") {
            auto *str_object = static_cast<panda::coretypes::String *>(object_header);
            if (str_object->GetLength() > 0 && !str_object->IsUtf16()) {
                *o_stream << "length = " << std::dec << str_object->GetLength() << std::endl;
                constexpr size_t BUFF_SIZE = 256;
                std::array<char, BUFF_SIZE> buff {0};
                strncpy_s(&buff[0], BUFF_SIZE, reinterpret_cast<const char *>(str_object->GetDataMUtf8()),
                          static_cast<size_t>(str_object->GetLength()));
                *o_stream << "String data: " << &buff[0] << std::endl;
            }
        }
        for (Field &field : fields) {
            *o_stream << "\tfield \"" << GetFieldName(field) << "\" ";
            size_t offset = field.GetOffset();
            panda_file::Type::TypeId type_id = field.GetType().GetId();
            if (type_id == panda_file::Type::TypeId::REFERENCE) {
                ObjectHeader *field_object = object_header->GetFieldObject(offset);
                if (field_object != nullptr) {
                    *o_stream << std::hex << field_object << std::endl;
                } else {
                    *o_stream << "NULL" << std::endl;
                }
            } else if (type_id != panda_file::Type::TypeId::VOID) {
                *o_stream << std::dec;
                switch (type_id) {
                    case panda_file::Type::TypeId::U1: {
                        auto val = object_header->GetFieldPrimitive<bool>(offset);
                        *o_stream << val << std::endl;
                    } break;
                    case panda_file::Type::TypeId::I8: {
                        auto val = object_header->GetFieldPrimitive<int8_t>(offset);
                        *o_stream << val << std::endl;
                    } break;
                    case panda_file::Type::TypeId::U8: {
                        auto val = object_header->GetFieldPrimitive<uint8_t>(offset);
                        *o_stream << val << std::endl;
                    } break;
                    case panda_file::Type::TypeId::I16: {
                        auto val = object_header->GetFieldPrimitive<int16_t>(offset);
                        *o_stream << val << std::endl;
                    } break;
                    case panda_file::Type::TypeId::U16: {
                        auto val = object_header->GetFieldPrimitive<uint16_t>(offset);
                        *o_stream << val << std::endl;
                    } break;
                    case panda_file::Type::TypeId::I32: {
                        auto val = object_header->GetFieldPrimitive<int32_t>(offset);
                        *o_stream << val << std::endl;
                    } break;
                    case panda_file::Type::TypeId::U32: {
                        auto val = object_header->GetFieldPrimitive<uint32_t>(offset);
                        *o_stream << val << std::endl;
                    } break;
                    case panda_file::Type::TypeId::F32: {
                        auto val = object_header->GetFieldPrimitive<float>(offset);
                        *o_stream << val << std::endl;
                    } break;
                    case panda_file::Type::TypeId::F64: {
                        auto val = object_header->GetFieldPrimitive<double>(offset);
                        *o_stream << val << std::endl;
                    } break;
                    case panda_file::Type::TypeId::I64: {
                        auto val = object_header->GetFieldPrimitive<int64_t>(offset);
                        *o_stream << val << std::endl;
                    } break;
                    case panda_file::Type::TypeId::U64: {
                        auto val = object_header->GetFieldPrimitive<uint64_t>(offset);
                        *o_stream << val << std::endl;
                    } break;
                    default:
                        LOG(FATAL, COMMON) << "Error at object dump - wrong type id";
                }
            }
        }
        cls = cls->GetBase();
    }
}

void DumpClass(Class *cls, std::basic_ostream<char, std::char_traits<char>> *o_stream)
{
    if (UNLIKELY(cls == nullptr)) {
        return;
    }
    std::function<void(Class *, ObjectHeader *, const Field *, ObjectHeader *)> field_dump(
        [o_stream]([[maybe_unused]] Class *kls, [[maybe_unused]] ObjectHeader *obj, const Field *field,
                   ObjectHeader *field_object) {
            *o_stream << "field = " << GetFieldName(*field) << std::hex << " " << field_object << std::endl;
        });
    // Dump class static fields
    *o_stream << "Dump class: addr = " << std::hex << cls << ", cls = " << cls->GetDescriptor() << std::endl;
    *o_stream << "Dump static fields:" << std::endl;
    const Span<Field> &fields = cls->GetStaticFields();
    ObjectHeader *cls_object = cls->GetManagedObject();
    TraverseFields(fields, cls, cls_object, field_dump);
    *o_stream << "Dump cls object fields:" << std::endl;
    DumpObject(cls_object);
}

ObjectHeader *GetForwardAddress(ObjectHeader *object_header)
{
    ASSERT(object_header->IsForwarded());
    MarkWord mark_word = object_header->AtomicGetMark();
    MarkWord::markWordSize addr = mark_word.GetForwardingAddress();
    return reinterpret_cast<ObjectHeader *>(addr);
}

const char *GetFieldName(const Field &field)
{
    static const char *empty_string = "";
    bool is_proxy = field.GetClass()->IsProxy();
    // For proxy class it is impossible to get field name in standard manner
    if (!is_proxy) {
        return reinterpret_cast<const char *>(field.GetName().data);
    }
    return empty_string;
}

void GCDynamicObjectHelpers::TraverseAllObjects(ObjectHeader *object_header,
                                                const std::function<void(ObjectHeader *, ObjectHeader *)> &obj_visitor)
{
    auto *cls = object_header->ClassAddr<HClass>();
    ASSERT(cls != nullptr);
    if (cls->IsString() || cls->IsNativePointer()) {
        return;
    }
    if (cls->IsArray()) {
        std::function<void(ObjectHeader *, const array_size_t, ObjectHeader *)> arr_fn(
            [&obj_visitor]([[maybe_unused]] ObjectHeader *arr_object_header, [[maybe_unused]] const array_size_t INDEX,
                           ObjectHeader *object_reference) { obj_visitor(arr_object_header, object_reference); });
        TraverseArray(object_header, cls, arr_fn);
    } else {
        std::function<void(ObjectHeader *, size_t, ObjectHeader *, bool)> dyn_obj_proxy(
            [&obj_visitor](ObjectHeader *obj_header, [[maybe_unused]] size_t offset, ObjectHeader *obj_reference,
                           [[maybe_unused]] bool is_update_classword) { obj_visitor(obj_header, obj_reference); });
        TraverseObject(object_header, cls, dyn_obj_proxy);
    }
}

void GCDynamicObjectHelpers::RecordDynWeakReference(GC *gc, coretypes::TaggedType *value)
{
    GCExtensionData *data = gc->GetExtensionData();
    ASSERT(data != nullptr);
    ASSERT(data->GetLangType() == LANG_TYPE_DYNAMIC);
    static_cast<GCDynamicData *>(data)->GetDynWeakReferences()->push(value);
}

void GCDynamicObjectHelpers::HandleDynWeakReferences(GC *gc)
{
    GCExtensionData *data = gc->GetExtensionData();
    ASSERT(data != nullptr);
    ASSERT(data->GetLangType() == LANG_TYPE_DYNAMIC);
    auto *weak_refs = static_cast<GCDynamicData *>(data)->GetDynWeakReferences();
    while (!weak_refs->empty()) {
        coretypes::TaggedType *object_pointer = weak_refs->top();
        weak_refs->pop();
        TaggedValue value(*object_pointer);
        if (value.IsUndefined()) {
            continue;
        }
        ASSERT(value.IsWeak());
        ObjectHeader *object = value.GetWeakReferent();
        /* Note: If it is in young GC, the weak reference whose referent is in tenured space will not be marked. The */
        /*       weak reference whose referent is in young space will be moved into the tenured space or reset in    */
        /*       CollecYoungAndMove. If the weak referent here is not moved in young GC, it should be cleared.       */
        if (gc->GetGCPhase() == GCPhase::GC_PHASE_MARK_YOUNG) {
            if (gc->GetObjectAllocator()->IsAddressInYoungSpace(ToUintPtr(object)) && !gc->IsMarked(object)) {
                *object_pointer = TaggedValue::Undefined().GetRawData();
            }
        } else {
            /* Note: When it is in tenured GC, we check whether the referent has been marked. */
            if (!gc->IsMarked(object)) {
                *object_pointer = TaggedValue::Undefined().GetRawData();
            }
        }
    }
}

void GCStaticObjectHelpers::TraverseAllObjects(ObjectHeader *object_header,
                                               const std::function<void(ObjectHeader *, ObjectHeader *)> &obj_visitor)
{
    auto *cls = object_header->ClassAddr<Class>();
    // If create new object when visiting card table, the ClassAddr of the new object may be null
    if (cls == nullptr) {
        return;
    }

    if (cls->IsObjectArrayClass()) {
        TraverseArray(object_header, cls, ArrayElementVisitor(obj_visitor));
    } else {
        if (cls->IsClassClass()) {
            auto object_cls = panda::Class::FromClassObject(object_header);
            if (object_cls->IsInitializing() || object_cls->IsInitialized()) {
                TraverseClass(object_cls, ClassFieldVisitor(obj_visitor));
            }
        }
        TraverseObject(object_header, cls, ObjectFieldVisitor(obj_visitor));
    }
}

void GCStaticObjectHelpers::UpdateRefsToMovedObjects(PandaVM *vm, ObjectHeader *object, BaseClass *base_cls)
{
    ASSERT(!base_cls->IsDynamicClass());
    auto *cls = static_cast<Class *>(base_cls);
    if (cls->IsObjectArrayClass()) {
        LOG_DEBUG_OBJ_HELPERS << " IsObjArrayClass";
        TraverseArray(object, cls, [vm](ObjectHeader *obj, array_size_t index, ObjectHeader *element) {
            MarkWord mark_word = element->GetMark();  // no need atomic because stw
            if (mark_word.GetState() == MarkWord::ObjectState::STATE_GC) {
                // update element without write barrier
                auto array_object = static_cast<coretypes::Array *>(obj);
                MarkWord::markWordSize addr = mark_word.GetForwardingAddress();
                LOG_DEBUG_OBJ_HELPERS << "  update obj ref for array  " << std::hex << obj << " index =  " << index
                                      << " from " << array_object->Get<ObjectHeader *>(index) << " to " << addr;
                array_object->Set<ObjectHeader *, false>(index, reinterpret_cast<ObjectHeader *>(addr));
            }
        });
    } else {
        LOG_DEBUG_OBJ_HELPERS << " IsObject";
        TraverseObject(
            object, cls, [vm](ObjectHeader *obj, ObjectHeader *field_object, uint32_t field_offset, bool is_volatile) {
                MarkWord mark_word = field_object->GetMark();  // no need atomic because stw
                if (mark_word.GetState() == MarkWord::ObjectState::STATE_GC) {
                    // update instance field without write barrier
                    MarkWord::markWordSize addr = mark_word.GetForwardingAddress();
                    LOG_DEBUG_OBJ_HELPERS << "  update obj ref for object " << std::hex << obj << " from "
                                          << field_object << " to " << addr;
                    if (is_volatile) {
                        obj->SetFieldObject<true, false>(field_offset, reinterpret_cast<ObjectHeader *>(addr));
                    } else {
                        obj->SetFieldObject<false, false>(field_offset, reinterpret_cast<ObjectHeader *>(addr));
                    }
                }
            });
        if (!cls->IsClassClass()) {
            return;
        }

        auto object_cls = panda::Class::FromClassObject(object);
        if (!object_cls->IsInitializing() && !object_cls->IsInitialized()) {
            return;
        }

        TraverseClass(
            object_cls, [](Class *object_kls, ObjectHeader *field_object, uint32_t field_offset, bool is_volatile) {
                MarkWord mark_word = field_object->GetMark();  // no need atomic because stw
                if (mark_word.GetState() == MarkWord::ObjectState::STATE_GC) {
                    // update static field without write barrier
                    MarkWord::markWordSize addr = mark_word.GetForwardingAddress();
                    if (is_volatile) {
                        object_kls->SetFieldObject<true, false>(field_offset, reinterpret_cast<ObjectHeader *>(addr));
                    } else {
                        object_kls->SetFieldObject<false, false>(field_offset, reinterpret_cast<ObjectHeader *>(addr));
                    }
                }
            });
    }
}

void GCDynamicObjectHelpers::UpdateRefsToMovedObjects(PandaVM *vm, ObjectHeader *object, BaseClass *base_cls)
{
    ASSERT(base_cls->IsDynamicClass());
    auto *cls = static_cast<HClass *>(base_cls);
    if (cls->IsNativePointer() || cls->IsString()) {
        return;
    }
    if (cls->IsArray()) {
        LOG_DEBUG_OBJ_HELPERS << " IsDynamicArrayClass";
        auto update_array_callback = [vm](ObjectHeader *obj, array_size_t index, ObjectHeader *obj_ref) {
            UpdateDynArray(vm, obj, index, obj_ref);
        };
        TraverseArray(object, cls, update_array_callback);
    } else {
        LOG_DEBUG_OBJ_HELPERS << " IsDynamicObject";
        auto update_object_callback = [vm](ObjectHeader *obj, size_t offset, ObjectHeader *field_obj_ref,
                                           bool is_update_classword) {
            UpdateDynObjectRef(vm, obj, offset, field_obj_ref, is_update_classword);
        };
        TraverseObject(object, cls, update_object_callback);
    }
}

void GCDynamicObjectHelpers::UpdateDynArray(PandaVM *vm, ObjectHeader *object, array_size_t index,
                                            ObjectHeader *obj_ref)
{
    TaggedValue value(obj_ref);
    bool is_dyn_weak = value.IsWeak();
    if (is_dyn_weak) {
        obj_ref = value.GetWeakReferent();
    }

    MarkWord mark_word = obj_ref->AtomicGetMark();
    if (mark_word.GetState() == MarkWord::ObjectState::STATE_GC) {
        auto arr = static_cast<coretypes::Array *>(object);
        MarkWord::markWordSize addr = mark_word.GetForwardingAddress();
        LOG_DEBUG_OBJ_HELPERS << "  update obj ref for array  " << std::hex << object << " index =  " << index
                              << " from " << std::hex << arr->Get<ObjectHeader *>(index) << " to " << addr;
        auto *field_object = reinterpret_cast<ObjectHeader *>(addr);
        if (is_dyn_weak) {
            field_object = TaggedValue(field_object).CreateAndGetWeakRef().GetRawHeapObject();
        }
        size_t offset = TaggedValue::TaggedTypeSize() * index;
        ObjectAccessor::SetDynObject<true>(vm->GetAssociatedThread(), arr->GetData(), offset, field_object);
    }
}

void GCDynamicObjectHelpers::UpdateDynObjectRef(PandaVM *vm, ObjectHeader *object, size_t offset,
                                                ObjectHeader *field_obj_ref, bool is_update_classword)
{
    TaggedValue value(field_obj_ref);
    bool is_dyn_weak = value.IsWeak();
    if (is_dyn_weak) {
        field_obj_ref = value.GetWeakReferent();
    }
    MarkWord mark_word = field_obj_ref->AtomicGetMark();
    if (mark_word.GetState() == MarkWord::ObjectState::STATE_GC) {
        MarkWord::markWordSize addr = mark_word.GetForwardingAddress();
        LOG_DEBUG_OBJ_HELPERS << "  update obj ref for object " << std::hex << object << " from "
                              << ObjectAccessor::GetDynValue<ObjectHeader *>(object, offset) << " to " << addr;
        auto *h_class = field_obj_ref->ClassAddr<HClass>();
        if (is_update_classword && h_class->IsHClass()) {
            addr += ObjectHeader::ObjectHeaderSize();
        }
        auto *field_object = reinterpret_cast<ObjectHeader *>(addr);
        if (is_dyn_weak) {
            field_object = TaggedValue(field_object).CreateAndGetWeakRef().GetRawHeapObject();
        }
        ObjectAccessor::SetDynObject(vm->GetAssociatedThread(), object, offset, field_object);
    }
}

}  // namespace panda::mem
