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

#ifndef PANDA_VERIFICATION_JOB_QUEUE_CACHE_H_
#define PANDA_VERIFICATION_JOB_QUEUE_CACHE_H_

#include "verification/util/flags.h"
#include "verification/util/synchronized.h"
#include "verification/util/access.h"
#include "verification/util/descriptor_string.h"
#include "verification/cache/file_entity_cache.h"
#include "verification/util/enum_array.h"
#include "verification/util/ref_wrapper.h"

#include "runtime/core/core_language_context.h"
#include "runtime/include/mem/panda_containers.h"
#include "runtime/include/mem/panda_string.h"
#include "runtime/include/language_context.h"

#include "libpandafile/file_items.h"
#include "libpandafile/file.h"
#include "libpandafile/method_data_accessor.h"
#include "libpandafile/field_data_accessor.h"

#include "libpandabase/utils/hash.h"

#include "verification/job_queue/index_table_cache.h"

#include <functional>
#include <cstdint>
#include <variant>
#include <optional>

namespace panda::verifier {
class CacheOfRuntimeThings {
public:
    using Id = uint64_t;

    struct CachedClass;
    struct CachedMethod;
    struct CachedField;

    using CachedClassRef = std::reference_wrapper<const CachedClass>;
    using CachedMethodRef = std::reference_wrapper<const CachedMethod>;
    using CachedFieldRef = std::reference_wrapper<const CachedField>;

    using DescriptorString = panda::verifier::DescriptorString<mode::ExactCmp>;

    using CachedClassRefOrDescriptor = std::variant<DescriptorString, CachedClassRef>;
    using CachedMethodRefOrEntityId = std::variant<panda_file::File::EntityId, CachedMethodRef>;
    using CachedFieldRefOrEntityId = std::variant<panda_file::File::EntityId, CachedFieldRef>;

    static bool IsRef(const CachedClassRefOrDescriptor &item)
    {
        return std::holds_alternative<CachedClassRef>(item);
    }

    static bool IsRef(const CachedMethodRefOrEntityId &item)
    {
        return std::holds_alternative<CachedMethodRef>(item);
    }

    static bool IsRef(const CachedFieldRefOrEntityId &item)
    {
        return std::holds_alternative<CachedFieldRef>(item);
    }

    static CachedClass &GetRef(const CachedClassRefOrDescriptor &item)
    {
        return const_cast<CachedClass &>(std::get<CachedClassRef>(item).get());
    }

    static CachedMethod &GetRef(const CachedMethodRefOrEntityId &item)
    {
        return const_cast<CachedMethod &>(std::get<CachedMethodRef>(item).get());
    }

    static CachedField &GetRef(const CachedFieldRefOrEntityId &item)
    {
        return const_cast<CachedField &>(std::get<CachedFieldRef>(item).get());
    }

    static CachedClass &GetRef(const CachedClassRef &item)
    {
        return const_cast<CachedClass &>(item.get());
    }

    static CachedMethod &GetRef(const CachedMethodRef &item)
    {
        return const_cast<CachedMethod &>(item.get());
    }

    static CachedField &GetRef(const CachedFieldRef &item)
    {
        return const_cast<CachedField &>(item.get());
    }

    static bool IsDescriptor(const CachedClassRefOrDescriptor &item)
    {
        return std::holds_alternative<DescriptorString>(item);
    }

    static const DescriptorString &GetDescriptor(const CachedClassRefOrDescriptor &item)
    {
        return std::get<DescriptorString>(item);
    }

    template <typename T>
    static bool IsEntityId(const T &item)
    {
        return std::holds_alternative<panda_file::File::EntityId>(item);
    }

    template <typename T>
    static panda_file::File::EntityId GetEntityId(const T &item)
    {
        return std::get<panda_file::File::EntityId>(item);
    }

    static bool IsLinked(const CachedClassRef &item)
    {
        return GetRef(item).linked;
    }

    static bool IsLinked(const CachedMethodRef &item)
    {
        return GetRef(item).linked;
    }

    static bool IsLinked(const CachedFieldRef &item)
    {
        return GetRef(item).linked;
    }

    static bool IsLinked(const CachedClassRefOrDescriptor &item)
    {
        return IsRef(item) && GetRef(item).linked;
    }

    static bool IsLinked(const CachedMethodRefOrEntityId &item)
    {
        return IsRef(item) && GetRef(item).linked;
    }

    static bool IsLinked(const CachedFieldRefOrEntityId &item)
    {
        return IsRef(item) && GetRef(item).linked;
    }

    static bool IsLinked(const CachedClass &item)
    {
        return item.linked;
    }

    static bool IsLinked(const CachedMethod &item)
    {
        return item.linked;
    }

