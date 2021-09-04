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

#ifndef PANDA_RUNTIME_INCLUDE_CLASS_H_
#define PANDA_RUNTIME_INCLUDE_CLASS_H_

#include <securec.h>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <memory>

#include "libpandafile/file.h"
#include "libpandafile/file_items.h"
#include "runtime/include/field.h"
#include "runtime/include/itable.h"
#include "runtime/include/method.h"
#include "runtime/include/object_header.h"
#include "libpandabase/macros.h"

namespace panda {

class ClassLinkerContext;
class Field;
class ManagedThread;

class BaseClass {
public:
    static constexpr uint32_t DYNAMIC_CLASS = 1U;

public:
    explicit BaseClass(panda_file::SourceLang lang) : lang_(lang) {}

    ~BaseClass() = default;

    DEFAULT_COPY_SEMANTIC(BaseClass);
    DEFAULT_MOVE_SEMANTIC(BaseClass);

    uint32_t GetFlags() const
    {
        return flags_;
    }

    void SetFlags(uint32_t flags)
    {
        flags_ = flags;
    }

    bool IsDynamicClass() const
    {
        return (flags_ & DYNAMIC_CLASS) != 0;
    }

    uint32_t GetObjectSize() const
    {
        return object_size_;
    }

    void SetObjectSize(uint32_t size)
    {
        object_size_ = size;
    }

    void SetManagedObject(ObjectHeader *obj)
    {
        managed_object_ = obj;
    }

    ObjectHeader *GetManagedObject() const
    {
        return managed_object_;
    }

    panda_file::SourceLang GetSourceLang() const
    {
        return lang_;
    }

    void SetSourceLang(panda_file::SourceLang lang)
    {
        lang_ = lang;
    }

    static constexpr size_t OffsetOfManageObject()
    {
        return offsetof(BaseClass, managed_object_);
    }

private:
    uint32_t flags_ {0};
    // Size of the object of this class. In case of static classes it is 0
    // for abstract classes, interfaces and classes whose objects
    // have variable size (for example strings).
    uint32_t object_size_ {0};
    ObjectHeader *managed_object_ {nullptr};
    panda_file::SourceLang lang_;
};

class Class : public BaseClass {
public:
    using UniqId = uint64_t;
    static constexpr uint32_t STRING_CLASS = 1U << 1U;
    static constexpr size_t IMTABLE_SIZE = 32;

    enum {
        DUMPCLASSFULLDETAILS = 1,
        DUMPCLASSCLASSLODER = 2,
        DUMPCLASSINITIALIZED = 4,
    };

    enum class State : uint8_t { INITIAL = 0, LOADED, VERIFIED, INITIALIZING, ERRONEOUS, INITIALIZED };

    Class(const uint8_t *descriptor, panda_file::SourceLang lang, uint32_t vtable_size, uint32_t imt_size,
          uint32_t size);

    Class *GetBase() const
    {
        return base_;
    }

    void SetBase(Class *base)
    {
        base_ = base;
    }

    panda_file::File::EntityId GetFileId() const
    {
        return file_id_;
    }

    void SetFileId(panda_file::File::EntityId file_id)
    {
        file_id_ = file_id;
    }

    const panda_file::File *GetPandaFile() const
    {
        return panda_file_;
    }

    void SetPandaFile(const panda_file::File *pf)
    {
        panda_file_ = pf;
    }

    const uint8_t *GetDescriptor() const
    {
        return descriptor_;
    }

    void SetMethods(Span<Method> methods, uint32_t num_vmethods, uint32_t num_smethods)
    {
        methods_ = methods.data();
        num_methods_ = num_vmethods + num_smethods;
        num_vmethods_ = num_vmethods;
        num_copied_methods_ = methods.size() - num_methods_;
    }

    Span<Method> GetMethods() const
    {
        return {methods_, num_methods_};
    }

    Span<Method> GetStaticMethods() const
    {
        return GetMethods().SubSpan(num_vmethods_);
    }

