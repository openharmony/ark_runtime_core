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

#ifndef PANDA_VERIFICATION_ABSINT_PANDA_TYPES_H_
#define PANDA_VERIFICATION_ABSINT_PANDA_TYPES_H_

#include "runtime/include/method.h"
#include "runtime/include/class.h"

#include "verification/type/type_systems.h"
#include "verification/type/type_system.h"
#include "verification/type/type_sort.h"
#include "verification/type/type_type_inl.h"

#include "verification/util/synchronized.h"
#include "verification/util/callable.h"
#include "verification/job_queue/cache.h"

#include "runtime/include/mem/panda_containers.h"
#include "runtime/include/mem/panda_string.h"

#include "libpandabase/os/mutex.h"

namespace panda::verifier {
class PandaTypes {
public:
    using Id = CacheOfRuntimeThings::Id;
    using TypeId = panda_file::Type::TypeId;

    using CachedMethod = CacheOfRuntimeThings::CachedMethod;
    using CachedClass = CacheOfRuntimeThings::CachedClass;

    const PandaString &ClassNameOfId(Id id)
    {
        return ClassNameOfId_[id];
    }

    const PandaString &MethodNameOfId(Id id)
    {
        return MethodNameOfId_[id];
    }

    Type NormalizedTypeOf(Type type);
    TypeParams NormalizeMethodSignature(const TypeParams &sig);

    const TypeParams &MethodSignature(const CachedMethod &method);
    const TypeParams &NormalizedMethodSignature(const CachedMethod &method);

    TypeId TypeIdOf(const Type &type) const;

    Type TypeOf(const CachedMethod &method);
    Type TypeOf(const CachedClass &klass);
    Type TypeOf(TypeId id) const;

    Type TypeOf(const TypeParamIdx &idx) const
    {
        return {kind_, idx};
    }

    void Init();

    void CloseAccumulatedSubtypingRelation()
    {
        TypeSystem_.CloseAccumulatedSubtypingRelation();
    };

    SortIdx GetSort(const PandaString &name) const
    {
        return TypeSystems::GetSort(kind_, name);
    }

    TypeSystemKind GetKind() const
    {
        return kind_;
    }

    explicit PandaTypes(size_t N)
        : kind_ {static_cast<TypeSystemKind>(static_cast<size_t>(TypeSystemKind::JAVA_0) + N)},
          TypeSystem_ {TypeSystems::Get(kind_)},
          Array_ {TypeSystem_.Parametric(GetSort("Array"))},
          Method_ {TypeSystem_.Parametric(GetSort("Method"))},
          NormalizedMethod_ {TypeSystem_.Parametric(GetSort("NormalizedMethod"))},
          Normalize_ {TypeSystem_.Parametric(GetSort("Normalize"))},
          Abstract_ {TypeSystem_.Parametric(GetSort("Abstract"))},
          Interface_ {TypeSystem_.Parametric(GetSort("Interface"))},
          TypeClass_ {TypeSystem_.Parametric(GetSort("TypeClass"))},

          U1_ {TypeSystem_.Parametric(GetSort("u1"))()},
          I8_ {TypeSystem_.Parametric(GetSort("i8"))()},
          U8_ {TypeSystem_.Parametric(GetSort("u8"))()},
          I16_ {TypeSystem_.Parametric(GetSort("i16"))()},
          U16_ {TypeSystem_.Parametric(GetSort("u16"))()},
          I32_ {TypeSystem_.Parametric(GetSort("i32"))()},
          U32_ {TypeSystem_.Parametric(GetSort("u32"))()},
          I64_ {TypeSystem_.Parametric(GetSort("i64"))()},
          U64_ {TypeSystem_.Parametric(GetSort("u64"))()},
          F32_ {TypeSystem_.Parametric(GetSort("f32"))()},
          F64_ {TypeSystem_.Parametric(GetSort("f64"))()},

