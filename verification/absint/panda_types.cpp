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

#include "panda_types.h"

#include "runtime/include/class_linker.h"
#include "runtime/include/method.h"
#include "runtime/include/method-inl.h"
#include "runtime/include/class.h"
#include "runtime/include/runtime.h"

#include "utils/span.h"
#include "verification/type/type_system.h"
#include "verification/type/type_sort.h"
#include "verification/type/type_image.h"
#include "verification/job_queue/cache.h"

#include "verification/type/type_params_inl.h"

#include "verifier_messages.h"

#include "utils/logger.h"

#include "runtime/include/mem/panda_containers.h"
#include "runtime/include/mem/panda_string.h"

#include "libpandabase/os/mutex.h"

namespace panda::verifier {

Type PandaTypes::NormalizedTypeOf(Type type)
{
    ASSERT(type.IsValid());
    if (type.IsBot() || type.IsTop()) {
        return type;
    }
    auto t = NormalizedTypeOf_.find(type);
    if (t != NormalizedTypeOf_.cend()) {
        return t->second;
    }
    Type result = type;
    if (type <= Integral32Type()) {
        result = Normalize()(~Integral32Type());
        NormalizedTypeOf_[type] = result;
    } else if (type <= Integral64Type()) {
        result = Normalize()(~Integral64Type());
        NormalizedTypeOf_[type] = result;
        // NOLINTNEXTLINE(bugprone-branch-clone)
    } else if (type <= F32()) {
        result = Normalize()(~F64());
        NormalizedTypeOf_[type] = result;
    } else if (type <= F64()) {
        result = Normalize()(~F64());
        NormalizedTypeOf_[type] = result;
    } else if (type <= MethodType()) {
        result = NormalizedMethod()(NormalizeMethodSignature(type.Params()));
    }
    NormalizedTypeOf_[type] = result;
    return result;
}

TypeParams PandaTypes::NormalizeMethodSignature(const TypeParams &sig)
{
    TypeParams result {kind_};
    sig.ForEach([&result, this](const auto &param) {
        const Type &type = param;
        result >> (NormalizedTypeOf(type) * param.Variance());
    });
    return result;
}

const TypeParams &PandaTypes::NormalizedMethodSignature(const PandaTypes::CachedMethod &method)
{
    auto &&method_id = method.id;
    auto it = NormalizedSigOfMethod_.find(method_id);
    if (it != NormalizedSigOfMethod_.end()) {
        return it->second;
    }
    auto &&sig = MethodSignature(method);
    auto &&normalized_sig = NormalizeMethodSignature(sig);
    NormalizedSigOfMethod_[method_id] = normalized_sig;
    return NormalizedSigOfMethod_[method_id];
}

const TypeParams &PandaTypes::MethodSignature(const PandaTypes::CachedMethod &method)
{
    auto &&method_id = method.id;
    auto it = SigOfMethod_.find(method_id);
    if (it != SigOfMethod_.end()) {
        return it->second;
    }
    TypeParams params {kind_};
    Type return_type;
    bool return_is_processed = false;
    for (const auto &arg : method.signature) {
        Type t;
        if (CacheOfRuntimeThings::IsRef(arg)) {
            const auto &cached_class = CacheOfRuntimeThings::GetRef(arg);
            if (cached_class.type_id == TypeId::VOID) {
                t = Top();
            } else {
                t = TypeOf(cached_class);
            }
        } else if (CacheOfRuntimeThings::IsDescriptor(arg)) {
            LOG_VERIFIER_JAVA_TYPES_METHOD_ARG_WAS_NOT_RESOLVED(method.GetName());
            t = Top();
        } else {
            LOG_VERIFIER_JAVA_TYPES_METHOD_ARG_CANNOT_BE_PROCESSED(method.GetName());
            t = Top();
        }
        if (!t.IsValid()) {
            LOG_VERIFIER_JAVA_TYPES_METHOD_ARG_CANNOT_BE_CONVERTED_TO_TYPE(method.GetName());
        }
        if (return_is_processed) {
            params >> -t;
        } else {
            return_type = t;
            return_is_processed = true;
        }
    }
    params >> +return_type;
    SigOfMethod_[method_id] = params;
    return SigOfMethod_[method_id];
}

PandaTypes::TypeId PandaTypes::TypeIdOf(const Type &type) const
{
    std::vector<std::pair<Type, PandaTypes::TypeId>> types_table = {
        {U1_, TypeId::U1},   {U8_, TypeId::U8},   {U16_, TypeId::U16}, {U32_, TypeId::U32},
        {U64_, TypeId::U64}, {I8_, TypeId::I8},   {I16_, TypeId::I16}, {I32_, TypeId::I32},
        {I64_, TypeId::I64}, {F32_, TypeId::F32}, {F64_, TypeId::F64}, {RefType_, TypeId::REFERENCE}};

    for (const auto &val : types_table) {
        if (val.first == type) {
            return val.second;
        }
    }

    if (type.IsTop()) {
        return TypeId::VOID;
    }

    return TypeId::INVALID;
}

Type PandaTypes::TypeOf(const PandaTypes::CachedMethod &method)
{
    auto id = method.id;
    auto k = TypeOfMethod_.find(id);
    if (k != TypeOfMethod_.end()) {
        return k->second;
    }
    ASSERT(!DoNotCalculateMethodType_);
    auto &&sig = MethodSignature(method);
    auto &&norm_sig = NormalizedMethodSignature(method);
    Type type;
    type = Method()(sig);
    type << MethodType();
    TypeOfMethod_[id] = type;
    // Normalize(Method) <: NormalizedMethod(NormalizedMethodSig)
    auto norm_type = Normalize()(~type);
    auto norm_method = NormalizedMethod()(norm_sig);
    norm_type << norm_method;
    NormalizedTypeOf_[type] = norm_method;
    MethodNameOfId_[id] = method.GetName();
    return type;
}

void PandaTypes::SetArraySubtyping(const Type &t)
{
    PandaVector<Type> to_process;
    PandaVector<Type> just_subtype;
    t.ForAllSupertypes([&]([[maybe_unused]] const Type &st) {
        if (!Array()[+st]) {
            to_process.emplace_back(st);
        } else {
            just_subtype.emplace_back(st);
        }
        return true;
    });
    auto array_type = Array()(+t);
    for (const auto &st : just_subtype) {
        array_type << Array()(+st);
    }
    for (const auto &st : to_process) {
        array_type << Array()(+st);
        SetArraySubtyping(st);
    }
}

Type PandaTypes::TypeOfArray(const PandaTypes::CachedClass &klass)
{
    ASSERT(klass.flags[CachedClass::Flag::ARRAY_CLASS]);

    Type type;
    const auto &component = klass.GetArrayComponent();
    if (!Valid(component)) {
        type = Array()(+Top());
        LOG_VERIFIER_JAVA_TYPES_ARRAY_COMPONENT_TYPE_IS_UNDEFINED();
    } else {
        auto component_type = TypeOf(component);
        type = Array()(+component_type);
        SetArraySubtyping(component_type);
    }
    type << ArrayType();
    if (klass.flags[CachedClass::Flag::OBJECT_ARRAY_CLASS]) {
        type << ObjectArrayType();
    }

    return type;
}

Type PandaTypes::TypeOf(const PandaTypes::CachedClass &klass)
{
    auto id = klass.id;
    auto k = TypeOfClass_.find(id);
    if (k != TypeOfClass_.end()) {
        return k->second;
    }

    PandaVector<Type> supertypes;
    for (const auto &ancestor : klass.ancestors) {
        // ancestor here cannot be unresolved descriptor
        ASSERT(CacheOfRuntimeThings::IsRef(ancestor));
        supertypes.emplace_back(TypeOf(CacheOfRuntimeThings::GetRef(ancestor)));
    }

    Type type;
    bool is_primitive = klass.flags[CachedClass::Flag::PRIMITIVE];
    bool is_string = klass.flags[CachedClass::Flag::STRING_CLASS];

    auto class_name = klass.GetName();

    if (klass.flags[CachedClass::Flag::ARRAY_CLASS]) {
        type = TypeOfArray(klass);
    } else if (!is_primitive) {
        type = TypeSystem_.Parametric(GetSort(class_name))();
    } else {
        type = TypeOf(klass.type_id);
    }

    if (!is_primitive) {
        if (!is_string) {
            type << ObjectType();
        } else {
            type << StringType();
        }
        NullRefType() << type << RefType();
        TypeClass()(~type) << TypeClassType() << RefType();
    }
    if (klass.flags[CachedClass::Flag::ABSTRACT]) {
        Abstract()(~type) << AbstractType();
    }
    for (auto &super : supertypes) {
        type << super;
    }
    ClassNameOfId_[id] = class_name;
    TypeOfClass_[id] = type;
    NormalizedTypeOf(type);
    return type;
}

Type PandaTypes::TypeOf(PandaTypes::TypeId id) const
{
    if (id == TypeId::VOID) {
        return Top();
    }

    std::vector<std::pair<Type, PandaTypes::TypeId>> types_table = {
        {U1_, TypeId::U1},   {U8_, TypeId::U8},   {U16_, TypeId::U16}, {U32_, TypeId::U32},
        {U64_, TypeId::U64}, {I8_, TypeId::I8},   {I16_, TypeId::I16}, {I32_, TypeId::I32},
        {I64_, TypeId::I64}, {F32_, TypeId::F32}, {F64_, TypeId::F64}, {RefType_, TypeId::REFERENCE}};

    for (const auto &val : types_table) {
        if (val.second == id) {
            return val.first;
        }
    }

    LOG_VERIFIER_JAVA_TYPES_CANNOT_CONVERT_TYPE_ID_TO_TYPE(id);
    return Top();
}

void PandaTypes::Init()
{
    TypeSystem_.SetIncrementalRelationClosureMode(false);

    // base subtyping of primitive types
    I8() << I16() << I32();
    U1() << U8() << U16() << U32();
    F32() << F64();
    // integral
    (U1() | I8() | U8()) << Integral8Type();
    (Integral8Type() | I16() | U16()) << Integral16Type();
    (Integral16Type() | I32() | U32()) << Integral32Type();
    (I64() | U64()) << Integral64Type();
    // sizes
    F32() << (Float32Type() | F64()) << Float64Type();
    (Integral32Type() | Float32Type()) << Bits32Type();
    (Integral64Type() | Float64Type()) << Bits64Type();
    (Bits32Type() | Bits64Type()) << PrimitiveType();

    TypeClassType() << RefType();
    NullRefType() << (PandaClass() | PandaObject() | JavaObject() | JavaClass() | JavaThrowable())
                  << (ObjectType() | RefType());
    NullRefType() << (ArrayType() | ObjectArrayType()) << RefType();
    TypeClass()(~PandaObject()) << TypeClassType();
    TypeClass()(~JavaObject()) << TypeClassType();

    TypeSystem_.CloseSubtypingRelation();

    TypeSystem_.SetIncrementalRelationClosureMode(false);
    TypeSystem_.SetDeferIncrementalRelationClosure(false);
}

}  // namespace panda::verifier
