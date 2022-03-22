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

#ifndef PANDA_VERIFICATION_UTIL_OBJ_POOL_H_
#define PANDA_VERIFICATION_UTIL_OBJ_POOL_H_

#include <cstdint>
#include <optional>
#include <utility>

#include "macros.h"

namespace panda::verifier {

template <typename T, template <typename...> class Vector, typename InitializerType = void (*)(T &, std::size_t),
          typename CleanerType = void (*)(T &)>
class ObjPool {
public:
    using ObjType = T;

    class Accessor {
    public:
        Accessor() : idx {0}, pool {nullptr} {}

        Accessor(std::size_t index, ObjPool *obj_pool) : idx {index}, pool {obj_pool}
        {
            if (obj_pool != nullptr) {
                obj_pool->IncRC(idx);
            }
        }

        Accessor(const Accessor &p) : idx {p.idx}, pool {p.pool}
        {
            if (pool != nullptr) {
                pool->IncRC(idx);
            }
        }

        Accessor(Accessor &&p) : idx {p.idx}, pool {p.pool}
        {
            p.Reset();
        }

        Accessor &operator=(const Accessor &p)
        {
            if (p.pool != nullptr) {
                p.pool->IncRC(p.idx);
            }
            if (pool != nullptr) {
                pool->DecRC(idx);
            }
            idx = p.idx;
            pool = p.pool;
            return *this;
        }

        Accessor &operator=(Accessor &&p)
        {
            auto old_idx = idx;
            auto old_pool = pool;
            idx = p.idx;
            pool = p.pool;
            if (old_pool != nullptr) {
                old_pool->DecRC(old_idx);
            }
            p.Reset();
            return *this;
        }

        ~Accessor()
        {
            if (pool != nullptr) {
                pool->DecRC(idx);
                Reset();
            }
        }

        T &operator*()
        {
            return pool->Storage[idx];
        }

        const T &operator*() const
        {
            return pool->Storage[idx];
        }

        operator bool() const
        {
            return pool != nullptr;
        }

        void Free()
        {
            if (pool != nullptr) {
                pool->DecRC(idx);
                Reset();
            }
        }

    private:
        void Reset()
        {
            idx = 0;
            pool = nullptr;
        }

        std::size_t idx {0};
        ObjPool *pool {nullptr};
    };

    ObjPool(InitializerType initializer, CleanerType cleaner) : Initializer {initializer}, Cleaner {cleaner} {}
    ObjPool(InitializerType initializer) : Initializer {initializer}, Cleaner {[](T &) { return; }} {}
    ObjPool() : Initializer {[](T &, std::size_t) { return; }}, Cleaner {[](T &) { return; }} {}
    ~ObjPool() = default;
    DEFAULT_COPY_SEMANTIC(ObjPool);
    DEFAULT_MOVE_SEMANTIC(ObjPool);

    Accessor New()
    {
        std::size_t idx;
        if (FreeCount() > 0) {
            idx = Free.back();
            Free.pop_back();
        } else {
            idx = Storage.size();
            Storage.emplace_back();
            RC.emplace_back(0);
        }
        Initializer(Storage[idx], idx);
        return {idx, this};
    }

    std::size_t FreeCount() const
    {
        return Free.size();
    }

    std::size_t Count() const
    {
        return Storage.size();
    }

    auto AllObjects()
    {
        return [this, idx {static_cast<size_t>(0)}]() mutable -> std::optional<Accessor> {
            while (idx < Storage.size() && RC[idx] == 0) {
                ++idx;
            }
            if (idx >= Storage.size()) {
                return std::nullopt;
            }
            return {Accessor {idx++, this}};
        };
    }

private:
    void IncRC(std::size_t idx)
    {
        ++RC[idx];
    }

    void DecRC(std::size_t idx)
    {
        if (--RC[idx] == 0) {
            Cleaner(Storage[idx]);
            Free.push_back(idx);
        }
    }

    InitializerType Initializer;
    CleanerType Cleaner;

    Vector<T> Storage;
    Vector<std::size_t> Free;
    Vector<std::size_t> RC;
};

}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_UTIL_OBJ_POOL_H_