    Span<Method> GetVirtualMethods() const
    {
        return {methods_, num_vmethods_};
    }

    Span<Method> GetCopiedMethods() const
    {
        Span<Method> res {methods_, num_methods_ + num_copied_methods_};
        return res.SubSpan(num_methods_);
    }

    Span<Field> GetFields() const
    {
        return {fields_, num_fields_};
    }

    Span<Field> GetStaticFields() const
    {
        return {fields_, num_sfields_};
    }

    Span<Field> GetInstanceFields() const
    {
        return GetFields().SubSpan(num_sfields_);
    }

    void SetFields(Span<Field> fields, uint32_t num_sfields)
    {
        fields_ = fields.data();
        num_fields_ = fields.size();
        num_sfields_ = num_sfields;
    }

    Span<Method *> GetVTable()
    {
        return GetClassSpan().SubSpan<Method *>(GetVTableOffset(), vtable_size_);
    }

    Span<Method *const> GetVTable() const
    {
        return GetClassSpan().SubSpan<Method *const>(GetVTableOffset(), vtable_size_);
    }

    Span<Class *> GetInterfaces() const
    {
        return {ifaces_, num_ifaces_};
    }

    void SetInterfaces(Span<Class *> ifaces)
    {
        ifaces_ = ifaces.data();
        num_ifaces_ = ifaces.size();
    }

    Span<Method *> GetIMT()
    {
        return GetClassSpan().SubSpan<Method *>(GetIMTOffset(), imt_size_);
    }

    Span<Method *const> GetIMT() const
    {
        return GetClassSpan().SubSpan<Method *const>(GetIMTOffset(), imt_size_);
    }

    uint32_t GetIMTableIndex(uint32_t method_offset) const
    {
        ASSERT(imt_size_ != 0);
        return method_offset % imt_size_;
    }

    uint32_t GetAccessFlags() const
    {
        return access_flags_;
    }

    void SetAccessFlags(uint32_t access_flags)
    {
        access_flags_ = access_flags;
    }

    bool IsPublic() const
    {
        return (access_flags_ & ACC_PUBLIC) != 0;
    }

    bool IsFinal() const
    {
        return (access_flags_ & ACC_FINAL) != 0;
    }

    bool IsAnnotation() const
    {
        return (access_flags_ & ACC_ANNOTATION) != 0;
    }

    bool IsEnum() const
    {
        return (access_flags_ & ACC_ENUM) != 0;
    }

    uint32_t GetVTableSize() const
    {
        return vtable_size_;
    }

    uint32_t GetIMTSize() const
    {
        return imt_size_;
    }

    uint32_t GetClassSize() const
    {
        return class_size_;
    }

    uint32_t GetObjectSize() const
    {
        ASSERT(!IsVariableSize());
        return BaseClass::GetObjectSize();
    }

    void SetObjectSize(uint32_t size)
    {
        ASSERT(!IsVariableSize());
        BaseClass::SetObjectSize(size);
    }

    static uint32_t GetTypeSize(panda_file::Type type);
    uint32_t GetComponentSize() const;

    Class *GetComponentType() const
    {
        return component_type_;
    }

    void SetComponentType(Class *type)
    {
        component_type_ = type;
    }

    bool IsArrayClass() const
    {
        return component_type_ != nullptr;
    }

    bool IsObjectArrayClass() const
    {
        return IsArrayClass() && !component_type_->IsPrimitive();
    }

    bool IsStringClass() const
    {
        return (GetFlags() & STRING_CLASS) != 0;
    }

    bool IsVariableSize() const
    {
        return IsArrayClass() || IsStringClass();
    }

    size_t GetStaticFieldsOffset() const
    {
        return ClassHelper::ComputeClassSize(vtable_size_, imt_size_, 0, 0, 0, 0, 0, 0);
    }

    panda_file::Type GetType() const
    {
        return type_;
    }

    void SetType(panda_file::Type type)
    {
        type_ = type;
    }

    bool IsPrimitive() const
    {
        return type_.IsPrimitive();
    }

