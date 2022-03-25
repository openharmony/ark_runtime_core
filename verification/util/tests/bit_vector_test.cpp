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

#ifdef PANDA_CATCH2
#include "util/bit_vector.h"
#include "util/set_operations.h"
#include "util/range.h"
#include "util/lazy.h"

#include <catch2/catch.hpp>
#include <rapidcheck/catch.h>
#include "util/tests/environment.h"

#include <cstdint>
#include <initializer_list>
#include <string>
#include <set>

using namespace panda::verifier;

using StdSet = std::set<size_t>;

struct BSet {
    StdSet Indices;
    BitVector Bits;
};

void showValue(const BSet &bitset, std::ostream &os)
{
    os << "BitSet{ .Indices {";
    bool comma = false;
    for (auto idx : bitset.Indices) {
        if (comma) {
            os << ", ";
        }
        comma = true;
        os << idx;
    }
    os << "}, ";
    os << ".Bits = {";
    for (size_t idx = 0; idx < bitset.Bits.size(); ++idx) {
        os << (static_cast<int>(bitset.Bits[idx]) ? '1' : '0');
        if (((idx + 1) & 0x7) == 0) {
            os << ' ';
        }
        if (((idx + 1) & 0x1F) == 0) {
            os << ' ';
        }
    }
    os << "} }";
}

namespace rc {

constexpr size_t max_value = 1024;
auto value_gen = gen::inRange<size_t>(0, max_value);

template <>
struct Arbitrary<BSet> {
    static Gen<BSet> arbitrary()
    {
        auto set_n_inc = gen::pair(gen::container<std::set<size_t>>(value_gen), gen::inRange(1, 100U));
        return gen::map(set_n_inc, [](auto param_set_n_inc) {
            auto &[set, inc] = param_set_n_inc;
            size_t size = (set.empty() ? 0 : *set.rbegin()) + inc;
            // CODECHECK-NOLINTNEXTLINE(C_RULE_ID_REDUNDANT_INIT)
            BitVector bits {size};
            for (const auto &idx : set) {
                bits[idx] = 1;
            }
            return BSet {set, bits};
        });
    }
};

template <>
struct Arbitrary<Range<size_t>> {
    static Gen<Range<size_t>> arbitrary()
    {
        return gen::map(gen::pair(value_gen, value_gen), [](auto pair) {
            return Range<size_t> {std::min(pair.first, pair.second), std::max(pair.first, pair.second)};
        });
    }
};

}  // namespace rc

