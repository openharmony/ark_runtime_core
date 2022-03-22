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

#ifndef PANDA_LIBPANDABASE_UTILS_HASH_H_
#define PANDA_LIBPANDABASE_UTILS_HASH_H_

#include "murmur3_hash.h"

namespace panda {

// Now, murmur3 Hash is used by default
template <uint32_t seed_value>
using DefaultHash = MurmurHash32<seed_value>;

// Default seed which is used in hash functions.
// NOTE: To create different seeds for your purposes,
// you must define it here and create new alias hash class
static constexpr uint32_t DEFAULT_SEED = 0x12345678U;

// Hash class alias with the default seed inside.
using Hash = DefaultHash<DEFAULT_SEED>;

/**
 * \brief Create 32 bits Hash from \param key via \param seed.
 * @param key - a key which should be hashed
 * @param len - length of the key in bytes
 * @param seed - seed which is used to calculate Hash
 * @return 32 bits Hash
 */
inline uint32_t GetHash32WithSeed(const uint8_t *key, size_t len, uint32_t seed)
{
    return Hash::GetHash32WithSeed(key, len, seed);
}

/**
 * \brief Create 32 bits Hash from \param key.
 * @param key - a key which should be hashed
 * @param len - length of the key in bytes
 * @return 32 bits Hash
 */
inline uint32_t GetHash32(const uint8_t *key, size_t len)
{
    return Hash::GetHash32(key, len);
}

/**
 * \brief Create 32 bits Hash from MUTF8 \param string.
 * @param string - a pointer to the MUTF8 string
 * @return 32 bits Hash
 */
inline uint32_t GetHash32String(const uint8_t *mutf8_string)
{
    return Hash::GetHash32String(mutf8_string);
}

/**
 * \brief Create 32 bits Hash from MUTF8 \param string.
 * @param string - a pointer to the MUTF8 string
 * @param seed - seed which is used to calculate Hash
 * @return 32 bits Hash
 */
inline uint32_t GetHash32StringWithSeed(const uint8_t *mutf8_string, uint32_t seed)
{
    return Hash::GetHash32StringWithSeed(mutf8_string, seed);
}

constexpr uint32_t FNV_INITIAL_SEED = 0x811c9dc5;

// Works like FNV Hash but operates over 4-byte words at a time instead of single bytes
template <typename Item>
uint32_t PseudoFnvHashItem(Item item, uint32_t seed = FNV_INITIAL_SEED)
{
    // NOLINTNEXTLINE(readability-braces-around-statements)
    if constexpr (sizeof(Item) <= 4) {
        constexpr uint32_t PRIME = 16777619U;
        return (seed ^ static_cast<uint32_t>(item)) * PRIME;
    } else if constexpr (sizeof(Item) == 8) {  // NOLINT(readability-misleading-indentation)
        auto item1 = static_cast<uint64_t>(item);
        uint32_t hash = PseudoFnvHashItem(static_cast<uint32_t>(item1), seed);
        constexpr uint32_t FOUR_BYTES = 32U;
        return PseudoFnvHashItem(static_cast<uint32_t>(item1 >> FOUR_BYTES), hash);
    } else {
        static_assert(sizeof(Item *) == 0, "PseudoFnvHashItem can only be called on types of size 1, 2, 4 or 8");
    }
}

// Works like FNV Hash but operates over 4-byte words at a time instead of single bytes
inline uint32_t PseudoFnvHashString(const uint8_t *str, uint32_t hash = FNV_INITIAL_SEED)
{
    while (true) {
        // NOLINTNEXTLINE(readability-implicit-bool-conversion, cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (!str[0] || !str[1] || !str[2] || !str[3]) {
            break;
        }
        constexpr uint32_t BYTE = 8U;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        uint32_t word32 = str[0];
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        word32 |= static_cast<uint32_t>(str[1]) << BYTE;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        word32 |= static_cast<uint32_t>(str[2U]) << (BYTE * 2U);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        word32 |= static_cast<uint32_t>(str[3U]) << (BYTE * 3U);
        hash = PseudoFnvHashItem(word32, hash);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        str += sizeof(uint32_t);
    }
    while (*str != 0) {
        hash = PseudoFnvHashItem(*str, hash);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        ++str;
    }
    return hash;
}

template <typename Container>
uint32_t FnvHash(const Container &data, uint32_t hash = FNV_INITIAL_SEED)
{
    for (const auto value : data) {
        hash = PseudoFnvHashItem(value, hash);
    }
    return hash;
}

// Combine lhash and rhash
inline size_t merge_hashes(size_t lhash, size_t rhash)
{
    constexpr const size_t magic_constant = 0x9e3779b9;
    size_t shl = lhash << 6U;
    size_t shr = lhash >> 2U;
    return lhash ^ (rhash + magic_constant + shl + shr);
}

}  // namespace panda

#endif  // PANDA_LIBPANDABASE_UTILS_HASH_H_