    static bool IsLinked(const CachedField &item)
    {
        return item.linked;
    }

    using MethodHash = uint64_t;
    using FieldHash = uint64_t;

    template <typename Handler>
    static MethodHash CalcMethodHash(const uint8_t *name, Handler &&handler)
    {
        uint64_t name_hash = std::hash<DescriptorString> {}(DescriptorString {name});
        uint64_t sig_hash = FNV_INITIAL_SEED;
        auto hash_str = [&sig_hash](const DescriptorString &descr) {
            sig_hash = PseudoFnvHashItem(std::hash<DescriptorString> {}(descr), sig_hash);
        };
        handler(hash_str);
        auto constexpr SHIFT = 32U;
        return (name_hash << SHIFT) | sig_hash;
    }

    static MethodHash CalcMethodHash(const panda_file::File *pf, const panda_file::MethodDataAccessor &mda);
    static CachedMethod &CalcMethodHash(CachedMethod &);

    static FieldHash CalcFieldNameAndTypeHash(const panda_file::File *, const panda_file::FieldDataAccessor &);

    using PrimitiveClassesArray =
        EnumArraySimple<Ref<const CachedClass>, panda_file::Type::TypeId, panda_file::Type::TypeId::VOID,
                        panda_file::Type::TypeId::U1, panda_file::Type::TypeId::U8, panda_file::Type::TypeId::I8,
                        panda_file::Type::TypeId::U16, panda_file::Type::TypeId::I16, panda_file::Type::TypeId::U32,
                        panda_file::Type::TypeId::I32, panda_file::Type::TypeId::U64, panda_file::Type::TypeId::I64,
                        panda_file::Type::TypeId::F32, panda_file::Type::TypeId::F64, panda_file::Type::TypeId::TAGGED>;

    static PandaString GetName(const CachedClass &);
    static PandaString GetName(const CachedMethod &);
    static PandaString GetName(const CachedField &);
    static PandaString GetName(const DescriptorString &);

    static PandaString GetName(const CachedClassRef &item)
    {
        return GetName(GetRef(item));
    }

    static PandaString GetName(const CachedMethodRef &item)
    {
        return GetName(GetRef(item));
    }

    static PandaString GetName(const CachedFieldRef &item)
    {
        return GetName(GetRef(item));
    }

    struct CachedClass {
        using Ref = CachedClassRef;
        using RefOrDescriptor = CachedClassRefOrDescriptor;
        enum class Flag {
            DYNAMIC_CLASS,
            PUBLIC,
            FINAL,
            ANNOTATION,
            ENUM,
            ARRAY_CLASS,
            OBJECT_ARRAY_CLASS,
            STRING_CLASS,
            VARIABLESIZE,
            PRIMITIVE,
            ABSTRACT,
            INTERFACE,
            INSTANTIABLE,
            OBJECT_CLASS,
            CLASS_CLASS,
            PROXY,
            SUPER,
            SYNTHETIC
        };
        using FlagsValue =
            FlagsForEnum<unsigned int, Flag, Flag::DYNAMIC_CLASS, Flag::PUBLIC, Flag::FINAL, Flag::ANNOTATION,
                         Flag::ENUM, Flag::ARRAY_CLASS, Flag::OBJECT_ARRAY_CLASS, Flag::STRING_CLASS,
                         Flag::VARIABLESIZE, Flag::PRIMITIVE, Flag::ABSTRACT, Flag::INTERFACE, Flag::INSTANTIABLE,
                         Flag::OBJECT_CLASS, Flag::CLASS_CLASS, Flag::PROXY, Flag::SUPER, Flag::SYNTHETIC>;
        Id id;
        DescriptorString name;
        panda_file::SourceLang source_lang;
        panda_file::Type::TypeId type_id;
        PandaVector<RefOrDescriptor> ancestors;
        RefOrDescriptor array_component;
        FlagsValue flags;
        PandaUnorderedMap<MethodHash, CachedMethodRef> methods;
        PandaUnorderedMap<FieldHash, CachedFieldRef> fields;
        bool linked;
        const panda_file::File *file;
        panda_file::File::EntityId file_id;

        PandaString GetName() const
        {
            return CacheOfRuntimeThings::GetName(*this);
        }

        const CachedClass &GetArrayComponent() const
        {
            ASSERT(IsRef(array_component));
            return GetRef(array_component);
        }
    };

    struct CachedCatchBlock {
        const uint8_t *try_block_start;
        const uint8_t *try_block_end;
        CachedClass::RefOrDescriptor exception_type;
        const uint8_t *handler_bytecode;
        size_t handler_bytecode_size;
    };