    bool IsAbstract() const
    {
        return (access_flags_ & ACC_ABSTRACT) != 0;
    }

    bool IsInterface() const
    {
        return (access_flags_ & ACC_INTERFACE) != 0;
    }

    bool IsInstantiable() const
    {
        return (!IsPrimitive() && !IsAbstract() && !IsInterface()) || IsArrayClass();
    }

    bool IsObjectClass() const
    {
        return !IsPrimitive() && GetBase() == nullptr;
    }

    /**
     * Check if the object is Class instance
     * @return true if the object is Class instance
     */
    bool IsClassClass() const
    {
        return GetManagedObject()->ClassAddr<Class>() == this;
    }

    bool IsSubClassOf(const Class *klass) const;

    /**
     * Check whether an instance of this class can be assigned from an instance of class "klass".
     * Object of type O is instance of type T if O is the same as T or is subtype of T. For arrays T should be a root
     * type in type hierarchy or T is such array that O array elements are the same or subtype of T array elements.
     */
    bool IsAssignableFrom(const Class *klass) const;

    bool IsProxy() const
    {
        return (GetAccessFlags() & ACC_PROXY) != 0;
    }

    bool Implements(const Class *klass) const;

    void SetITable(ITable itable)
    {
        itable_ = itable;
    }

    ITable GetITable() const
    {
        return itable_;
    }

    State GetState() const
    {
        return state_;
    }

    void SetState(State state);

    bool IsVerified() const
    {
        return state_ >= State::VERIFIED;
    }

    bool IsInitializing() const
    {
        return state_ == State::INITIALIZING;
    }

    bool IsInitialized() const
    {
        return state_ == State::INITIALIZED;
    }

    bool IsLoaded() const
    {
        return state_ >= State::LOADED;
    }

    bool IsErroneous() const
    {
        return state_ == State::ERRONEOUS;
    }

    static constexpr uint32_t GetStateOffset()
    {
        return MEMBER_OFFSET(Class, state_);
    }

    static constexpr uint32_t GetComponentTypeOffset()
    {
        return MEMBER_OFFSET(Class, component_type_);
    }

    static constexpr uint32_t GetTypeOffset()
    {
        return MEMBER_OFFSET(Class, type_);
    }

    static constexpr uint32_t GetBaseOffset()
    {
        return MEMBER_OFFSET(Class, base_);
    }

    uint8_t GetInitializedValue()
    {
        return static_cast<uint8_t>(State::INITIALIZED);
    }

    bool IsVerifiedSuccess() const
    {
        return (IsVerified() && (!IsErroneous()));
    }
    void SetInitTid(uint32_t id)
    {
        init_tid_ = id;
    }

    uint32_t GetInitTid() const
    {
        return init_tid_;
    }

    static size_t GetVTableOffset()
    {
        return ClassHelper::ComputeClassSize(0, 0, 0, 0, 0, 0, 0, 0);
    }

    uint32_t GetNumVirtualMethods() const
    {
        return num_vmethods_;
    }

    void SetNumVirtualMethods(uint32_t n)
    {
        num_vmethods_ = n;
    }

    uint32_t GetNumCopiedMethods() const
    {
        return num_copied_methods_;
    }

    void SetNumCopiedMethods(uint32_t n)
    {
        num_copied_methods_ = n;
    }

    uint32_t GetNumStaticFields() const
    {
        return num_sfields_;
    }

    void SetNumStaticFields(uint32_t n)
    {
        num_sfields_ = n;
    }

    void SetHasDefaultMethods()
    {
        access_flags_ |= ACC_HAS_DEFAULT_METHODS;
    }

    bool HasDefaultMethods() const
    {
        return (access_flags_ & ACC_HAS_DEFAULT_METHODS) != 0;
    }

    size_t GetIMTOffset()
    {
        return GetVTableOffset() + vtable_size_ * sizeof(uintptr_t);
    }

    size_t GetIMTOffset() const
    {
        return GetVTableOffset() + vtable_size_ * sizeof(uintptr_t);
    }

