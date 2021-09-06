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

#ifndef PANDA_VERIFICATION_TYPE_TYPE_INFO_H_
#define PANDA_VERIFICATION_TYPE_TYPE_INFO_H_

#include "verification/util/lazy.h"
#include "type_sort.h"
#include "type_index.h"

namespace panda::verifier {
class TypeInfo {
public:
    TypeInfo(SortIdx sort, const TypeParamsIdx &params) : Sort_(sort), ParamsIdx_(params) {}
    TypeInfo(SortIdx sort, TypeParamsIdx &&params) : Sort_(sort), ParamsIdx_(std::move(params)) {}
    ~TypeInfo() = default;
    bool operator==(const TypeInfo &rhs) const
    {
        return Sort_ == rhs.Sort_ && ParamsIdx_ == rhs.ParamsIdx_;
    }
    size_t Arity() const
    {
        return ParamsIdx_.size();
    }
    const SortIdx &Sort() const
    {
        return Sort_;
    }
    const TypeParamsIdx &ParamsIdx() const
    {
        return ParamsIdx_;
    }

private:
    SortIdx Sort_;
    TypeParamsIdx ParamsIdx_;
};
}  // namespace panda::verifier

namespace std {
template <>
struct hash<panda::verifier::TypeInfo> {
    size_t operator()(const panda::verifier::TypeInfo &ti) const
    {
        size_t result = ti.Sort();
        auto hash_func = [&result](size_t v) {
#ifdef PANDA_TARGET_64
            result ^= (v ^ (v << 17U)) + (v << 39U);      // NOLINT(readability-magic-numbers)
            result ^= (result >> 33U) | (result << 31U);  // NOLINT(readability-magic-numbers)
            result *= 0xff51afd7ed558ccdULL;              // NOLINT(readability-magic-numbers)
            result ^= (result >> 33U) | (result << 31U);  // NOLINT(readability-magic-numbers)
            result *= 0xc4ceb9fe1a85ec53ULL;              // NOLINT(readability-magic-numbers)
            result ^= (result >> 33U) | (result << 31U);  // NOLINT(readability-magic-numbers)
#else
            result ^= (v ^ (v << 9U)) + (v << 25U);       // NOLINT(readability-magic-numbers)
            result ^= (result >> 17U) | (result << 16U);  // NOLINT(readability-magic-numbers)
            result *= 0xed558ccdUL;                       // NOLINT(readability-magic-numbers)
            result ^= (result >> 17U) | (result << 16U);  // NOLINT(readability-magic-numbers)
            result *= 0x1a85ec53UL;                       // NOLINT(readability-magic-numbers)
            result ^= (result >> 17U) | (result << 16U);  // NOLINT(readability-magic-numbers)
#endif
        };
        hash_func(ti.ParamsIdx().size());
        for (const auto &v : ti.ParamsIdx()) {
            hash_func(v.GetIndex());
            hash_func(static_cast<size_t>(v.Variance()));
        }
        return result;
    }
};
}  // namespace std

#endif  // PANDA_VERIFICATION_TYPE_TYPE_INFO_H_
