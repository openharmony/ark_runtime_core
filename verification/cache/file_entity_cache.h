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

#ifndef PANDA_VERIFICATION_CACHE_FILE_ENTITY_CACHE_H_
#define PANDA_VERIFICATION_CACHE_FILE_ENTITY_CACHE_H_

#include "macros.h"

#include "verification/util/misc.h"
#include "verification/util/invalid_ref.h"

#include "libpandafile/file.h"

#include "runtime/include/mem/panda_containers.h"

#include <cstdint>
#include <tuple>
#include <type_traits>

namespace panda::verifier {

template <typename... CachedTypes>
class FileEntityCache {
    using Storage = PandaUnorderedMap<std::pair<uint64_t, std::pair<uint32_t, size_t>>, void *>;
    using EntityTypes = std::tuple<CachedTypes...>;

public:
    template <typename Entity>
    Entity &GetCached(const panda_file::File *pf, uint32_t file_offset) const
    {
        const auto it = storage.find(std::make_pair(
            pf->GetUniqId(), std::make_pair(file_offset, std::tuple_type_index<Entity, EntityTypes>::value)));
        if (it != storage.cend()) {
            return *reinterpret_cast<Entity *>(it->second);
        }
        return Invalid<Entity>();
    }

    template <typename Entity>
    void AddToCache(const panda_file::File *pf, uint32_t file_offset, const Entity &entity)
    {
        storage.insert_or_assign(
            std::make_pair(pf->GetUniqId(),
                           std::make_pair(file_offset, std::tuple_type_index<Entity, EntityTypes>::value)),
            const_cast<void *>(reinterpret_cast<const void *>(&entity)));
    }

private:
    Storage storage;
};

}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_CACHE_FILE_ENTITY_CACHE_H_
