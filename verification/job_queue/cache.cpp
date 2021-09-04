/**
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

#include "cache.h"

#include "runtime/include/runtime.h"

#include "runtime/include/class.h"
#include "runtime/include/method.h"
#include "runtime/include/field.h"

#include "runtime/include/class_helper.h"
#include "runtime/include/language_context.h"

#include "libpandabase/utils/logger.h"
#include "libpandabase/utils/utf.h"
#include "libpandabase/utils/hash.h"

#include "libpandafile/method_data_accessor-inl.h"
#include "libpandafile/class_data_accessor.h"
#include "libpandafile/class_data_accessor-inl.h"
#include "libpandafile/code_data_accessor.h"
#include "libpandafile/code_data_accessor-inl.h"
#include "libpandafile/field_data_accessor-inl.h"
#include "libpandafile/proto_data_accessor.h"
#include "libpandafile/proto_data_accessor-inl.h"
#include "libpandafile/modifiers.h"

#include "verification/util/invalid_ref.h"

#include "macros.h"

#include <iostream>

namespace panda::verifier {

using FastAPIClassRW = CacheOfRuntimeThings::FastAPIClass<access::ReadWrite>;
using FastAPIClassRO = CacheOfRuntimeThings::FastAPIClass<access::ReadOnly>;

PandaString CacheOfRuntimeThings::GetName(const CacheOfRuntimeThings::CachedClass &cached_class)
{
    if (cached_class.type_id == panda_file::Type::TypeId::REFERENCE) {
        return ClassHelper::GetName<PandaString>(cached_class.name);
    }
    return {ClassHelper::GetPrimitiveTypeStr(cached_class.type_id)};
}

PandaString CacheOfRuntimeThings::GetName(const DescriptorString &descriptor)
{
    return ClassHelper::GetName<PandaString>(descriptor);
}

PandaString CacheOfRuntimeThings::GetName(const CacheOfRuntimeThings::CachedMethod &cached_method)
{
    PandaOStringStream out;
    out << GetName(cached_method.klass);
    out << "::";
    out << utf::Mutf8AsCString(cached_method.name);
    out << " : ";
    size_t idx = 0;
    for (const auto &arg : cached_method.signature) {
        if (idx > 1) {
            out << ", ";
        }
        if (IsDescriptor(arg)) {
            out << GetName(GetDescriptor(arg));
        } else if (IsRef(arg)) {
            out << GetName(GetRef(arg));
        } else {
            UNREACHABLE();
        }
        if (idx == 0) {
            out << " ( ";
        }
        ++idx;
    }
    out << " )";
    return out.str();
}

PandaString CacheOfRuntimeThings::GetName(const CacheOfRuntimeThings::CachedField &cached_field)
{
    auto str = GetName(cached_field.klass);
    str += ".";
    str += utf::Mutf8AsCString(cached_field.name);
    str += " : ";
    const auto &type = cached_field.type;
    if (IsRef(type)) {
        str += GetName(GetRef(type));
    } else if (IsDescriptor(type)) {
        str += GetName(GetDescriptor(type));
    } else {
        UNREACHABLE();
    }
    return str;
}

template <>
const panda::LanguageContextBase &FastAPIClassRW::GetLanguageContextBase(panda_file::SourceLang src_lang) const
{
    if (src_lang == panda_file::SourceLang::PANDA_ASSEMBLY) {
        return core_lang_ctx;
    }
    UNREACHABLE();
    return core_lang_ctx;
}

template <>
const panda::LanguageContextBase &FastAPIClassRO::GetLanguageContextBase(panda_file::SourceLang src_lang) const
{
    if (src_lang == panda_file::SourceLang::PANDA_ASSEMBLY) {
        return core_lang_ctx;
    }
    UNREACHABLE();
    return core_lang_ctx;
}

template <>
const CacheOfRuntimeThings::CachedClass &FastAPIClassRO::GetPrimitiveClass(panda_file::SourceLang src_lang,
                                                                           panda_file::Type::TypeId id) const
{
    return GetContext(src_lang).primitive_classes[id];
}

template <>
const CacheOfRuntimeThings::CachedClass &FastAPIClassRW::GetPrimitiveClass(panda_file::SourceLang src_lang,
                                                                           panda_file::Type::TypeId id) const
{
    return GetContext(src_lang).primitive_classes[id];
}

template <>
template <>
CacheOfRuntimeThings::CachedClass &FastAPIClassRW::Link<CacheOfRuntimeThings::CachedClass>(
    CacheOfRuntimeThings::CachedClass &cached_class);

template <>
template <>
CacheOfRuntimeThings::CachedMethod &FastAPIClassRW::Link<CacheOfRuntimeThings::CachedMethod>(
    CacheOfRuntimeThings::CachedMethod &cached_method);

template <>
template <>
CacheOfRuntimeThings::CachedField &FastAPIClassRW::Link<CacheOfRuntimeThings::CachedField>(
    CacheOfRuntimeThings::CachedField &cached_field);

template <>
template <>
CacheOfRuntimeThings::CachedClass &FastAPIClassRW::GetFromCache<CacheOfRuntimeThings::CachedClass>(
    panda_file::SourceLang src_lang, CacheOfRuntimeThings::Id id)
{
    auto &class_cache = GetContext(src_lang).class_cache;
    auto it = class_cache.find(id);
    if (it == class_cache.end()) {
        return Invalid<CachedClass>();
    }
    return Link(it->second);
}

template <>
template <>
CacheOfRuntimeThings::CachedMethod &FastAPIClassRW::GetFromCache<CacheOfRuntimeThings::CachedMethod>(
    panda_file::SourceLang src_lang, CacheOfRuntimeThings::Id id)
{
    auto &method_cache = GetContext(src_lang).method_cache;
    auto it = method_cache.find(id);
    if (it == method_cache.end()) {
        return Invalid<CachedMethod>();
    }
    return Link(it->second);
}

template <>
template <>
CacheOfRuntimeThings::CachedField &FastAPIClassRW::GetFromCache<CacheOfRuntimeThings::CachedField>(
    panda_file::SourceLang src_lang, CacheOfRuntimeThings::Id id)
{
    auto &field_cache = GetContext(src_lang).field_cache;
    auto it = field_cache.find(id);
    if (it == field_cache.end()) {
        return Invalid<CachedField>();
    }
    return Link(it->second);
}

namespace {

CacheOfRuntimeThings::CachedClass::FlagsValue GetClassFlags(uint32_t raw_flags)
{
    CacheOfRuntimeThings::CachedClass::FlagsValue flags;
    flags[CacheOfRuntimeThings::CachedClass::Flag::PUBLIC] = (raw_flags & ACC_PUBLIC) != 0;
    flags[CacheOfRuntimeThings::CachedClass::Flag::FINAL] = (raw_flags & ACC_FINAL) != 0;
    flags[CacheOfRuntimeThings::CachedClass::Flag::ANNOTATION] = (raw_flags & ACC_ANNOTATION) != 0;
    flags[CacheOfRuntimeThings::CachedClass::Flag::ENUM] = (raw_flags & ACC_ENUM) != 0;

    flags[CacheOfRuntimeThings::CachedClass::Flag::ABSTRACT] = (raw_flags & ACC_ABSTRACT) != 0;
    flags[CacheOfRuntimeThings::CachedClass::Flag::INTERFACE] = (raw_flags & ACC_INTERFACE) != 0;

    return flags;
}

CacheOfRuntimeThings::CachedMethod::FlagsValue GetMethodFlags(const panda_file::MethodDataAccessor &mda)
{
    CacheOfRuntimeThings::CachedMethod::FlagsValue flags;

    flags[CacheOfRuntimeThings::CachedMethod::Flag::STATIC] = mda.IsStatic();
    flags[CacheOfRuntimeThings::CachedMethod::Flag::NATIVE] = mda.IsNative();
    flags[CacheOfRuntimeThings::CachedMethod::Flag::PUBLIC] = mda.IsPublic();
    flags[CacheOfRuntimeThings::CachedMethod::Flag::PRIVATE] = mda.IsPrivate();
    flags[CacheOfRuntimeThings::CachedMethod::Flag::PROTECTED] = mda.IsProtected();
    flags[CacheOfRuntimeThings::CachedMethod::Flag::SYNTHETIC] = mda.IsSynthetic();
    flags[CacheOfRuntimeThings::CachedMethod::Flag::ABSTRACT] = mda.IsAbstract();
    flags[CacheOfRuntimeThings::CachedMethod::Flag::FINAL] = mda.IsFinal();
    return flags;
}

CacheOfRuntimeThings::CachedField::FlagsValue GetFieldFlags(const panda_file::FieldDataAccessor &fda)
{
    CacheOfRuntimeThings::CachedField::FlagsValue flags;
    flags[CacheOfRuntimeThings::CachedField::Flag::STATIC] = fda.IsStatic();
    flags[CacheOfRuntimeThings::CachedField::Flag::VOLATILE] = fda.IsVolatile();
    flags[CacheOfRuntimeThings::CachedField::Flag::PUBLIC] = fda.IsPublic();
    flags[CacheOfRuntimeThings::CachedField::Flag::PROTECTED] = fda.IsProtected();
    flags[CacheOfRuntimeThings::CachedField::Flag::FINAL] = fda.IsFinal();
    flags[CacheOfRuntimeThings::CachedField::Flag::PRIVATE] = fda.IsPrivate();
    return flags;
}

}  // namespace

template <>
CacheOfRuntimeThings::CachedClass &FastAPIClassRW::MakeSyntheticClass(panda_file::SourceLang src_lang,
                                                                      const uint8_t *descriptor,
                                                                      panda_file::Type::TypeId type_id, uint32_t flags)
{
    auto &data = GetContext(src_lang);

    auto id = Class::CalcUniqId(descriptor);

    CachedClass cached_class {id, descriptor, src_lang, type_id, {}, {}, GetClassFlags(flags),
                              {}, {},         false,    nullptr, {}};

    auto &result = data.class_cache.emplace(id, std::move(cached_class)).first->second;
    data.descr_lookup.emplace(result.name, std::cref(result));
    return result;
}

template <>
CacheOfRuntimeThings::CachedMethod &FastAPIClassRW::MakeSyntheticMethod(
    CacheOfRuntimeThings::CachedClass &cached_class, const uint8_t *name,
    const std::function<void(CacheOfRuntimeThings::CachedClass &, CacheOfRuntimeThings::CachedMethod &)> &sig_filler)
{
    auto id = Method::CalcUniqId(cached_class.name, name);

    CacheOfRuntimeThings::CachedMethod cached_method {
        id, {}, name, std::cref(cached_class), {}, {}, {}, {}, {}, 0, 0, {}, nullptr, 0, false, nullptr, {}};

    auto &data = GetContext(cached_class.source_lang);
    auto &result = data.method_cache.emplace(id, std::move(cached_method)).first->second;
    sig_filler(cached_class, result);
    CalcMethodHash(result);
    cached_class.methods.insert_or_assign(result.hash, std::cref(result));
    return result;
}

template <>
CacheOfRuntimeThings::CachedMethod &FastAPIClassRW::AddArrayCtor(CacheOfRuntimeThings::CachedClass &array)
{
    auto &lang_ctx = GetLanguageContextBase(array.source_lang);
    auto &&h = [&](CacheOfRuntimeThings::CachedClass &c, CacheOfRuntimeThings::CachedMethod &cm) {
        // register in methods_cache by id
        size_t dims = ClassHelper::GetDimensionality(c.name);
        cm.num_args = dims;
        // method return type first
        cm.signature.push_back(std::cref(c));
        while (dims-- != 0) {
            cm.signature.push_back(
                DescriptorString(ClassHelper::GetPrimitiveTypeDescriptorStr(panda_file::Type::TypeId::I32)));
        }
    };
    return MakeSyntheticMethod(array, lang_ctx.GetCtorName(), h);
}

template <>
CacheOfRuntimeThings::CachedClass &FastAPIClassRW::AddArray(panda_file::SourceLang src_lang, const uint8_t *descr)
{
    auto &data = GetContext(src_lang);
    auto &array =
        MakeSyntheticClass(src_lang, descr, panda_file::Type::TypeId::REFERENCE, ACC_PUBLIC | ACC_FINAL | ACC_ABSTRACT);
    array.flags[CacheOfRuntimeThings::CachedClass::Flag::ARRAY_CLASS] = true;
    array.ancestors.push_back(data.object_descr);
    auto comp_descr = DescriptorString(ClassHelper::GetComponentDescriptor(descr));
    array.array_component = comp_descr;
    if (comp_descr.GetLength() > 1) {
        array.flags[CacheOfRuntimeThings::CachedClass::Flag::OBJECT_ARRAY_CLASS] = true;
    }
    AddArrayCtor(array);
    return array;
}

template <>
void FastAPIClassRW::InitializePandaAssemblyPrimitiveRoot(panda_file::Type::TypeId type_id)
{
    auto &data = GetContext(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto &prim_classes = data.primitive_classes;
    auto &C =
        MakeSyntheticClass(panda_file::SourceLang::PANDA_ASSEMBLY, ClassHelper::GetPrimitiveTypeDescriptorStr(type_id),
                           type_id, ACC_PUBLIC | ACC_FINAL | ACC_ABSTRACT);
    C.flags[CacheOfRuntimeThings::CachedClass::Flag::PRIMITIVE] = true;
    prim_classes[type_id] = std::cref(C);
}

template <>
void FastAPIClassRW::InitializePandaAssemblyRootClasses()
{
    DescriptorString obj_descriptor = core_lang_ctx.GetObjectClassDescriptor();

    auto &data = GetContext(panda_file::SourceLang::PANDA_ASSEMBLY);

    data.object_descr = obj_descriptor;
    data.string_descr = core_lang_ctx.GetStringClassDescriptor();
    data.string_array_descr = core_lang_ctx.GetStringArrayClassDescriptor();

    // primitive
    InitializePandaAssemblyPrimitiveRoot(panda_file::Type::TypeId::VOID);
    InitializePandaAssemblyPrimitiveRoot(panda_file::Type::TypeId::U1);
    InitializePandaAssemblyPrimitiveRoot(panda_file::Type::TypeId::I8);
    InitializePandaAssemblyPrimitiveRoot(panda_file::Type::TypeId::U8);
    InitializePandaAssemblyPrimitiveRoot(panda_file::Type::TypeId::I16);
    InitializePandaAssemblyPrimitiveRoot(panda_file::Type::TypeId::U16);
    InitializePandaAssemblyPrimitiveRoot(panda_file::Type::TypeId::I32);
    InitializePandaAssemblyPrimitiveRoot(panda_file::Type::TypeId::U32);
    InitializePandaAssemblyPrimitiveRoot(panda_file::Type::TypeId::I64);
    InitializePandaAssemblyPrimitiveRoot(panda_file::Type::TypeId::U64);
    InitializePandaAssemblyPrimitiveRoot(panda_file::Type::TypeId::F32);
    InitializePandaAssemblyPrimitiveRoot(panda_file::Type::TypeId::F64);
    InitializePandaAssemblyPrimitiveRoot(panda_file::Type::TypeId::TAGGED);

    // object
    MakeSyntheticClass(panda_file::SourceLang::PANDA_ASSEMBLY, data.object_descr, panda_file::Type::TypeId::REFERENCE,
                       ACC_PUBLIC | ACC_FINAL | ACC_ABSTRACT);

    auto &Str = MakeSyntheticClass(panda_file::SourceLang::PANDA_ASSEMBLY, core_lang_ctx.GetStringClassDescriptor(),
                                   panda_file::Type::TypeId::REFERENCE, ACC_PUBLIC | ACC_FINAL | ACC_ABSTRACT);
    auto &Class = MakeSyntheticClass(panda_file::SourceLang::PANDA_ASSEMBLY, core_lang_ctx.GetClassClassDescriptor(),
                                     panda_file::Type::TypeId::REFERENCE, ACC_PUBLIC | ACC_FINAL | ACC_ABSTRACT);

    Str.ancestors.push_back(obj_descriptor);
    Class.ancestors.push_back(obj_descriptor);

    AddArray(panda_file::SourceLang::PANDA_ASSEMBLY, reinterpret_cast<const uint8_t *>("[Z"));
    AddArray(panda_file::SourceLang::PANDA_ASSEMBLY, reinterpret_cast<const uint8_t *>("[B"));
    AddArray(panda_file::SourceLang::PANDA_ASSEMBLY, reinterpret_cast<const uint8_t *>("[S"));
    AddArray(panda_file::SourceLang::PANDA_ASSEMBLY, reinterpret_cast<const uint8_t *>("[C"));
    AddArray(panda_file::SourceLang::PANDA_ASSEMBLY, reinterpret_cast<const uint8_t *>("[I"));
    AddArray(panda_file::SourceLang::PANDA_ASSEMBLY, reinterpret_cast<const uint8_t *>("[J"));
    AddArray(panda_file::SourceLang::PANDA_ASSEMBLY, reinterpret_cast<const uint8_t *>("[F"));
    AddArray(panda_file::SourceLang::PANDA_ASSEMBLY, reinterpret_cast<const uint8_t *>("[D"));
    AddArray(panda_file::SourceLang::PANDA_ASSEMBLY, core_lang_ctx.GetStringArrayClassDescriptor());
}

CacheOfRuntimeThings::MethodHash CacheOfRuntimeThings::CalcMethodHash(const panda_file::File *pf,
                                                                      const panda_file::MethodDataAccessor &mda)
{
    return CalcMethodHash(pf->GetStringData(mda.GetNameId()).data, [&](auto hash_str) {
        const_cast<panda_file::MethodDataAccessor &>(mda).EnumerateTypesInProto([&](auto type, auto class_file_id) {
            if (type.GetId() == panda_file::Type::TypeId::REFERENCE) {
                hash_str(pf->GetStringData(class_file_id).data);
            } else {
                hash_str(ClassHelper::GetPrimitiveTypeDescriptorStr(type.GetId()));
            }
        });
    });
}

CacheOfRuntimeThings::CachedMethod &CacheOfRuntimeThings::CalcMethodHash(
    CacheOfRuntimeThings::CachedMethod &cached_method)
{
    cached_method.hash = CalcMethodHash(cached_method.name, [&](auto hash_str) {
        for (const auto &arg : cached_method.signature) {
            if (CacheOfRuntimeThings::IsDescriptor(arg)) {
                hash_str(CacheOfRuntimeThings::GetDescriptor(arg));
            } else {
                hash_str(CacheOfRuntimeThings::GetRef(arg).name);
            }
        }
    });
    return cached_method;
}

CacheOfRuntimeThings::FieldHash CacheOfRuntimeThings::CalcFieldNameAndTypeHash(const panda_file::File *pf,
                                                                               const panda_file::FieldDataAccessor &fda)
{
    uint64_t hash;
    uint64_t name_hash = PseudoFnvHashString(pf->GetStringData(fda.GetNameId()).data);

    uint64_t type_hash;

    auto type = panda_file::Type::GetTypeFromFieldEncoding(fda.GetType());

    if (type.GetId() != panda_file::Type::TypeId::REFERENCE) {
        type_hash = PseudoFnvHashItem(ClassHelper::GetPrimitiveTypeDescriptorChar(type.GetId()));
    } else {
        auto type_class_id = panda_file::File::EntityId(fda.GetType());
        const auto *descr = pf->GetStringData(type_class_id).data;
        type_hash = PseudoFnvHashString(descr);
    }

    auto constexpr SHIFT = 32U;

    hash = (name_hash << SHIFT) | type_hash;

    return hash;
}

template <>
CacheOfRuntimeThings::CachedClass &FastAPIClassRW::AddToCache(const panda_file::File *pf,
                                                              panda_file::File::EntityId entity_id);

template <>
CacheOfRuntimeThings::CachedMethod &FastAPIClassRW::AddToCache(const CacheOfRuntimeThings::CachedClass &cached_class,
                                                               const panda_file::File *pf,
                                                               const panda_file::MethodDataAccessor &mda);

template <>
CacheOfRuntimeThings::CachedField &FastAPIClassRW::AddToCache(const CacheOfRuntimeThings::CachedClass &cached_class,
                                                              const panda_file::File *pf,
                                                              const panda_file::FieldDataAccessor &fda);

static void AddAncestors(CacheOfRuntimeThings::CachedClass *cached_class, panda_file::ClassDataAccessor *cda,
                         const CacheOfRuntimeThings::LangContext &data)
{
    auto *pf = &cda->GetPandaFile();

    cda->EnumerateInterfaces([&](auto entity_id) {
        DescriptorString descr;
        if (entity_id.GetOffset() == 0) {
            descr = data.object_descr;
        } else {
            descr = pf->GetStringData(entity_id).data;
        }
        if (descr != cached_class->name) {
            cached_class->ancestors.emplace_back(descr);
        }
    });

    auto super_class_id = cda->GetSuperClassId();

    DescriptorString descr;

    if (super_class_id.GetOffset() == 0) {
        descr = data.object_descr;
    } else {
        descr = pf->GetStringData(super_class_id).data;
    }

    if (descr != cached_class->name) {
        cached_class->ancestors.emplace_back(descr);
    }
}

template <>
CacheOfRuntimeThings::CachedClass &FastAPIClassRW::AddToCache(const panda_file::File *pf,
                                                              panda_file::File::EntityId entity_id)
{
    auto id = Class::CalcUniqId(pf, entity_id);

    panda_file::ClassDataAccessor cda {*pf, entity_id};

    panda_file::SourceLang src_lang;

    auto src_lang_opt = cda.GetSourceLang();
    if (!src_lang_opt) {
        src_lang = panda_file::SourceLang::PANDA_ASSEMBLY;
    } else {
        src_lang = *src_lang_opt;
    }

    auto &cached_class_ref = GetFromCache<CachedClass>(src_lang, id);
    if (Valid(cached_class_ref)) {
        return cached_class_ref;
    }

    CachedClass cached_class;

    cached_class.flags = GetClassFlags(cda.GetAccessFlags());

    cached_class.id = id;

    cached_class.source_lang = src_lang;
    cached_class.type_id = panda_file::Type::TypeId::REFERENCE;

    cached_class.ancestors.reserve(cda.GetIfacesNumber() + 1);

    auto descriptor = cda.GetDescriptor();

    cached_class.name = descriptor;

    auto &data = GetContext(src_lang);

    AddAncestors(&cached_class, &cda, data);

    cached_class.file = pf;
    cached_class.file_id = entity_id;
    cached_class.linked = false;

    cached_class.methods.reserve(cda.GetMethodsNumber());
    cached_class.fields.reserve(cda.GetFieldsNumber());

    auto &stored_cached_class = data.class_cache.emplace(id, std::move(cached_class)).first->second;

    cda.EnumerateMethods([&](const panda_file::MethodDataAccessor &mda) {
        if (!pf->IsExternal(mda.GetMethodId())) {
            auto &cached_method = AddToCache(stored_cached_class, pf, mda);
            stored_cached_class.methods.insert_or_assign(cached_method.hash, std::cref(cached_method));
        }
    });

    cda.EnumerateFields([&](const panda_file::FieldDataAccessor &fda) {
        if (!pf->IsExternal(fda.GetFieldId())) {
            auto &cached_field = AddToCache(stored_cached_class, pf, fda);
            stored_cached_class.fields.insert_or_assign(cached_field.hash, std::cref(cached_field));
        }
    });

    if (data.descr_lookup.count(stored_cached_class.name) == 0) {
        data.descr_lookup.emplace(stored_cached_class.name, std::cref(stored_cached_class));
    }

    data.file_cache.AddToCache<CachedClass>(pf, entity_id.GetOffset(), stored_cached_class);

    return stored_cached_class;
}

static void InitializeClassIndex(CacheOfRuntimeThings::CachedMethod *cached_method,
                                 CacheOfRuntimeThings::LangContext *data)
{
    auto *pf = cached_method->file;
    auto file_id = cached_method->file_id;

    auto &&class_index_table = pf->GetClassIndex(file_id);

    auto &class_index_table_ref =
        data->index_table_cache.GetFromCache<CacheOfRuntimeThings::ClassIndex>(pf, class_index_table);
    if (Valid(class_index_table_ref)) {
        cached_method->class_index = std::ref(class_index_table_ref);
    } else {
        CacheOfRuntimeThings::ClassIndex class_index;
        for (auto idx_class_id : class_index_table) {
            DescriptorString descr;

            auto type = panda_file::Type::GetTypeFromFieldEncoding(idx_class_id.GetOffset());

            if (type.IsReference()) {
                descr = pf->GetStringData(idx_class_id).data;
            } else {
                descr = CacheOfRuntimeThings::GetRef(data->primitive_classes[type.GetId()]).name;
            }

            class_index.emplace_back(descr);
        }
        class_index.shrink_to_fit();
        auto &index_table_ref = data->index_table_cache.AddToCache(pf, class_index_table, std::move(class_index));
        cached_method->class_index = std::ref(index_table_ref);
    }
}

static void InitializeMethodIndex(CacheOfRuntimeThings::CachedMethod *cached_method,
                                  CacheOfRuntimeThings::LangContext *data)
{
    auto *pf = cached_method->file;
    auto file_id = cached_method->file_id;

    auto &&method_index_table = pf->GetMethodIndex(file_id);

    auto &method_index_table_ref =
        data->index_table_cache.GetFromCache<CacheOfRuntimeThings::MethodIndex>(pf, method_index_table);
    if (Valid(method_index_table_ref)) {
        cached_method->method_index = std::ref(method_index_table_ref);
    } else {
        CacheOfRuntimeThings::MethodIndex method_index;
        for (auto idx_method_id : method_index_table) {
            method_index.emplace_back(idx_method_id);
        }
        method_index.shrink_to_fit();
        auto &index_table_ref = data->index_table_cache.AddToCache(pf, method_index_table, std::move(method_index));
        cached_method->method_index = std::ref(index_table_ref);
    }
}

static void InitializeFieldIndex(CacheOfRuntimeThings::CachedMethod *cached_method,
                                 CacheOfRuntimeThings::LangContext *data)
{
    auto *pf = cached_method->file;
    auto file_id = cached_method->file_id;

    auto &&field_index_table = pf->GetFieldIndex(file_id);

    auto &field_index_table_ref =
        data->index_table_cache.GetFromCache<CacheOfRuntimeThings::FieldIndex>(pf, field_index_table);
    if (Valid(field_index_table_ref)) {
        cached_method->field_index = std::ref(field_index_table_ref);
    } else {
        CacheOfRuntimeThings::FieldIndex field_index;
        for (auto idx_field_id : field_index_table) {
            field_index.emplace_back(idx_field_id);
        }
        field_index.shrink_to_fit();
        auto &index_table_ref = data->index_table_cache.AddToCache(pf, field_index_table, std::move(field_index));
        cached_method->field_index = std::ref(index_table_ref);
    }
}

static void InitializeHash(CacheOfRuntimeThings::CachedMethod *cached_method, const panda_file::MethodDataAccessor &mda,
                           const CacheOfRuntimeThings::LangContext &data)
{
    auto *pf = cached_method->file;

    cached_method->hash = CacheOfRuntimeThings::CalcMethodHash(cached_method->name, [&](auto hash_str) {
        const_cast<panda_file::MethodDataAccessor &>(mda).EnumerateTypesInProto([&](auto type, auto class_file_id) {
            auto type_id = type.GetId();
            if (type_id == panda_file::Type::TypeId::REFERENCE) {
                const auto *descr = pf->GetStringData(class_file_id).data;
                hash_str(descr);
                cached_method->signature.push_back(DescriptorString {descr});
            } else {
                hash_str(ClassHelper::GetPrimitiveTypeDescriptorStr(type_id));
                cached_method->signature.push_back(data.primitive_classes[type_id]);
            }
        });
    });
}

static void InitializeCode(CacheOfRuntimeThings::CachedMethod *cached_method, const panda_file::MethodDataAccessor &mda)
{
    auto *pf = cached_method->file;

    auto code_id = const_cast<panda_file::MethodDataAccessor &>(mda).GetCodeId();
    if (code_id) {
        panda_file::CodeDataAccessor cda {*pf, *code_id};
        cached_method->num_vregs = cda.GetNumVregs();
        cached_method->num_args = cda.GetNumArgs();
        cached_method->bytecode = cda.GetInstructions();
        cached_method->bytecode_size = cda.GetCodeSize();
        cda.EnumerateTryBlocks([&](const auto &try_block) {
            auto try_block_start = reinterpret_cast<const uint8_t *>(
                reinterpret_cast<uintptr_t>(cached_method->bytecode) + static_cast<uintptr_t>(try_block.GetStartPc()));
            auto try_block_end = reinterpret_cast<const uint8_t *>(reinterpret_cast<uintptr_t>(try_block_start) +
                                                                   static_cast<uintptr_t>(try_block.GetLength()));
            const_cast<panda_file::CodeDataAccessor::TryBlock &>(try_block).EnumerateCatchBlocks(
                [&](const auto &catch_block) {
                    auto handler_pc_ptr =
                        reinterpret_cast<const uint8_t *>(reinterpret_cast<uintptr_t>(cached_method->bytecode) +
                                                          static_cast<uintptr_t>(catch_block.GetHandlerPc()));
                    CacheOfRuntimeThings::CachedCatchBlock cached_catch_block {
                        try_block_start, try_block_end, DescriptorString {}, handler_pc_ptr, catch_block.GetCodeSize()};
                    auto type_idx = catch_block.GetTypeIdx();
                    if (type_idx != panda_file::INVALID_INDEX) {
                        if (type_idx < cached_method->class_index.get().size()) {
                            auto cls_item = cached_method->class_index.get()[type_idx];
                            CacheOfRuntimeThings::DescriptorString descr;
                            if (CacheOfRuntimeThings::IsDescriptor(cls_item)) {
                                descr = CacheOfRuntimeThings::GetDescriptor(cls_item);
                            } else {
                                descr = CacheOfRuntimeThings::GetRef(cls_item).name;
                            }
                            cached_catch_block.exception_type = descr;
                        } else {
                        }
                    }
                    // NOLINTNEXTLINE(performance-move-const-arg)
                    cached_method->catch_blocks.emplace_back(std::move(cached_catch_block));
                    return true;
                });
            return true;
        });
        cached_method->catch_blocks.shrink_to_fit();
    } else {
        cached_method->num_vregs = 0;
        cached_method->num_args = 0;
        cached_method->bytecode = nullptr;
        cached_method->bytecode_size = 0;
    }
}

static void InitializeCachedMethod(CacheOfRuntimeThings::CachedMethod *cached_method,
                                   const panda_file::MethodDataAccessor &mda, CacheOfRuntimeThings::LangContext *data)
{
    InitializeClassIndex(cached_method, data);
    InitializeMethodIndex(cached_method, data);
    InitializeFieldIndex(cached_method, data);
    InitializeHash(cached_method, mda, *data);
    InitializeCode(cached_method, mda);
}

template <>
CacheOfRuntimeThings::CachedMethod &FastAPIClassRW::AddToCache(const CacheOfRuntimeThings::CachedClass &cached_class,
                                                               const panda_file::File *pf,
                                                               const panda_file::MethodDataAccessor &mda)
{
    auto file_id = mda.GetMethodId();

    auto id = Method::CalcUniqId(pf, file_id);

    panda_file::SourceLang src_lang;

    auto src_lang_opt = const_cast<panda_file::MethodDataAccessor &>(mda).GetSourceLang();
    if (!src_lang_opt) {
        src_lang = cached_class.source_lang;
    } else {
        src_lang = *src_lang_opt;
    }

    auto &cached_method_ref = GetFromCache<CachedMethod>(src_lang, id);
    if (Valid(cached_method_ref)) {
        return cached_method_ref;
    }

    CachedMethod cached_method {id,
                                {},
                                pf->GetStringData(mda.GetNameId()).data,
                                std::cref(cached_class),
                                {},
                                {},
                                {},
                                {},
                                {},
                                0,
                                0,
                                GetMethodFlags(mda),
                                nullptr,
                                0,
                                false,
                                pf,
                                file_id};

    auto &data = GetContext(src_lang);

    InitializeCachedMethod(&cached_method, mda, &data);

    auto &result = data.method_cache.emplace(id, std::move(cached_method)).first->second;

    data.file_cache.AddToCache<CachedMethod>(pf, file_id.GetOffset(), result);

    return result;
}

template <>
CacheOfRuntimeThings::CachedField &FastAPIClassRW::AddToCache(const CacheOfRuntimeThings::CachedClass &cached_class,
                                                              const panda_file::File *pf,
                                                              const panda_file::FieldDataAccessor &fda)
{
    auto file_id = fda.GetFieldId();

    auto id = Field::CalcUniqId(pf, file_id);

    auto &cached_field_ref = GetFromCache<CachedField>(cached_class.source_lang, id);
    if (Valid(cached_field_ref)) {
        return cached_field_ref;
    }

    CachedField cached_field {id,
                              {},
                              pf->GetStringData(fda.GetNameId()).data,
                              std::cref(cached_class),
                              std::cref(Invalid<CachedClass>()),
                              GetFieldFlags(fda),
                              false,
                              pf,
                              file_id};

    auto type = panda_file::Type::GetTypeFromFieldEncoding(fda.GetType());

    // NB! keep hashing in sync with CalcFieldNameAndTypeHash
    uint64_t name_hash = PseudoFnvHashString(cached_field.name);

    uint64_t type_hash;

    auto &data = GetContext(cached_class.source_lang);

    if (type.GetId() != panda_file::Type::TypeId::REFERENCE) {
        cached_field.type = std::cref(data.primitive_classes[type.GetId()]);
        type_hash = PseudoFnvHashItem(ClassHelper::GetPrimitiveTypeDescriptorChar(type.GetId()));
    } else {
        auto type_class_id = panda_file::File::EntityId(fda.GetType());
        const auto *descr = pf->GetStringData(type_class_id).data;
        cached_field.type = DescriptorString(descr);
        type_hash = PseudoFnvHashString(descr);
    }

    auto constexpr SHIFT = 32U;

    cached_field.hash = (name_hash << SHIFT) | type_hash;

    // NOLINTNEXTLINE(performance-move-const-arg)
    auto &result = data.field_cache.emplace(id, std::move(cached_field)).first->second;

    data.file_cache.AddToCache<CachedField>(pf, file_id.GetOffset(), result);

    return result;
}

template <>
CacheOfRuntimeThings::CachedClass &FastAPIClassRW::ResolveByDescriptor(panda_file::SourceLang src_lang,
                                                                       const DescriptorString &descr_string)
{
    auto &data = GetContext(src_lang);

    const auto it = data.descr_lookup.find(descr_string);
    if (it != data.descr_lookup.cend()) {
        return const_cast<CachedClass &>(it->second.get());
    }

    // check if it is an array descr
    if (!ClassHelper::IsArrayDescriptor(descr_string)) {
        return Invalid<CachedClass>();
    }

    return AddArray(src_lang, descr_string);
}

template <>
CacheOfRuntimeThings::CachedClass &FastAPIClassRW::LinkArrayClass(CacheOfRuntimeThings::CachedClass &cached_class)
{
    auto &array_comp = cached_class.array_component;
    if (!IsLinked(array_comp)) {
        if (IsDescriptor(array_comp)) {
            auto &resolved_comp = ResolveByDescriptor(cached_class.source_lang, GetDescriptor(array_comp));
            if (Valid(resolved_comp)) {
                array_comp = std::cref(resolved_comp);
            }
        }
        if (IsRef(array_comp)) {
            if (!IsLinked(array_comp)) {
                auto &linked_comp = Link(GetRef(array_comp));
                if (Valid(linked_comp)) {
                    array_comp = std::cref(linked_comp);
                } else {
                    cached_class.linked = false;
                }
            }
        } else {
            cached_class.linked = false;
        }
    }

    if (!IsLinked(cached_class)) {
        return Invalid<CachedClass>();
    }

    return cached_class;
}

template <>
template <>
CacheOfRuntimeThings::CachedClass &FastAPIClassRW::Link<CacheOfRuntimeThings::CachedClass>(
    CacheOfRuntimeThings::CachedClass &cached_class)
{
    if (IsLinked(cached_class)) {
        return cached_class;
    }

    cached_class.linked = true;

    for (auto &ancestor : cached_class.ancestors) {
        if (IsLinked(ancestor)) {
            continue;
        }
        if (IsDescriptor(ancestor)) {
            auto &resolved_ancestor = ResolveByDescriptor(cached_class.source_lang, GetDescriptor(ancestor));
            if (Valid(resolved_ancestor)) {
                ancestor = std::cref(resolved_ancestor);
            }
        }
        if (IsRef(ancestor)) {
            auto &ancestor_ref = GetRef(ancestor);
            auto &linked_ancestor = Link(ancestor_ref);
            if (Valid(linked_ancestor)) {
                ancestor = std::cref(linked_ancestor);
                continue;
            }
        }
        cached_class.linked = false;
    }

    if (cached_class.flags[CacheOfRuntimeThings::CachedClass::Flag::ARRAY_CLASS]) {
        return LinkArrayClass(cached_class);
    }

    if (!IsLinked(cached_class)) {
        return Invalid<CachedClass>();
    }

    return cached_class;
}

template <>
void FastAPIClassRW::LinkCatchBlocks(CacheOfRuntimeThings::CachedMethod &cached_method)
{
    auto &cached_class = CacheOfRuntimeThings::GetRef(cached_method.klass);
    auto src_lang = cached_class.source_lang;

    for (auto &catch_block : cached_method.catch_blocks) {
        auto &exc_type = catch_block.exception_type;
        if (IsLinked(exc_type)) {
            continue;
        }
        // special case: invalid descriptor indicates catch_all section
        if (IsDescriptor(exc_type) && !GetDescriptor(exc_type).IsValid()) {
            continue;
        }
        if (IsDescriptor(exc_type)) {
            auto &resolved = ResolveByDescriptor(src_lang, GetDescriptor(exc_type));
            if (Valid(resolved)) {
                exc_type = std::cref(resolved);
            }
        }
        if (IsRef(exc_type)) {
            auto &linked = Link(GetRef(exc_type));
            if (Valid(linked)) {
                exc_type = std::cref(linked);
                continue;
            }
        }
        cached_method.linked = false;
    }
}

template <>
template <>
CacheOfRuntimeThings::CachedMethod &FastAPIClassRW::Link<CacheOfRuntimeThings::CachedMethod>(
    CacheOfRuntimeThings::CachedMethod &cached_method)
{
    if (IsLinked(cached_method)) {
        return cached_method;
    }

    if (!IsLinked(cached_method.klass)) {
        auto &class_ref = GetRef(cached_method.klass);
        auto &linked_ref = Link(class_ref);
        if (Invalid(linked_ref)) {
            return Invalid<CachedMethod>();
        }
    }

    auto &cached_class = GetRef(cached_method.klass);

    auto src_lang = cached_class.source_lang;

    cached_method.linked = true;

    auto resolve_arg = [&](CachedClass::RefOrDescriptor &arg) -> bool {
        if (IsDescriptor(arg)) {
            const auto &descr = GetDescriptor(arg);
            auto &arg_class = ResolveByDescriptor(src_lang, descr);
            if (Valid(arg_class)) {
                arg = std::cref(arg_class);
            } else {
                return false;
            }
        }
        ASSERT(IsRef(arg));
        if (IsLinked(GetRef(arg))) {
            return true;
        }
        return Valid(Link(GetRef(arg)));
    };

    for (auto &arg : cached_method.signature) {
        cached_method.linked &= resolve_arg(arg);
    }

    LinkCatchBlocks(cached_method);

    if (!IsLinked(cached_method)) {
        return Invalid<CachedMethod>();
    }

    return cached_method;
}

template <>
template <>
CacheOfRuntimeThings::CachedField &FastAPIClassRW::Link<CacheOfRuntimeThings::CachedField>(
    CacheOfRuntimeThings::CachedField &cached_field)
{
    if (IsLinked(cached_field)) {
        return cached_field;
    }

    if (!IsLinked(cached_field.klass)) {
        auto &class_ref = GetRef(cached_field.klass);
        auto &linked = Link(class_ref);
        if (Invalid(linked)) {
            return Invalid<CachedField>();
        }
    }

    auto &class_ref = GetRef(cached_field.klass);
    auto src_lang = class_ref.source_lang;

    if (IsDescriptor(cached_field.type)) {
        auto &type_class = ResolveByDescriptor(src_lang, GetDescriptor(cached_field.type));
        if (Invalid(type_class)) {
            return Invalid<CachedField>();
        }
        cached_field.type = std::cref(type_class);
    }

    auto &type_ref = GetRef(cached_field.type);

    auto &linked = Link(type_ref);
    if (Invalid(linked)) {
        return Invalid<CachedField>();
    }

    cached_field.type = std::cref(linked);

    cached_field.linked = true;

    return cached_field;
}

template <>
template <>
const CacheOfRuntimeThings::CachedClass &FastAPIClassRW::GetFromCache<CacheOfRuntimeThings::CachedClass>(
    const CacheOfRuntimeThings::CachedMethod &cached_method_, uint16_t idx)
{
    auto &cached_method = const_cast<CachedMethod &>(cached_method_);

    auto &index = cached_method.class_index.get();
    if (idx >= index.size()) {
        return Invalid<CachedClass>();
    }
    auto &item = index[idx];
    if (IsRef(item)) {
        return GetRef(item);
    }
    if (IsDescriptor(item)) {
        auto &descr = GetDescriptor(item);

        auto src_lang = GetRef(cached_method.klass).source_lang;

        auto &class_ref = ResolveByDescriptor(src_lang, descr);
        if (Invalid(class_ref)) {
            return Invalid<CachedClass>();
        }

        auto &linked = Link(class_ref);
        if (Invalid(linked)) {
            return Invalid<CachedClass>();
        }

        item = std::cref(linked);

        return linked;
    }
    UNREACHABLE();
    return Invalid<CachedClass>();
}

template <>
const CacheOfRuntimeThings::CachedMethod &FastAPIClassRW::ResolveMethod(
    const CacheOfRuntimeThings::CachedMethod &cached_method, panda_file::File::EntityId id)
{
    panda_file::MethodDataAccessor mda {*cached_method.file, id};

    DescriptorString descr {cached_method.file->GetStringData(mda.GetClassId()).data};

    panda_file::SourceLang method_src_lang;

    auto src_lang = GetRef(cached_method.klass).source_lang;
    auto &data = GetContext(src_lang);

    auto src_lang_opt = mda.GetSourceLang();
    if (!src_lang_opt) {
        method_src_lang = src_lang;
    } else {
        method_src_lang = *src_lang_opt;
    }

    auto &class_ref = ResolveByDescriptor(method_src_lang, descr);
    if (Invalid(class_ref)) {
        return Invalid<CachedMethod>();
    }

    auto &methods_table = class_ref.methods;

    MethodHash method_hash;

    if (class_ref.flags[CachedClass::Flag::ARRAY_CLASS]) {
        // NB: current assumption, that array classes have the only
        // one method - constructor
        ASSERT(methods_table.size() == 1);
        method_hash = methods_table.begin()->second.get().hash;
    } else {
        method_hash = CalcMethodHash(cached_method.file, mda);
    }

    auto it = methods_table.find(method_hash);
    if (it == methods_table.end()) {
        return Invalid<CachedMethod>();
    }

    auto &resolved_method = GetRef(it->second);
    ASSERT(Valid(resolved_method));

    auto &linked_method = Link(resolved_method);
    if (Invalid(linked_method)) {
        return Invalid<CachedMethod>();
    }

    data.file_cache.AddToCache(cached_method.file, id.GetOffset(), linked_method);
    return linked_method;
}

template <>
template <>
const CacheOfRuntimeThings::CachedMethod &FastAPIClassRW::GetFromCache<CacheOfRuntimeThings::CachedMethod>(
    const CacheOfRuntimeThings::CachedMethod &cached_method_, uint16_t idx)
{
    auto &cached_method = const_cast<CachedMethod &>(cached_method_);
    auto &index = cached_method.method_index.get();
    if (idx >= index.size()) {
        return Invalid<CachedMethod>();
    }
    auto &item = index[idx];
    if (IsRef(item)) {
        return GetRef(item);
    }
    if (IsEntityId(item)) {
        auto id = GetEntityId(item);

        auto src_lang = GetRef(cached_method.klass).source_lang;

        auto &data = GetContext(src_lang);

        auto &method_ref = data.file_cache.GetCached<CachedMethod>(cached_method.file, id.GetOffset());
        if (Valid(method_ref)) {
            if (IsLinked(method_ref)) {
                item = std::cref(method_ref);
                return method_ref;
            }
            auto &linked = Link(method_ref);
            if (Invalid(linked)) {
                return Invalid<CachedMethod>();
            }
            item = std::cref(linked);
            return linked;
        }

        auto &res = ResolveMethod(cached_method, id);
        item = std::cref(res);
        return res;
    }

    UNREACHABLE();
    return Invalid<CachedMethod>();
}

template <>
template <>
const CacheOfRuntimeThings::CachedField &FastAPIClassRW::GetFromCache<CacheOfRuntimeThings::CachedField>(
    const CacheOfRuntimeThings::CachedMethod &cached_method_, uint16_t idx)
{
    auto &cached_method = const_cast<CachedMethod &>(cached_method_);
    auto &index = cached_method.field_index.get();
    if (idx >= index.size()) {
        return Invalid<CachedField>();
    }
    auto &item = index[idx];
    if (IsRef(item)) {
        return GetRef(item);
    }
    if (IsEntityId(item)) {
        auto entity_id = GetEntityId(item);

        auto src_lang = GetRef(cached_method.klass).source_lang;

        auto &data = GetContext(src_lang);

        auto &field_ref = data.file_cache.GetCached<CachedField>(cached_method.file, entity_id.GetOffset());
        if (Valid(field_ref)) {
            if (IsLinked(field_ref)) {
                item = std::cref(field_ref);
                return field_ref;
            }
            auto &linked = Link(field_ref);
            if (Invalid(linked)) {
                return Invalid<CachedField>();
            }
            item = std::cref(linked);
            return linked;
        }

        panda_file::FieldDataAccessor fda {*cached_method.file, entity_id};

        DescriptorString descr {cached_method.file->GetStringData(fda.GetClassId()).data};

        auto &class_ref = ResolveByDescriptor(src_lang, descr);
        if (Invalid(class_ref)) {
            return Invalid<CachedField>();
        }

        auto &fields_table = class_ref.fields;

        FieldHash field_hash = CalcFieldNameAndTypeHash(cached_method.file, fda);

        auto it = fields_table.find(field_hash);
        if (it == fields_table.end()) {
            return Invalid<CachedField>();
        }

        auto &resolved_field = GetRef(it->second);
        ASSERT(Valid(resolved_field));

        auto &linked_field = Link(resolved_field);
        if (Invalid(linked_field)) {
            return Invalid<CachedField>();
        }

        data.file_cache.AddToCache(cached_method.file, entity_id.GetOffset(), linked_field);
        item = std::cref(linked_field);
        return linked_field;
    }
    UNREACHABLE();
    return Invalid<CachedField>();
}

template <>
const CacheOfRuntimeThings::CachedClass &FastAPIClassRW::GetStringClass(
    const CacheOfRuntimeThings::CachedMethod &method)
{
    auto &klass = GetRef(method.klass);
    auto src_lang = klass.source_lang;
    auto &ctx = GetContext(src_lang);
    auto &str_ref = ResolveByDescriptor(src_lang, ctx.string_descr);
    if (Invalid(str_ref)) {
        return Invalid<CachedClass>();
    }
    return Link(str_ref);
}

template <>
const CacheOfRuntimeThings::CachedClass &FastAPIClassRW::GetStringArrayClass(
    const CacheOfRuntimeThings::CachedMethod &method)
{
    auto &klass = GetRef(method.klass);
    auto src_lang = klass.source_lang;
    auto &ctx = GetContext(src_lang);
    auto &str_array_ref = ResolveByDescriptor(src_lang, ctx.string_array_descr);
    if (Invalid(str_array_ref)) {
        return Invalid<CachedClass>();
    }
    return Link(str_array_ref);
}

template <>
void FastAPIClassRW::ProcessFile(const panda_file::File *pf)
{
    auto classes_indices = pf->GetClasses();
    for (auto idx : classes_indices) {
        panda_file::File::EntityId entity_id {idx};
        if (!pf->IsExternal(entity_id)) {
            AddToCache(pf, entity_id);
        }
    }
}

}  // namespace panda::verifier