    std::string GetName() const;

    ClassLinkerContext *GetLoadContext() const
    {
        ASSERT(load_context_ != nullptr);
        return load_context_;
    }

    void SetLoadContext(ClassLinkerContext *context)
    {
        ASSERT(context != nullptr);
        load_context_ = context;
    }

    template <class Pred>
    Field *FindInstanceField(Pred pred) const;

    template <class Pred>
    Field *FindStaticField(Pred pred) const;

    template <class Pred>
    Field *FindField(Pred pred) const;

    template <class Pred>
    Field *FindDeclaredField(Pred pred) const;

    Field *GetInstanceFieldByName(const uint8_t *mutf8_name) const;

    Field *GetStaticFieldByName(const uint8_t *mutf8_name) const;

    Field *GetDeclaredFieldByName(const uint8_t *mutf8_name) const;

    template <class Pred>
    Method *FindVirtualClassMethod(Pred pred) const;

    template <class Pred>
    Method *FindInterfaceMethod(Pred pred) const;

    template <class Pred>
    Method *FindVirtualInterfaceMethod(Pred pred) const;

    template <class Pred>
    Method *FindStaticInterfaceMethod(Pred pred) const;

    template <class Pred>
    Method *FindStaticClassMethod(Pred pred) const;

    template <class Pred>
    Method *FindClassMethod(Pred pred) const;

    Method *GetDirectMethod(const uint8_t *mutf8_name, const Method::Proto &proto) const;

    Method *GetClassMethod(const uint8_t *mutf8_name, const Method::Proto &proto) const;

    Method *GetInterfaceMethod(const uint8_t *mutf8_name, const Method::Proto &proto) const;

    Method *GetDirectMethod(const uint8_t *mutf8_name) const;

    Method *GetClassMethod(const uint8_t *mutf8_name) const;

    Method *ResolveVirtualMethod(const Method *method) const;

    template <class T, bool is_volatile = false>
    T GetFieldPrimitive(size_t offset) const;

    template <class T, bool is_volatile = false>
    void SetFieldPrimitive(size_t offset, T value);

    template <bool is_volatile = false, bool need_read_barrier = true>
    ObjectHeader *GetFieldObject(size_t offset) const;

    template <bool is_volatile = false, bool need_write_barrier = true>
    void SetFieldObject(size_t offset, ObjectHeader *value);

    template <class T>
    T GetFieldPrimitive(const Field &field) const;

    template <class T>
    void SetFieldPrimitive(const Field &field, T value);

    template <bool need_read_barrier = true>
    ObjectHeader *GetFieldObject(const Field &field) const;

    template <bool need_write_barrier = true>
    void SetFieldObject(const Field &field, ObjectHeader *value);

    // Pass thread parameter to speed up interpreter
    template <bool need_read_barrier = true>
    ObjectHeader *GetFieldObject(ManagedThread *thread, const Field &field) const;

    template <bool need_write_barrier = true>
    void SetFieldObject(ManagedThread *thread, const Field &field, ObjectHeader *value);

    template <class T>
    T GetFieldPrimitive(size_t offset, std::memory_order memory_order) const;

    template <class T>
    void SetFieldPrimitive(size_t offset, T value, std::memory_order memory_order);

    template <bool need_read_barrier = true>
    ObjectHeader *GetFieldObject(size_t offset, std::memory_order memory_order) const;

    template <bool need_write_barrier = true>
    void SetFieldObject(size_t offset, ObjectHeader *value, std::memory_order memory_order);

    template <typename T>
    bool CompareAndSetFieldPrimitive(size_t offset, T old_value, T new_value, std::memory_order memory_order,
                                     bool strong);

    template <bool need_write_barrier = true>
    bool CompareAndSetFieldObject(size_t offset, ObjectHeader *old_value, ObjectHeader *new_value,
                                  std::memory_order memory_order, bool strong);

    template <typename T>
    T CompareAndExchangeFieldPrimitive(size_t offset, T old_value, T new_value, std::memory_order memory_order,
                                       bool strong);

