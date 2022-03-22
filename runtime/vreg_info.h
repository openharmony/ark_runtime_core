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

#ifndef PANDA_RUNTIME_VREG_INFO_H_
#define PANDA_RUNTIME_VREG_INFO_H_

#include "libpandabase/utils/bit_field.h"
#include "libpandabase/utils/bit_table.h"
#include "libpandabase/utils/small_vector.h"
#include "libpandabase/utils/span.h"

namespace panda {

class VRegInfo final {
public:
    enum class Location : int8_t { NONE, SLOT, REGISTER, FP_REGISTER, CONSTANT, COUNT = CONSTANT, INVALID = -1 };

    enum class Type : uint8_t { UNDEFINED, OBJECT, INT32, INT64, FLOAT32, FLOAT64, BOOL, COUNT = BOOL };

    VRegInfo()
    {
        FieldLocation::Set(Location::NONE, &info_);
        ASSERT(!IsLive());
    }
    VRegInfo(uint32_t value, VRegInfo::Location location, Type type, bool is_acc) : value_(value)
    {
        FieldLocation::Set(location, &info_);
        FieldType::Set(type, &info_);
        FieldIsAccumulator ::Set(is_acc, &info_);
    }
    VRegInfo(uint32_t value, VRegInfo::Location location, Type type, bool is_acc, uint32_t index)
        : VRegInfo(value, location, type, is_acc)
    {
        FieldVRegIndex::Set(index, &info_);
    }
    VRegInfo(uint32_t value, uint32_t packed_info) : value_(value), info_(packed_info) {}

    static VRegInfo Invalid()
    {
        return VRegInfo(0, Location::INVALID, Type::UNDEFINED, false);
    }

    ~VRegInfo() = default;

    DEFAULT_COPY_SEMANTIC(VRegInfo);
    DEFAULT_MOVE_SEMANTIC(VRegInfo);

    uint32_t GetValue() const
    {
        return value_;
    }

    void SetValue(uint32_t value)
    {
        value_ = value;
    }

    Location GetLocation() const
    {
        return FieldLocation::Get(info_);
    }

    Type GetType() const
    {
        return FieldType::Get(info_);
    }

    uint16_t GetIndex() const
    {
        return FieldVRegIndex::Get(info_);
    }
    void SetIndex(uint16_t value)
    {
        FieldVRegIndex::Set(value, &info_);
    }

    bool IsAccumulator() const
    {
        return FieldIsAccumulator::Get(info_);
    }

    bool IsLive() const
    {
        return GetLocation() != Location::NONE;
    }

    bool IsObject() const
    {
        return GetType() == Type::OBJECT;
    }

    bool IsFloat() const
    {
        return GetType() == Type::FLOAT32 || GetType() == Type::FLOAT64;
    }

    bool Has64BitValue() const
    {
        return GetType() == VRegInfo::Type::FLOAT64 || GetType() == VRegInfo::Type::INT64;
    }

    bool IsLocationRegister() const
    {
        auto location = GetLocation();
        return location == Location::REGISTER || location == Location::FP_REGISTER;
    }

    uint32_t GetConstantLowIndex() const
    {
        ASSERT(GetLocation() == Location::CONSTANT);
        return GetValue() & ((1U << BITS_PER_UINT16) - 1);
    }

    uint32_t GetConstantHiIndex() const
    {
        ASSERT(GetLocation() == Location::CONSTANT);
        return (GetValue() >> BITS_PER_UINT16) & ((1U << BITS_PER_UINT16) - 1);
    }

    void SetConstantIndices(uint16_t low, uint16_t hi)
    {
        value_ = low | (static_cast<uint32_t>(hi) << BITS_PER_UINT16);
    }

    bool operator==(const VRegInfo &rhs) const
    {
        return value_ == rhs.value_ && info_ == rhs.info_;
    }
    bool operator!=(const VRegInfo &rhs) const
    {
        return !(*this == rhs);
    }

    uint32_t GetInfo()
    {
        return info_;
    }

    const char *GetTypeString() const
    {
        switch (GetType()) {
            case Type::OBJECT:
                return "OBJECT";
            case Type::INT64:
                return "INT64";
            case Type::INT32:
                return "INT32";
            case Type::FLOAT32:
                return "FLOAT32";
            case Type::FLOAT64:
                return "FLOAT64";
            case Type::BOOL:
                return "BOOL";
            default:
                break;
        }
        UNREACHABLE();
    }

    const char *GetLocationString() const
    {
        switch (GetLocation()) {
            case Location::NONE:
                return "NONE";
            case Location::SLOT:
                return "SLOT";
            case Location::REGISTER:
                return "REGISTER";
            case Location::FP_REGISTER:
                return "FP_REGISTER";
            case Location::CONSTANT:
                return "CONSTANT";
            default:
                break;
        }
        UNREACHABLE();
    }

    void Dump(std::ostream &os) const
    {
        os << "VReg #" << GetIndex() << ":" << GetTypeString() << ", " << GetLocationString() << "="
           << helpers::ToSigned(GetValue());
        if (IsAccumulator()) {
            os << ", ACC";
        }
    }

private:
    uint32_t value_ {0};
    uint32_t info_ {0};

    using FieldLocation = BitField<Location, 0, MinimumBitsToStore(static_cast<uint32_t>(Location::COUNT))>;
    using FieldType = FieldLocation::NextField<Type, MinimumBitsToStore(static_cast<uint32_t>(Type::COUNT))>;
    using FieldIsAccumulator = FieldType::NextFlag;
    using FieldVRegIndex = FieldIsAccumulator::NextField<uint16_t, BITS_PER_UINT16>;
};

static_assert(sizeof(VRegInfo) <= sizeof(uint64_t));

inline std::ostream &operator<<(std::ostream &os, const VRegInfo &vreg)
{
    vreg.Dump(os);
    return os;
}

}  // namespace panda

#endif  // PANDA_RUNTIME_VREG_INFO_H_
