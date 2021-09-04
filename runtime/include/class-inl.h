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

#ifndef PANDA_RUNTIME_INCLUDE_CLASS_INL_H_
#define PANDA_RUNTIME_INCLUDE_CLASS_INL_H_

#include "runtime/include/class.h"
#include "runtime/include/coretypes/tagged_value.h"
#include "runtime/include/object_accessor-inl.h"

namespace panda {

inline uint32_t Class::GetTypeSize(panda_file::Type type)
{
    switch (type.GetId()) {
        case panda_file::Type::TypeId::U1:
        case panda_file::Type::TypeId::I8:
        case panda_file::Type::TypeId::U8:
            return sizeof(uint8_t);
        case panda_file::Type::TypeId::I16:
        case panda_file::Type::TypeId::U16:
            return sizeof(uint16_t);
        case panda_file::Type::TypeId::I32:
        case panda_file::Type::TypeId::U32:
        case panda_file::Type::TypeId::F32:
            return sizeof(uint32_t);
        case panda_file::Type::TypeId::I64:
        case panda_file::Type::TypeId::U64:
        case panda_file::Type::TypeId::F64:
            return sizeof(uint64_t);
        case panda_file::Type::TypeId::TAGGED:
            return coretypes::TaggedValue::TaggedTypeSize();
        case panda_file::Type::TypeId::REFERENCE:
            return ClassHelper::OBJECT_POINTER_SIZE;
        default:
            UNREACHABLE();
    }
}

inline uint32_t Class::GetComponentSize() const
{
    if (component_type_ == nullptr) {
        return 0;
    }

    return GetTypeSize(component_type_->GetType());
}

inline bool Class::IsSubClassOf(const Class *klass) const
{
    const Class *current = this;

    do {
        if (current == klass) {
            return true;
        }

        current = current->GetBase();
    } while (current != nullptr);

    return false;
}

inline bool Class::IsAssignableFrom(const Class *klass) const
{
    if (klass == this) {
        return true;
    }
    if (IsObjectClass()) {
        return !klass->IsPrimitive();
    }
    if (IsInterface()) {
        return klass->Implements(this);
    }
    if (klass->IsArrayClass()) {
        return IsArrayClass() && GetComponentType()->IsAssignableFrom(klass->GetComponentType());
    }
    return !klass->IsInterface() && klass->IsSubClassOf(this);
}

inline bool Class::Implements(const Class *klass) const
{
    for (const auto &elem : itable_.Get()) {
        if (elem.GetInterface() == klass) {
            return true;
        }
    }

    return false;
}

template <Class::FindFilter filter>
inline Span<Field> Class::GetFields() const
{
    switch (filter) {
        case FindFilter::STATIC:
            return GetStaticFields();
        case FindFilter::INSTANCE:
            return GetInstanceFields();
        case FindFilter::ALL:
            return GetFields();
        default:
            UNREACHABLE();
    }
}

template <Class::FindFilter filter, class Pred>
inline Field *Class::FindDeclaredField(Pred pred) const
{
    auto fields = GetFields<filter>();
    auto it = std::find_if(fields.begin(), fields.end(), pred);
    if (it != fields.end()) {
        return &*it;
    }
    return nullptr;
}

template <Class::FindFilter filter, class Pred>
inline Field *Class::FindField(Pred pred) const
{
    auto *cls = this;
    while (cls != nullptr) {
        auto *field = cls->FindDeclaredField<filter>(pred);
        if (field != nullptr) {
            return field;
        }

        cls = cls->GetBase();
    }

    if (filter == FindFilter::STATIC || filter == FindFilter::ALL) {
        auto *kls = this;
        while (kls != nullptr) {
            for (auto *iface : kls->GetInterfaces()) {
                auto *field = iface->FindField<filter>(pred);
                if (field != nullptr) {
                    return field;
                }
            }

            kls = kls->GetBase();
        }
    }

    return nullptr;
}

template <Class::FindFilter filter>
inline Span<Method> Class::GetMethods() const
{
    switch (filter) {
        case FindFilter::STATIC:
            return GetStaticMethods();
        case FindFilter::INSTANCE:
            return GetVirtualMethods();
        case FindFilter::ALL:
            return GetMethods();
        case FindFilter::COPIED:
            return GetCopiedMethods();
        default:
            UNREACHABLE();
    }
}

template <Class::FindFilter filter, class Pred>
inline Method *Class::FindDirectMethod(Pred pred) const
{
    auto methods = GetMethods<filter>();
    auto it = std::find_if(methods.begin(), methods.end(), pred);
    if (it != methods.end()) {
        return &*it;
    }
    return nullptr;
}

template <Class::FindFilter filter, class Pred>
inline Method *Class::FindClassMethod(Pred pred) const
{
    auto *cls = this;
    while (cls != nullptr) {
        auto *method = cls->FindDirectMethod<filter>(pred);
        if (method != nullptr) {
            return method;
        }

        cls = cls->GetBase();
    }

    if (filter == FindFilter::ALL || filter == FindFilter::INSTANCE) {
        return FindClassMethod<FindFilter::COPIED>(pred);
    }

    return nullptr;
}

template <Class::FindFilter filter, class Pred>
inline Method *Class::FindInterfaceMethod(Pred pred) const
{
    static_assert(filter != FindFilter::COPIED, "interfaces don't have copied methods");

    if (LIKELY(IsInterface())) {
        auto *method = FindDirectMethod<filter>(pred);
        if (method != nullptr) {
            return method;
        }
    }

    if (filter == FindFilter::STATIC) {
        return nullptr;
    }

    for (const auto &entry : itable_.Get()) {
        auto *iface = entry.GetInterface();
        auto *method = iface->FindDirectMethod<FindFilter::INSTANCE>(pred);
        if (method != nullptr) {
            return method;
        }
    }

    if (LIKELY(IsInterface())) {
        return GetBase()->FindDirectMethod<FindFilter::INSTANCE>(
            [&pred](const Method &method) { return method.IsPublic() && pred(method); });
    }

    return nullptr;
}

template <class Pred>
inline Method *Class::FindInterfaceMethod(Pred pred) const
{
    return FindInterfaceMethod<FindFilter::ALL>(pred);
}

template <class Pred>
inline Method *Class::FindVirtualInterfaceMethod(Pred pred) const
{
    return FindInterfaceMethod<FindFilter::INSTANCE>(pred);
}

template <class Pred>
inline Method *Class::FindStaticInterfaceMethod(Pred pred) const
{
    return FindInterfaceMethod<FindFilter::STATIC>(pred);
}

template <class Pred>
inline Field *Class::FindInstanceField(Pred pred) const
{
    return FindField<FindFilter::INSTANCE>(pred);
}

template <class Pred>
inline Field *Class::FindStaticField(Pred pred) const
{
    return FindField<FindFilter::STATIC>(pred);
}

template <class Pred>
inline Field *Class::FindField(Pred pred) const
{
    return FindField<FindFilter::ALL>(pred);
}

template <class Pred>
inline Field *Class::FindDeclaredField(Pred pred) const
{
    return FindDeclaredField<FindFilter::ALL>(pred);
}

inline Field *Class::GetInstanceFieldByName(const uint8_t *mutf8_name) const
{
    panda_file::File::StringData sd = {static_cast<uint32_t>(panda::utf::MUtf8ToUtf16Size(mutf8_name)), mutf8_name};
    return FindInstanceField([sd](const Field &field) { return field.GetName() == sd; });
}

inline Field *Class::GetStaticFieldByName(const uint8_t *mutf8_name) const
{
    panda_file::File::StringData sd = {static_cast<uint32_t>(panda::utf::MUtf8ToUtf16Size(mutf8_name)), mutf8_name};
    return FindStaticField([sd](const Field &field) { return field.GetName() == sd; });
}

inline Field *Class::GetDeclaredFieldByName(const uint8_t *mutf8_name) const
{
    panda_file::File::StringData sd = {static_cast<uint32_t>(panda::utf::MUtf8ToUtf16Size(mutf8_name)), mutf8_name};
    return FindDeclaredField([sd](const Field &field) { return field.GetName() == sd; });
}

template <class Pred>
inline Method *Class::FindVirtualClassMethod(Pred pred) const
{
    return FindClassMethod<FindFilter::INSTANCE>(pred);
}

template <class Pred>
inline Method *Class::FindStaticClassMethod(Pred pred) const
{
    return FindClassMethod<FindFilter::STATIC>(pred);
}

template <class Pred>
inline Method *Class::FindClassMethod(Pred pred) const
{
    return FindClassMethod<FindFilter::ALL>(pred);
}

inline Method *Class::GetDirectMethod(const uint8_t *mutf8_name, const Method::Proto &proto) const
{
    panda_file::File::StringData sd = {static_cast<uint32_t>(panda::utf::MUtf8ToUtf16Size(mutf8_name)), mutf8_name};
    return FindDirectMethod<FindFilter::ALL>(
        [sd, proto](const Method &method) { return method.GetName() == sd && method.GetProto() == proto; });
}

inline Method *Class::GetClassMethod(const uint8_t *mutf8_name, const Method::Proto &proto) const
{
    panda_file::File::StringData sd = {static_cast<uint32_t>(panda::utf::MUtf8ToUtf16Size(mutf8_name)), mutf8_name};
    return FindClassMethod(
        [sd, proto](const Method &method) { return method.GetName() == sd && method.GetProto() == proto; });
}

inline Method *Class::GetInterfaceMethod(const uint8_t *mutf8_name, const Method::Proto &proto) const
{
    panda_file::File::StringData sd = {static_cast<uint32_t>(panda::utf::MUtf8ToUtf16Size(mutf8_name)), mutf8_name};
    return FindInterfaceMethod(
        [sd, proto](const Method &method) { return method.GetName() == sd && method.GetProto() == proto; });
}

inline Method *Class::GetDirectMethod(const uint8_t *mutf8_name) const
{
    panda_file::File::StringData sd = {static_cast<uint32_t>(panda::utf::MUtf8ToUtf16Size(mutf8_name)), mutf8_name};
    return FindDirectMethod<FindFilter::ALL>([sd](const Method &method) { return method.GetName() == sd; });
}

inline Method *Class::GetClassMethod(const uint8_t *mutf8_name) const
{
    panda_file::File::StringData sd = {static_cast<uint32_t>(panda::utf::MUtf8ToUtf16Size(mutf8_name)), mutf8_name};
    return FindClassMethod([sd](const Method &method) { return method.GetName() == sd; });
}

inline Method *Class::ResolveVirtualMethod(const Method *method) const
{
    Method *resolved = nullptr;

    ASSERT(!IsInterface());

    if (method->GetClass()->IsInterface() && !method->IsDefaultInterfaceMethod()) {
        // find method in imtable
        auto imtable_size = GetIMTSize();
        if (LIKELY(imtable_size != 0)) {
            auto imtable = GetIMT();
            auto method_id = GetIMTableIndex(method->GetFileId().GetOffset());
            resolved = imtable[method_id];
            if (resolved != nullptr) {
                return resolved;
            }
        }

        // find method in itable
        auto *iface = method->GetClass();
        auto itable = GetITable();
        for (size_t i = 0; i < itable.Size(); i++) {
            auto &entry = itable[i];
            if (entry.GetInterface() != iface) {
                continue;
            }

            resolved = entry.GetMethods()[method->GetVTableIndex()];
        }
    } else {
        // find method in vtable
        auto vtable = GetVTable();
        ASSERT(method->GetVTableIndex() < vtable.size());
        resolved = vtable[method->GetVTableIndex()];
    }

    return resolved;
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_COMMENT_LOCATION)
template <class T, bool is_volatile /* = false */>
inline T Class::GetFieldPrimitive(size_t offset) const
{
    ASSERT(IsInitializing() || IsInitialized());
    return ObjectAccessor::GetPrimitive<T, is_volatile>(this, offset);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_COMMENT_LOCATION)
template <class T, bool is_volatile /* = false */>
inline void Class::SetFieldPrimitive(size_t offset, T value)
{
    ObjectAccessor::SetPrimitive<T, is_volatile>(this, offset, value);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool is_volatile /* = false */, bool need_read_barrier /* = true */>
inline ObjectHeader *Class::GetFieldObject(size_t offset) const
{
    ASSERT(IsInitializing() || IsInitialized());
    return ObjectAccessor::GetObject<is_volatile, need_read_barrier>(this, offset);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool is_volatile /* = false */, bool need_write_barrier /* = true */>
inline void Class::SetFieldObject(size_t offset, ObjectHeader *value)
{
    auto object = GetManagedObject();
    auto new_offset = offset + (ToUintPtr(this) - ToUintPtr(object));
    ObjectAccessor::SetObject<is_volatile, need_write_barrier>(object, new_offset, value);
}

template <class T>
inline T Class::GetFieldPrimitive(const Field &field) const
{
    return ObjectAccessor::GetFieldPrimitive<T>(this, field);
}

template <class T>
inline void Class::SetFieldPrimitive(const Field &field, T value)
{
    ObjectAccessor::SetFieldPrimitive(this, field, value);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_COMMENT_LOCATION)
template <bool need_read_barrier /* = true */>
inline ObjectHeader *Class::GetFieldObject(const Field &field) const
{
    return ObjectAccessor::GetFieldObject<need_read_barrier>(this, field);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_COMMENT_LOCATION)
template <bool need_write_barrier /* = true */>
inline void Class::SetFieldObject(const Field &field, ObjectHeader *value)
{
    auto object = GetManagedObject();
    auto offset = field.GetOffset() + (ToUintPtr(this) - ToUintPtr(object));
    if (UNLIKELY(field.IsVolatile())) {
        ObjectAccessor::SetObject<true, need_write_barrier>(object, offset, value);
    } else {
        ObjectAccessor::SetObject<false, need_write_barrier>(object, offset, value);
    }
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_COMMENT_LOCATION)
template <bool need_read_barrier /* = true */>
inline ObjectHeader *Class::GetFieldObject(ManagedThread *thread, const Field &field) const
{
    return ObjectAccessor::GetFieldObject<need_read_barrier>(thread, this, field);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_COMMENT_LOCATION)
template <bool need_write_barrier /* = true */>
inline void Class::SetFieldObject(ManagedThread *thread, const Field &field, ObjectHeader *value)
{
    auto object = GetManagedObject();
    auto offset = field.GetOffset() + (ToUintPtr(this) - ToUintPtr(object));
    if (UNLIKELY(field.IsVolatile())) {
        ObjectAccessor::SetObject<true, need_write_barrier>(thread, object, offset, value);
    } else {
        ObjectAccessor::SetObject<false, need_write_barrier>(thread, object, offset, value);
    }
}

template <class T>
inline T Class::GetFieldPrimitive(size_t offset, std::memory_order memory_order) const
{
    return ObjectAccessor::GetFieldPrimitive<T>(this, offset, memory_order);
}

template <class T>
inline void Class::SetFieldPrimitive(size_t offset, T value, std::memory_order memory_order)
{
    ObjectAccessor::SetFieldPrimitive(this, offset, value, memory_order);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool need_read_barrier /* = true */>
inline ObjectHeader *Class::GetFieldObject(size_t offset, std::memory_order memory_order) const
{
    return ObjectAccessor::GetFieldObject<need_read_barrier>(this, offset, memory_order);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool need_write_barrier /* = true */>
inline void Class::SetFieldObject(size_t offset, ObjectHeader *value, std::memory_order memory_order)
{
    ObjectAccessor::SetFieldObject<need_write_barrier>(this, offset, value, memory_order);
}

template <typename T>
inline bool Class::CompareAndSetFieldPrimitive(size_t offset, T old_value, T new_value, std::memory_order memory_order,
                                               bool strong)
{
    return ObjectAccessor::CompareAndSetFieldPrimitive(this, offset, old_value, new_value, memory_order, strong).first;
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool need_write_barrier /* = true */>
inline bool Class::CompareAndSetFieldObject(size_t offset, ObjectHeader *old_value, ObjectHeader *new_value,
                                            std::memory_order memory_order, bool strong)
{
    return ObjectAccessor::CompareAndSetFieldObject<need_write_barrier>(this, offset, old_value, new_value,
                                                                        memory_order, strong)
        .first;
}

template <typename T>
inline T Class::CompareAndExchangeFieldPrimitive(size_t offset, T old_value, T new_value,
                                                 std::memory_order memory_order, bool strong)
{
    return ObjectAccessor::CompareAndSetFieldPrimitive(this, offset, old_value, new_value, memory_order, strong).second;
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool need_write_barrier /* = true */>
inline ObjectHeader *Class::CompareAndExchangeFieldObject(size_t offset, ObjectHeader *old_value,
                                                          ObjectHeader *new_value, std::memory_order memory_order,
                                                          bool strong)
{
    return ObjectAccessor::CompareAndSetFieldObject<need_write_barrier>(this, offset, old_value, new_value,
                                                                        memory_order, strong)
        .second;
}

template <typename T>
inline T Class::GetAndSetFieldPrimitive(size_t offset, T value, std::memory_order memory_order)
{
    return ObjectAccessor::GetAndSetFieldPrimitive(this, offset, value, memory_order);
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_HORIZON_SPACE, C_RULE_ID_COMMENT_LOCATION)
template <bool need_write_barrier /* = true */>
inline ObjectHeader *Class::GetAndSetFieldObject(size_t offset, ObjectHeader *value, std::memory_order memory_order)
{
    return ObjectAccessor::GetAndSetFieldObject<need_write_barrier>(this, offset, value, memory_order);
}

template <typename T>
inline T Class::GetAndAddFieldPrimitive(size_t offset, T value, std::memory_order memory_order)
{
    return ObjectAccessor::GetAndAddFieldPrimitive(this, offset, value, memory_order);
}

template <typename T>
inline T Class::GetAndBitwiseOrFieldPrimitive(size_t offset, T value, std::memory_order memory_order)
{
    return ObjectAccessor::GetAndBitwiseOrFieldPrimitive(this, offset, value, memory_order);
}

template <typename T>
inline T Class::GetAndBitwiseAndFieldPrimitive(size_t offset, T value, std::memory_order memory_order)
{
    return ObjectAccessor::GetAndBitwiseAndFieldPrimitive(this, offset, value, memory_order);
}

template <typename T>
inline T Class::GetAndBitwiseXorFieldPrimitive(size_t offset, T value, std::memory_order memory_order)
{
    return ObjectAccessor::GetAndBitwiseXorFieldPrimitive(this, offset, value, memory_order);
}

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_CLASS_INL_H_
