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

#ifndef PANDA_VERIFICATION_JOB_QUEUE_INDEX_TABLE_CACHE_H_
#define PANDA_VERIFICATION_JOB_QUEUE_INDEX_TABLE_CACHE_H_

#include "verification/util/invalid_ref.h"
#include "verification/util/misc.h"

#include "libpandafile/file.h"
#include "runtime/include/mem/panda_containers.h"

#include "macros.h"

#include <variant>
#include <tuple>

namespace panda::verifier {

template <typename... IndexTables>
class IndexTableCache {
    using Types = std::tuple<IndexTables...>;
    using Value = std::variant<IndexTables...>;
    using Cache = PandaUnorderedMap<std::pair<const panda_file::File *, std::pair<size_t, const void *>>, Value>;

public:
    template <typename T, typename Span>
    T &GetFromCache(const panda_file::File *pf, const Span &span)
    {
        auto ptr = reinterpret_cast<const void *>(span.data());
        auto type_idx = std::tuple_type_index<T, Types>::value;
        auto it = cache.find(std::make_pair(pf, std::make_pair(type_idx, ptr)));
        if (it == cache.end()) {
            return Invalid<T>();
        }
        if (!std::holds_alternative<T>(it->second)) {
            return Invalid<T>();
        }
        return std::get<T>(it->second);
    }

    template <typename T, typename Span>
    T &AddToCache(const panda_file::File *pf, const Span &span, T &&table)
    {
#ifndef NDEBUG
        ASSERT(Invalid(GetFromCache<T>(pf, span)));
#endif
        auto ptr = reinterpret_cast<const void *>(span.data());
        auto type_idx = std::tuple_type_index<T, Types>::value;
        auto &ref = cache.emplace(std::make_pair(pf, std::make_pair(type_idx, ptr)), std::move(table)).first->second;

        return std::get<T>(ref);
    }

private:
    Cache cache;
};

}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_JOB_QUEUE_INDEX_TABLE_CACHE_H_