namespace panda::verifier::test {

using namespace rc;

using Interval = panda::verifier::Range<size_t>;
using Intervals = std::initializer_list<Interval>;

void ClassifySize(std::string name, size_t size, const Intervals &intervals)
{
    for (const auto &i : intervals) {
        RC_CLASSIFY(i.Contains(size), name + " " + std::to_string(i));
    }
}

const EnvOptions Options {"VERIFIER_TEST"};

Intervals stat_intervals = {{0, 10}, {11, 50}, {51, 100}, {101, max_value}};

void stat(const BSet &bitset)
{
    if (Options.Get<bool>("verbose", false)) {
        ClassifySize("Bits.size() in", bitset.Bits.size(), stat_intervals);
        ClassifySize("Indices.size() in", bitset.Indices.size(), stat_intervals);
    }
}

StdSet Universum(size_t size)
{
    return ToSet<StdSet>(Interval(0, size - 1));
}

// CODECHECK-NOLINTNEXTLINE(C_RULE_ID_FUNCTION_SIZE)
TEST_CASE("Test bitvector", "verifier_bitvector")
{
    SECTION("Basic tests")
    {
        prop("SetBitsCount()", [](const BSet &bit_set) {
            stat(bit_set);
            RC_ASSERT(bit_set.Bits.SetBitsCount() == bit_set.Indices.size());
        });
        prop("operator=", [](const BSet &bit_set) {
            stat(bit_set);
            auto bits = bit_set.Bits;
            RC_ASSERT(bits.size() == bit_set.Bits.size());
            for (size_t idx = 0; idx < bits.size(); ++idx) {
                RC_ASSERT(bits[idx] == bit_set.Bits[idx]);
            }
            RC_ASSERT(bits.SetBitsCount() == bit_set.Bits.SetBitsCount());
        });
        prop("clr()", [](BSet &&bset) {
            stat(bset);
            bset.Bits.clr();
            RC_ASSERT(bset.Bits.SetBitsCount() == 0U);
        });
        prop("set()", [](BSet &&bset) {
            stat(bset);
            bset.Bits.set();
            RC_ASSERT(bset.Bits.SetBitsCount() == bset.Bits.size());
        });
        prop("invert()", [](BSet &&bset) {
            stat(bset);
            auto zero_bits = bset.Bits.size() - bset.Bits.SetBitsCount();
            bset.Bits.invert();
            RC_ASSERT(bset.Bits.SetBitsCount() == zero_bits);
        });
        prop("Clr(size_t idx)", [](BSet &&bset, std::set<size_t> &&indices) {
            stat(bset);
            auto size = bset.Bits.size();
            for (const auto &idx : indices) {
                bset.Bits.Clr(idx % size);
                bset.Indices.erase(idx % size);
            }
            RC_ASSERT(bset.Bits.SetBitsCount() == bset.Indices.size());
        });
        prop("Set(size_t idx)", [](BSet &&bset, std::set<size_t> &&indices) {
            stat(bset);
            auto size = bset.Bits.size();
            for (const auto &idx : indices) {
                auto i = idx % size;
                bset.Bits.Set(i);
                bset.Indices.insert(i);
            }
            RC_ASSERT(bset.Bits.SetBitsCount() == bset.Indices.size());
        });
        prop("invert(size_t idx)", [](BSet &&bset, std::set<size_t> &&indices) {
            stat(bset);
            auto size = bset.Bits.size();
            for (const auto &idx : indices) {
                auto i = idx % size;
                bset.Bits.invert(i);
                if (bset.Indices.count(i) > 0) {
                    bset.Indices.erase(i);
                } else {
                    bset.Indices.insert(i);
                }
            }
            RC_ASSERT(bset.Bits.SetBitsCount() == bset.Indices.size());
        });
        prop("clr(size_t from, size_t to)", [](BSet &&bset, Interval &&interval) {
            RC_PRE(interval.End() < bset.Bits.size());
            stat(bset);
            bset.Bits.Clr(interval.Start(), interval.End());
            for (auto idx : interval) {
                bset.Indices.erase(idx);
            }
            RC_ASSERT(bset.Bits.SetBitsCount() == bset.Indices.size());
        });
        prop("set(size_t from, size_t to)", [](BSet &&bset, Interval &&interval) {
            RC_PRE(interval.End() < bset.Bits.size());
            stat(bset);
            bset.Bits.Set(interval.Start(), interval.End());
            for (auto idx : interval) {
                bset.Indices.insert(idx);
            }
            RC_ASSERT(bset.Bits.SetBitsCount() == bset.Indices.size());
        });
        prop("invert(size_t from, size_t to)", [](BSet &&bset, Interval &&interval) {
            RC_PRE(interval.End() < bset.Bits.size());
            stat(bset);
            bset.Bits.invert(interval.Start(), interval.End());
            for (auto idx : interval) {
                if (bset.Indices.count(idx) > 0) {
                    bset.Indices.erase(idx);
                } else {
                    bset.Indices.insert(idx);
                }
            }
            RC_ASSERT(bset.Bits.SetBitsCount() == bset.Indices.size());
        });
        prop("operator&=", [](BSet &&lhs, BSet &&rhs) {
            stat(rhs);
            stat(lhs);
            auto old_size = lhs.Bits.size();
            lhs.Bits &= rhs.Bits;
            StdSet universum = Universum(rhs.Bits.size());
            StdSet inversion = SetDifference(universum, rhs.Indices);
            lhs.Indices = SetDifference(lhs.Indices, inversion);
            RC_ASSERT(lhs.Bits.size() == old_size);
            RC_ASSERT(lhs.Bits.SetBitsCount() == lhs.Indices.size());
        });
        prop("operator|=", [](BSet &&lhs, BSet &&rhs) {
            stat(rhs);
            stat(lhs);
            auto old_size = lhs.Bits.size();
            StdSet universum = Universum(lhs.Bits.size());
            StdSet clamped = SetIntersection(rhs.Indices, universum);
            lhs.Bits |= rhs.Bits;
            lhs.Indices = SetUnion(lhs.Indices, clamped);
            RC_ASSERT(lhs.Bits.size() == old_size);
            RC_ASSERT(lhs.Bits.SetBitsCount() == lhs.Indices.size());
        });
        prop("operator^=", [](BSet &&lhs, BSet &&rhs) {
            stat(rhs);
            stat(lhs);
            auto old_size = lhs.Bits.size();
            StdSet universum = Universum(lhs.Bits.size());
            StdSet clamped = SetIntersection(rhs.Indices, universum);
            lhs.Bits ^= rhs.Bits;
            lhs.Indices = SetDifference(SetUnion(lhs.Indices, clamped), SetIntersection(lhs.Indices, clamped));
            RC_ASSERT(lhs.Bits.size() == old_size);
            RC_ASSERT(lhs.Bits.SetBitsCount() == lhs.Indices.size());
        });
        prop("resize(size_t)", [](BSet &&bset, size_t new_size) {
            stat(bset);
            new_size %= bset.Bits.size();
            bset.Bits.resize(new_size);
            auto universum = Universum(new_size);
            bset.Indices = SetIntersection(universum, bset.Indices);
            RC_ASSERT(bset.Bits.SetBitsCount() == bset.Indices.size());
            RC_ASSERT(bset.Bits.size() == new_size);
        });
    }
    SECTION("Iterators")
    {
        prop("for_all_idx_val(Handler)", [](BSet &&bset) {
            stat(bset);
            StdSet selected;
            auto handler = [&selected](auto idx, auto val) {
                while (val) {
                    if (val & 1U) {
                        selected.insert(idx);
                    }
                    val >>= 1U;
                    ++idx;
                }
                return true;
            };
            bset.Bits.for_all_idx_val(handler);
            RC_ASSERT(selected == bset.Indices);
        });
        prop("for_all_idx_of<1>(Handler)", [](BSet &&bset) {
            stat(bset);
            StdSet result;
            bset.Bits.for_all_idx_of<1>([&result](auto idx) {
                result.insert(idx);
                return true;
            });
            RC_ASSERT(result == bset.Indices);
        });
        prop("for_all_idx_of<0>(Handler)", [](BSet &&bset) {
            stat(bset);
            StdSet result;
            bset.Bits.for_all_idx_of<0>([&result](auto idx) {
                result.insert(idx);
                return true;
            });
            StdSet universum = Universum(bset.Bits.size());
            RC_ASSERT(result == SetDifference(universum, bset.Indices));
        });
    }
    SECTION("Lazy iterators")
    {
        prop("LazyIndicesOf<1>(from, to)", [](BSet &&bset) {
            stat(bset);
            size_t from = *gen::inRange<size_t>(0, bset.Bits.size() - 1);
            size_t to = *gen::oneOf(gen::inRange<size_t>(from, bset.Bits.size()),
                                    gen::just(std::numeric_limits<size_t>::max()));
            auto result = ContainerOf<StdSet>(bset.Bits.LazyIndicesOf<1>(from, to));
            StdSet expected = bset.Indices;
            if (from > 0) {
                expected.erase(expected.begin(), expected.lower_bound(from));
            }
            if (!expected.empty() && to < *expected.rbegin()) {
                expected.erase(expected.upper_bound(to), expected.end());
            }
            RC_ASSERT(result == expected);
        });
        prop("LazyIndicesOf<0>(from)", [](BSet &&bset) {
            stat(bset);
            size_t from = *gen::inRange<size_t>(0, bset.Bits.size() - 1);
            auto result = ContainerOf<StdSet>(bset.Bits.LazyIndicesOf<0>(from));
            StdSet universum = Universum(bset.Bits.size());
            StdSet expected = SetDifference(universum, bset.Indices);
            if (from > 0) {
                expected.erase(expected.begin(), expected.lower_bound(from));
            }
            RC_ASSERT(result == expected);
        });
    }
    SECTION("Power of folded bitsets")
    {
        prop("power_of_and(arg,arg)", [](BSet &&bset1, BSet &&bset2) {
            stat(bset1);
            stat(bset2);
            auto result = BitVector::power_of_and(bset1.Bits, bset2.Bits);
            RC_ASSERT(result == SetIntersection(bset1.Indices, bset2.Indices).size());
        });
        prop("power_of_and(arg,arg,arg)", [](BSet &&bset1, BSet &&bset2, BSet &&bset3) {
            stat(bset1);
            stat(bset2);
            stat(bset3);
            auto result = BitVector::power_of_and(bset1.Bits, bset2.Bits, bset3.Bits);
            RC_ASSERT(result == SetIntersection(bset1.Indices, bset2.Indices, bset3.Indices).size());
        });
        prop("power_of_or(arg,arg)", [](BSet &&bset1, BSet &&bset2) {
            stat(bset1);
            stat(bset2);
            auto result = BitVector::power_of_or(bset1.Bits, bset2.Bits);
            auto size = std::min(bset1.Bits.size(), bset2.Bits.size());
            auto universum = Universum(size);
            RC_ASSERT(result == SetIntersection(universum, SetUnion(bset1.Indices, bset2.Indices)).size());
        });
        prop("power_of_or(arg,arg,arg)", [](BSet &&bset1, BSet &&bset2, BSet &&bset3) {
            stat(bset1);
            stat(bset2);
            stat(bset3);
            auto result = BitVector::power_of_or(bset1.Bits, bset2.Bits, bset3.Bits);
            auto size = std::min(std::min(bset1.Bits.size(), bset2.Bits.size()), bset3.Bits.size());
            auto universum = Universum(size);
            RC_ASSERT(result ==
                      SetIntersection(universum, SetUnion(bset1.Indices, bset2.Indices, bset3.Indices)).size());
        });
        prop("power_of_xor(arg,arg)", [](BSet &&bset1, BSet &&bset2) {
            stat(bset1);
            stat(bset2);
            auto result = BitVector::power_of_xor(bset1.Bits, bset2.Bits);
            auto size = std::min(bset1.Bits.size(), bset2.Bits.size());
            auto universum = Universum(size);
            auto set_result = SetIntersection(universum, SetDifference(SetUnion(bset1.Indices, bset2.Indices),
                                                                       SetIntersection(bset1.Indices, bset2.Indices)));
            RC_ASSERT(result == set_result.size());
        });
        prop("power_of_xor(arg,arg,arg)", [](BSet &&bset1, BSet &&bset2, BSet &&bset3) {
            stat(bset1);
            stat(bset2);
            stat(bset3);
            auto result = BitVector::power_of_xor(bset1.Bits, bset2.Bits, bset3.Bits);
            auto size = std::min(std::min(bset1.Bits.size(), bset2.Bits.size()), bset3.Bits.size());
            auto universum = Universum(size);
            auto xor1 =
                SetDifference(SetUnion(bset1.Indices, bset2.Indices), SetIntersection(bset1.Indices, bset2.Indices));
            auto xor2 = SetDifference(SetUnion(xor1, bset3.Indices), SetIntersection(xor1, bset3.Indices));
            auto set_result = SetIntersection(universum, xor2);
            RC_ASSERT(result == set_result.size());
        });
        prop("power_of_and_not(arg,arg)", [](BSet &&bset1, BSet &&bset2) {
            stat(bset1);
            stat(bset2);
            auto result = BitVector::power_of_and_not(bset1.Bits, bset2.Bits);
            auto size = std::min(bset1.Bits.size(), bset2.Bits.size());
            auto universum = Universum(size);
            auto set_result = SetIntersection(universum, SetDifference(bset1.Indices, bset2.Indices));
            RC_ASSERT(result == set_result.size());
        });
    }
    SECTION("Lazy iterators over folded bitsets")
    {
        prop("lazy_and_then_indices_of<1>()", [](BSet &&bset1, BSet &&bset2, BSet &&bset3) {
            stat(bset1);
            stat(bset2);
            stat(bset3);
            auto result =
                ContainerOf<StdSet>(BitVector::lazy_and_then_indices_of<1>(bset1.Bits, bset2.Bits, bset3.Bits));
            auto size = std::min(std::min(bset1.Bits.size(), bset2.Bits.size()), bset3.Bits.size());
            auto universum = Universum(size);
            auto set_result = SetIntersection(universum, bset1.Indices, bset2.Indices, bset3.Indices);
            RC_ASSERT(result == set_result);
        });
        prop("lazy_and_then_indices_of<0>()", [](BSet &&bset1, BSet &&bset2, BSet &&bset3) {
            stat(bset1);
            stat(bset2);
            stat(bset3);
            auto result =
                ContainerOf<StdSet>(BitVector::lazy_and_then_indices_of<0>(bset1.Bits, bset2.Bits, bset3.Bits));
            auto size = std::min(std::min(bset1.Bits.size(), bset2.Bits.size()), bset3.Bits.size());
            auto universum = Universum(size);
            auto set_result = SetDifference(universum, SetIntersection(bset1.Indices, bset2.Indices, bset3.Indices));
            RC_ASSERT(result == set_result);
        });
        prop("lazy_or_then_indices_of<1>()", [](BSet &&bset1, BSet &&bset2, BSet &&bset3) {
            stat(bset1);
            stat(bset2);
            stat(bset3);
            auto result =
                ContainerOf<StdSet>(BitVector::lazy_or_then_indices_of<1>(bset1.Bits, bset2.Bits, bset3.Bits));
            auto size = std::min(std::min(bset1.Bits.size(), bset2.Bits.size()), bset3.Bits.size());
            auto universum = Universum(size);
            auto set_result = SetIntersection(universum, SetUnion(bset1.Indices, bset2.Indices, bset3.Indices));
            RC_ASSERT(result == set_result);
        });
        prop("lazy_or_then_indices_of<0>()", [](BSet &&bset1, BSet &&bset2, BSet &&bset3) {
            stat(bset1);
            stat(bset2);
            stat(bset3);
            auto result =
                ContainerOf<StdSet>(BitVector::lazy_or_then_indices_of<0>(bset1.Bits, bset2.Bits, bset3.Bits));
            auto size = std::min(std::min(bset1.Bits.size(), bset2.Bits.size()), bset3.Bits.size());
            auto universum = Universum(size);
            auto set_result = SetDifference(universum, SetUnion(bset1.Indices, bset2.Indices, bset3.Indices));
            RC_ASSERT(result == set_result);
        });
        prop("lazy_xor_then_indices_of<1>()", [](BSet &&bset1, BSet &&bset2, BSet &&bset3) {
            stat(bset1);
            stat(bset2);
            stat(bset3);
            auto result =
                ContainerOf<StdSet>(BitVector::lazy_xor_then_indices_of<1>(bset1.Bits, bset2.Bits, bset3.Bits));
            auto size = std::min(std::min(bset1.Bits.size(), bset2.Bits.size()), bset3.Bits.size());
            auto universum = Universum(size);
            auto xor1 =
                SetDifference(SetUnion(bset1.Indices, bset2.Indices), SetIntersection(bset1.Indices, bset2.Indices));
            auto xor2 = SetDifference(SetUnion(xor1, bset3.Indices), SetIntersection(xor1, bset3.Indices));
            auto set_result = SetIntersection(universum, xor2);
            RC_ASSERT(result == set_result);
        });
        prop("lazy_xor_then_indices_of<0>()", [](BSet &&bset1, BSet &&bset2, BSet &&bset3) {
            stat(bset1);
            stat(bset2);
            stat(bset3);
            auto result =
                ContainerOf<StdSet>(BitVector::lazy_xor_then_indices_of<0>(bset1.Bits, bset2.Bits, bset3.Bits));
            auto size = std::min(std::min(bset1.Bits.size(), bset2.Bits.size()), bset3.Bits.size());
            auto universum = Universum(size);
            auto xor1 =
                SetDifference(SetUnion(bset1.Indices, bset2.Indices), SetIntersection(bset1.Indices, bset2.Indices));
            auto xor2 = SetDifference(SetUnion(xor1, bset3.Indices), SetIntersection(xor1, bset3.Indices));
            auto set_result = SetDifference(universum, xor2);
            RC_ASSERT(result == set_result);
        });
        prop("lazy_and_not_then_indices_of<1>()", [](BSet &&bset1, BSet &&bset2, BSet &&bset3) {
            stat(bset1);
            stat(bset2);
            stat(bset3);
            auto result =
                ContainerOf<StdSet>(BitVector::lazy_and_not_then_indices_of<1>(bset1.Bits, bset2.Bits, bset3.Bits));
            auto size = std::min(std::min(bset1.Bits.size(), bset2.Bits.size()), bset3.Bits.size());
            auto universum = Universum(size);
            auto set_and = SetIntersection(bset1.Indices, bset2.Indices);
            auto set_not = SetDifference(universum, bset3.Indices);
            auto set_result = SetIntersection(set_and, set_not);
            RC_ASSERT(result == set_result);
        });
        prop("lazy_and_not_then_indices_of<0>()", [](BSet &&bset1, BSet &&bset2, BSet &&bset3) {
            stat(bset1);
            stat(bset2);
            stat(bset3);
            auto result =
                ContainerOf<StdSet>(BitVector::lazy_and_not_then_indices_of<0>(bset1.Bits, bset2.Bits, bset3.Bits));
            auto size = std::min(std::min(bset1.Bits.size(), bset2.Bits.size()), bset3.Bits.size());
            auto universum = Universum(size);
            auto set_and = SetIntersection(bset1.Indices, bset2.Indices);
            auto set_not = SetDifference(universum, bset3.Indices);
            auto set_result = SetDifference(universum, SetIntersection(set_and, set_not));
            RC_ASSERT(result == set_result);
        });
    }
}

#endif  // !PANDA_CATCH2

}  // namespace panda::verifier::test
