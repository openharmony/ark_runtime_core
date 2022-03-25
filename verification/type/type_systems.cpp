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

#include "type_system.h"
#include "type_sort.h"
#include "type_image.h"
#include "type_system_kind.h"
#include "type_params_inl.h"
#include "type_systems.h"

#include "verification/util/enum_array.h"

#include "runtime/include/mem/allocator.h"

#include "macros.h"

namespace panda::verifier {

using Names = SortNames<PandaString>;
using Image = TypeImage<Names>;

class FullTypeSystem {
public:
    explicit FullTypeSystem(TypeSystemKind kind)
        : sort_ {"Bot", "Top"},
          type_image_ {sort_},
          bot_sort_ {sort_["Bot"]},
          top_sort_ {sort_["Top"]},
          type_system_ {bot_sort_, top_sort_, kind}
    {
    }

    NO_MOVE_SEMANTIC(FullTypeSystem);
    NO_COPY_SEMANTIC(FullTypeSystem);
    ~FullTypeSystem() = default;

    SortIdx GetSort(const PandaString &name)
    {
        return sort_[name];
    }

    const PandaString &ImageOfType(const Type &type)
    {
        return type_image_[type];
    }

    PandaString ImageOfTypeParams(const TypeParams &type_params)
    {
        return type_image_.ImageOfTypeParams(type_params);
    }

    TypeSystem &GetTypeSystem()
    {
        return type_system_;
    }

private:
    Names sort_;
    Image type_image_;
    SortIdx bot_sort_;
    SortIdx top_sort_;
    TypeSystem type_system_;
};

struct TypeSystems::Impl {
    template <TypeSystemKind... kinds>
    using TypeSystemsArray = EnumArray<FullTypeSystem, TypeSystemKind, kinds...>;

    template <TypeSystemKind... kinds>
    using VariablesArray = EnumArray<Variables, TypeSystemKind, kinds...>;

    TypeSystemsArray<TypeSystemKind::PANDA, TypeSystemKind::JAVA_0, TypeSystemKind::JAVA_1, TypeSystemKind::JAVA_2,
                     TypeSystemKind::JAVA_3, TypeSystemKind::JAVA_4, TypeSystemKind::JAVA_5, TypeSystemKind::JAVA_6,
                     TypeSystemKind::JAVA_7, TypeSystemKind::JAVA_8, TypeSystemKind::JAVA_9, TypeSystemKind::JAVA_10,
                     TypeSystemKind::JAVA_11, TypeSystemKind::JAVA_12, TypeSystemKind::JAVA_13, TypeSystemKind::JAVA_14,
                     TypeSystemKind::JAVA_15>
        type_systems;
    VariablesArray<TypeSystemKind::PANDA, TypeSystemKind::JAVA_0, TypeSystemKind::JAVA_1, TypeSystemKind::JAVA_2,
                   TypeSystemKind::JAVA_3, TypeSystemKind::JAVA_4, TypeSystemKind::JAVA_5, TypeSystemKind::JAVA_6,
                   TypeSystemKind::JAVA_7, TypeSystemKind::JAVA_8, TypeSystemKind::JAVA_9, TypeSystemKind::JAVA_10,
                   TypeSystemKind::JAVA_11, TypeSystemKind::JAVA_12, TypeSystemKind::JAVA_13, TypeSystemKind::JAVA_14,
                   TypeSystemKind::JAVA_15>
        variables;
};

TypeSystems::Impl *TypeSystems::impl {nullptr};

void TypeSystems::Initialize()
{
    if (impl != nullptr) {
        return;
    }
    impl = new (mem::AllocatorAdapter<TypeSystems::Impl>().allocate(1)) Impl {};
}

void TypeSystems::Destroy()
{
    if (impl == nullptr) {
        return;
    }
    impl->~Impl();
    mem::AllocatorAdapter<TypeSystems::Impl>().deallocate(impl, 1);
    impl = nullptr;
}

const PandaString &TypeSystems::ImageOfType(const Type &type)
{
    ASSERT(impl != nullptr);
    return impl->type_systems[type.GetTypeSystem().GetKind()].ImageOfType(type);
}

PandaString TypeSystems::ImageOfTypeParams(const TypeParams &type)
{
    ASSERT(impl != nullptr);
    return impl->type_systems[type.GetTypeSystem().GetKind()].ImageOfTypeParams(type);
}

SortIdx TypeSystems::GetSort(TypeSystemKind kind, const PandaString &name)
{
    ASSERT(impl != nullptr);
    return impl->type_systems[kind].GetSort(name);
}

TypeSystem &TypeSystems::Get(TypeSystemKind kind)
{
    ASSERT(impl != nullptr);
    return impl->type_systems[kind].GetTypeSystem();
}

Variables::Var TypeSystems::GetVar(TypeSystemKind kind)
{
    ASSERT(impl != nullptr);
    return impl->variables[kind].NewVar();
}

}  // namespace panda::verifier
