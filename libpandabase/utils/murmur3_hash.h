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

// This is the murmur3 32 bit hash implementation
// The main structure and constants were taken from Austin Appleby.
// From his gitlab (https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp):
// - MurmurHash3 was written by Austin Appleby, and is placed in the public
// - domain. The author hereby disclaims copyright to this source code.

#ifndef PANDA_LIBPANDABASE_UTILS_MURMUR3_HASH_H_
#define PANDA_LIBPANDABASE_UTILS_MURMUR3_HASH_H_

#include "hash_base.h"
#include "logger.h"

namespace panda {

// In general murmur hash looks like that:
// key = |....|....|....|.|.|.|
//         32   32   32  8 8 8
// Firstly, we proceed each 32 bits block from key;
// Secondly, we proceed last 8 bits block which were not covered in previous step.
template <uint32_t seed_value>
class MurmurHash32 final : public HashBase<MurmurHash32<seed_value>> {
public:
    static uint32_t GetHash32WithSeedImpl(const uint8_t *key, size_t len, uint32_t seed)
    {
        return MurmurHash3(key, len, seed);
    }
    static uint32_t GetHash32Impl(const uint8_t *key, size_t len)
    {
        return GetHash32WithSeedImpl(key, len, seed_value);
    }
    static uint32_t GetHash32StringImpl(const uint8_t *mutf8_string)
    {
        return MurmurHash3String(mutf8_string, seed_value);
    }
    static uint32_t GetHash32StringWithSeedImpl(const uint8_t *mutf8_string, uint32_t seed)
    {
        return MurmurHash3String(mutf8_string, seed);
    }

private:
    // Here are the main constants for hash counting:
    // Many of them look like some kinds of magic
    static constexpr uint32_t C1 = 0xCC9E2D51U;
    static constexpr uint32_t C2 = 0x1B873593U;
    static constexpr uint32_t MAX_BITS = 32;
    static constexpr uint32_t FINALIZE_FIRST_SHIFT = 16;
    static constexpr uint32_t FINALIZE_SECOND_SHIFT = 13;
    static constexpr uint32_t FINALIZE_THIRD_SHIFT = 16;
    static constexpr uint32_t FINALIZE_FIRST_MULTIPLICATOR = 0x85EBCA6BU;
    static constexpr uint32_t FINALIZE_SECOND_MULTIPLICATOR = 0xC2BAE35U;
    static constexpr uint32_t MAIN_FIRST_SHIFT = 15;
    static constexpr uint32_t MAIN_SECOND_SHIFT = 13;
    static constexpr uint32_t MAIN_CONSTANT = 0xE6546B64U;
    static constexpr uint32_t MAIN_MULTIPLICATOR = 5;
    static constexpr uint32_t TAIL_SHIFT = 8;
    static constexpr uint32_t TAIL_LAST_SHIFT = 15;
    static constexpr uint32_t BLOCK_SIZE = 4;

    static uint32_t Rotl(uint32_t word, uint8_t shift)
    {
        return (word << shift) | (word >> (MAX_BITS - shift));
    }

    // Finalize the result of the hash function
    static uint32_t Finalize(uint32_t h)
    {
        h ^= h >> FINALIZE_FIRST_SHIFT;
        h *= FINALIZE_FIRST_MULTIPLICATOR;
        h ^= h >> FINALIZE_SECOND_SHIFT;
        h *= FINALIZE_SECOND_MULTIPLICATOR;
        h ^= h >> FINALIZE_THIRD_SHIFT;
        return h;
    }

    static uint32_t MurmurHash3(const uint8_t *key, size_t len, uint32_t seed)
    {
        // We start hashing from the seed
        uint32_t hash = seed;

        // Do the main part:
        // Iterate for each 32bits
        auto blocks = reinterpret_cast<uintptr_t>(key);
        std::array<uint8_t, BLOCK_SIZE> memblock = {'\0', '\0', '\0', '\0'};
        for (size_t i = len / BLOCK_SIZE; i != 0; i--) {
            for (auto &j : memblock) {
                j = *reinterpret_cast<uint8_t *>(blocks);
                blocks += sizeof(uint8_t);
            }
            // Do this because we don't want to dispatch Big/Little endianness.
            uint32_t k1 = *reinterpret_cast<uint32_t *>(memblock.data());

            k1 *= C1;
            k1 = Rotl(k1, MAIN_FIRST_SHIFT);
            k1 *= C2;

            hash ^= k1;
            hash = Rotl(hash, MAIN_SECOND_SHIFT);
            hash = hash * MAIN_MULTIPLICATOR + MAIN_CONSTANT;
        }

        // Proceed the tail:
        // blocks is a pointer to the end of 32bits section
        auto tail = blocks;
        uint32_t k1 = 0;
        for (size_t i = len & 3U; i > 0; i--) {
            // Get ((uint8_t*)tail)[i - 1]:
            uintptr_t block_pointer = tail + sizeof(uint8_t) * (i - 1);
            uint8_t block = *reinterpret_cast<uint8_t *>(block_pointer);
            uint32_t temp = (block << (TAIL_SHIFT * (i - 1U)));
            k1 ^= temp;
            if (i == 1) {
                k1 = Rotl(k1, TAIL_LAST_SHIFT);
                k1 *= C2;
                hash ^= k1;
            }
        }

        // Finalize the result
        hash ^= len;
        hash = Finalize(hash);

        return hash;
    }

    static uint32_t MurmurHash3String(const uint8_t *mutf8_string, uint32_t seed)
    {
        // We start hashing from the seed
        uint32_t hash = seed;
        // We should still compute length of the string, because we will need it later
        size_t mutf8_length = 0;
        // Do the main part:
        // Iterate for each 32bits
        auto blocks = reinterpret_cast<uintptr_t>(mutf8_string);
        std::array<uint8_t, BLOCK_SIZE> memblock = {'\0', '\0', '\0', '\0'};
        size_t tail_len = 0;
        while (true) {
            tail_len = 0;
            for (unsigned char &i : memblock) {
                i = *reinterpret_cast<uint8_t *>(blocks);
                blocks += sizeof(uint8_t);
                if (i == '\0') {
                    break;
                }
                tail_len++;
            }
            if (tail_len != BLOCK_SIZE) {
                // We couldn't read four bytes value
                break;
            }
            // Do this because we don't want to dispatch Big/Little endianness.
            uint32_t k1 = *reinterpret_cast<uint32_t *>(memblock.data());

            k1 *= C1;
            k1 = Rotl(k1, MAIN_FIRST_SHIFT);
            k1 *= C2;

            hash ^= k1;
            hash = Rotl(hash, MAIN_SECOND_SHIFT);
            hash = hash * MAIN_MULTIPLICATOR + MAIN_CONSTANT;
            mutf8_length += BLOCK_SIZE;
        }

        // Proceed the tail
        mutf8_length += tail_len;
        uint32_t k1 = 0;
        for (size_t i = tail_len; i > 0; i--) {
            uint8_t block = memblock[i - 1U];
            uint32_t temp = (block << (TAIL_SHIFT * (i - 1U)));
            k1 ^= temp;
            if (i == 1) {
                k1 = Rotl(k1, TAIL_LAST_SHIFT);
                k1 *= C2;
                hash ^= k1;
            }
        }

        // Finalize the result
        hash ^= mutf8_length;
        hash = Finalize(hash);

        return hash;
    }
};

}  // namespace panda

#endif  // PANDA_LIBPANDABASE_UTILS_MURMUR3_HASH_H_
