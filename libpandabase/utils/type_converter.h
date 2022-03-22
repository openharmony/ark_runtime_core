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

#ifndef PANDA_LIBPANDABASE_UTILS_TYPE_CONVERTER_H_
#define PANDA_LIBPANDABASE_UTILS_TYPE_CONVERTER_H_

#include <array>
#include <cassert>
#include <cmath>
#include <iomanip>
#include <string_view>
#include <variant>

#include "macros.h"

namespace panda::helpers {

/**
 * \brief Class for presentation \param value_ in a readable format.
 * Presented as two fields: \param value_ and its \param literal_
 * @param value_ - double number of value
 * @param literal_ - string designations of the unit
 * @param precision_ - precision for output
 */
class ValueUnit {
public:
    ValueUnit(uint64_t value, std::string_view literal);

    ValueUnit(double value, std::string_view literal);

    /**
     *  \brief Set new \param precision for output
     *  @param precision
     */
    void SetPrecision(size_t precision);

    std::variant<double, uint64_t> GetValue() const;

    double GetDoubleValue() const;

    uint64_t GetUint64Value() const;

    std::string_view GetLiteral() const;

    size_t GetPrecision() const;

    virtual ~ValueUnit() = default;

    DEFAULT_COPY_SEMANTIC(ValueUnit);
    DEFAULT_MOVE_SEMANTIC(ValueUnit);

private:
    std::variant<double, uint64_t> value_;
    std::string_view literal_;
    size_t precision_ = DEFAULT_PRECISION;

    static constexpr size_t DEFAULT_PRECISION = 3;
};

bool operator==(const ValueUnit &lhs, const ValueUnit &rhs);
bool operator!=(const ValueUnit &lhs, const ValueUnit &rhs);

std::ostream &operator<<(std::ostream &os, const ValueUnit &element);

enum class ValueType {
    VALUE_TYPE_OBJECT,  // Type for objects
    VALUE_TYPE_TIME,    // Type for time
    VALUE_TYPE_MEMORY   // Type for memory
};

constexpr std::array<double, 4> COEFFS_MEMORY = {1024, 1024, 1024, 1024};
constexpr std::array<double, 6> COEFFS_TIME = {1000, 1000, 1000, 60, 60, 24};

constexpr std::array<std::string_view, 5> LITERALS_MEMORY = {"B", "KB", "MB", "GB", "TB"};
constexpr std::array<std::string_view, 7> LITERALS_TIME = {"ns", "us", "ms", "s", "m", "h", "day"};

/**
 *  Convert time from nanoseconds to readable format
 *  @param time_in_nanos
 */
ValueUnit TimeConverter(uint64_t times_in_nanos);

/**
 *  Convert memory from bytes to readable format
 *  @param bytes
 */
ValueUnit MemoryConverter(uint64_t bytes);

/**
 *  Convert any value
 *  @param value of type
 *  @param type what type is value
 */
ValueUnit ValueConverter(uint64_t value, ValueType type);

}  // namespace panda::helpers

#endif  // PANDA_LIBPANDABASE_UTILS_TYPE_CONVERTER_H_
