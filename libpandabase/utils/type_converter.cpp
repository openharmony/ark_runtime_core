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

#include "type_converter.h"

#include <array>
#include <cassert>
#include <cmath>
#include <iomanip>
#include <string_view>

namespace panda::helpers {

ValueUnit::ValueUnit(uint64_t value, std::string_view literal) : value_(value), literal_(literal) {}

ValueUnit::ValueUnit(double value, std::string_view literal) : value_(value), literal_(literal) {}

void ValueUnit::SetPrecision(size_t precision)
{
    precision_ = precision;
}

std::variant<double, uint64_t> ValueUnit::GetValue() const
{
    return value_;
}

double ValueUnit::GetDoubleValue() const
{
    return std::get<double>(value_);
}

uint64_t ValueUnit::GetUint64Value() const
{
    return std::get<uint64_t>(value_);
}

std::string_view ValueUnit::GetLiteral() const
{
    return literal_;
}

size_t ValueUnit::GetPrecision() const
{
    return precision_;
}

bool operator==(const ValueUnit &lhs, const ValueUnit &rhs)
{
    constexpr size_t NUMERAL_SYSTEM = 10;
    if (lhs.GetValue().index() != rhs.GetValue().index()) {
        return false;
    }
    if (lhs.GetLiteral() != rhs.GetLiteral()) {
        return false;
    }
    if (lhs.GetValue().index() == 0U) {
        return std::fabs(lhs.GetDoubleValue() - rhs.GetDoubleValue()) <
               std::pow(NUMERAL_SYSTEM, -std::max(lhs.GetPrecision(), rhs.GetPrecision()));
    }
    if (lhs.GetValue().index() == 1U) {
        return lhs.GetUint64Value() == rhs.GetUint64Value();
    }
    return false;
}

bool operator!=(const ValueUnit &lhs, const ValueUnit &rhs)
{
    return !(lhs == rhs);
}

std::ostream &operator<<(std::ostream &os, const ValueUnit &element)
{
    if (element.GetValue().index() == 0U) {
        os << std::fixed << std::setprecision(element.GetPrecision()) << element.GetDoubleValue()
           << std::setprecision(-1);
    } else {
        os << element.GetUint64Value();
    }
    return os << element.GetLiteral();
}

template <size_t SIZE>
ValueUnit TypeConverter(const std::array<double, SIZE> &coeffs, const std::array<std::string_view, SIZE + 1> &literals,
                        uint64_t value_base_dimension)
{
    constexpr double LIMIT = 1.0;
    double division_ratio = 1;
    for (size_t index_coeff = 0; index_coeff < SIZE; ++index_coeff) {
        if (value_base_dimension / (division_ratio * coeffs[index_coeff]) < LIMIT) {
            return ValueUnit(value_base_dimension / division_ratio, literals[index_coeff]);
        }
        division_ratio *= coeffs[index_coeff];
    }

    ASSERT(division_ratio != 0);
    return ValueUnit(value_base_dimension / division_ratio, literals[SIZE]);
}

ValueUnit TimeConverter(uint64_t times_in_nanos)
{
    return TypeConverter(COEFFS_TIME, LITERALS_TIME, times_in_nanos);
}

ValueUnit MemoryConverter(uint64_t bytes)
{
    ValueUnit bytes_format = TypeConverter(COEFFS_MEMORY, LITERALS_MEMORY, bytes);
    bytes_format.SetPrecision(0);
    return bytes_format;
}

ValueUnit ValueConverter(uint64_t value, ValueType type)
{
    switch (type) {
        case ValueType::VALUE_TYPE_TIME:
            return TimeConverter(value);
        case ValueType::VALUE_TYPE_MEMORY:
            return MemoryConverter(value);
        default:
            return ValueUnit(value, "");
    }
}

}  // namespace panda::helpers
