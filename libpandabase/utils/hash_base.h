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

#ifndef PANDA_LIBPANDABASE_UTILS_HASH_BASE_H_
#define PANDA_LIBPANDABASE_UTILS_HASH_BASE_H_

#include <array>
#include <cstdint>
#include <cstdlib>
#include <macros.h>

namespace panda {

// Superclass for all hash classes. Defines interfaces for hash methods.
template <typename HashImpl>
class HashBase {
public:
    /**
     * \brief Create 32 bits Hash from \param key via \param seed.
     * @param key - a key which should be hashed
     * @param len - length of the key in bytes
     * @param seed - seed which is used to calculate hash
     * @return 32 bits hash
     */
    static uint32_t GetHash32WithSeed(const uint8_t *key, size_t len, uint32_t seed)
    {
        return HashImpl::GetHash32WithSeedImpl(key, len, seed);
    }

    /**
     * \brief Create 32 bits Hash from \param key.
     * @param key - a key which should be hashed
     * @param len - length of the key in bytes
     * @return 32 bits hash
     */
    static uint32_t GetHash32(const uint8_t *key, size_t len)
    {
        return HashImpl::GetHash32Impl(key, len);
    }

    /**
     * \brief Create 32 bits Hash from MUTF8 \param string.
     * @param string - a pointer to the MUTF8 string
     * @return 32 bits hash
     */
    static uint32_t GetHash32String(const uint8_t *mutf8_string)
    {
        return HashImpl::GetHash32StringImpl(mutf8_string);
    }

    /**
     * \brief Create 32 bits Hash from MUTF8 \param string.
     * @param string - a pointer to the MUTF8 string
     * @param seed - seed which is used to calculate hash
     * @return 32 bits hash
     */
    static uint32_t GetHash32StringWithSeed(const uint8_t *mutf8_string, uint32_t seed)
    {
        return HashImpl::GetHash32StringWithSeedImpl(mutf8_string, seed);
    }
};

}  // namespace panda

#endif  // PANDA_LIBPANDABASE_UTILS_HASH_BASE_H_
