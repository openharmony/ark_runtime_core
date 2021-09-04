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

#ifndef PANDA_RUNTIME_INCLUDE_HISTOGRAM_H_
#define PANDA_RUNTIME_INCLUDE_HISTOGRAM_H_

#include "libpandabase/utils/type_converter.h"
#include "libpandabase/macros.h"
#include "mem/panda_containers.h"
#include "mem/panda_string.h"

namespace panda {

/**
 * \brief Class for providing distribution statistics
 * Minimum, maximum, count, average, sum, dispersion
 */
template <class Value>
class SimpleHistogram {
public:
    explicit SimpleHistogram(helpers::ValueType type_of_value = helpers::ValueType::VALUE_TYPE_OBJECT)
        : type_of_value_(type_of_value)
    {
    }

    /**
     *  \brief Add all element to statistics at the half-interval from \param start to \param finish
     *  @param start begin of values inclusive
     *  @param finish end of values ​​not inclusive
     */
    template <class ForwardIterator>
    SimpleHistogram(ForwardIterator start, ForwardIterator finish,
                    helpers::ValueType type_of_value = helpers::ValueType::VALUE_TYPE_OBJECT);

    /**
     *  \brief Output the General statistics of Histogram
     *  @return PandaString with Sum, Avg, Max
     */
    PandaString GetGeneralStatistic() const;

    /**
     *  \brief Add \param element to statistics \param number of times
     *  @param element
     *  @param count
     */
    void AddValue(const Value &element, size_t number = 1);

    size_t GetCount() const
    {
        return count_;
    }

    Value GetSum() const
    {
        return sum_;
    }

    Value GetMin() const
    {
        return min_;
    }

    Value GetMax() const
    {
        return max_;
    }

    double GetAvg() const
    {
        if (count_ > 0U) {
            return sum_ / static_cast<double>(count_);
        }
        return 0;
    }

    double GetDispersion() const
    {
        if (count_ > 0U) {
            return sum_of_squares_ / static_cast<double>(count_) - GetAvg() * GetAvg();
        }
        return 0;
    }

    ~SimpleHistogram() = default;

    DEFAULT_COPY_SEMANTIC(SimpleHistogram);
    DEFAULT_NOEXCEPT_MOVE_SEMANTIC(SimpleHistogram);

private:
    size_t count_ = 0;
    Value sum_ = 0;
    Value sum_of_squares_ = 0;
    Value min_ = 0;
    Value max_ = 0;
    helpers::ValueType type_of_value_;
};

/**
 * \brief Class for providing distribution statistics
 * Minimum, maximum, count, average, sum, dispersion
 */
template <class Value>
class Histogram : public SimpleHistogram<Value> {
public:
    explicit Histogram(helpers::ValueType type_of_value = helpers::ValueType::VALUE_TYPE_OBJECT)
        : SimpleHistogram<Value>(type_of_value)
    {
    }
    ~Histogram() = default;
    DEFAULT_COPY_SEMANTIC(Histogram);
    DEFAULT_MOVE_SEMANTIC(Histogram);

    /**
     *  \brief Add all elements to statistics at the half-interval from \param start to \param finish
     *  @param start begin of values inclusive
     *  @param finish end of values ​​not inclusive
     */
    template <class ForwardIterator>
    Histogram(ForwardIterator start, ForwardIterator finish,
              helpers::ValueType type_of_value = helpers::ValueType::VALUE_TYPE_OBJECT);

    /**
     *  \brief Output the first \param count_top of the lowest values by key
     *  with the number of their count
     *  @param count_top Number of first values to output
     *  @return PandaString with in format: "[key:count[,]]*"
     */
    PandaString GetTopDump(size_t count_top = DEFAULT_TOP_SIZE) const;

    /**
     *  \brief Add \param element to statistics \param number of times
     *  @param element
     *  @param count
     */
    void AddValue(const Value &element, size_t number = 1);

    size_t GetCountDifferent() const
    {
        return frequency_.size();
    }

private:
    PandaMap<Value, uint32_t> frequency_;

    static constexpr size_t DEFAULT_TOP_SIZE = 10;
};

}  // namespace panda

#endif  // PANDA_RUNTIME_INCLUDE_HISTOGRAM_H_
