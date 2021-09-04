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

#include "runtime/mark_word.h"

namespace panda {

template <bool HashPolicy>
inline uint32_t MarkWord::GetHashConfigured() const
{
    LOG_IF(GetState() != STATE_HASHED, DEBUG, RUNTIME) << "Wrong State";
    return static_cast<uint32_t>((Value() >> HASH_SHIFT) & HASH_MASK);
}

template <>
inline uint32_t MarkWord::GetHashConfigured<false>() const
{
    LOG(ERROR, RUNTIME) << "Hash is not stored inside object header!";
    return 0;
}

uint32_t MarkWord::GetHash() const
{
    return GetHashConfigured<CONFIG_IS_HASH_IN_OBJ_HEADER>();
}

template <bool HashPolicy>
inline MarkWord MarkWord::DecodeFromHashConfigured(uint32_t hash)
{
    // Clear hash and status bits
    markWordSize temp = Value() & (~(HASH_MASK_IN_PLACE | STATUS_MASK_IN_PLACE));
    markWordSize hash_in_place = (static_cast<markWordSize>(hash) & HASH_MASK) << HASH_SHIFT;
    return MarkWord(temp | hash_in_place | (STATUS_HASHED << STATUS_SHIFT));
}

template <>
inline MarkWord MarkWord::DecodeFromHashConfigured<false>(uint32_t hash)
{
    (void)hash;
    LOG(ERROR, RUNTIME) << "Hash is not stored inside object header!";
    return MarkWord(0);
}

MarkWord MarkWord::DecodeFromHash(uint32_t hash)
{
    return DecodeFromHashConfigured<CONFIG_IS_HASH_IN_OBJ_HEADER>(hash);
}

template <bool HashPolicy>
inline MarkWord MarkWord::SetHashedConfigured()
{
    LOG(ERROR, RUNTIME) << "Hash is stored inside object header and we don't use hash status bit!";
    return MarkWord(0);
}

template <>
inline MarkWord MarkWord::SetHashedConfigured<false>()
{
    return MarkWord((Value() & (~HASH_STATUS_MASK_IN_PLACE)) | HASH_STATUS_MASK_IN_PLACE);
}

inline MarkWord MarkWord::SetHashed()
{
    return SetHashedConfigured<CONFIG_IS_HASH_IN_OBJ_HEADER>();
}

template <bool HashPolicy>
inline bool MarkWord::IsHashedConfigured() const
{
    LOG(ERROR, RUNTIME) << "Hash is stored inside object header and we don't use hash status bit!";
    return false;
}

template <>
inline bool MarkWord::IsHashedConfigured<false>() const
{
    return (Value() & HASH_STATUS_MASK_IN_PLACE) != 0U;
}

inline bool MarkWord::IsHashed() const
{
    return IsHashedConfigured<CONFIG_IS_HASH_IN_OBJ_HEADER>();
}

}  // namespace panda
