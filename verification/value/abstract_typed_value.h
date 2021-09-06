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

#ifndef PANDA_VERIFICATION_VALUE_ABSTRACT_TYPED_VALUE_H_
#define PANDA_VERIFICATION_VALUE_ABSTRACT_TYPED_VALUE_H_

#include "verification/value/abstract_type.h"
#include "verification/value/abstract_value.h"

#include "verification/value/origin.h"

#include "libpandafile/bytecode_instruction.h"

#include "macros.h"

namespace panda::verifier {
class AbstractTypedValue {
    using ValueOrigin = Origin<panda::BytecodeInstructionSafe>;

public:
    bool IsNone() const
    {
        return Type_.IsNone();
    }
    AbstractTypedValue() = default;
    AbstractTypedValue(const AbstractTypedValue &) = default;
    AbstractTypedValue(AbstractTypedValue &&) = default;
    AbstractTypedValue &operator=(const AbstractTypedValue &) = default;
    AbstractTypedValue &operator=(AbstractTypedValue &&) = default;
    ~AbstractTypedValue() = default;
    AbstractTypedValue(const AbstractType &type, const AbstractValue &value) : Value_ {value}, Type_ {type} {}
    AbstractTypedValue(const AbstractTypedValue &atv, const panda::BytecodeInstructionSafe &inst)
        : Value_ {atv.Value_}, Type_ {atv.Type_}, Origin_ {inst}
    {
    }
    AbstractTypedValue(const AbstractType &type, const AbstractValue &value, const panda::BytecodeInstructionSafe &inst)
        : Value_ {value}, Type_ {type}, Origin_ {inst}
    {
    }
    AbstractTypedValue(const AbstractType &type, const AbstractValue &value, const ValueOrigin &origin)
        : Value_ {value}, Type_ {type}, Origin_ {origin}
    {
    }
    struct Start {
    };
    AbstractTypedValue(const AbstractType &type, const AbstractValue &value, [[maybe_unused]] Start start, size_t n)
        : Value_ {value}, Type_ {type}, Origin_ {OriginType::START, n}
    {
    }
    AbstractTypedValue &SetAbstractType(const AbstractType &type)
    {
        Type_ = type;
        return *this;
    }
    AbstractTypedValue &SetAbstractValue(const AbstractValue &value)
    {
        Value_ = value;
        return *this;
    }
    const AbstractType &GetAbstractType() const
    {
        return Type_;
    }
    const AbstractValue &GetAbstractValue() const
    {
        return Value_;
    }
    AbstractTypedValue operator&(const AbstractTypedValue &rhs) const
    {
        if (Origin_.IsValid() && rhs.Origin_.IsValid() && Origin_ == rhs.Origin_) {
            return {Type_ & rhs.GetAbstractType(), Value_ & rhs.GetAbstractValue(), Origin_};
        }
        return {Type_ & rhs.GetAbstractType(), Value_ & rhs.GetAbstractValue()};
    }
    bool IsConsistent() const
    {
        return Type_.IsConsistent();
    }
    ValueOrigin &GetOrigin()
    {
        return Origin_;
    }
    const ValueOrigin &GetOrigin() const
    {
        return Origin_;
    }
    template <typename StrT, typename TypeImageFunc>
    StrT Image(TypeImageFunc type_img_func) const
    {
        // currently only types and origin printed
        StrT result {GetAbstractType().Image<StrT>(type_img_func)};
        if (Origin_.IsValid()) {
            if (Origin_.AtStart()) {
                result += "@start";
            } else {
                result += "@" + NumToStr<StrT>(Origin_.GetOffset(), 16U, sizeof(decltype(Origin_.GetOffset())) * 2U);
            }
        }
        return result;
    }

private:
    AbstractValue Value_;
    AbstractType Type_;
    ValueOrigin Origin_;
};

}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_VALUE_ABSTRACT_TYPED_VALUE_H_