    using ClassIndex = PandaVector<CachedClassRefOrDescriptor>;
    using MethodIndex = PandaVector<CachedMethodRefOrEntityId>;
    using FieldIndex = PandaVector<CachedFieldRefOrEntityId>;

    using ClassIndexRef = Ref<ClassIndex>;
    using MethodIndexRef = Ref<MethodIndex>;
    using FieldIndexRef = Ref<FieldIndex>;

    struct CachedMethod {
        using Ref = CachedMethodRef;
        enum class Flag {
            STATIC,
            PUBLIC,
            PRIVATE,
            PROTECTED,
            NATIVE,
            INTRINSIC,
            SYNTHETIC,
            ABSTRACT,
            FINAL,
            SYNCHRONIZED,
            HAS_SINGLE_IMPLEMENTATION,
            DEFAULT_INTERFACE_METHOD,
            CONSTRUCTOR,
            INSTANCE_CONSTRUCTOR,
            STATIC_CONSTRUCTOR,
            ARRAY_CONSTRUCTOR
        };
        using FlagsValue =
            FlagsForEnum<unsigned int, Flag, Flag::STATIC, Flag::PUBLIC, Flag::PRIVATE, Flag::PROTECTED, Flag::NATIVE,
                         Flag::INTRINSIC, Flag::SYNTHETIC, Flag::ABSTRACT, Flag::FINAL, Flag::SYNCHRONIZED,
                         Flag::HAS_SINGLE_IMPLEMENTATION, Flag::DEFAULT_INTERFACE_METHOD, Flag::CONSTRUCTOR,
                         Flag::INSTANCE_CONSTRUCTOR, Flag::STATIC_CONSTRUCTOR, Flag::ARRAY_CONSTRUCTOR>;
        Id id;
        MethodHash hash;
        DescriptorString name;
        CachedClass::Ref klass;
        PandaVector<CachedClass::RefOrDescriptor> signature;
        PandaVector<CachedCatchBlock> catch_blocks;
        ClassIndexRef class_index;
        MethodIndexRef method_index;
        FieldIndexRef field_index;
        size_t num_vregs;
        size_t num_args;
        FlagsValue flags;
        const uint8_t *bytecode;
        size_t bytecode_size;
        bool linked;
        /*
        Keep here extended verification result in debug mode:
        in case of verification problems, save bitmap of instructions
        that were successfully verified with contexts at the beginnings of
        unverified blocks to debug them
        */
        const panda_file::File *file;
        panda_file::File::EntityId file_id;

        PandaString GetName() const
        {
            return CacheOfRuntimeThings::GetName(*this);
        }

        const CachedClass &GetClass() const
        {
            ASSERT(Valid(GetRef(klass)));
            return GetRef(klass);
        }

        bool IsStatic() const
        {
            return flags[Flag::STATIC];
        }
    };

    struct CachedField {
        using Ref = std::reference_wrapper<const CachedField>;
        enum class Flag { PUBLIC, PRIVATE, PROTECTED, STATIC, VOLATILE, FINAL };
        using FlagsValue = FlagsForEnum<unsigned int, Flag, Flag::PUBLIC, Flag::PRIVATE, Flag::PROTECTED, Flag::STATIC,
                                        Flag::VOLATILE, Flag::FINAL>;
        Id id;
        FieldHash hash;
        DescriptorString name;
        CachedClass::Ref klass;
        CachedClass::RefOrDescriptor type;
        FlagsValue flags;
        bool linked;
        const panda_file::File *file;
        panda_file::File::EntityId file_id;

        PandaString GetName() const
        {
            return CacheOfRuntimeThings::GetName(*this);
        }

        const CachedClass &GetClass() const
        {
            ASSERT(Valid(GetRef(klass)));
            return GetRef(klass);
        }

        const CachedClass &GetType() const
        {
            ASSERT(IsRef(type));
            ASSERT(Valid(IsRef(type)));
            return GetRef(type);
        }
    };

    CacheOfRuntimeThings() = default;
    CacheOfRuntimeThings(const CacheOfRuntimeThings &) = delete;
    CacheOfRuntimeThings(CacheOfRuntimeThings &&) = delete;
    CacheOfRuntimeThings &operator=(const CacheOfRuntimeThings &) = delete;
    CacheOfRuntimeThings &operator=(CacheOfRuntimeThings &&) = delete;
    ~CacheOfRuntimeThings() = default;

    template <typename Access>
    class FastAPIClass;

