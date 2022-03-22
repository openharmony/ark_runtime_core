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

#ifndef PANDA_VERIFICATION_VALUE_ORIGIN_H_
#define PANDA_VERIFICATION_VALUE_ORIGIN_H_

#include "verification/util/tagged_index.h"

#include "macros.h"

#include <cstdint>
#include <limits>

namespace panda::verifier {
enum class OriginType { START, INSTRUCTION, __LAST__ = INSTRUCTION };

template <typename BytecodeInstruction>
class Origin : public TaggedIndex<OriginType> {
    using Base = TaggedIndex<OriginType>;

public:
    Origin(const BytecodeInstruction &inst) : Base {OriginType::INSTRUCTION, inst.GetOffset()} {}

    Origin(OriginType t, size_t val) : Base {t, val} {}

    bool AtStart() const
    {
        ASSERT(Base::IsValid());
        return Base::GetTag() == OriginType::START;
    }

    uint32_t GetOffset() const
    {
        ASSERT(Base::IsValid());
        return static_cast<uint32_t>(Base::GetInt());
    };

    Origin() = default;
    Origin(const Origin &) = default;
    Origin(Origin &&) = default;
    ~Origin() = default;
    Origin &operator=(const Origin &) = default;
    Origin &operator=(Origin &&) = default;
};
}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_VALUE_ORIGIN_H_
