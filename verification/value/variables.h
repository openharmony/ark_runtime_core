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

#ifndef PANDA_VERIFICATION_VALUE_VARIABLES_H_
#define PANDA_VERIFICATION_VALUE_VARIABLES_H_

#include <memory>

#include "verification/util/lazy.h"

#include "runtime/include/mem/panda_containers.h"

#include "util/str.h"
#include "verification/util/obj_pool.h"

#include "verification/type/type_system_kind.h"

namespace panda::verifier {

class Variables {
    struct Wrapper {
        static void Init(Wrapper &v, size_t idx)
        {
            v.Idx_ = idx;
        }
        size_t Idx_;
    };

    using PoolType = ObjPool<Wrapper, PandaVector, void (*)(Wrapper &, size_t)>;

public:
    class Var {
    public:
        Var() = default;
        Var(PoolType::Accessor &&a) : accessor {std::move(a)} {}
        Var(const Var &) = default;
        Var(Var &&) = default;
        Var &operator=(const Var &a) = default;
        Var &operator=(Var &&a) = default;
        ~Var() = default;

        bool operator==(const Var &v)
        {
            return (*accessor).Idx_ == (*v.accessor).Idx_;
        }

        bool operator!=(const Var &v)
        {
            return (*accessor).Idx_ != (*v.accessor).Idx_;
        }

        template <typename StrT>
        StrT Image(const char *prefix = "V") const
        {
            return StrT {prefix} + NumToStr<StrT>((*accessor).Idx_);
        }

    private:
        PoolType::Accessor accessor;
    };

    using VarIdx = size_t;

    Variables() = default;
    Variables(TypeSystemKind) {}
    Variables(const Variables &) = default;
    Variables(Variables &&) = default;
    Variables &operator=(const Variables &) = default;
    Variables &operator=(Variables &&) = default;
    ~Variables() = default;

    Var NewVar()
    {
        return {VarPool.New()};
    }

    size_t AmountOfUsedVars() const
    {
        return VarPool.Count() - VarPool.FreeCount();
    }

    auto AllVariables()
    {
        return [fetcher = VarPool.AllObjects()]() mutable -> std::optional<Var> {
            if (auto v = fetcher()) {
                return {Var {std::move(*v)}};
            }
            return std::nullopt;
        };
    }

private:
    PoolType VarPool {Wrapper::Init};
};

}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_VALUE_VARIABLES_H_
