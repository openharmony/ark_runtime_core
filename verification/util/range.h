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

#ifndef PANDA_VERIFICATION_UTIL_RANGE_H_
#define PANDA_VERIFICATION_UTIL_RANGE_H_

#include <limits>
#include <type_traits>
#include <string>
#include <algorithm>
#include <iterator>

namespace panda::verifier {

template <typename... T>
class Range;

template <typename Int>
class Range<Int> {
public:
    class Iterator {
    public:
        typedef Int value_type;
        typedef Int *pointer;
        typedef Int &reference;
        typedef std::make_signed_t<Int> difference_type;
        typedef std::forward_iterator_tag iterator_category;

        Iterator(const Int val) : Val_ {val} {}
        Iterator() = default;
        Iterator(const Iterator &) = default;
        Iterator(Iterator &&) = default;
        Iterator &operator=(const Iterator &) = default;
        Iterator &operator=(Iterator &&) = default;
        ~Iterator() = default;

        Iterator operator++(int)
        {
            Iterator old {*this};
            ++Val_;
            return old;
        }

        Iterator &operator++()
        {
            ++Val_;
            return *this;
        }

        Iterator operator--(int)
        {
            Iterator old {*this};
            --Val_;
            return old;
        }

        Iterator &operator--()
        {
            --Val_;
            return *this;
        }

        bool operator==(const Iterator &rhs)
        {
            return Val_ == rhs.Val_;
        }

        bool operator!=(const Iterator &rhs)
        {
            return Val_ != rhs.Val_;
        }

        Int operator*()
        {
            return Val_;
        }

    private:
        Int Val_ = std::numeric_limits<Int>::min();
    };

    template <typename Container>
    Range(const Container &cont) : From_ {0}, To_ {cont.size() - 1}
    {
    }

    Range(const Int from, const Int to) : From_ {std::min(from, to)}, To_ {std::max(from, to)} {}
    Range() = default;
    ~Range() = default;

    Iterator begin() const
    {
        return {From_};
    }

    Iterator cbegin() const
    {
        return {From_};
    }

    Iterator end() const
    {
        return {To_ + 1};
    }

    Iterator cend() const
    {
        return {To_ + 1};
    }

    Range BasedAt(Int point) const
    {
        return Range {point, point + To_ - From_};
    }

    bool Contains(Int point) const
    {
        return point >= From_ && point <= To_;
    }

    Int PutInBounds(Int point) const
    {
        if (point < From_) {
            return From_;
        }
        if (point > To_) {
            return To_;
        }
        return point;
    }

    size_t Length() const
    {
        return To_ - From_ + 1;
    }

    Int OffsetOf(Int val) const
    {
        return val - From_;
    }

    Int IndexOf(Int offset) const
    {
        return offset + From_;
    }

    Int Start() const
    {
        return From_;
    }

    Int End() const
    {
        return To_;
    }

    bool operator==(const Range &rhs) const
    {
        return From_ == rhs.From_ && To_ == rhs.To_;
    }

private:
    Int From_;
    Int To_;
};

template <typename Int>
Range(Int, Int, typename std::enable_if<std::is_integral<Int>::value, bool>::type b = true)->Range<Int>;

}  // namespace panda::verifier

namespace std {

template <typename Int>
string to_string(const panda::verifier::Range<Int> &range)
{
    return string {"[ "} + to_string(range.Start()) + " .. " + to_string(range.End()) + " ]";
}

}  // namespace std

#endif  // PANDA_VERIFICATION_UTIL_RANGE_H_