    template <bool need_write_barrier = true>
    ObjectHeader *CompareAndExchangeFieldObject(size_t offset, ObjectHeader *old_value, ObjectHeader *new_value,
                                                std::memory_order memory_order, bool strong);

    template <typename T>
    T GetAndSetFieldPrimitive(size_t offset, T value, std::memory_order memory_order);

    template <bool need_write_barrier = true>
    ObjectHeader *GetAndSetFieldObject(size_t offset, ObjectHeader *value, std::memory_order memory_order);

    template <typename T>
    T GetAndAddFieldPrimitive(size_t offset, T value, std::memory_order memory_order);

    template <typename T>
    T GetAndBitwiseOrFieldPrimitive(size_t offset, T value, std::memory_order memory_order);

    template <typename T>
    T GetAndBitwiseAndFieldPrimitive(size_t offset, T value, std::memory_order memory_order);

    template <typename T>
    T GetAndBitwiseXorFieldPrimitive(size_t offset, T value, std::memory_order memory_order);

    void DumpClass(std::ostream &os, size_t flags);

    static UniqId CalcUniqId(const panda_file::File *file, panda_file::File::EntityId file_id);

    // for synthetic classes, like arrays
    static UniqId CalcUniqId(const uint8_t *descriptor);

    UniqId GetUniqId() const
    {
        auto id = uniq_id_.load();
        if (id == 0) {
            id = CalcUniqId();
            uniq_id_.store(id);
        }
        return id;
    }

    void SetRefFieldsNum(uint32_t num, bool is_static)
    {
        if (is_static) {
            num_refsfields_ = num;
        } else {
            num_reffields_ = num;
        }
    }

    void SetRefFieldsOffset(uint32_t offset, bool is_static)
    {
        if (is_static) {
            offset_refsfields_ = offset;
        } else {
            offset_reffields_ = offset;
        }
    }

    void SetVolatileRefFieldsNum(uint32_t num, bool is_static)
    {
        if (is_static) {
            volatile_refsfields_num_ = num;
        } else {
            volatile_reffields_num_ = num;
        }
    }

    template <bool is_static>
    uint32_t GetRefFieldsNum() const
    {
        return is_static ? num_refsfields_ : num_reffields_;
    }

    template <bool is_static>
    uint32_t GetRefFieldsOffset() const
    {
        return is_static ? offset_refsfields_ : offset_reffields_;
    }

    template <bool is_static>
    uint32_t GetVolatileRefFieldsNum() const
    {
        return is_static ? volatile_refsfields_num_ : volatile_reffields_num_;
    }

    panda_file::File::EntityId ResolveClassIndex(panda_file::File::Index idx) const
    {
        return class_idx_[idx];
    }

    panda_file::File::EntityId ResolveMethodIndex(panda_file::File::Index idx) const
    {
        return method_idx_[idx];
    }

    panda_file::File::EntityId ResolveFieldIndex(panda_file::File::Index idx) const
    {
        return field_idx_[idx];
    }

    Span<const panda_file::File::EntityId> GetClassIndex() const
    {
        return class_idx_;
    }

    void SetClassIndex(Span<const panda_file::File::EntityId> index)
    {
        class_idx_ = index;
    }

    Span<const panda_file::File::EntityId> GetMethodIndex() const
    {
        return method_idx_;
    }

    void SetMethodIndex(Span<const panda_file::File::EntityId> index)
    {
        method_idx_ = index;
    }

    Span<const panda_file::File::EntityId> GetFieldIndex() const
    {
        return field_idx_;
    }

    void SetFieldIndex(Span<const panda_file::File::EntityId> index)
    {
        field_idx_ = index;
    }

    static Class *FromClassObject(const ObjectHeader *obj);

    static size_t GetClassObjectSizeFromClass(Class *cls);

    ~Class() = default;

    NO_COPY_SEMANTIC(Class);
    NO_MOVE_SEMANTIC(Class);

private:
    enum class FindFilter { STATIC, INSTANCE, ALL, COPIED };