          RefType_ {TypeSystem_.Parametric(GetSort("RefType"))()},
          ObjectType_ {TypeSystem_.Parametric(GetSort("ObjectType"))()},
          StringType_ {TypeSystem_.Parametric(GetSort("StringType"))()},
          PrimitiveType_ {TypeSystem_.Parametric(GetSort("PrimitiveType"))()},
          AbstractType_ {TypeSystem_.Parametric(GetSort("AbstractType"))()},
          InterfaceType_ {TypeSystem_.Parametric(GetSort("InterfaceType"))()},
          TypeClassType_ {TypeSystem_.Parametric(GetSort("TypeClassType"))()},
          InstantiableType_ {TypeSystem_.Parametric(GetSort("InstantiableType"))()},
          ArrayType_ {TypeSystem_.Parametric(GetSort("ArrayType"))()},
          ObjectArrayType_ {TypeSystem_.Parametric(GetSort("ObjectArrayType"))()},
          MethodType_ {TypeSystem_.Parametric(GetSort("MethodType"))()},
          StaticMethodType_ {TypeSystem_.Parametric(GetSort("StaticMethodType"))()},
          NonStaticMethodType_ {TypeSystem_.Parametric(GetSort("NonStaticMethodType"))()},
          VirtualMethodType_ {TypeSystem_.Parametric(GetSort("VirtualMethodType"))()},
          NullRefType_ {TypeSystem_.Parametric(GetSort("NullRefType"))()},
          Bits32Type_ {TypeSystem_.Parametric(GetSort("32Bits"))()},
          Bits64Type_ {TypeSystem_.Parametric(GetSort("64Bits"))()},
          Integral8Type_ {TypeSystem_.Parametric(GetSort("Integral8Bits"))()},
          Integral16Type_ {TypeSystem_.Parametric(GetSort("Integral16Bits"))()},
          Integral32Type_ {TypeSystem_.Parametric(GetSort("Integral32Bits"))()},
          Integral64Type_ {TypeSystem_.Parametric(GetSort("Integral64Bits"))()},
          Float32Type_ {TypeSystem_.Parametric(GetSort("Float32Bits"))()},
          Float64Type_ {TypeSystem_.Parametric(GetSort("Float64Bits"))()},
          // NB: next types should be in sync with runtime and libraries
          PandaObject_ {TypeSystem_.Parametric(GetSort("panda.Object"))()},
          PandaClass_ {TypeSystem_.Parametric(GetSort("panda.Class"))()},
          JavaObject_ {TypeSystem_.Parametric(GetSort("java.lang.Object"))()},
          JavaClass_ {TypeSystem_.Parametric(GetSort("java.lang.Class"))()},
          JavaThrowable_ {TypeSystem_.Parametric(GetSort("java.lang.Throwable"))()}
    {
    }
    ~PandaTypes() = default;
    Type Bot() const
    {
        return TypeSystem_.Bot();
    }
    Type Top() const
    {
        return TypeSystem_.Top();
    }
    const ParametricType &Array()
    {
        return Array_;
    }
    const ParametricType &Method()
    {
        return Method_;
    }
    const ParametricType &NormalizedMethod()
    {
        return NormalizedMethod_;
    }
    const ParametricType &Normalize()
    {
        return Normalize_;
    }
    const ParametricType &Abstract()
    {
        return Abstract_;
    }
    const ParametricType &Interface()
    {
        return Interface_;
    }
    const ParametricType &TypeClass()
    {
        return TypeClass_;
    }

    const Type &U1() const
    {
        return U1_;
    }
    const Type &I8() const
    {
        return I8_;
    }
    const Type &U8() const
    {
        return U8_;
    }
    const Type &I16() const
    {
        return I16_;
    }
    const Type &U16() const
    {
        return U16_;
    }
    const Type &I32() const
    {
        return I32_;
    }
    const Type &U32() const
    {
        return U32_;
    }
    const Type &I64() const
    {
        return I64_;
    }
    const Type &U64() const
    {
        return U64_;
    }
    const Type &F32() const
    {
        return F32_;
    }
    const Type &F64() const
    {
        return F64_;
    }