    using ClassCache = PandaUnorderedMap<Id, CachedClass>;
    using MethodCache = PandaUnorderedMap<Id, CachedMethod>;
    using FieldCache = PandaUnorderedMap<Id, CachedField>;
    using DescriptorLookup = PandaUnorderedMap<DescriptorString, CachedClass::Ref>;
    using FileCache = FileEntityCache<CachedClass, CachedMethod, CachedField>;
    using FileIndexTableCache = IndexTableCache<ClassIndex, MethodIndex, FieldIndex>;

    struct LangContext {
        ClassCache class_cache;
        MethodCache method_cache;
        FieldCache field_cache;
        PrimitiveClassesArray primitive_classes;
        DescriptorLookup descr_lookup;
        FileCache file_cache;
        FileIndexTableCache index_table_cache;
        DescriptorString string_descr;
        DescriptorString object_descr;
        DescriptorString string_array_descr;
    };

    using Data = EnumArraySimple<LangContext, panda_file::SourceLang, panda_file::SourceLang::ECMASCRIPT,
                                 panda_file::SourceLang::PANDA_ASSEMBLY>;

    using SyncData = Synchronized<Data, FastAPIClass<access::ReadOnly>, FastAPIClass<access::ReadWrite>>;

    template <typename Access>
    class FastAPIClass {
        ACCESS_IS_READONLY_OR_READWRITE(Access);

        SyncData &data_;
        const panda::CoreLanguageContext &core_lang_ctx;

        explicit FastAPIClass(CacheOfRuntimeThings &cache) : data_ {cache.data_}, core_lang_ctx {cache.core_lang_ctx}
        {
            if constexpr (access::if_readonly<Access>) {
                data_.ReadLock();
            } else {
                data_.WriteLock();
            }
        }

        friend class CacheOfRuntimeThings;

    public:
        ~FastAPIClass()
        {
            data_.Unlock();
        }

        const panda::LanguageContextBase &GetLanguageContextBase(panda_file::SourceLang) const;

        const LangContext &GetContext(panda_file::SourceLang src_lang) const
        {
            return data_.GetObj()[src_lang];
        }
        LangContext &GetContext(panda_file::SourceLang src_lang)
        {
            return data_.GetObj()[src_lang];
        }

        const CachedClass &GetStringClass(const CachedMethod &method);
        const CachedClass &GetStringArrayClass(const CachedMethod &method);

        CachedClass &MakeSyntheticClass(panda_file::SourceLang, const uint8_t *, panda_file::Type::TypeId, uint32_t);

        CachedMethod &MakeSyntheticMethod(CachedClass &, const uint8_t *name,
                                          const std::function<void(CachedClass &, CachedMethod &)> &);

        CachedMethod &AddArrayCtor(CachedClass &array);
        CachedClass &AddArray(panda_file::SourceLang, const uint8_t *);

        CachedClass &ResolveByDescriptor(panda_file::SourceLang, const DescriptorString &);

        void InitializePandaAssemblyRootClasses();

        void ProcessFile(const panda_file::File *pf);

        const CachedClass &GetPrimitiveClass(panda_file::SourceLang, panda_file::Type::TypeId) const;

        template <typename CachedEntity>
        CachedEntity &GetFromCache(panda_file::SourceLang, Id id);

        template <typename CachedEntity>
        const CachedEntity &GetFromCache(const CachedMethod &, uint16_t idx);

        template <typename CachedEntity>
        CachedEntity &Link(CachedEntity &);

        CachedClass &AddToCache(const panda_file::File *, panda_file::File::EntityId);
        CachedMethod &AddToCache(const CachedClass &, const panda_file::File *, const panda_file::MethodDataAccessor &);
        CachedField &AddToCache(const CachedClass &, const panda_file::File *, const panda_file::FieldDataAccessor &);

    private:
        void InitializePandaAssemblyPrimitiveRoot(panda_file::Type::TypeId type_id);

        CachedClass &LinkArrayClass(CachedClass &);

        void LinkCatchBlocks(CachedMethod &);

        const CachedMethod &ResolveMethod(const CachedMethod &, panda_file::File::EntityId id);
    };

    FastAPIClass<access::ReadWrite> FastAPI()
    {
        return FastAPIClass<access::ReadWrite> {*this};
    }

    const FastAPIClass<access::ReadOnly> FastAPI() const
    {
        return FastAPIClass<access::ReadOnly> {const_cast<CacheOfRuntimeThings &>(*this)};
    }

    template <typename CachedEntity>
    CachedEntity &GetFromCache(panda_file::SourceLang src_lang, Id id)
    {
        return FastAPI().GetFromCache<CachedEntity>(src_lang, id);
    }

private:
    template <typename Access>
    friend class FastAPIClass;
    SyncData data_;

    const panda::CoreLanguageContext core_lang_ctx {};
};
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_JOB_QUEUE_CACHE_H_