    template <FindFilter filter>
    Span<Field> GetFields() const;

    template <FindFilter filter, class Pred>
    Field *FindDeclaredField(Pred pred) const;

    template <FindFilter filter, class Pred>
    Field *FindField(Pred pred) const;

    template <FindFilter filter>
    Span<Method> GetMethods() const;

    template <FindFilter filter, class Pred>
    Method *FindDirectMethod(Pred pred) const;

    template <FindFilter filter, class Pred>
    Method *FindClassMethod(Pred pred) const;

    template <FindFilter filter, class Pred>
    Method *FindInterfaceMethod(Pred pred) const;

    Span<std::byte> GetClassSpan()
    {
        return Span(reinterpret_cast<std::byte *>(this), class_size_);
    }

    Span<const std::byte> GetClassSpan() const
    {
        return Span(reinterpret_cast<const std::byte *>(this), class_size_);
    }

    Class *base_ {nullptr};
    const panda_file::File *panda_file_ {nullptr};
    // Decscriptor is a valid MUTF8 string. See docs/file_format.md#typedescriptor for more information.
    const uint8_t *descriptor_;
    Method *methods_ {nullptr};
    Field *fields_ {nullptr};
    Class **ifaces_ {nullptr};

    panda_file::File::EntityId file_id_ {};
    uint32_t vtable_size_;
    uint32_t imt_size_;
    uint32_t class_size_;
    uint32_t access_flags_ {0};

    uint32_t num_methods_ {0};
    uint32_t num_vmethods_ {0};
    uint32_t num_copied_methods_ {0};
    uint32_t num_fields_ {0};
    uint32_t num_sfields_ {0};
    uint32_t num_ifaces_ {0};
    uint32_t init_tid_ {0};

    ITable itable_;

    // For array types this field contains array's element size, for non-array type it should be zero.
    Class *component_type_ {nullptr};

    ClassLinkerContext *load_context_ {nullptr};

    panda_file::Type type_ {panda_file::Type::TypeId::REFERENCE};
    std::atomic<State> state_ {State::INITIAL};

    UniqId CalcUniqId() const;
    mutable std::atomic<UniqId> uniq_id_ {0};

    uint32_t num_reffields_ {0};      // instance reference fields num
    uint32_t num_refsfields_ {0};     // static reference fields num
    uint32_t offset_reffields_ {0};   // first instance reference fields offset in object layout
    uint32_t offset_refsfields_ {0};  // first static reference fields offset in object layout
    uint32_t volatile_reffields_num_ {0};
    uint32_t volatile_refsfields_num_ {0};

    Span<const panda_file::File::EntityId> class_idx_ {nullptr, nullptr};
    Span<const panda_file::File::EntityId> method_idx_ {nullptr, nullptr};
    Span<const panda_file::File::EntityId> field_idx_ {nullptr, nullptr};
};

std::ostream &operator<<(std::ostream &os, const Class::State &state);

#ifdef PANDA_TARGET_64

constexpr uint32_t CLASS_MANAGE_OBJECT_OFFSET = 8U;
static_assert(CLASS_MANAGE_OBJECT_OFFSET == Class::OffsetOfManageObject());

constexpr uint32_t CLASS_STATE_OFFSET = 153U;
static_assert(CLASS_STATE_OFFSET == panda::Class::GetStateOffset());

constexpr uint32_t CLASS_COMPONENT_TYPE_OFFSET = 136U;
static_assert(CLASS_COMPONENT_TYPE_OFFSET == panda::Class::GetComponentTypeOffset());

constexpr uint32_t CLASS_TYPE_OFFSET = 152U;
static_assert(CLASS_TYPE_OFFSET == panda::Class::GetTypeOffset());

constexpr uint32_t CLASS_BASE_OFFSET = 24U;
static_assert(CLASS_BASE_OFFSET == panda::Class::GetBaseOffset());

#endif

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_CLASS_H_