    const Type &RefType() const
    {
        return RefType_;
    }
    const Type &ObjectType() const
    {
        return ObjectType_;
    }
    const Type &StringType() const
    {
        return StringType_;
    }
    const Type &PrimitiveType() const
    {
        return PrimitiveType_;
    }
    const Type &AbstractType() const
    {
        return AbstractType_;
    }
    const Type &InterfaceType() const
    {
        return InterfaceType_;
    }
    const Type &TypeClassType() const
    {
        return TypeClassType_;
    }
    const Type &InstantiableType() const
    {
        return InstantiableType_;
    }
    const Type &ArrayType() const
    {
        return ArrayType_;
    }
    const Type &ObjectArrayType() const
    {
        return ObjectArrayType_;
    }
    const Type &MethodType() const
    {
        return MethodType_;
    }
    const Type &StaticMethodType() const
    {
        return StaticMethodType_;
    }
    const Type &NonStaticMethodType() const
    {
        return NonStaticMethodType_;
    }
    const Type &VirtualMethodType() const
    {
        return VirtualMethodType_;
    }
    const Type &NullRefType() const
    {
        return NullRefType_;
    }
    const Type &Bits32Type() const
    {
        return Bits32Type_;
    }
    const Type &Bits64Type() const
    {
        return Bits64Type_;
    }
    const Type &Integral8Type() const
    {
        return Integral8Type_;
    }
    const Type &Integral16Type() const
    {
        return Integral16Type_;
    }
    const Type &Integral32Type() const
    {
        return Integral32Type_;
    }
    const Type &Integral64Type() const
    {
        return Integral64Type_;
    }
    const Type &Float32Type() const
    {
        return Float32Type_;
    }
    const Type &Float64Type() const
    {
        return Float64Type_;
    }
    const Type &PandaObject() const
    {
        return PandaObject_;
    }
    const Type &PandaClass() const
    {
        return PandaClass_;
    }
    const Type &JavaObject() const
    {
        return JavaObject_;
    }
    const Type &JavaClass() const
    {
        return JavaClass_;
    }
    const Type &JavaThrowable() const
    {
        return JavaThrowable_;
    }
    const PandaString &ImageOf(const Type &type)
    {
        return TypeSystems::ImageOfType(type);
    }
    PandaString ImageOf(const TypeParams &params)
    {
        return TypeSystems::ImageOfTypeParams(params);
    }
    template <typename Handler>
    void ForSubtypesOf(const Type &type, Handler &&handler) const
    {
        type.ForAllSubtypes(std::move(handler));
    }
    template <typename Handler>
    void ForSupertypesOf(const Type &type, Handler &&handler) const
    {
        type.ForAllSupertypes(std::move(handler));
    }
    PandaVector<Type> SubtypesOf(const Type &type) const
    {
        PandaVector<Type> result;
        type.ForAllSubtypes([&result](const auto &t) {
            result.push_back(t);
            return true;
        });
        return result;
    }
    PandaVector<Type> SupertypesOf(const Type &type) const
    {
        PandaVector<Type> result;
        type.ForAllSupertypes([&result](const auto &t) {
            result.push_back(t);
            return true;
        });
        return result;
    }
    template <typename Handler>
    void DisplayMethods(Handler handler)
    {
        if (DoNotCalculateMethodType_) {
            for (const auto &item : SigOfMethod_) {
                handler(MethodNameOfId(item.first), ImageOf(item.second));
            }
        } else {
            for (const auto &item : TypeOfMethod_) {
                handler(MethodNameOfId(item.first), ImageOf(item.second));
            }
        }
    }
    template <typename Handler>
    void DisplayClasses(Handler handler)
    {
        for (const auto &item : TypeOfClass_) {
            handler(ClassNameOfId(item.first), ImageOf(item.second));
        }
    }
    template <typename Handler>
    void DisplaySubtyping(Handler handler)
    {
        TypeSystem_.ForAllTypes([this, &handler](const Type &type) {
            type.ForAllSupertypes([this, &handler, &type](const Type &supertype) {
                handler(ImageOf(type), ImageOf(supertype));
                return true;
            });
            return true;
        });
    }
    template <typename Handler>
    void DisplayTypeSystem(Handler handler)
    {
        handler(PandaString {"Classes:"});
        DisplayClasses([&handler](const auto &name, const auto &type) { handler(name + " : " + type); });
        handler(PandaString {"Methods:"});
        DisplayMethods([&handler](const auto &name, const auto &type) { handler(name + " : " + type); });
        handler(PandaString {"Subtyping (type <: supertype):"});
        DisplaySubtyping([&handler](const auto &type, const auto &supertype) { handler(type + " <: " + supertype); });
    }

    bool DoNotCalculateMethodType() const
    {
        return DoNotCalculateMethodType_;
    }

private:
    TypeSystemKind kind_;
    PandaUnorderedMap<Id, Type> TypeOfClass_;
    PandaUnorderedMap<Id, Type> TypeOfMethod_;
    PandaUnorderedMap<Id, TypeParams> SigOfMethod_;
    PandaUnorderedMap<Id, TypeParams> NormalizedSigOfMethod_;
    PandaUnorderedMap<Id, PandaString> ClassNameOfId_;
    PandaUnorderedMap<Id, PandaString> MethodNameOfId_;
    PandaUnorderedMap<Type, Type> NormalizedTypeOf_;
    TypeSystem &TypeSystem_;

    // base sorts
    const ParametricType Array_;
    const ParametricType Method_;
    const ParametricType NormalizedMethod_;
    const ParametricType Normalize_;
    const ParametricType Abstract_;
    const ParametricType Interface_;
    const ParametricType TypeClass_;

    const Type U1_;
    const Type I8_;
    const Type U8_;
    const Type I16_;
    const Type U16_;
    const Type I32_;
    const Type U32_;
    const Type I64_;
    const Type U64_;
    const Type F32_;
    const Type F64_;

    const Type RefType_;
    const Type ObjectType_;
    const Type StringType_;
    const Type PrimitiveType_;
    const Type AbstractType_;
    const Type InterfaceType_;
    const Type TypeClassType_;
    const Type InstantiableType_;
    const Type ArrayType_;
    const Type ObjectArrayType_;
    const Type MethodType_;
    const Type StaticMethodType_;
    const Type NonStaticMethodType_;
    const Type VirtualMethodType_;
    const Type NullRefType_;
    const Type Bits32Type_;
    const Type Bits64Type_;
    const Type Integral8Type_;
    const Type Integral16Type_;
    const Type Integral32Type_;
    const Type Integral64Type_;
    const Type Float32Type_;
    const Type Float64Type_;
    const Type PandaObject_;
    const Type PandaClass_;
    const Type JavaObject_;
    const Type JavaClass_;
    const Type JavaThrowable_;

    void SetArraySubtyping(const Type &t);

    Type TypeOfArray(const PandaTypes::CachedClass &klass);

    bool DoNotCalculateMethodType_ {true};
};
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_ABSINT_PANDA_TYPES_H_
